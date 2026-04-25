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
- NOTE: all timings are relative to the first scheduled job at `t = 0`. This avoids mixing scheduler behavior with startup latency and capture offsets between runs.

# MP Test Cases + Results

## Global EDF Tests

### Test 1 - Basic Implicit Deadline Dispatch
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_1.c`
- Entry point: `mp_global_edf_1_run()`

#### Task Set
- G1: T=4000 ms, C=1200 ms, D=T
- G2: T=5000 ms, C=1200 ms, D=T
- G3: T=9000 ms, C=700 ms,  D=T

#### Trace Task IDs
- `G1 -> 1`
- `G2 -> 2`
- `G3 -> 4`

#### Expected Results:
- `t = 0`: `G1` (`D=4000`) and `G2` (`D=5000`) dispatch on the two cores; `G3` (`D=9000`) must wait.
- `0-1200 ms`: only task IDs `1` and `2` should be visible; either ID may be on either core because both tasks are unrestricted.
- `~1200 ms`: one of `G1/G2` completes, freeing a core; task ID `4` should appear immediately after that transition, never before it.
- `~1200-1900 ms`: `G3` runs for about `700 ms`, then disappears; once all three first jobs finish, the system may idle until the next release.
- Later releases should preserve the same EDF ordering: `G1` at `4000 ms`, `G2` at `5000 ms`, and `G3` only when no earlier-deadline ready job occupies the free core. No miss pulse is expected.
- The measured waveform is consistent with this: both cores are active first, task ID `4` only appears after one of the `1/2` segments ends, and no deadline-miss activity is visible.

#### Expected Waveform
![Test 1 MP Global Expected Result](<../test_assets/MP Tests/Global EDF Tests/Test 1/Test 1 MP Global Expected Result.png>)

#### Measured Waveform
![Test 1 MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Test 1/Test 1 MP Global Waveform.png>)

### Test 2 - Preemption
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_2.c`
- Entry point: `mp_global_edf_2_run()`

#### Task Set
- G1: T=8000 ms, C=3000 ms, D=T, initial delay 0 ms
- G2: T=9000 ms, C=3000 ms, D=T, initial delay 0 ms
- G3: T=2000 ms, C=500 ms,  D=T, initial delay 800 ms

#### Trace Task IDs
- `G1 -> 1`
- `G2 -> 2`
- `G3 -> 4`

#### Expected Results:
- `t = 0`: `G1` (`D=8000`) and `G2` (`D=9000`) dispatch; task ID `4` is absent because `G3` is delayed by `800 ms`.
- `~800 ms`: `G3` releases with deadline `2800 ms`, earlier than both running jobs, so global EDF should preempt immediately.
- At the first cut-in, the expected victim is `G2`: one core should keep task ID `1`, while the other shows the `2 -> 4` switch because `G2` has the later running deadline.
- `~800-1300 ms`: task ID `4` runs for about `500 ms`; when it finishes, the displaced long job should resume, giving the visible `2 -> 4 -> 2` pattern.
- The same earlier-deadline cut-in should repeat near `t = 2800 ms` and later releases while the long jobs are still active.
- The measured waveform is consistent with this: short task-ID `4` bursts repeatedly interrupt a long-running job, while the other long job continues on the opposite core and no miss pulse appears.

#### Measured Waveform
![Test 2 MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Test 2/Test 2 MP Global Waveform.png>)

### Test 3 - Migration
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_3.c`
- Entry point: `mp_global_edf_3_run()`

#### Task Set
- Migrator M: unrestricted, T=3000 ms, C=700 ms, D=T, initial delay 0 ms
- Blocker B0: pinned to core 0, T=5000 ms, C=2500 ms, D=T, initial delay 0 ms
- Blocker B1: pinned to core 1, T=5000 ms, C=2500 ms, D=T, initial delay 2000 ms

#### Trace Task IDs
- `Migrator M -> 1`
- `Blocker B0 -> 2`
- `Blocker B1 -> 4`

#### Expected Results:
- `t = 0`: `M` and `B0` release together. Because `B0` is pinned to core 0, task ID `2` should appear on core 0 while task ID `1` runs on core 1.
- `~2000 ms`: `B1` releases with no competing new job from `M`, so task ID `4` should appear on core 1 and only on core 1.
- `~3000 ms`: the next job of `M` releases while `B1` still occupies core 1, so task ID `1` should now appear on core 0 instead of core 1.
- Over a longer window, task ID `1` should be seen on both core banks across different jobs, while task ID `2` remains core-0-only and task ID `4` remains core-1-only.
- The measured waveform is consistent with this combined claim: migration is visible for `M`, but the two pinned blockers never cross their assigned cores.

#### Measured Waveform
![Test 3 MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Test 3/Test 3 MP Global Waveform.png>)

### Test 4 - Overrun and Deadline Miss Trace
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_4.c`
- Entry point: `mp_global_edf_4_run()`

#### Task Set
- G4 OVR: T=5000 ms, C=1000 ms, D=1800 ms, work=2200 ms
- G4 N1: T=8000 ms, C=1000 ms, D=8000 ms, work=800 ms
- G4 N2: T=10000 ms, C=1000 ms, D=10000 ms, work=800 ms

#### Trace Task IDs
- `G4 OVR -> 1`
- `G4 N1 -> 2`
- `G4 N2 -> 4`

#### Expected Results:
- `t = 0`: the earliest two ready deadlines are task IDs `1` and `2`, so those jobs should start first while task ID `4` waits for slack.
- `~800 ms`: one normal job can finish and task ID `4` may then use the freed core, but task ID `1` should still be the only miss candidate because it has the constrained `1800 ms` deadline and `2200 ms` of work.
- `~1000 ms`: task ID `1` should trigger the WCET-overrun report first.
- `~1800 ms`: the same job of task ID `1` should trigger the deadline-miss report and the miss indicator, then continue until its work finishes.
- Task IDs `2` and `4` should continue using the available slack and may appear on either core because all three tasks are globally runnable; they are not expected to report misses.
- The measured assets are consistent with this: the waveform shows task ID `1` recurring around the normal jobs, and the saved UART output reports WCET overrun and deadline miss for task ID `1` only.

#### Notes
- This test flushes deferred WCET/deadline/MP UART output immediately before `xTaskDelayUntil()`. If the flush work makes `xTaskDelayUntil()` return late, the task re-anchors `xLastWakeTime` to the current tick. Later printed release/deadline/next-release values can therefore drift away from exact period multiples even though the progression remains deterministic.

#### Measured Waveform
![Test 4 MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Test 4/Test 4 MP Global Waveform.png>)

#### Console Output
![Test 4 MP Global Output](<../test_assets/MP Tests/Global EDF Tests/Test 4/Test 4 MP Global Output.png>)

### Dhall Test - Conservative Global Admission
#### Test Implementation
- File: `mp_tests/global_edf_tests/test_dhall.c`
- Entry point: `mp_test_dhall_run()`

#### Trace Task IDs
- `Heavy -> 1`
- `Light -> 2`
- `Bad candidate` is expected to be rejected and should not appear on the trace pins

#### Expected Results:
- Base utilization is `0.90 + 0.10 = 1.00`. Adding `Bad` would raise `U_total` to `1.75` with `U_max = 0.90`.
- For `m = 2`, the implemented sufficient bound is `U_total <= m - (m - 1)U_max = 2 - 0.90 = 1.10`, so the candidate must be rejected even though `1.75 < 2.0`.
- If `Bad` were admitted with `T = 4000 ms`, `C = 3000 ms`, `D = 3000 ms`, its first job would release at `t = 0` but wait behind `Heavy` and `Light`, whose first deadlines are both `2000 ms`.
- `Light` finishes near `t = 200 ms`, so `Bad` can only start then. Because one job can execute on only one core at a time, the most service it can receive by `t = 3000 ms` is about `2800 ms`.
- `Bad` would therefore be the miss victim, missing its first deadline at about `t = 3000 ms` by roughly `200 ms`.
- The measured waveform is consistent with the intended rejection path: only task IDs `1` and `2` appear, and the rejected `Bad` candidate never reaches the trace pins.
#### Measured Waveform
![Dhall Test MP Global Waveform](<../test_assets/MP Tests/Global EDF Tests/Dhall Test/Dhall Test MP Global Waveform.png>)

## Partitioned EDF Tests

### Test 1 - Basic Partitioned Dispatch
#### Test Implementation
- File: `mp_tests/partitioned_edf_tests/test_1.c`
- Entry point: `mp_partitioned_edf_1_run()`

#### Task Set
- P0A: pinned to core 0, T=4000 ms, C=900 ms, D=T
- P0B: pinned to core 0, T=6000 ms, C=800 ms, D=T
- P1A: pinned to core 1, T=3000 ms, C=800 ms, D=T

#### Trace Task IDs
- `P0A -> 1`
- `P0B -> 2`
- `P1A -> 4`

#### Expected Results:
- `t = 0`: core 0 releases `P0A` and `P0B`, so `P0A` (`D=4000`) should run before `P0B` (`D=6000`); core 1 has only `P1A`, so task ID `4` should start immediately there.
- `~0-800 ms`: core 1 should show only task ID `4`; `~0-900 ms`: core 0 should show only task ID `1`.
- `~900-1700 ms`: after `P0A` completes, core 0 should switch to task ID `2`; core 1 should already be idle once `P1A` finishes.
- Later releases remain partition-local: `P1A` repeats every `3000 ms` on core 1, `P0A` every `4000 ms` on core 0, and `P0B` every `6000 ms` on core 0. At shared core-0 release points such as `t = 12000 ms`, task ID `1` should still precede task ID `2` because it has the earlier absolute deadline.
- No cross-partition execution should ever appear: core 0 should show only `1`, `2`, and idle; core 1 should show only `4` and idle.
- The measured waveform is consistent with this: core 0 shows the `1 -> 2 -> idle` pattern, while core 1 shows only repeating `4 -> idle` windows.

#### Measured Waveform
![Test 1 MP Partitioned Waveform](<../test_assets/MP Tests/Partitioned EDF Tests/Test 1/Test 1 MP Partitioned Waveform.png>)

### Test 2 - Explicit Migration
#### Test Implementation
- File: `mp_tests/partitioned_edf_tests/test_2.c`
- Entry point: `mp_partitioned_edf_2_run()`

#### Task Set
- Migrating task M: initially pinned to core 0, T=4000 ms, C=900 ms, D=T
- Core-0 background: pinned to core 0, T=5000 ms, C=1200 ms, D=T
- Core-1 background: pinned to core 1, T=5000 ms, C=1200 ms, D=T

#### Trace Task IDs
- `Migrating task -> 1`
- `Core-0 background -> 2`
- `Core-1 background -> 4`

#### Expected Results:
- `t = 0`: core 0 should run task ID `1` first, then task ID `2`, because `M` has the earlier core-0 deadline (`4000 ms` vs `5000 ms`); core 1 should run task ID `4`.
- Before the affinity change, every visible job of task ID `1` should appear only on core 0. Task ID `2` should stay on core 0 for the full run, and task ID `4` should stay on core 1 for the full run.
- The migration request is issued only after the fifth completed job of `M`, so that fifth job should still finish on core 0. The currently executing job is not expected to jump cores mid-execution.
- The first job that should reflect the new affinity is the next release after that fifth completion, nominally the `~20000 ms` release relative to startup. Once `M` is pinned to core 1, task ID `1` should stop appearing on core 0.
- After migration, task ID `1` should appear on core 1 and compete only with task ID `4`. When both are ready together, task ID `1` should run first because its absolute deadline is earlier than the core-1 background task's deadline.
- The measured waveform is consistent with this handoff: in the earlier part of the capture, task ID `1` is seen only on the core-0 bank; in the later part, task ID `1` appears on the core-1 bank while `2` remains core-0-only and `4` remains core-1-only.

#### Measured Waveform
![Test 2 MP Partitioned Waveform](<../test_assets/MP Tests/Partitioned EDF Tests/Test 2/Test 2 MP Partitioned Waveform.png>)

### Test 3 - Overrun and Deadline Miss Trace
#### Test Implementation
- File: `mp_tests/partitioned_edf_tests/test_3.c`
- Entry point: `mp_partitioned_edf_3_run()`

#### Task Set
- P3 C0O: pinned to core 0, T=5000 ms, C=1000 ms, D=1800 ms, work=2200 ms
- P3 C0N: pinned to core 0, T=10000 ms, C=1000 ms, D=10000 ms, work=800 ms
- P3 C1O: pinned to core 1, T=8000 ms, C=1000 ms, D=2200 ms, work=2600 ms
- P3 C1N: pinned to core 1, T=10000 ms, C=1000 ms, D=10000 ms, work=800 ms

#### Trace Task IDs
- `P3 C0O -> 1`
- `P3 C0N -> 2`
- `P3 C1O -> 4`
- `P3 C1N -> 3`

#### Expected Results:
- `t = 0`: core 0 should run task ID `1` before `2` because `D=1800 ms` is earlier than `10000 ms`; core 1 should run task ID `4` before `3` because `D=2200 ms` is earlier than `10000 ms`.
- Around `t = 1000 ms`, task IDs `1` and `4` should each exceed WCET first, so the UART log should report WCET overruns for those two tasks before their deadline-miss reports.
- Around `t = 1800 ms`, task ID `1` should miss its deadline on core 0 while still executing; it should continue until about `2200 ms`, after which task ID `2` should finally run. Around `t = 2200 ms`, task ID `4` should miss its deadline on core 1 while still executing; it should continue until about `2600 ms`, after which task ID `3` should run.
- Partition ownership should still hold during and after recovery: core 0 should show only `1`, `2`, and idle, while core 1 should show only `4`, `3`, and idle.
- Later releases repeat independently by partition, so the recurring visible pattern should be `overrun task -> normal task -> idle` on each core, with task IDs `1` and `4` being the only miss victims.
- The measured assets are consistent with this: the waveform shows `1 then 2` on core 0 and `4 then 3` on core 1, while the saved console output reports WCET overrun and deadline-miss events for task IDs `1` and `4` only, not for `2` or `3`.

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

#### Expected Results:
- `Global EDF:` the startup base set `U = 0.40 + 0.40 = 0.80` should be admitted, but the initial bad candidate should be rejected because adding it gives `U_total = 1.433` and `U_max = 0.633`, which violates the sufficient bound `2 - 0.633 = 1.367`.
- `Global EDF:` after the delayed controller activation, the repeated bad create should fail again, then the good candidate should succeed because the post-addition utilization is only `0.90`. The good task should appear only after the controller activity, and the bad tag should never appear.
- `Partitioned EDF:` the two startup base tasks each have utilization `0.60`, so they should occupy separate partitions. The bad candidate should be rejected in both attempts because adding `0.35` raises total utilization to `1.55`, exceeding the implemented `1.5` screen for two cores.
- `Partitioned EDF:` the delayed good candidate raises total utilization to `1.40`, so it should be admitted and placed on one valid core. The waveform should still show strict per-core ownership, with no appearance of the rejected bad tag.
- Short task-ID `7` controller activity may appear around the delayed create window, but the decisive evidence is the saved flag pattern `0, 0, 1` and the fact that only the good candidate reaches the waveform in both modes.

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

#### Expected Results:
- `Global EDF:` the startup base set uses `U = 0.30 + 0.30 = 0.60`, so both base tasks should run normally before the controller intervenes. Around the first delayed create window, the good runtime task should be admitted because adding `0.20` keeps the task set feasible.
- `Global EDF:` after the good task is present, the later bad create should fail because adding it would give `U_total = 1.533` with `U_max = 0.733`, which violates the sufficient bound `2 - 0.733 = 1.267`. The waveform should therefore show task ID `4` after the first controller event, but never the rejected bad tag.
- `Partitioned EDF:` the two startup base tasks each have utilization `0.50`, so best-fit packs them onto one core at utilization `1.0`, leaving the other core idle at first.
- `Partitioned EDF:` the good runtime task has utilization `0.25`, so it cannot fit on the full core and should appear on the previously idle core after the first controller event. The later bad candidate should be rejected because total utilization would rise to `1.80`, above the implemented `1.5` screen.
- The measured assets are consistent with this in both modes: the controller tag appears around the create windows, the saved flags show `xGoodCreateResult = 1` and `xBadCreateResult = 0`, and only the good runtime task reaches the trace pins.

#### Global EDF Waveform
![Test 2 MP Admission Global Waveform](<../test_assets/MP Tests/Admission Tests/Test 2 MP Admission Global Waveform.png>)

#### Global EDF Result Flag
![Test 2 MP Admission Global Result Flag](<../test_assets/MP Tests/Admission Tests/Test 2 MP Admission Global Result Flag.png>)

#### Partitioned EDF Waveform
![Test 2 MP Admission Partitioned Waveform](<../test_assets/MP Tests/Admission Tests/Test 2 MP Admission Partitioned Waveform.png>)

#### Partitioned EDF Result Flags
![Test 2 MP Admission Partitioned Result Flags](<../test_assets/MP Tests/Admission Tests/Test 2 MP Admission Partitioned Result Flags.png>)

## Comparison Test

### Shared Task Set
- Demo A: T=4000 ms, C=1050 ms, D=T, tag 1
- Demo B: T=5000 ms, C=850 ms, D=T, tag 2
- Demo C: T=8000 ms, C=650 ms, D=T, tag 4
- Total utilization: `0.30 + 0.20 + 0.10 = 0.60`

#### Expected Results:
- Both comparison tests use the same low-utilization task set with no explicit affinity, so the key difference should come only from the scheduling policy.
- Under global EDF, the jobs remain globally runnable. At startup, the two earliest-deadline jobs, task IDs `1` and `2`, should occupy the two cores simultaneously, while task ID `4` waits for one of them to finish.
- The global capture and its annotation are consistent with that behavior: both core banks become active even though total utilization is only `0.60`, which shows that global EDF is using shared runnable placement rather than packing the work onto one core.
- Under partitioned EDF, admission chooses a partition during `xTaskCreate()`. Because best-fit placement is used and the full task set still fits on one core (`0.60 < 1.0`), all three tasks should be packed onto core 0.
- The partitioned capture and its annotation are consistent with that: core 0 shows the expected single-core EDF order `1 -> 2 -> 4`, while core 1 stays idle for the full window.
- Together, the two waveforms show the intended policy difference clearly: global EDF spreads the same task set across both cores, while partitioned EDF keeps the schedule correct but confines all work to one bin-packed partition.

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
