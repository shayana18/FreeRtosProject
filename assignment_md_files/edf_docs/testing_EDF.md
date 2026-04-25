# EDF Hardware Testing Setup
- In addition to UART debug output, nine GPIO pins are used for runtime trace.
- Seven pins encode the currently running task ID (idle task is ID 0).
- One pin is a task-switch strobe used as a sampling clock for decoding task ID changes.
- These eight pins are sampled by a Saleae Logic 8 analyzer. For small tests, one-hot tags can be used for easier waveform readability; for larger tests, binary task IDs allow visibility up to 127 tasks.
- The ninth pin is a deadline-miss indicator. It is driven high on deadline miss and is wired to an LED for quick visual detection. We can choose to move one of the logic analyzer pins to sample this output as well if we have < 64 tasks in the test.

# EDF Software Testing Setup
- EDF tests are in the `edf_tests` folder.
- The captured hardware assets for this document live under `assignment_md_files/test_assets/EDF Tests/`.
- Each test exposes an `edf_X_run()` entry point called from `main.c`.
- Only one test should be enabled in `main.c` at a time, then compile and flash to run that scenario.
- Common trace and helper utilities are in `task_trace.c/h` and `test_utils.c/h`.

# EDF Test Methods
- Task IDs are observed on the GPIO task-code pins and decoded using the task-switch strobe as a sampling clock. On the waveform, the strobe edge marks when the binary task-code output should be read.
- In the normal binary-trace setup, up to 7 GPIO bits can be used for task ID output. When a given test has fewer tasks and deadline-miss observation is important, the highest binary-output pin may instead be repurposed as a direct deadline-miss probe.
- When a test has very few tasks, one-hot encoding may be used instead of binary encoding to make the waveform easier to read directly.
- For precise timing analysis, the captured discussion in this document focuses on smaller windows that are sufficient to prove the intended scheduling or synchronization feature. We intentionally do not require full-hyperperiod inspection for every test when a shorter interval already demonstrates correctness.
- For small task sets, manually derive expected scheduling windows (typically first ~30 s) and compare against logic-analyzer traces.
- For admission-control tests, verify both task-creation return values and waveform behavior.
- For larger stress tests, verify absence of deadline-miss pulses and spot-check fairness/order patterns.

# EDF Test Index
| Test # | Summary Name | File / Entry Point | Type | Task Count | Key Goal |
|---|---|---|---|---:|---|
| 1 | Simple Three Task Set | `edf_tests/test_1.c`, `edf_1_run()` | Implicit deadline | 3 | Baseline EDF dispatch correctness |
| 2 | Four-Task High Utilization Harmonic Set | `edf_tests/test_2.c`, `edf_2_run()` | Implicit deadline | 4 | High utilization scheduling without misses |
| 3 | Implicit Admission Control (Reject then Accept) | `edf_tests/test_3.c`, `edf_3_run()` | Implicit deadline + runtime create | 3 baseline + candidates | Validate utilization-based admission behavior |
| 4 | Three-Task Constrained Deadline Set | `edf_tests/test_4.c`, `edf_4_run()` | Constrained deadline | 3 | Validate EDF ordering with D != T |
| 5 | Four-Task Constrained Higher Utilization Set | `edf_tests/test_5.c`, `edf_5_run()` | Constrained deadline | 4 | Validate constrained-deadline behavior at higher load |
| 6 | Constrained Admission Control | `edf_tests/test_6.c`, `edf_6_run()` | Constrained deadline + runtime create | 2 baseline + candidates | Validate DBF-style constrained admission path |
| 7 | Implicit Admission Control (Utilization Path) | `edf_tests/test_7.c`, `edf_7_run()` | Implicit deadline + runtime create | 2 baseline + candidates | Validate implicit-deadline utilization admission path |
| 8 | Seven-Task Mixed Miss/No-Miss Pattern | `edf_tests/test_8.c`, `edf_8_run()` | Mixed deadlines | 7 | Validate controlled periodic deadline misses |
| 9 | 100-Task Constrained Deadline Stress Test | `edf_tests/test_9.c`, `edf_9_run()` | Constrained deadline stress | 100 | Validate requirement of being able to schedule and run 100 tasks |

# EDF Test Cases + Results

## Test 1 - Simple Three Task Set
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| Task1 | 1500 | 5000 | 5000 | 0 |
| Task2 | 1500 | 7000 | 7000 | 0 |
| Task3 | 2000 | 8000 | 8000 | 0 |

### Description of Test
Baseline implicit-deadline EDF sanity test with three periodic tasks released from the same anchor tick. The objective is to verify basic preemption and periodic release handling under moderate load.

### Test Implementation
- File: `edf_tests/test_1.c`
- Entry point: `edf_1_run()`

### Trace Task IDs
- `Task1 -> 1`
- `Task2 -> 2`
- `Task3 -> 4`

### Expected Results and Results
Expected waveform:
![Test 1 EDF Answer](<../test_assets/EDF Tests/Test 1/Test 1 EDF Answer.jpg>)

Measured waveform:
![Test 1 EDF Waveform](<../test_assets/EDF Tests/Test 1/Test 1 EDF Waveform.png>)

#### Expected
- At `t=0s`, all jobs are released and Task 1 (earliest absolute deadline) starts first.
- After running for its designated `WCET = 1500ms`, it is followed by Task 2 and then 3 for their respective `WCET`s.
- At `t=10s`, J<sub>1,3</sub> arrives and has deadline of `15s` while the current running J<sub>3,2</sub> has a deadline of `16s`. Therefore J<sub>1,3</sub> will preempt the current running job.
- Tasks that can run as soon as they arrive have correct period over which they are submitted again.
- Current running task is also preempted by Task 1 at `t=15s` and `t=25s` for similar reasons. 

#### Actual
- As can be seen from the logic analyzer waveform it follows the expected results exactly. Note that each grid on the plot is equal to one second.

#### Verified
- Implicit earliest deadline selection from idle.
- Implicit earliest deadline preemption. 
- EDF is configurable (test executed with EDF-enabled build configuration).

## Test 2 - Four-Task High Utilization Harmonic Set
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| T1 | 1000 | 2000 | 2000 | 0 |
| T2 | 1000 | 4000 | 4000 | 0 |
| T3 | 1000 | 8000 | 8000 | 0 |
| T4 | 1000 | 16000 | 16000 | 0 |

### Description of Test
Implicit-deadline harmonic task set with total utilization 15/16 (0.9375). This scenario checks that EDF remains stable and deadlines are met near high processor utilization.

### Test Implementation
- File: `edf_tests/test_2.c`
- Entry point: `edf_2_run()`

### Trace Task IDs
- `T1 -> 1`
- `T2 -> 2`
- `T3 -> 4`
- `T4 -> 8`

### Expected Results and Results
Measured waveform:
![Test 2 EDF Waveform](<../test_assets/EDF Tests/Test 2/Test 2 EDF Waveform.png>)

#### Expected
- At `t=0s`, T1/T2/T3/T4 all release together with deadlines at `2s`, `4s`, `8s`, and `16s`, so T1 must execute first.
- At `t=2s`, next T1 job releases; at `t=4s`, T1 and T2 jobs release; at `t=8s`, T1/T2/T3 jobs release; at `t=16s`, all four release together again.
- In the `0s` to `16s` window, T1 should appear every `2s`, T2 every `4s`, T3 every `8s`, and T4 every `16s`, with no deadline-miss indicator pulse.
- The tasks will execute in forming a "symmetric" pattern on the waveform due to the harmonic periods.

#### Actual
- As can be seen from the logic analyzer waveform, the described symmetric pattern can be observed. Note that the small pulses that can be seen intermittently are an artifact of small amounts of jitter since each task spins for its full WCET.

#### Verified
- EDF was enabled via configuration and the kernel executed EDF periodic dispatch instead of fixed-priority dispatch.
- Near-saturation periodic load (`U = 15/16`) remained schedulable in practice for the observed interval, with no unexpected deadline-miss pulses.

## Test 3 - Implicit Admission Control (Reject then Accept)
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| Baseline B1 | 1000 | 2000 | 2000 | 0 |
| Baseline B2 | 1000 | 4000 | 4000 | 0 |
| Baseline B3 | 800 | 8000 | 8000 | 0 |
| Bad Candidate | 400 | 2000 | 2000 | ~0 attempt, ~10000 retry |
| Good Candidate | 400 | 8000 | 8000 | ~10000 |
| Admission Controller | 10 | 20000 | 20000 | 0 |

### Description of Test
Starts with a schedulable baseline implicit-deadline set. A bad candidate is attempted at startup (expected reject) and retried after ~10 s by a controller task. A good candidate is then attempted and should be admitted. This validates runtime admission behavior and scheduler continuity during create attempts.

### Test Implementation
- File: `edf_tests/test_3.c`
- Entry point: `edf_3_run()`

### Trace Task IDs
- `Baseline B1 -> 1`
- `Baseline B2 -> 2`
- `Baseline B3 -> 4`
- `Good Candidate -> 8` when admitted after the delayed create
- `Admission Controller -> 64`
- `Bad Candidate` is expected to be rejected and therefore should not appear on the trace pins

### Expected Results and Results
Measured waveform:
![Test 3 EDF Waveform](<../test_assets/EDF Tests/Test 3/Test 3 EDF Waveform.png>)

#### Expected
- The times below describe the ideal admission-controller timeline once the controller task gets CPU time. Because the controller is configured with a very large period/deadline to avoid introducing deadline misses of its own, it has a later absolute deadline than the baseline periodic tasks. Under EDF it can therefore be preempted by those tasks, so its retry attempt and the following good-candidate create call may be delayed past the nominal `t~10s` point.
- At startup (`t~0s`), the initial bad candidate create call should return rejection immediately.
- Around `t=10s`, the controller task becomes eligible to perform a second bad-candidate create attempt, which should also be rejected.
- Once the delayed controller window runs, the good candidate create call should be accepted and begin periodic releases from that point onward.
- Baseline tasks should continue their periodic pattern across `0s-10s` and after `10s`, showing runtime task creation does not stall scheduler progress.

#### Actual
- Initial bad admission return: The bad candidate was unable to be created at startup because no task ID corresponding to the bad candidate appears on the waveform.
- Delayed bad admission return: Though the timing is skewed by the controller's long deadline, the waveform shows that the bad candidate was still not created at about `t=15s` because no corresponding task ID appears on the trace pins after that time.
- Delayed good admission return: We can see that the good candidate was created at around `t=15s` since we see the corresponding task id on the trace pins from that point onward. 

#### Verified
- Admission control rejected unschedulable additions and accepted schedulable additions in the same test scenario.
- The system accepted a new periodic task while already running (post-start controller-triggered create).
- EDF was enabled via configuration and remained active during runtime create operations.

## Test 4 - Three-Task Constrained Deadline Set
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| T1 | 1000 | 5000 | 3000 | 0 |
| T2 | 1500 | 7000 | 3500 | 0 |
| T3 | 1000 | 10000 | 8000 | 0 |

### Description of Test
Constrained-deadline EDF test with all tasks released synchronously. Used to verify that deadline ordering is based on absolute deadlines when D != T.

### Test Implementation
- File: `edf_tests/test_4.c`
- Entry point: `edf_4_run()`

### Trace Task IDs
- `T1 -> 1`
- `T2 -> 2`
- `T3 -> 4`

### Expected Results and Results
Expected waveform:
![Test 4 EDF Answer](<../test_assets/EDF Tests/Test 4/Test 4 EDF Answer.jpg>)

Measured waveform:
![Test 4 EDF CD Waveform](<../test_assets/EDF Tests/Test 4/Test 4 EDF CD Waveform.png>)

#### Expected
- At `t=0s`, all three tasks release with absolute deadlines at `3s`, `3.5s`, and `8s`, so first dispatch order should be T1 before T2 before T3.
- At `t=5s`, T1 releases again (deadline `8s`), and at `t=7s`, T2 releases again (deadline `10.5s`); trace ordering should reflect deadline-based arbitration, not just period size.
- At `t=10s`, T1 and T3 both release; T1's earlier deadline (`13s`) should place it ahead of T3 (`18s`) when both contend.
- No deadline-miss pulse is expected over the observed schedule window.
- At `t=14s`, T2 releases again and starts executing. At `t=15s`, T1 releases again while T2 is not finished executing; T1's deadline (`18s`) is later than T2's deadline (`17.5s`). Therefore, T1 should not preempt T2 at `t=15s` and should only start executing after T2 finishes at around `t=17.5s`.

#### Actual
- We can see from the waveform that as described in what was expected, T1 does not preempt T2 at `t=15s` and only starts executing after T2 finishes at around `t=17.5s`. This shows that the scheduler is correctly prioritizing based on absolute deadlines rather than just periods.

#### Verified
- Constrained-deadline EDF ordering (`D != T`) was exercised and matched absolute-deadline-driven dispatch.
- EDF was enabled via configuration and used for all scheduling decisions in this run.

## Test 5 - Four-Task Constrained Higher Utilization Set
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| T1 | 1200 | 4000 | 3000 | 0 |
| T2 | 1500 | 6000 | 5000 | 0 |
| T3 | 1800 | 9000 | 7000 | 0 |
| T4 | 1200 | 12000 | 10000 | 0 |

### Description of Test
Constrained-deadline task set with higher aggregate load than Test 4. Intended to exercise frequent EDF ordering changes while still remaining schedulable.

### Test Implementation
- File: `edf_tests/test_5.c`
- Entry point: `edf_5_run()`

### Trace Task IDs
- `T1 -> 1`
- `T2 -> 2`
- `T3 -> 4`
- `T4 -> 8`

### Expected Results and Results
Expected waveform:
![Test 5 EDF Answer](<../test_assets/EDF Tests/Test 5/Test 5 EDF Answer.png>)

Measured waveform:
![Test 5 EDF Waveform](<../test_assets/EDF Tests/Test 5/Test 5 EDF Waveform.png>)

#### Expected
- At `t=0s`, all tasks release with deadlines `3s`, `5s`, `7s`, and `10s`; initial order should prioritize T1 then T2 before later-deadline jobs.
- Around `t=4s`, a new T1 job arrives (deadline `7s`) while longer jobs are active, so an earlier-deadline takeover/preemption is expected.
- Around `t=6s` and `t=8s`, additional T2/T1 releases create repeated reorder points; trace should show deadline-based re-selection at these boundaries.
- At `t=18s`, T3 releases again with deadline `25s`; however, it cannot execute until T2 finishes at around `t=19s`. T3 is expected to run until `t=20s`, when T1 is released with deadline `23s` and preempts T3. T1 should then run until it finishes at around `t=22s`, after which T3 should resume and finish at around `t=25s`.

#### Actual
- As can be seen from the waveform, T3 releases at `t=18s` but cannot execute until T2 finishes at around `t=19s`. T3 then runs until `t=20s` when T1 is released with deadline `23s` and preempts T3. T1 then runs until it finishes at around `t=22s`, and then T3 resumes and finishes at around `t=25s`. This shows that the scheduler is correctly handling the complex interplay of releases, deadlines, and preemptions in this higher-utilization constrained-deadline scenario.

#### Verified
- Constrained-deadline EDF remained stable under higher utilization with expected reorder/preemption behavior.
- EDF was enabled via configuration and drove dispatch choices throughout this test.

## Test 6 - Constrained Admission Control
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| Baseline B1 | 1200 | 4000 | 1600 | 0 |
| Baseline B2 | 800 | 8000 | 2000 | 0 |
| Bad Candidate | 3200 | 10000 | 3600 | ~0 attempt |
| Good Candidate | 3200 | 10000 | 6400 | ~0 (immediately after bad reject) |

### Description of Test
Constrained-deadline admission-control scenario. A bad candidate (tight deadline) should be rejected, then a good candidate with the same period/WCET but relaxed deadline should be accepted. This validates the constrained-deadline admission logic.

### Test Implementation
- File: `edf_tests/test_6.c`
- Entry point: `edf_6_run()`

### Trace Task IDs
- `Baseline B1 -> 1`
- `Baseline B2 -> 2`
- `Good Candidate -> 4` when admitted
- `Bad Candidate` is expected to be rejected and should not appear on the trace pins
- If the bad candidate ever appears, the code assigns it `8` specifically to flag unexpected admission

### Expected Results and Results
Measured waveform:
![Test 6 EDF Waveform](<../test_assets/EDF Tests/Test 6/Test 6 EDF Waveform.png>)

#### Expected
- At startup before scheduler launch (`t~0s` setup phase), bad constrained candidate create should return reject.
- Immediately after bad reject in the same setup phase, good constrained candidate create should return accept.
- After scheduler start, only baseline + accepted-good tasks should appear in trace; rejected-bad task should never execute.

#### Actual
- Bad admission return: The bad constrained-deadline candidate does not appear on the trace pins, including the reserved bad-candidate tag `8`, so it was rejected before it could enter the schedule.
- Good admission return: The good candidate appears with trace ID `4` alongside the two baseline tasks, showing that the relaxed-deadline version was accepted and scheduled.
- The waveform therefore matches the intended reject-then-accept constrained admission-control behavior.

#### Verified
- Admission control correctly distinguished failing vs passing constrained-deadline candidates at creation time.
- Accepted task set executed while rejected task remained absent from runtime trace.
- EDF was enabled via configuration and used for runtime dispatch.

## Test 7 - Implicit Admission Control (Utilization Path)
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| Baseline B1 | 1500 | 6000 | 6000 | 0 |
| Baseline B2 | 2000 | 9000 | 9000 | 0 |
| Bad Candidate | 7000 | 12000 | 12000 | ~0 attempt |
| Good Candidate | 5000 | 12000 | 12000 | ~0 (immediately after bad reject) |

### Description of Test
Implicit-deadline admission scenario (all tasks have D = T), which should follow utilization-based admission. The bad candidate should fail by utilization, while the reduced-WCET good candidate should pass.

### Test Implementation
- File: `edf_tests/test_7.c`
- Entry point: `edf_7_run()`

### Trace Task IDs
- `Baseline B1 -> 1`
- `Baseline B2 -> 2`
- `Good Candidate -> 4` when admitted
- `Bad Candidate` is expected to be rejected and should not appear on the trace pins
- If the bad candidate ever appears, the code assigns it `8` specifically to flag unexpected admission

### Expected Results and Results
Measured waveform:
![Test 7 EDF Waveform](<../test_assets/EDF Tests/Test 7/Test 7 EDF Waveform.png>)

#### Expected
- At startup before scheduler launch (`t~0s` setup phase), bad implicit candidate create should be rejected on utilization grounds.
- Immediately afterward in the same setup phase, good implicit candidate create should be accepted.
- After scheduler starts, only baseline + accepted-good tasks should run; the rejected bad candidate should never appear on trace pins.

#### Actual
- Bad admission return: We can see that the bad candidate was not able to be created at startup since there is no task id on the trace pins that corresponds to the bad candidate. This shows that the bad candidate was correctly rejected by the implicit-deadline utilization-based admission control logic.
- Good admission return: We can see that the good candidate was created at startup since we see the corresponding task id on the trace pins from the beginning of the run. This shows that the good candidate was correctly accepted by the implicit-deadline utilization-based admission control logic.

#### Verified
- Utilization-path admission control for implicit deadlines (`D = T`) behaved as expected (reject then accept).
- Runtime trace reflects only admitted tasks, confirming rejected candidate never executed.
- EDF was enabled via configuration and active for dispatch.

## Test 8 - Seven-Task Mixed Miss/No-Miss Pattern
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| T1 | 300 | 2000 | 2000 | 0 |
| T2 | 350 | 3000 | 3000 | 0 |
| T3 | 500 | 4000 | 4000 | 0 |
| T4 | 600 | 5000 | 5000 | 0 |
| T5 | 650 | 7000 | 7000 | 0 |
| T6 (periodic miss) | 700 | 4000 | 1800 | 0 |
| T7 (periodic miss) | 900 | 6000 | 2200 | 0 |

### Description of Test
Mixed behavior scenario with five always-meeting tasks and two intentionally periodic-miss tasks. T6 and T7 now miss on their second jobs so the first deadline-miss pulses appear early in the run, which makes the waveform easier to inspect in the first 10 seconds while still confirming the separation between WCET-overrun reporting and deadline-miss handling.

Because the miss-task code flushes deferred WCET/deadline UART output immediately before `xTaskDelayUntil()`, a late return from `xTaskDelayUntil()` can cause the task to re-anchor its local `xLastWakeTime` to the current tick. When that happens, later printed release/deadline values may drift away from neat period multiples even though the behavior is still deterministic.

### Test Implementation
- File: `edf_tests/test_8.c`
- Entry point: `edf_8_run()`

### Trace Task IDs
- `T1 -> 1`
- `T2 -> 2`
- `T3 -> 3`
- `T4 -> 4`
- `T5 -> 5`
- `T6 -> 6`
- `T7 -> 7`

### Expected Results and Results
Measured waveform:
![Test 8 EDF Waveform](<../test_assets/EDF Tests/Test 8/Test 8 EDF Waveform.png>)

#### Expected
- T1-T5 should maintain regular periodic execution with no forced overruns.
- T7's first intentional deadline miss is expected at about `t=5.8s`.
- T6's second-job intentional deadline miss is expected at about `t=8.2s`.
- Deadline-miss indicator should pulse around those windows, and normal dispatch should resume afterward.

#### Actual
- Deadline miss count observed: Two observed at the expected times, note that a deadline miss pulse lasts for 1.00s so if multiple misses occur in quick succession, the pulses may visually merge into a longer pulse. We can confirm with measurement that each pulse is exactly 1.00s long, which indicates that they are individual misses rather than a single miss event with an extended pulse.

#### Verified
- WCET-overrun reporting and deadline-miss handling were both exercised by intentional long-running jobs without collapsing scheduler progress.
- Mixed workload behavior (always-meet tasks plus forced-miss tasks) remained observable and recoverable over time.
- EDF was enabled via configuration and continued normal dispatch between miss events.

## Test 9 - 100-Task Constrained Deadline Stress Test
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| Task 1 | 95 | 11000 | 100 | 0 |
| Task 2 | 95 | 11000 | 200 | 0 |
| ... | 95 | 11000 | ... | 0 |
| Task 100 | 95 | 11000 | 10000 | 0 |

Generation rule:
- For task i in [1, 100]: C = 95 ms, T = 11000 ms, D = i * 100 ms, release at 0 ms.

### Description of Test
Scalability stress test with 100 periodic tasks. All tasks share the same period and WCET, while deadlines increase linearly by task index. This creates a predictable EDF priority gradient and is used to validate runtime stability and high task-count behavior.

### Test Implementation
- File: `edf_tests/test_9.c`
- Entry point: `edf_9_run()`

### Trace Task IDs
- `Task i -> i` for `i = 1..100`
- The application task tag matches the numbered task name directly in this stress test

### Expected Results and Results
Measured waveform:
![Test 9 EDF Waveform](<../test_assets/EDF Tests/Test 9/Test 9 EDF Waveform.png>)

#### Expected
- At `t=0s`, all 100 tasks are released; earliest-deadline tasks (Task 1, Task 2, ...) should appear before later-deadline tasks in the first major window.
- With `WCET = 95ms` each, total nominal work per major cycle is about `9.5s`; with period `11s`, the first cycle should complete before the next common release at `t=11s`.
- Around `t=11s`, the second cycle begins and should show the same ordering trend with no sustained lockup or starvation signature.

#### Actual
- All 100 tasks were observed in the logic analyzer trace with the expected ordering pattern (Task 1 before Task 2 before Task 3, etc.). This was also shown during demo for multiple cycles.

#### Verified
- Roughly 100 periodic EDF tasks were instantiated and scheduled in a single stress scenario.
- Large-task-set execution maintained periodic behavior across cycle boundaries (for example `0s` and `11s`).
- EDF was enabled via configuration and remained active under high task-count load.
