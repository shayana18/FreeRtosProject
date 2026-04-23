# EDF Hardware Testing Setup
- In addition to UART debug output, nine GPIO pins are used for runtime trace.
- Seven pins encode the currently running task ID (idle task is ID 0).
- One pin is a task-switch strobe used as a sampling clock for decoding task ID changes.
- These eight pins are sampled by a Saleae Logic 8 analyzer. For small tests, one-hot tags can be used for easier waveform readability; for larger tests, binary task IDs allow visibility up to 127 tasks.
- The ninth pin is a deadline-miss indicator. It is driven high on deadline miss and is wired to an LED for quick visual detection. We can choose to move one of the logic analyzer pins to sample this output as well if we have < 64 tasks in the test.

# EDF Software Testing Setup
- EDF tests are in the `edf_tests` folder.
- Each test exposes an `edf_X_run()` entry point called from `main.c`.
- Only one test should be enabled in `main.c` at a time, then compile and flash to run that scenario.
- Common trace and helper utilities are in `task_trace.c/h` and `test_utils.c/h`.

# EDF Test Methods
- For small task sets, manually derive expected scheduling windows (typically first ~30 s) and compare against logic-analyzer traces.
- For admission-control tests, verify both task-creation return values and waveform behavior.
- For larger stress tests, verify absence of deadline-miss pulses and spot-check fairness/order patterns.

# EDF Test Index
| Test # | Summary Name | Type | Task Count | Key Goal |
|---|---|---|---:|---|
| 1 | Simple Three Task Set | Implicit deadline | 3 | Baseline EDF dispatch correctness |
| 2 | Four-Task High Utilization Harmonic Set | Implicit deadline | 4 | High utilization scheduling without misses |
| 3 | Implicit Admission Control (Reject then Accept) | Implicit deadline + runtime create | 3 baseline + candidates | Validate utilization-based admission behavior |
| 4 | Three-Task Constrained Deadline Set | Constrained deadline | 3 | Validate EDF ordering with D != T |
| 5 | Four-Task Constrained Higher Utilization Set | Constrained deadline | 4 | Validate constrained-deadline behavior at higher load |
| 6 | Constrained Admission Control | Constrained deadline + runtime create | 2 baseline + candidates | Validate DBF-style constrained admission path |
| 7 | Implicit Admission Control (Utilization Path) | Implicit deadline + runtime create | 2 baseline + candidates | Validate implicit-deadline utilization admission path |
| 8 | Seven-Task Mixed Miss/No-Miss Pattern | Mixed deadlines | 7 | Validate controlled periodic deadline misses |
| 9 | 100-Task Constrained Deadline Stress Test | Constrained deadline stress | 100 | Validate scalability and scheduling stability |

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

### Expected Results and Results
Expected waveform placeholder:
![Test 1 Expected](images/edf_test_1_expected.png)

Measured waveform placeholder:
![Test 1 Results](images/edf_test_1_results.png)

#### Expected
- At `t=0`, all jobs are released and Task 1 (earliest absolute deadline) starts first.

#### Actual
-

#### Verified
-

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

### Expected Results and Results
Expected waveform placeholder:
![Test 2 Expected](images/edf_test_2_expected.png)

Measured waveform placeholder:
![Test 2 Results](images/edf_test_2_results.png)

#### Expected
-

#### Actual
-

#### Verified
-

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

### Expected Results and Results
Expected waveform placeholder:
![Test 3 Expected](images/edf_test_3_expected.png)

Measured waveform placeholder:
![Test 3 Results](images/edf_test_3_results.png)

#### Expected
-

#### Actual
-
- Initial bad admission return:
- Delayed bad admission return:
- Delayed good admission return:

#### Verified
-

## Test 4 - Three-Task Constrained Deadline Set
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| T1 | 1000 | 5000 | 3000 | 0 |
| T2 | 1500 | 7000 | 3500 | 0 |
| T3 | 1000 | 10000 | 8000 | 0 |

### Description of Test
Constrained-deadline EDF test with all tasks released synchronously. Used to verify that deadline ordering is based on absolute deadlines when D != T.

### Expected Results and Results
Expected waveform placeholder:
![Test 4 Expected](images/edf_test_4_expected.png)

Measured waveform placeholder:
![Test 4 Results](images/edf_test_4_results.png)

#### Expected
-

#### Actual
-

#### Verified
-

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

### Expected Results and Results
Expected waveform placeholder:
![Test 5 Expected](images/edf_test_5_expected.png)

Measured waveform placeholder:
![Test 5 Results](images/edf_test_5_results.png)

#### Expected
-

#### Actual
-

#### Verified
-

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

### Expected Results and Results
Expected waveform placeholder:
![Test 6 Expected](images/edf_test_6_expected.png)

Measured waveform placeholder:
![Test 6 Results](images/edf_test_6_results.png)

#### Expected
-

#### Actual
-
- Bad admission return:
- Good admission return:

#### Verified
-

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

### Expected Results and Results
Expected waveform placeholder:
![Test 7 Expected](images/edf_test_7_expected.png)

Measured waveform placeholder:
![Test 7 Results](images/edf_test_7_results.png)

#### Expected
-

#### Actual
-
- Bad admission return:
- Good admission return:

#### Verified
-

## Test 8 - Seven-Task Mixed Miss/No-Miss Pattern
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | First Release (ms) |
|---|---:|---:|---:|---:|
| T1 | 300 | 2000 | 2000 | 0 |
| T2 | 350 | 3000 | 3000 | 0 |
| T3 | 500 | 4000 | 4000 | 0 |
| T4 | 600 | 5000 | 5000 | 0 |
| T5 | 650 | 7000 | 7000 | 0 |
| T6 (periodic miss) | 900 | 9000 | 2200 | 0 |
| T7 (periodic miss) | 1200 | 12000 | 2500 | 0 |

### Description of Test
Mixed behavior scenario with five always-meeting tasks and two intentionally periodic-miss tasks. T6 and T7 overrun every Nth job by design to confirm deadline-miss detection and trace behavior while other tasks continue periodic execution.

### Expected Results and Results
Expected waveform placeholder:
![Test 8 Expected](images/edf_test_8_expected.png)

Measured waveform placeholder:
![Test 8 Results](images/edf_test_8_results.png)

#### Expected
-

#### Actual
-
- Deadline miss count observed:

#### Verified
-

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

### Expected Results and Results
Expected waveform placeholder:
![Test 9 Expected](images/edf_test_9_expected.png)

Measured waveform placeholder:
![Test 9 Results](images/edf_test_9_results.png)

#### Expected
-

#### Actual
-
- Deadline miss count observed:

#### Verified
-
