# SRP Hardware Testing Setup
- SRP uses the same GPIO trace approach as EDF tests: task-code pins, a task-switch strobe, and a deadline-miss indicator.
- UART plus GPIO tracing is used so scheduler/resource behavior can be matched against expected timing.
- Deadline-miss GPIO can be sampled directly (or via LED) to confirm miss/overrun behavior.

# SRP Software Testing Setup
- SRP tests are in the `srp_tests` folder.
- Each test exposes an `srp_X_run()` entry point called from `main.c`.
- To run SRP tests, keep `configUSE_EDF == 1` and `configUSE_SRP == 1` in `schedulingConfig.h`, then select one test in `main.c`.
- `configUSE_SRP_SHARED_STACKS` toggles storage backend for comparing per-task stacks vs shared-stack mode.
- Shared helpers and trace utilities come from `test_utils.c/h` and `task_trace.c/h`.

# SRP Test Methods
- First validate scheduling/resource protocol behavior with shared stacks disabled (`configUSE_SRP_SHARED_STACKS = 0`).
- For lock/ceiling tests, compare GPIO waveform to expected release/preemption/ceiling-block ordering.
- Repeat with shared stacks enabled (`configUSE_SRP_SHARED_STACKS = 1`) to isolate stack-backend effects.
- Use runtime flags/asserts/UART prints for admission and overrun cases where pass criteria are API outcomes.

# SRP Test Index
| Test # | Summary Name | File / Entry Point | Type | Task Count | Key Goal |
|---|---|---|---|---:|---|
| 1 | Basic SRP Scheduling with Binary Semaphores | `srp_tests/test_1.c`, `srp_1_run()` | Periodic SRP scheduling | 3 | Validate SRP ordering and resource claims |
| 2 | SRP Ceiling Blocking with Constrained Task | `srp_tests/test_2.c`, `srp_2_run()` | Ceiling/priority interaction | 6 | Validate ceiling block against earlier-deadline task |
| 3 | SRP Admission with Blocking Terms | `srp_tests/test_3.c`, `srp_3_run()` | Admission control | 1 baseline + 2 candidates | Validate blocking-aware admission decisions |
| 4 | Shared-Stack Quantitative Study | `srp_tests/test_4.c`, `srp_4_run()` | Stack backend analysis | 12 workers + 1 reporter | Validate shared-stack savings and runtime stats |
| 5 | WCET Overrun in Critical Section | `srp_tests/test_5.c`, `srp_5_run()` | Overrun handling | 2 | Validate forced release and recovery after overrun |

# SRP Test Cases + Results

## Test 1 - Basic SRP Scheduling with Binary Semaphores
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Resources Claimed | First Release (ms) |
|---|---:|---:|---:|---|---:|
| T1 | 1500 | 4000 | 4000 | R1 | 0 |
| T2 | 1500 | 6000 | 6000 | R1, R2 | 0 |
| T3 | 2500 | 12000 | 12000 | R2 | 0 |

### Description of Test
Three synchronous periodic tasks share R1/R2 through SRP-aware binary semaphores. This baseline test confirms SRP ceiling ordering and absence of resource-induced inversion.

### Test Implementation
- File: `srp_tests/test_1.c`
- Entry point: `srp_1_run()`

### Expected Results and Results
Expected waveform placeholder:
![SRP Test 1 Expected](images/srp_test_1_expected.png)

Measured waveform placeholder:
![SRP Test 1 Results](images/srp_test_1_results.png)

#### Expected
- At `t=0s`, all three tasks release and the earliest eligible deadline executes first.
- While a task holds R1 or R2, lower-eligibility contenders must remain blocked by SRP ceiling rules.
- Across one 12 s hyperperiod, all jobs should complete without unexpected deadline-miss pulses.

#### Actual
-

#### Verified
- SRP resource claim/take/give path works under periodic load.
- Ceiling protocol prevents inversion while preserving EDF ordering among eligible tasks.
- Test behavior is consistent across shared-stack disabled/enabled configurations.

## Test 2 - SRP Ceiling Blocking with Constrained Task
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Resources Claimed | First Release (ms) |
|---|---:|---:|---:|---|---:|
| T1-T5 (same-deadline set) | ~1500-1800 each | 10000 | 10000 | Mix of R1/R2 | 0 |
| T6 (constrained) | 1600 | 10000 | 4000 | R1 | phased offset |

### Description of Test
Five same-deadline tasks share resources while a sixth shorter-deadline task is phased to become ready near an R1 critical section. The test validates that SRP ceiling blocks T6 until release.

### Test Implementation
- File: `srp_tests/test_2.c`
- Entry point: `srp_2_run()`

### Expected Results and Results
Expected waveform placeholder:
![SRP Test 2 Expected](images/srp_test_2_expected.png)

Measured waveform placeholder:
![SRP Test 2 Results](images/srp_test_2_results.png)

#### Expected
- T6 should become ready at its phase offset but be ineligible to run while R1-induced ceiling is active.
- T6 should only run after the blocking task releases R1 and the system ceiling drops.
- Same-deadline tasks should preserve expected periodic behavior around the T6 interference window.

#### Actual
-

#### Verified
- SRP system-ceiling gating overrides earlier deadline when resource protocol requires blocking.
- Resource release correctly re-enables blocked higher-urgency work.

## Test 3 - SRP Admission with Blocking Terms
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Resource Claim | First Release (ms) |
|---|---:|---:|---:|---|---:|
| Base task | 5000 | 10000 | 10000 | R1 long claim | 0 |
| Bad candidate | 1000 | 5000 | 5000 | R1 short claim | create-time |
| Good candidate | 1000 | 8000 | 8000 | R1 short claim | create-time |

### Description of Test
Admission-control test where a long R1 blocking interval makes a candidate fail once blocking terms are included. A second candidate with looser timing should pass.

### Test Implementation
- File: `srp_tests/test_3.c`
- Entry point: `srp_3_run()`

### Expected Results and Results
Expected waveform placeholder:
![SRP Test 3 Expected](images/srp_test_3_expected.png)

Measured waveform placeholder:
![SRP Test 3 Results](images/srp_test_3_results.png)

#### Expected
- Bad candidate create should return `pdFAIL`.
- Good candidate create should return `pdPASS`.
- Only admitted tasks should appear on task-code GPIO traces.

#### Actual
-
- Bad admission return:
- Good admission return:

#### Verified
- Blocking-aware SRP admission logic (not raw utilization-only) is active.
- Rejected tasks are not admitted to runtime schedule.

## Test 4 - Shared-Stack Quantitative Study
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Purpose | First Release (ms) |
|---|---:|---:|---:|---|---:|
| 12 worker tasks | 250 each | 20000 | test-specific | Shared-stack utilization across levels | 0 |
| Reporter task | lightweight | periodic | periodic | Calls SRP runtime stack stats API | periodic |

### Description of Test
A multi-task stack-usage study that compares theoretical non-shared allocation vs shared-stack allocation and checks runtime-reported values.

### Test Implementation
- File: `srp_tests/test_4.c`
- Entry point: `srp_4_run()`

### Expected Results and Results
Expected waveform placeholder:
![SRP Test 4 Expected](images/srp_test_4_expected.png)

Measured waveform placeholder:
![SRP Test 4 Results](images/srp_test_4_results.png)

#### Expected
- Reporter assertions for theoretical shared/non-shared sizes should hold during periodic checks.
- Shared-stack mode should report lower theoretical allocation than non-shared mode for this task set.
- No guard corruption or scheduler instability should appear during stats sampling.

#### Actual
-

#### Verified
- Shared-stack backend is functionally integrated with SRP scheduling.
- Runtime stats API output is consistent with configured stack-region model.

## Test 5 - WCET Overrun in Critical Section
### Task Table
| Task | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Resource Claim | First Release (ms) |
|---|---:|---:|---:|---|---:|
| Overrun task | 1000 (configured) / 1800 work | 8000 | 8000 | R1 | 0 |
| Observer task | 700 | 10000 | 10000 | R1 | 0 |

### Description of Test
Overrun task intentionally exceeds WCET while in an R1 critical section. Observer verifies the resource becomes available after kernel overrun handling.

### Test Implementation
- File: `srp_tests/test_5.c`
- Entry point: `srp_5_run()`

### Expected Results and Results
Expected waveform placeholder:
![SRP Test 5 Expected](images/srp_test_5_expected.png)

Measured waveform placeholder:
![SRP Test 5 Results](images/srp_test_5_results.png)

#### Expected
- Overrun task should trigger deadline-miss/overrun handling during its critical section.
- Kernel should force release resource state so observer can subsequently acquire R1.
- System should continue scheduling subsequent periods after the overrun event.

#### Actual
-
- Forced release observed flag:
- Observer acquire count:

#### Verified
- Overrun management mechanism works when the overrunning task holds SRP-managed resources.
- Recovery path restores resource availability and scheduler progress.
