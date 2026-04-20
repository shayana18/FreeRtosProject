# MP Hardware Testing Setup
- In MP mode, both cores may switch tasks close together, so a single shared binary task-code bus is no longer safe for clear waveform interpretation.
- To avoid races on the trace output, GPIO ownership should be partitioned by core:
  - core 0 task-code bank: GPIO `2`, `3`, `4`
  - core 0 task-switch strobe: GPIO `5`
  - core 1 task-code bank: GPIO `6`, `7`, `8`
  - core 1 task-switch strobe: GPIO `9`
- A shared clock or switch pin is not recommended in MP mode. If both cores toggle the same strobe line, two logically separate switch events can collapse into one unreadable waveform. For that reason, this MP test plan uses one strobe pin per core.
- The deadline-miss indicator can continue to use a separate dedicated GPIO, such as GPIO `15`, because it is not encoding per-core task identity.
- With only three task-code pins per core, MP test task tags should stay in the range `0..7` so each core's displayed task ID remains readable in binary on the logic analyzer.

# MP Test Plan

## Global EDF basic dispatch

Use case:
- verify that the two earliest-deadline ready jobs are dispatched simultaneously on the two cores

Expected observation:
- one valid task code appears on core 0's GPIO bank
- one valid task code appears on core 1's GPIO bank
- the two displayed tasks should be the two earliest-deadline jobs in the test set

## Global EDF preemption

Use case:
- verify that when an earlier-deadline job is released, the kernel requests a reschedule on the correct core and replaces the currently running worse job

Expected observation:
- after the earlier-deadline task is released, one core's displayed task code changes immediately
- the replacement should match the earlier-deadline task

## Global EDF migration

Use case:
- verify that unrestricted global EDF tasks are not pinned to one core and may execute on different cores across different jobs

Expected observation:
- structure releases so the same task can appear on core 0 in one job and later on core 1 in another job
- this demonstrates migration under global EDF rather than static partition assignment

## Partitioned EDF basic

Use case:
- verify that partitioned EDF dispatches only from the assigned core's partition queue

Expected observation:
- each task should only ever appear on the GPIO bank of its assigned core
- no task should spontaneously appear on the other core's bank unless its affinity is explicitly changed

## Partitioned EDF explicit migration

Use case:
- verify that `vTaskCoreAffinitySet()` moves a partitioned EDF task into a new partition and that future jobs run only on the new core

Expected observation:
- a controller task changes the task's one-hot affinity mask
- after migration, the task stops appearing on the old core's GPIO bank
- later executions appear only on the new core's bank

## Admission control

Timing note:
- In admission tests that use a separate controller task to create tasks at runtime, observed creation timing is not always exactly as predicted.
- If the controller task has a large deadline, it can be delayed while other runnable tasks execute.

Use case:
- verify acceptance and rejection behavior without overloading the GPIO trace with non-scheduling metadata

Method:
- reuse the same style as `edf_tests/test_3.c`
- store create-time results in `volatile` variables
- use GPIO only to validate runtime scheduling of accepted tasks

## Run-time task creation

Use case:
- verify that MP EDF admission, registration, ready-list insertion, and preemption still work when tasks are created after the scheduler has already started

Expected observation:
- a controller task creates a new periodic task at runtime
- if admitted, it should appear on the appropriate core bank(s) and preempt according to the active MP EDF policy
- if rejected, the `volatile` result flag should record failure and no new GPIO task code should appear

## Two-task partitioned first-fit test

Use case:
- verify the simple online partition placement rule with an easy hand-checkable case

Method:
- create two partitioned EDF tasks whose combined utilization still fits on one core
- the expected placement is that both are accepted onto core 0 by the current first-fit style placement rule

Expected observation:
- both task codes appear only on core 0's GPIO bank
- core 1 should remain idle for this test

## Dhall-effect test

Use case:
- demonstrate that MP EDF admission uses conservative sufficient bounds rather than exact schedulability tests

Method:
- create a task set that illustrates the gap between raw multicore capacity and the implemented sufficient admission condition
- the point of the test is not to prove an exact Dhall impossibility result, but to show that the chosen admission control intentionally limits what the kernel accepts

Expected observation:
- the task set is rejected by admission control
- the rejection is recorded through `volatile` result flags

## Near-full-utilization scheduling

Use case:
- verify that the MP EDF implementation remains stable when utilization is close to the implemented admission bound

Expected observation:
- tasks continue to alternate correctly on the two core banks
- no unexpected deadline-miss signal should appear during the observation window

# MP Test Method

- For small MP task sets, derive the expected first several seconds of the schedule by hand and compare the observed core-0 and core-1 GPIO banks against that expected behavior.
- For migration and preemption tests, focus on the exact release instant that should trigger a reschedule and confirm that the correct core changes state.
- For admission-control tests, use `volatile` pass/fail variables as the primary result and use GPIO only to confirm the behavior of accepted tasks.
- For higher-utilization tests, prioritize checking for deadline-miss signals and correct core ownership of tasks rather than trying to hand-derive a very long exact schedule.
