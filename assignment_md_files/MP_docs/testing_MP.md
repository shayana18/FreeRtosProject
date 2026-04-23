# MP Hardware Testing Setup
- MP mode uses per-core GPIO task-code banks plus per-core switch strobes to avoid ambiguity during near-simultaneous switches.
- Core 0 task-code bank: GPIO `2,3,4`; core 0 switch strobe: GPIO `5`.
- Core 1 task-code bank: GPIO `6,7,8`; core 1 switch strobe: GPIO `9`.
- Shared deadline-miss indicator: GPIO `15`.

# MP Software Testing Setup
- MP tests are in the `mp_tests` tree (`global_edf_tests`, `partitioned_edf_tests`, and shared MP tests).
- Test entry points are exposed as `mp_*_run()` and selected in `main.c`.
- Build mode must match policy under test:
  - Global EDF: `GLOBAL_EDF_ENABLE=1`, `PARTITIONED_EDF_ENABLE=0`
  - Partitioned EDF: `GLOBAL_EDF_ENABLE=0`, `PARTITIONED_EDF_ENABLE=1`
- Admission/runtime-create tests additionally record `volatile` result flags for `xTaskCreate()` outcomes.

# MP Test Organization
- The MP test numbering in this document spans three buckets: policy-specific global EDF tests, policy-specific partitioned EDF tests, and shared MP tests that can run under either active MP mode.
- Because of that split, the numbered test in this document is not always the same as the numeric suffix of the source file. For example, the shared admission and runtime-create tests live in `mp_tests/test_1.c` and `mp_tests/test_2.c`, while policy-specific tests live in `mp_tests/global_edf_tests/` or `mp_tests/partitioned_edf_tests/`.
- For that reason, each test case below includes an explicit file path and entry point.

# Additional MP Test Files
- The MP source tree also contains policy-specific overrun/UART-focused tests that are useful during debugging even though they are not part of the main indexed GPIO comparison set in this document.
- In particular, `mp_tests/global_edf_tests/test_4.c` exposes `mp_global_edf_4_run()` and `mp_tests/partitioned_edf_tests/test_3.c` exposes `mp_partitioned_edf_3_run()`.
- Those tests are still referenced in `main.c`, so keeping them noted here makes the MP test tree easier to navigate even though the main results table emphasizes the more presentation-ready dispatch, migration, affinity, admission, and demo scenarios.

# MP Test Methods
- Use GPIO waveforms as primary evidence for dispatch, preemption, migration, and core ownership.
- For admission/runtime-create tests, pair waveform analysis with `volatile` accept/reject result variables.
- Validate each policy in its correct build mode before hardware-run interpretation.
- For small sets, compare first releases manually; for stress/bound tests, prioritize acceptance/rejection plus stability signals.

# MP Test Index
| Test # | Summary Name | File / Entry Point | Policy Mode | Type | Key Goal |
|---|---|---|---|---|---|
| 1 | Global EDF Basic Dispatch | `mp_tests/global_edf_tests/test_1.c`, `mp_global_edf_1_run()` | Global | Scheduling | Two earliest deadlines occupy two cores |
| 2 | Global EDF Preemption | `mp_tests/global_edf_tests/test_2.c`, `mp_global_edf_2_run()` | Global | Scheduling | Cross-core preemption/yield correctness |
| 3 | Global EDF Migration | `mp_tests/global_edf_tests/test_3.c`, `mp_global_edf_3_run()` | Global | Scheduling | Unrestricted task appears on different cores |
| 4 | Global EDF Affinity Enforcement | `mp_tests/global_edf_tests/test_5.c`, `mp_global_edf_5_run()` | Global | Scheduling/affinity | Pinned tasks stay on assigned cores |
| 5 | Partitioned EDF Basic Dispatch | `mp_tests/partitioned_edf_tests/test_1.c`, `mp_partitioned_edf_1_run()` | Partitioned | Scheduling/ownership | Tasks execute only on assigned partitions |
| 6 | Partitioned EDF Explicit Migration | `mp_tests/partitioned_edf_tests/test_2.c`, `mp_partitioned_edf_2_run()` | Partitioned | Migration | Task shifts to new partition after affinity change |
| 7 | MP Admission Control | `mp_tests/test_1.c`, `mp_edf_admission_1_run()` | Global/Partitioned | Admission | Reject unschedulable, accept schedulable tasks |
| 8 | MP Runtime Task Creation | `mp_tests/test_2.c`, `mp_edf_runtime_create_1_run()` | Global/Partitioned | Runtime create | Admit/reject tasks while scheduler is running |
| 9 | Global EDF Demo Compare | `mp_tests/test_compare_glob.c`, `mp_compare_glob_run()` | Global | Demo | Visual global EDF behavior comparison |
| 10 | Partitioned EDF Best-Fit Demo | `mp_tests/test_compare_part.c`, `mp_compare_part_run()` | Partitioned | Demo | Verify best-fit partition placement behavior |
| 11 | Conservative Global Admission (Dhall) | `mp_tests/global_edf_tests/test_dhall.c`, `mp_test_dhall_run()` | Global | Admission bound demo | Validate sufficient-bound rejection behavior |

# MP Test Cases + Results

## Test 1 - Global EDF Basic Dispatch
### Task Table
| Task/Class | Timing Definition | Affinity | First Release (ms) |
|---|---|---|---:|
| Startup periodic set (see `mp_tests/global_edf_tests/test_1.c`) | defined in source | Unrestricted/global | 0 |

### Description of Test
Baseline global EDF test to confirm simultaneous dispatch on two cores picks the two best (earliest deadline) runnable jobs.

### Test Implementation
- File: `mp_tests/global_edf_tests/test_1.c`
- Entry point: `mp_global_edf_1_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 1 Expected](images/mp_test_1_expected.png)

Measured waveform placeholder:
![MP Test 1 Results](images/mp_test_1_results.png)

#### Expected
- At startup, two earliest-deadline jobs should appear concurrently across core 0 and core 1 GPIO banks.
- Worse-deadline ready jobs should remain pending until one core frees.

#### Actual
-

#### Verified
- Global EDF dual-core selection behavior is correct for simultaneous ready jobs.

## Test 2 - Global EDF Preemption
### Task Table
| Task/Class | Timing Definition | Affinity | Trigger |
|---|---|---|---|
| Preemption scenario set (see `mp_tests/global_edf_tests/test_2.c`) | defined in source | Unrestricted/global | arrival of earlier-deadline job |

### Description of Test
Checks that when a better-deadline job arrives, kernel yields the appropriate running core and replaces a worse active job.

### Test Implementation
- File: `mp_tests/global_edf_tests/test_2.c`
- Entry point: `mp_global_edf_2_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 2 Expected](images/mp_test_2_expected.png)

Measured waveform placeholder:
![MP Test 2 Results](images/mp_test_2_results.png)

#### Expected
- During steady execution, an arriving earlier-deadline job should trigger a core switch.
- Preempted job should disappear from one bank and reappear later when eligible.

#### Actual
-

#### Verified
- Cross-core preemption logic and yield signaling are functioning under global EDF.

## Test 3 - Global EDF Migration
### Task Table
| Task/Class | Timing Definition | Affinity | Observation Window |
|---|---|---|---|
| Migration scenario set (see `mp_tests/global_edf_tests/test_3.c`) | defined in source | Unrestricted/global | multiple releases |

### Description of Test
Validates that globally runnable tasks are not permanently tied to one core and can migrate across jobs.

### Test Implementation
- File: `mp_tests/global_edf_tests/test_3.c`
- Entry point: `mp_global_edf_3_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 3 Expected](images/mp_test_3_expected.png)

Measured waveform placeholder:
![MP Test 3 Results](images/mp_test_3_results.png)

#### Expected
- Same task ID should appear on core 0 in one job and core 1 in another job under interference changes.
- Migration should not violate EDF ordering among runnable tasks.

#### Actual
-

#### Verified
- Global-task migration across cores is supported and observable on GPIO banks.

## Test 4 - Global EDF Affinity Enforcement
### Task Table
| Task/Class | Timing Definition | Affinity | First Release (ms) |
|---|---|---|---:|
| Pinned tasks + unrestricted tasks (see `mp_tests/global_edf_tests/test_5.c`) | defined in source | mixed pinned/unrestricted | 0 |

### Description of Test
Ensures per-task affinity masks are respected even while unrestricted tasks are globally scheduled.

### Test Implementation
- File: `mp_tests/global_edf_tests/test_5.c`
- Entry point: `mp_global_edf_5_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 4 Expected](images/mp_test_4_expected.png)

Measured waveform placeholder:
![MP Test 4 Results](images/mp_test_4_results.png)

#### Expected
- Pinned task IDs appear only on their assigned core bank for the full run.
- Unrestricted task IDs may appear on either core.

#### Actual
-

#### Verified
- Affinity-mask enforcement remains correct under global EDF.

## Test 5 - Partitioned EDF Basic Dispatch
### Task Table
| Task/Class | Timing Definition | Affinity | First Release (ms) |
|---|---|---|---:|
| Partition-owned tasks (see `mp_tests/partitioned_edf_tests/test_1.c`) | defined in source | explicit one-hot partition affinity | 0 |

### Description of Test
Baseline partitioned EDF ownership test to confirm tasks execute only on their assigned partition/core.

### Test Implementation
- File: `mp_tests/partitioned_edf_tests/test_1.c`
- Entry point: `mp_partitioned_edf_1_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 5 Expected](images/mp_test_5_expected.png)

Measured waveform placeholder:
![MP Test 5 Results](images/mp_test_5_results.png)

#### Expected
- Each task ID appears only on its assigned core bank.
- No cross-partition execution occurs without explicit migration/affinity change.

#### Actual
-

#### Verified
- Partition ownership enforcement works for static affinities.

## Test 6 - Partitioned EDF Explicit Migration
### Task Table
| Task/Class | Timing Definition | Affinity | Migration Event |
|---|---|---|---|
| Migration scenario tasks (see `mp_tests/partitioned_edf_tests/test_2.c`) | defined in source | changed at runtime | controller-triggered affinity update |

### Description of Test
Controller changes a task's affinity from one partition/core to the other.

### Test Implementation
- File: `mp_tests/partitioned_edf_tests/test_2.c`
- Entry point: `mp_partitioned_edf_2_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 6 Expected](images/mp_test_6_expected.png)

Measured waveform placeholder:
![MP Test 6 Results](images/mp_test_6_results.png)

#### Expected
- Before migration instant, migrating task appears only on original core bank.
- After migration instant, task appears only on new core bank.

#### Actual
-

#### Verified
- Runtime affinity change is honored by partitioned EDF scheduler.

## Test 7 - MP Admission Control
### Task Table
| Task/Class | Period/WCET Source | Affinity/Policy | Creation Phase |
|---|---|---|---|
| Baseline tasks + bad/good candidates (`mp_tests/test_1.c`) | mode-dependent macros in source | active MP mode | startup and/or controller |

### Description of Test
Admission-control validation in MP mode with both unschedulable and schedulable candidates.

### Test Implementation
- File: `mp_tests/test_1.c`
- Entry point: `mp_edf_admission_1_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 7 Expected](images/mp_test_7_expected.png)

Measured waveform placeholder:
![MP Test 7 Results](images/mp_test_7_results.png)

#### Expected
- Bad candidate creation returns `pdFAIL`.
- Good candidate creation returns `pdPASS`.
- Rejected task does not appear on either core GPIO bank.

#### Actual
-
- Bad admission return:
- Good admission return:

#### Verified
- MP admission-control path distinguishes feasible/infeasible candidates.
- Admit/reject API outcomes match observed runtime presence/absence.

## Test 8 - MP Runtime Task Creation
### Task Table
| Task/Class | Period/WCET Source | Affinity/Policy | Creation Phase |
|---|---|---|---|
| Runtime create scenario (`mp_tests/test_2.c`) | mode-dependent macros in source | active MP mode | after scheduler start |

### Description of Test
Controller creates candidate tasks during runtime to verify dynamic insertion behavior in MP EDF mode.

### Test Implementation
- File: `mp_tests/test_2.c`
- Entry point: `mp_edf_runtime_create_1_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 8 Expected](images/mp_test_8_expected.png)

Measured waveform placeholder:
![MP Test 8 Results](images/mp_test_8_results.png)

#### Expected
- Runtime good candidate is admitted and appears on an eligible core bank.
- Runtime bad candidate is rejected and remains absent from both banks.
- Existing workload continues without scheduler stall.

#### Actual
-
- Runtime bad admission return:
- Runtime good admission return:

#### Verified
- Runtime task admission while scheduler is active.
- MP policy handles dynamic task insertion/rejection correctly.

## Test 9 - Global EDF Demo Compare
### Task Table
| Task/Class | Timing Definition | Affinity | First Release (ms) |
|---|---|---|---:|
| Demo global set (`mp_tests/test_compare_glob.c`) | defined in source | unrestricted/global | 0 |

### Description of Test
Presentation/demo scenario for visualizing global EDF behavior on two cores.

### Test Implementation
- File: `mp_tests/test_compare_glob.c`
- Entry point: `mp_compare_glob_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 9 Expected](images/mp_test_9_expected.png)

Measured waveform placeholder:
![MP Test 9 Results](images/mp_test_9_results.png)

#### Expected
- Two-core execution reflects global EDF ordering for demo task set.

#### Actual
-

#### Verified
- Demo waveform matches intended global EDF narrative.

## Test 10 - Partitioned EDF Best-Fit Demo
### Task Table
| Task/Class | Timing Definition | Affinity | Placement Rule |
|---|---|---|---|
| Demo partitioned set (`mp_tests/test_compare_part.c`) | defined in source | initially unpinned/create-time decided | best-fit partition selection |

### Description of Test
Demo scenario showing partition assignment behavior for non-explicitly pinned tasks.

### Test Implementation
- File: `mp_tests/test_compare_part.c`
- Entry point: `mp_compare_part_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 10 Expected](images/mp_test_10_expected.png)

Measured waveform placeholder:
![MP Test 10 Results](images/mp_test_10_results.png)

#### Expected
- New tasks are assigned to partitions according to best-fit placement rule.
- After placement, each task executes only on its selected partition core.

#### Actual
-

#### Verified
- Partition placement heuristic behavior is observable and stable.

## Test 11 - Conservative Global Admission (Dhall)
### Task Table
| Task/Class | Timing Definition | Affinity | Admission Criterion |
|---|---|---|---|
| Dhall-bound scenario (`mp_tests/global_edf_tests/test_dhall.c`) | defined in source | unrestricted/global | sufficient-bound check |

### Description of Test
Demonstrates that sufficient-bound-based global admission may reject a candidate even when raw total capacity appears available.

### Test Implementation
- File: `mp_tests/global_edf_tests/test_dhall.c`
- Entry point: `mp_test_dhall_run()`

### Expected Results and Results
Expected waveform placeholder:
![MP Test 11 Expected](images/mp_test_11_expected.png)

Measured waveform placeholder:
![MP Test 11 Results](images/mp_test_11_results.png)

#### Expected
- Candidate violating the configured sufficient bound is rejected (`pdFAIL`).
- Accepted tasks continue running normally on both cores.

#### Actual
-
- Dhall candidate admission return:

#### Verified
- Conservative global admission bound is enforced in implementation.
