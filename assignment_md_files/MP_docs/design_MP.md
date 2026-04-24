# MP EDF design

This implementation extends the FreeRTOS SMP kernel with two implicit-deadline multiprocessor EDF policies: global EDF and partitioned EDF. The design keeps the mode split explicit:

- `configUSE_EDF == 0U`: stock FreeRTOS fixed-priority scheduling
- `configUSE_EDF == 1U` and `configUSE_UP == 1U`: uniprocessor EDF-family scheduling
- `configUSE_EDF == 1U` and `configUSE_MP == 1U`: multiprocessor EDF scheduling
- `PARTITIONED_EDF_ENABLE == 1U`: partitioned EDF
- `PARTITIONED_EDF_ENABLE == 0U`: global EDF by default

Global EDF is the default MP EDF policy because the assignment requires global EDF when no multicore algorithm is explicitly selected. The implementation therefore treats "MP EDF enabled and partitioned EDF disabled" as global EDF, even if `GLOBAL_EDF_ENABLE` is left as `0U`.

## Shared MP EDF model

Both MP policies reuse the EDF metadata already added to the task control block:

- absolute job release time
- period
- WCET
- current job execution count
- relative deadline
- absolute deadline
- EDF registry list item

The MP scheduler does not introduce a separate timing subsystem. The normal FreeRTOS tick path still drives job accounting through `xTaskIncrementTick()`. On every tick, the MP EDF path scans the currently running task on each core, increments its execution count, and checks whether the job has either reached its WCET boundary while still executing or actually missed its deadline. A WCET overrun only triggers the overrun hook. A real deadline miss advances the job to its next release, resets its execution count, delays the task until the next period, and forces the affected core to reschedule.

## Ready queues and registry lists

The main structural difference between global and partitioned EDF is where ready state is stored.

Global EDF uses one shared EDF ready queue and one shared EDF registry:

- `xReadyEDFTasksList_Glob_MP`
- `xEDFTaskRegistryList_Glob_MP`

Partitioned EDF uses one EDF ready queue and one EDF registry per core:

- `xReadyEDFTasksLists_Part_MP[ configNUMBER_OF_CORES ]`
- `xEDFTaskRegistryLists_Part_MP[ configNUMBER_OF_CORES ]`

The registry lists track admitted periodic tasks for admission and job-release accounting. The ready lists track runnable jobs in EDF order. The ready-list item is ordered by absolute deadline so the head of an EDF ready list is the earliest-deadline runnable job for that scheduling domain.

## Task creation and affinity

`xTaskCreate()` is the main MP EDF integration point. In MP EDF mode, the overloaded task-creation path accepts:

- task function and normal FreeRTOS task metadata
- period
- WCET
- relative deadline
- core affinity mask

At creation time, the kernel validates the periodic task model, converts timing values to ticks, applies admission control, initializes EDF metadata, resolves affinity, and inserts the task into the correct EDF ready and registry structures.

Affinity has different meaning in the two MP policies:

- In global EDF, affinity is an eligibility mask. A task with unrestricted affinity may run on any core, and a task with a restricted mask may only be selected by cores in that mask.
- In partitioned EDF, affinity is a partition assignment. A task must belong to exactly one core queue. If the caller passes `tskNO_AFFINITY`, the kernel chooses a partition during admission. If the caller passes an explicit affinity, it must decode to one valid core.

This distinction is important because global EDF permits migration, while partitioned EDF treats migration as an explicit movement from one partition to another.

## Admission control

The MP assignment only requires implicit-deadline periodic tasks, so admission is based on utilization.

For global EDF, the implementation uses the sufficient test:

```text
U_total <= m - (m - 1) U_max
```

where `m` is the number of cores, `U_total` is total task-set utilization, and `U_max` is the largest individual task utilization. This is conservative, so some feasible task sets may be rejected. We chose this approach because the assignment only asks for a simple admission-control test, and a necessary-and-sufficient global EDF test for multiprocessors would add much more analysis complexity than the project needs.

For partitioned EDF, the implementation combines a conservative total-utilization screen with a per-core fit check:

```text
U_total <= (m + 1) / 2
and
there exists an eligible core where U_core + U_new <= 1
```

If multiple cores can accept the task, partitioned EDF uses online best-fit placement: it chooses the eligible core with the highest post-insertion utilization that does not exceed `1.0`. This is not the only valid bin-packing choice, but it is a defensible one for runtime task creation because it is simple, deterministic, and less dependent on core scan order than first-fit.

For example, if the current core utilizations are `0.2` and `0.6` and a new task has utilization `0.3`, best-fit places it on the second core, producing `0.2` and `0.9`. That leaves one core largely free for a future larger task.

## Dispatch and preemption

Each core still enters the normal SMP `vTaskSwitchContext()` path, but the task selector is EDF-aware.

In global EDF:

- all runnable jobs are in one shared EDF ready list
- each core scans the shared list in deadline order
- a task is eligible only if it is not already running on another core and its affinity allows the current core
- the selected task is the earliest-deadline eligible task

In partitioned EDF:

- each core has its own EDF ready list
- each core selects only from its assigned partition
- unrestricted tasks are assigned to one partition during admission
- explicit migration moves the task between partition lists

The yield path is also EDF-aware. In partitioned EDF, a newly ready task only needs to be compared with the task currently running on its assigned core. In global EDF, a newly ready task may need to preempt a different core than the one that observed the wakeup, so `prvYieldForTask()` searches eligible running cores and targets the core running the worst current EDF job.

## Migration

Global EDF supports migration naturally because tasks are selected from a shared ready list. A task may run on one core during one job and another core during a later job if its affinity allows it. This includes job migration after preemption: a preempted job may later resume on a different eligible core.

Partitioned EDF supports explicit migration through affinity changes. `vTaskCoreAffinitySet()` validates the new one-hot partition assignment and moves the task between the old and new partition ready/registry lists when needed. Multi-bit affinity masks are rejected in partitioned EDF because a partitioned task must belong to exactly one queue.

## Interrupts, timers, and core startup

A separate periodic kernel timer per core is not required. The design uses one global tick source to advance FreeRTOS time and perform EDF job accounting. Cross-core preemption is handled through the existing SMP yield machinery, so the scheduler can request that another core reschedule when the EDF decision requires it.

Core startup uses the existing FreeRTOS SMP port model. The scheduler is started once, one idle task is created per core, and the port brings both cores into the scheduling loop. If a core has no useful work, it runs its idle task rather than being explicitly stopped.

## Summary of design choices

- Global EDF is implemented as one shared EDF scheduler over all cores.
- Partitioned EDF is implemented as one EDF queue per core inside the same kernel.
- Admission control is conservative because exact MP EDF feasibility is much more complex than the sufficient tests used here.
- Partitioned placement uses best-fit because task creation is online and best-fit is simple, deterministic, and less biased than first-fit.
- Timing and overrun handling stay in the normal FreeRTOS tick path.
- MP EDF assumes independent periodic tasks, matching the Task 4 scope.
