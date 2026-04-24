# MP Hardware Testing Setup
- MP mode uses per-core GPIO task-code banks plus per-core switch strobes to avoid ambiguity during near-simultaneous switches.
- Core 0 task-code bank: GPIO `2,3,4`; core 0 switch strobe: GPIO `5`.
- Core 1 task-code bank: GPIO `6,7,8`; core 1 switch strobe: GPIO `9`.
- Shared deadline-miss indicator: GPIO `15`.

# MP Software Testing Setup
- MP tests are in the `mp_tests` tree.
- The captured hardware assets for this document live under `assignment_md_files/test_assets/MP Tests/`.
- Build mode must match policy under test:
  - Global EDF: `GLOBAL_EDF_ENABLE=1`, `PARTITIONED_EDF_ENABLE=0`
  - Partitioned EDF: `GLOBAL_EDF_ENABLE=0`, `PARTITIONED_EDF_ENABLE=1`
- Admission tests additionally record `volatile` result flags for `xTaskCreate()` outcomes.
- For CBS-style aperiodic activity elsewhere in the project, the server/worker task has the task ID in traces; individual submitted jobs do not. MP tests in this document are ordinary MP EDF tests, not per-job CBS traces.

# MP Test Methods
- Task IDs are observed on the GPIO task-code pins and decoded using the task-switch strobe as a sampling clock. On the waveform, the strobe edge marks when the binary task-code output should be read.
- In the normal binary-trace setup, up to 7 GPIO bits can be used for task ID output. When a given test has fewer tasks and deadline-miss observation is important, the highest binary-output pin may instead be repurposed as a direct deadline-miss probe.
- When a test has very few tasks, one-hot encoding may be used instead of binary encoding to make the waveform easier to read directly.
- For precise timing analysis, the captured discussion in this document focuses on smaller windows that are sufficient to prove the intended scheduling or synchronization feature. We intentionally do not require full-hyperperiod inspection for every test when a shorter interval already demonstrates correctness.
- Use GPIO waveforms as primary evidence for dispatch, preemption, migration, and core ownership.
- Use UART output captures where included for overrun/deadline-miss tests.
- For admission tests, pair waveform analysis with the saved result-flag captures.
- Validate each policy in its correct build mode before hardware-run interpretation.

# MP Test Cases + Results

## Global EDF Tests

### Test 1 - Basic Dispatch
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_1.c`
- Entry point: `mp_global_edf_1_run()`

#### Trace Task IDs
- `G1 -> 1`
- `G2 -> 2`
- `G3 -> 4`

#### Expected
- At startup, the two earliest-deadline runnable jobs should occupy the two cores.
- Worse-deadline jobs should wait until one core becomes free.

#### Measured Waveform
![Test 1 MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Test 1/Test 1 MP Global Waveform.png>)

### Test 2 - Preemption
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_2.c`
- Entry point: `mp_global_edf_2_run()`

#### Trace Task IDs
- `G1 -> 1`
- `G2 -> 2`
- `G3 -> 4`

#### Expected
- An arriving earlier-deadline job should preempt a worse currently running job.
- The displaced task should resume later when it becomes eligible again.

#### Measured Waveform
![Test 2 MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Test 2/Test 2 MP Global Waveform.png>)

### Test 3 - Migration
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_3.c`
- Entry point: `mp_global_edf_3_run()`

#### Trace Task IDs
- `Migrator M -> 1`
- `Blocker B0 -> 2`
- `Blocker B1 -> 4`

#### Expected
- A globally runnable task may execute on different cores across different jobs.
- Migration should still preserve EDF ordering among runnable jobs.

#### Measured Waveform
![Test 3 MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Test 3/Test 3 MP Global Waveform.png>)

### Test 4 - Overrun and Deadline Miss Trace
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_4.c`
- Entry point: `mp_global_edf_4_run()`

#### Trace Task IDs
- `G4 OVR -> 1`
- `G4 N1 -> 2`
- `G4 N2 -> 4`

#### Expected
- The designated overrun task should trigger a WCET-overrun print first and then a deadline-miss print.
- The deadline-miss GPIO should pulse when the miss occurs.
- The scheduler should continue running after the miss.

#### Notes
- This test flushes deferred WCET/deadline/MP UART output immediately before `xTaskDelayUntil()`. If the flush work makes `xTaskDelayUntil()` return late, the task re-anchors `xLastWakeTime` to the current tick. Later printed release/deadline/next-release values can therefore drift away from exact period multiples even though the progression remains deterministic.

#### Measured Waveform
![Test 4 MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Test 4/Test 4 MP Global Waveform.png>)

#### Console Output
![Test 4 MP Global Output](<../test_assets/MP Tests/Global EDF Tests/Test 4/Test 4 MP Global Output.png>)

### Test 5 - Affinity Enforcement
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_5.c`
- Entry point: `mp_global_edf_5_run()`

#### Trace Task IDs
- `G5 Free0 -> 1`
- `G5 Core0 -> 2`
- `G5 Core1 -> 4`
- `G5 Free1 -> 8`

#### Expected
- Pinned tasks should stay on their assigned core for the full run.
- Unrestricted tasks may appear on either core.

#### Measured Waveform
![Test 5 MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Test 5/Test 5 MP Global Waveform.png>)

### Dhall Test - Conservative Global Admission
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_dhall.c`
- Entry point: `mp_test_dhall_run()`

#### Trace Task IDs
- `Heavy -> 1`
- `Light -> 2`
- `Bad candidate` is expected to be rejected and should not appear on the trace pins

#### Expected
- A candidate that violates the configured sufficient bound should be rejected even if raw total utilization still appears below total capacity.
- The already-admitted tasks should continue running normally.

#### Measured Waveform
![Dhall Test MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Dhall Test/Dhall Test MP Global Waveform.png>)

## Partitioned EDF Tests

### Test 1 - Basic Dispatch
#### Test Implementation
- File: `mp_tests/partitioned_edf_tests/test_1.c`
- Entry point: `mp_partitioned_edf_1_run()`

#### Trace Task IDs
- `P0A -> 1`
- `P0B -> 2`
- `P1A -> 4`

#### Expected
- Each task should execute only on its assigned partition/core.
- No cross-partition execution should occur without an explicit affinity change.

#### Measured Waveform
![Test 1 MP Partitioned Waveform](<../test_assets/MP Tests/Partitioned EDF Tests/Test 1/Test 1 MP Partitioned Waveform.png>)

### Test 2 - Explicit Migration
#### Test Implementation
- File: `mp_tests/partitioned_edf_tests/test_2.c`
- Entry point: `mp_partitioned_edf_2_run()`

#### Trace Task IDs
- `Migrating task -> 1`
- `Core-0 background -> 2`
- `Core-1 background -> 4`

#### Expected
- Before the runtime affinity change, the migrating task should appear only on its original core.
- In this implementation, the currently executing job may finish on the original core after migration is requested.
- The migrated affinity should become visible on the next released job.

#### Measured Waveform
![Test 2 MP Partitioned Waveform](<../test_assets/MP Tests/Partitioned EDF Tests/Test 2/Test 2 MP Partitioned Waveform.png>)

### Test 3 - Overrun and Deadline Miss Trace
#### Test Implementation
- File: `mp_tests/partitioned_edf_tests/test_3.c`
- Entry point: `mp_partitioned_edf_3_run()`

#### Trace Task IDs
- `P3 C0O -> 1`
- `P3 C0N -> 2`
- `P3 C1O -> 4`
- `P3 C1N -> 3`

#### Expected
- The designated overrun tasks should trigger WCET-overrun prints and then deadline-miss prints.
- The deadline-miss GPIO should assert on real misses.
- Partition ownership should continue to hold after recovery.

#### Notes
- This test also flushes deferred WCET/deadline/MP UART output immediately before `xTaskDelayUntil()`. If the flush work makes `xTaskDelayUntil()` return late, the task re-anchors `xLastWakeTime` to the current tick. Later printed release/deadline/next-release values can therefore drift away from exact period multiples even though the progression remains deterministic.

#### Measured Waveform
![Test 3 MP Partitioned Waveform](<../test_assets/MP Tests/Partitioned EDF Tests/Test 3/Test 3 MP Partitioned Waveform.png>)

#### Console Output
![Test 3 MP Partitioned Output](<../test_assets/MP Tests/Partitioned EDF Tests/Test 3/Test 3 MP Partitioned Output.png>)

## Admission Tests

### Test 1 - Startup Admission
#### Test Implementation
- Shared MP admission test assets captured for both policy modes.

#### Trace Task IDs
- `Base 1 -> 1`
- `Base 2 -> 2`
- `Good candidate -> 4` when admitted after the delayed create
- `Admission controller -> 7`
- `Bad candidate` is expected to be rejected and should not appear on the trace pins

#### Expected
- Unschedulable candidates should be rejected.
- Schedulable candidates should be admitted.
- The waveform and result flags should agree.

#### Global EDF Waveform
![Test 1 MP Admission Global Waveform](<../test_assets/MP Tests/Admission Tests/Test 1 MP Admission Global Waveform.png>)

#### Global EDF Result Flags
![Test 1 MP Admission Global Result Flags](<../test_assets/MP Tests/Admission Tests/Test 1 MP Admission Global Result Flags.png>)

#### Partitioned EDF Waveform
![Test 1 MP Admission Partitioned Waveform](<../test_assets/MP Tests/Admission Tests/Test 1 MP Admission Partitioned Waveform.png>)

#### Partitioned EDF Result Flags
![Test 1 MP Admission Partitioned Result Flags](<../test_assets/MP Tests/Admission Tests/Test 1 MP Admission Partitioned Result Flags.png>)

### Test 2 - Runtime Admission / Runtime Create
#### Test Implementation
- Shared MP runtime admission/create test assets captured for both policy modes.

#### Trace Task IDs
- `Base 1 -> 1`
- `Base 2 -> 2`
- `Good runtime task -> 4` when admitted at runtime
- `Controller -> 7`
- `Bad runtime task` is expected to be rejected and should not appear on the trace pins

#### Expected
- Runtime creation should admit feasible tasks and reject infeasible ones without stalling the scheduler.
- The saved flags should match the observed core activity.

#### Global EDF Waveform
![Test 2 MP Admission Global Waveform](<../test_assets/MP Tests/Admission Tests/Test 2 MP Admission Global Waveform.png>)

#### Global EDF Result Flag
![Test 2 MP Admission Global Result Flag](<../test_assets/MP Tests/Admission Tests/Test 2 MP Admission Global Result Flag.png>)

#### Partitioned EDF Waveform
![Test 2 MP Admission Partitioned Waveform](<../test_assets/MP Tests/Admission Tests/Test 2 MP Admission Partitioned Waveform.png>)

#### Partitioned EDF Result Flags
![Test 2 MP Admission Partitioned Result Flags](<../test_assets/MP Tests/Admission Tests/Test 2 MP Admission Partitioned Result Flags.png>)

## Comparison Test

### Global EDF Comparison
#### Test Implementation
- File: `mp_tests/test_compare_glob.c`
- Entry point: `mp_compare_glob_run()`

#### Trace Task IDs
- `GDemoA -> 1`
- `GDemoB -> 2`
- `GDemoC -> 4`

#### Measured Waveform
![Comparison Test MP Global Waveform](<../test_assets/MP Tests/comparison test/Comparison Test MP Global Waveform.png>)

#### Annotation
![Comparison Test MP Global Caption](<../test_assets/MP Tests/comparison test/Comparison Test MP Global Caption.png>)

### Partitioned EDF Comparison
#### Test Implementation
- File: `mp_tests/test_compare_part.c`
- Entry point: `mp_compare_part_run()`

#### Trace Task IDs
- `PDemoA -> 1`
- `PDemoB -> 2`
- `PDemoC -> 4`

#### Measured Waveform
![Comparison Test MP Partitioned Waveform](<../test_assets/MP Tests/comparison test/Comparison Test MP Partitioned Waveform.png>)

#### Annotation
![Comparison Test MP Partitioned Caption](<../test_assets/MP Tests/comparison test/Comparison Test MP Partitioned Caption.png>)
