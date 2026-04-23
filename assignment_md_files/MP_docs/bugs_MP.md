# MP bugs

No current MP EDF bugs are documented within the Task 4 scope.

## Bugs fixed during development

- Partitioned EDF originally failed during scheduler startup because the SMP idle tasks were created through the stock task-creation path without a one-hot core assignment. In global EDF this was harmless because unrestricted affinity is valid in the global ready queue, but in partitioned EDF every runnable task must belong to exactly one core queue. As a result, `prvPartEDFAddTaskToReadyList()` decoded the idle task affinity as invalid and hit the `configASSERT` that checks for a valid assigned core. The fix was to create each idle task with an explicit core affinity in `prvCreateIdleTasks()`, using the affinity-aware create APIs so idle task `i` is pinned to core `i` before it enters the partitioned ready list.

- After the idle-task fix, partitioned EDF still failed during scheduler startup when the timer service task was created. `xTimerCreateTimerTask()` already used the SMP affinity-aware create API, but because `configTIMER_SERVICE_TASK_CORE_AFFINITY` was left undefined it defaulted to `tskNO_AFFINITY`. That default is valid in global EDF but invalid in partitioned EDF for the same reason as above: the task cannot be inserted into a per-core partition queue without a unique assigned core. The fix was to define `configTIMER_SERVICE_TASK_CORE_AFFINITY` in `FreeRTOSConfig.h` and pin the timer service task to core 0.
