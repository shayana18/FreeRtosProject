# SRP Hardware Testing Setup
- SRP uses the same GPIO trace approach as EDF tests: task-code pins, a task-switch strobe, and a deadline-miss indicator.
- UART plus GPIO tracing is used so scheduler/resource behavior can be matched against expected timing.
- Deadline-miss GPIO can be sampled directly (or via LED) to confirm deadline misses. Plain WCET overruns are observed through UART hook prints.

# SRP Software Testing Setup
- SRP tests are in the `srp_tests` folder.
- The captured hardware assets for this document live under `assignment_md_files/test_assets/SRP Tests/`.
- Each test exposes an `srp_X_run()` entry point called from `main.c`.
- To run SRP tests, keep `configUSE_EDF == 1` and `configUSE_SRP == 1` in `schedulingConfig.h`, then select one test in `main.c`.
- `configUSE_SRP_SHARED_STACKS` toggles storage backend for comparing per-task stacks vs shared-stack mode.
- Shared helpers and trace utilities come from `test_utils.c/h` and `task_trace.c/h`.

# SRP Test Methods
- Task IDs are observed on the GPIO task-code pins and decoded using the task-switch strobe as a sampling clock. On the waveform, the strobe edge marks when the binary task-code output should be read.
- In the normal binary-trace setup, up to 7 GPIO bits can be used for task ID output. When a given test has fewer tasks and deadline-miss observation is important, the highest binary-output pin may instead be repurposed as a direct deadline-miss probe.
- When a test has very few tasks, one-hot encoding may be used instead of binary encoding to make the waveform easier to read directly.
- For precise timing analysis, the captured discussion in this document focuses on smaller windows that are sufficient to prove the intended scheduling or synchronization feature. We intentionally do not require full-hyperperiod inspection for every test when a shorter interval already demonstrates correctness.
- First validate scheduling/resource protocol behavior with shared stacks disabled (`configUSE_SRP_SHARED_STACKS = 0`).
- For lock/ceiling tests, compare GPIO waveform to the expected ordering.
- Repeat with shared stacks enabled (`configUSE_SRP_SHARED_STACKS = 1`) to isolate stack-backend effects.
- Use UART output captures where included for admission and deadline-miss cleanup tests.

# SRP Test Cases + Results

## Non-Stack Sharing

### Test 1 - Basic SRP Scheduling with Binary Semaphores
#### Test Implementation
- File: `srp_tests/test_1.c`
- Entry point: `srp_1_run()`
- Run with `configUSE_SRP_SHARED_STACKS = 0`.

#### Trace Task IDs
- `T1 -> 1`
- `T2 -> 2`
- `T3 -> 4`

#### Expected
- The earliest eligible task should run first.
- SRP ceilings should prevent resource-induced inversion.
- No deadline misses should occur over the observed interval.

#### Measured Waveform
![Test 1 SRP Non-Stack Sharing Waveform](<../test_assets/SRP Tests/Non-Stack Sharing/Test 1/Test 1 SRP Non-Stack Sharing Waveform.png>)

#### Expected Waveform
![Test 1 SRP Non-Stack Sharing Answer](<../test_assets/SRP Tests/Non-Stack Sharing/Test 1/Test 1 SRP Non-Stack Sharing Answer.png>)

### Test 3 - SRP Admission with Blocking Terms
#### Test Implementation
- File: `srp_tests/test_3.c`
- Entry point: `srp_3_run()`
- Run with `configUSE_SRP_SHARED_STACKS = 0`.

#### Trace Task IDs
- `Base task -> 1`
- `Good candidate -> 2`
- `Bad candidate` is expected to be rejected and should not appear on the trace pins

#### Expected
- The blocking-aware bad candidate should be rejected.
- The good candidate should be admitted.
- Only admitted tasks should appear in the observed execution and console output.

#### Measured Waveform
![Test 3 SRP Non-Stack Sharing Waveform](<../test_assets/SRP Tests/Non-Stack Sharing/Test 3/Test 3 SRP Non-Stack Sharing Waveform.png>)

#### Console Output
![Test 3 SRP Non-Stack Sharing Output](<../test_assets/SRP Tests/Non-Stack Sharing/Test 3/Test 3 SRP Non-Stack Sharing Output.png>)

### Test 5 - Deadline Miss in Critical Section
#### Test Implementation
- File: `srp_tests/test_5.c`
- Entry point: `srp_5_run()`
- Run with `configUSE_SRP_SHARED_STACKS = 0`.

#### Trace Task IDs
- `Overrun task -> 1`
- `Observer task -> 2`

#### Expected
- The overrun task should trigger a WCET-overrun print and then a deadline miss while still holding `R1`.
- Kernel deadline-miss cleanup should force-release the resource.
- The observer should later acquire `R1`, and the original owner should observe that the resource was already released.

#### Notes
- This test flushes deferred WCET/deadline UART output immediately before `xTaskDelayUntil()`. If that flush work makes `xTaskDelayUntil()` return late, the task re-anchors `xLastWakeTime` to the current tick. Later printed release/deadline values can therefore drift away from exact period multiples even though the sequence remains deterministic.

#### Measured Waveform
![Test 5 SRP Non-Stack Sharing Waveform](<../test_assets/SRP Tests/Non-Stack Sharing/Test 5/Test 5 SRP Non-Stack Sharing Waveform.png>)

#### Console Output
![Test 5 SRP Non-Stack Sharing Output](<../test_assets/SRP Tests/Non-Stack Sharing/Test 5/Test 5 SRP Non-Stack Sharing Output.png>)

## Stack Sharing

### Test 2 - SRP Ceiling Blocking with Constrained Task
#### Test Implementation
- File: `srp_tests/test_2.c`
- Entry point: `srp_2_run()`
- Run with `configUSE_SRP_SHARED_STACKS = 1`.

#### Trace Task IDs
- `T1 -> 1`
- `T2 -> 2`
- `T3 -> 4`
- `T4 -> 8`
- `T5 -> 16`
- `T6 -> 32`

#### Expected
- The shorter-deadline constrained task should remain blocked while the SRP ceiling is active.
- It should only run once the conflicting resource is released.

#### Measured Waveform
![Test 2 SRP Stack Sharing Waveform](<../test_assets/SRP Tests/Stack Sharing/Test 2/Test 2 SRP Stack Sharing Waveform.png>)

### Test 4 - Shared-Stack Quantitative Study
#### Test Implementation
- File: `srp_tests/test_4.c`
- Entry point: `srp_4_run()`
- Run with `configUSE_SRP_SHARED_STACKS = 1`.

#### Trace Task IDs
- `A0 -> 1`
- `A1 -> 2`
- `A2 -> 3`
- `A3 -> 4`
- `B0 -> 5`
- `B1 -> 6`
- `B2 -> 7`
- `B3 -> 8`
- `C0 -> 9`
- `C1 -> 10`
- `C2 -> 11`
- `C3 -> 12`

#### Expected
- The built-in SRP stack-usage helper/API path should report both theoretical and runtime stack usage.
- Shared-stack mode should show lower theoretical allocation than per-task stack mode for this task set.
- Scheduler behavior should remain stable while those stats are sampled.

#### Measured Waveform
![Test 4 SRP Stack Sharing Waveform](<../test_assets/SRP Tests/Stack Sharing/Test 4/Test 4 SRP Stack Sharing Waveform.png>)

#### Console Output
![Test 4 SRP Stack Sharing Output](<../test_assets/SRP Tests/Stack Sharing/Test 4/Test 4 SRP Stack Sharing Output.png>)
