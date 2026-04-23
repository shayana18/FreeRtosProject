# MP EDF kernel changes

This document summarizes the FreeRTOS changes made for multiprocessor EDF support. The same code base supports global EDF and partitioned EDF, but only one policy is active in a given build.

## Files changed

### `schedulingConfig.h`

- Added MP scheduler selection flags:
  - `configUSE_UP`
  - `configUSE_MP`
  - `GLOBAL_EDF_ENABLE`
  - `PARTITIONED_EDF_ENABLE`
- Added compile-time checks so:
  - UP and MP modes cannot both be enabled
  - global and partitioned EDF cannot both be enabled
  - MP EDF policy flags require `configUSE_MP == 1U` and `configUSE_EDF == 1U`

### `FreeRTOSConfig.h`

- Enabled SMP-specific FreeRTOS configuration when `configUSE_MP == 1U`.
- Set `configNUMBER_OF_CORES` to `2` for the Pico target in MP mode.
- Enabled core affinity support through `configUSE_CORE_AFFINITY`.
- Added `configTIMER_SERVICE_TASK_CORE_AFFINITY` so the timer service task is pinned to one valid core in partitioned EDF.

### `FreeRTOS-Kernel/include/task.h`

- Added the MP EDF `xTaskCreate()` overload that accepts period, WCET, relative deadline, and core affinity mask.
- Kept the stock FreeRTOS `xTaskCreate()` prototype active whenever `configUSE_EDF == 0U`.
- Reused the existing SMP affinity API surface for migration, especially `vTaskCoreAffinitySet()`.

### `FreeRTOS-Kernel/tasks.c`

- Added global EDF ready and registry lists:
  - `xReadyEDFTasksList_Glob_MP`
  - `xEDFTaskRegistryList_Glob_MP`
- Added partitioned EDF ready and registry lists:
  - `xReadyEDFTasksLists_Part_MP[ configNUMBER_OF_CORES ]`
  - `xEDFTaskRegistryLists_Part_MP[ configNUMBER_OF_CORES ]`
- Extended task creation, admission, ready-list insertion, context switching, tick accounting, and affinity changes for MP EDF.
- Updated idle-task creation so each idle task has a valid one-hot core affinity before it can enter a partitioned ready list.

### `task_trace.h` and `task_trace.c`

- Added MP-aware GPIO trace banks so each core has its own task-code pins and task-switch strobe.
- Kept the deadline-miss indicator on a separate shared pin.

### `main.c`

- Added guarded MP test selection so global EDF, partitioned EDF, and MP demo tests can be selected from the application entry point.

### `mp_tests/`

- Added MP-specific test sources for:
  - global EDF basic dispatch
  - global EDF preemption
  - global EDF migration
  - partitioned EDF basic dispatch
  - partitioned EDF explicit migration
  - MP admission control
  - MP runtime task creation
  - MP demo tests for global EDF, partitioned EDF best-fit, and conservative global admission

## New or substantially changed functions

### Admission and placement

- `prvGlobEDFUtilTestWithNew()`
  - Applies the sufficient global EDF utilization test `U_total <= m - (m - 1) U_max`.
  - This keeps admission control simple and safe, which fits the Task 4 requirement for a simple admission-control test.
- `prvPartEDFUtilTestWithNew()`
  - Applies the partitioned EDF admission checks.
  - Chooses a partition using online best-fit placement, which is simple enough to run during task creation and avoids obvious scan-order bias.
- `prvTestEDFUtilPerCoreWithNewQ32()`
  - Computes fixed-point utilization for a candidate task added to a given EDF registry list.
- `prvPartEDFCoreFromAffinityMask()`
  - Decodes and validates one-hot partition affinity masks.

### Ready-list and registry management

- `prvPartEDFAddTaskToReadyList()`
  - Inserts a task into the ready list for its assigned partition.
- `prvPartEDFMoveTaskToAssignedCore()`
  - Moves a partitioned EDF task between per-core ready/registry lists when its affinity changes.
- `prvPeriodicTaskAdvanceAndReinsert()`
  - Advances a periodic EDF job to its next release and reinserts the registry item in release order.
- `prvAddTaskToDelayedListForTask()`
  - Delays an explicit MP task during tick-time overrun/deadline handling, instead of relying on the UP-only current-task helper.

### Dispatch and preemption

- `prvSelectGlobEDFTaskForCore()`
  - Selects the earliest-deadline eligible task from the shared global EDF ready list.
- `prvSelectPartEDFTaskForCore()`
  - Selects the earliest-deadline task from the current core's partition ready list.
- `prvYieldForTask()`
  - Extended so MP EDF can target the correct core for preemption.
  - In global EDF, it may yield a different core than the one observing the wakeup.
  - In partitioned EDF, it yields only the task's assigned core.
- `vTaskSwitchContext()`
  - Extended so MP EDF dispatch uses the global or partitioned EDF selectors instead of stock priority selection.

### Task creation, migration, and timing

- `xTaskCreate()`
  - Added MP EDF overload with timing parameters and core affinity.
  - Runs admission control before inserting the task into EDF structures.
- `vTaskCoreAffinitySet()`
  - Extended so partitioned EDF affinity changes perform explicit partition migration.
- `xTaskIncrementTick()`
  - Extended so MP EDF accounts execution time for the task running on each core.
  - Handles WCET exhaustion and deadline miss by advancing the job, delaying the task, and yielding the affected core.
- `xTaskDelayUntil()`
  - Updated so EDF release/deadline metadata is advanced in the correct UP, global, or partitioned EDF registry list.

## Design impact

The implementation keeps MP EDF integrated with FreeRTOS's existing SMP machinery rather than creating a separate scheduler task. There is still one kernel and one context-switch path; the part that changes is the task selector used by that path.

Global EDF uses one shared scheduling domain. Partitioned EDF uses per-core scheduling domains. Both policies share the same task metadata, timing model, and tick-time overrun handling.
