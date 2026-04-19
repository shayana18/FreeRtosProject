# EDF kernel changes

- Added `schedulingConfig.h` flags so EDF can be enabled or disabled without changing the base FreeRTOS scheduler.
- Extended `TCB_t` with EDF metadata: absolute release time, period in ticks, WCET in ticks, current job execution ticks, relative deadline, absolute deadline, and an EDF registry list item.
- Mirrored the EDF fields in `StaticTask_t` so static allocation remains size-compatible with the modified `TCB_t`.
- Overloaded `xTaskCreate()` in EDF mode so the user provides period, WCET, and relative deadline instead of a fixed priority.
- Added EDF parameter validation to reject invalid task models where `T <= 0`, `C <= 0`, `D <= 0`, or `D > T`.
- Added EDF admission control at task creation time.
- Used utilization-based admission for implicit-deadline task sets.
- Used processor-demand analysis for constrained-deadline task sets.
- Added `xReadyEDFTasksList_UP` as a ready queue sorted by absolute deadline.
- Added `xEDFTaskRegistryList_UP` to keep track of all EDF tasks for admission control and periodic release metadata updates.
- Updated task-list initialization so the EDF ready list and EDF registry list are initialized with the rest of the kernel lists.
- Changed `prvAddTaskToReadyList` so EDF tasks are inserted into the EDF ready list in ascending order of absolute deadline instead of into fixed-priority ready queues.
- Modified `vTaskSwitchContext()` so EDF mode selects the head of the EDF ready list as the next running task.
- Updated `xTaskResumeAll()` so when deferred-ready tasks are released after scheduler suspension, the kernel requests a yield using absolute-deadline comparison instead of fixed-priority comparison; otherwise EDF-preemptible tasks could be resumed without triggering the correct context switch.

- Updated `xTaskDelayUntil()` so EDF tasks refresh absolute release time, absolute deadline, and per-job execution budget on each periodic release.
- Updated `xTaskIncrementTick()` so the currently running EDF task accumulates execution time each tick.
- Added WCET and deadline handling in `xTaskIncrementTick()` so a task that exhausts its budget or misses its deadline is delayed until its next release.
- Removed EDF tasks from the EDF registry list when they are deleted.

