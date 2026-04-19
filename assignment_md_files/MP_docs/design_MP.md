# MP EDF design

This MP implementation extends the existing FreeRTOS SMP kernel with two EDF policies: global EDF and partitioned EDF. The design keeps the configuration split explicit: UP EDF, MP EDF, and stock fixed-priority scheduling are mutually exclusive modes, and the source branches directly on `configUSE_UP`, `configUSE_MP`, `configUSE_EDF`, and `PARTITIONED_EDF_ENABLE`. The assignment requires global EDF to be the default multicore policy, so the MP EDF code treats "partitioned disabled" as "global EDF enabled" rather than falling back to stock SMP fixed-priority scheduling.

The two MP EDF policies intentionally share the same kernel infrastructure where the behavior is mechanically identical, but they differ where the scheduling model actually changes. Both modes reuse the EDF timing metadata already attached to the TCB, use tick-time execution accounting, and trigger preemption through the SMP yield machinery. The main difference is where ready and registry state lives. Global EDF uses one shared ready queue and one shared registry for all processors. Partitioned EDF uses one ready queue and one registry per core, and tasks are treated as members of exactly one partition at a time.

`xTaskCreate()` is the main MP EDF integration point. In MP EDF mode it validates the periodic task model, converts timing parameters to ticks, resolves the caller's affinity request, runs the appropriate admission test, initializes EDF metadata, and inserts the task into the correct ready and registry lists. For global EDF the affinity mask is treated as an eligibility filter over the shared task set. For partitioned EDF the affinity mask is interpreted as a one-hot partition assignment; if no explicit core is given, placement is chosen during admission.

At dispatch time the SMP `vTaskSwitchContext()` path is EDF-aware. In partitioned EDF, each core selects from only its own per-core EDF ready queue. In global EDF, each core scans the shared EDF ready list in deadline order and chooses the earliest-deadline task that is not already running elsewhere and that is allowed on that core by affinity. The SMP yield path was also extended so a newly ready EDF task causes the correct core to reschedule: in partitioned EDF only the assigned core matters, while in global EDF the kernel may have to preempt a different core than the one that observed the wakeup event.

Periodic runtime behavior still flows through the normal FreeRTOS timing paths. `xTaskIncrementTick()` was extended so each running EDF task in MP mode accumulates execution time, detects budget completion or deadline miss, advances to the next release, and is delayed until the next job. This mirrors the UP EDF model rather than introducing a second independent timing subsystem for MP.

## Direct answers to the assignment design questions

### 1. Interrupt handling and timer functionality

A separate periodic kernel timer per core is not required. One shared kernel tick source is sufficient as long as the SMP port also provides per-core reschedule or yield signaling. That is the model used in this project:

- one global periodic tick drives `xTaskIncrementTick()`
- cross-core rescheduling is requested with `prvYieldCore()` / `portYIELD_CORE()`

This keeps time accounting global while still letting each core be interrupted when it must reschedule.

### 2. How to stop/start each core

This project does not implement dynamic CPU hotplug. It uses the existing SMP startup path in the FreeRTOS port:

- one idle task is created per core
- each core starts by running its idle task
- `xPortStartScheduler()` hands control to the SMP port

When a core has no useful work, it runs its idle task rather than being explicitly powered down. Stopping the scheduler is global, not per-core.

### 3. Dispatching tasks to certain cores, and migration

Core-specific dispatch is handled through `uxCoreAffinityMask` and EDF mode-specific ready queues.

- In global EDF, affinity is an eligibility mask over the shared ready set. Jobs may migrate automatically across cores if their affinity allows it.
- In partitioned EDF, affinity is treated as a one-hot assignment to one partition. A change in affinity is interpreted as an explicit partition migration, and the task is moved between the old core's ready/registry lists and the new core's ready/registry lists.

This means partitioned EDF supports explicit migration, while global EDF supports both task migration and job migration as part of normal scheduling.

### 4. Separate scheduler per core?

Conceptually:

- partitioned EDF: yes
- global EDF: no

What "separate scheduler per core" means here is not "spawn a different scheduler task on each core." It means each core makes its decision from a local ready queue that contains only tasks assigned to that partition. That is exactly how the partitioned EDF path in this project is structured: each core has its own EDF ready queue and its own EDF registry, and the local selector only looks at that core's queue.

Global EDF is different. There is still one kernel and one context-switch function, but logically there is one shared scheduler over all cores because:

- there is one shared EDF ready queue
- one core's dispatch decision depends on what is already running on the other core
- preemption can target another core if that core is running the worst eligible current job

So the clean answer is:

- partitioned EDF behaves like one local EDF scheduler per core, implemented inside one shared kernel
- global EDF behaves like one shared EDF scheduler across all cores
