# CBS Hardware Testing Setup
- CBS uses the same GPIO trace setup as EDF and SRP: task-code pins, a task-switch strobe, and a deadline-miss indicator.
- UART output is used alongside GPIO traces to report CBS-specific state such as server deadline changes, submissions, and job completions.
- Deadline-miss GPIO indicates periodic EDF deadline misses. CBS budget exhaustion does not use that pin because the server handles it by pushing its deadline forward.

# CBS Software Testing Setup
- CBS tests are in the `cbs_tests` folder.
- The captured hardware assets for this document live under `assignment_md_files/test_assets/CBS Tests/`.
- Each test exposes a `cbs_X_run()` entry point called from `main.c`.
- To run CBS tests, keep `configUSE_EDF == 1`, `configUSE_UP == 1`, `configUSE_CBS == 1`, and `configUSE_SRP == 0` in `schedulingConfig.h`, then select one test in `main.c`.
- Common trace and helper utilities are in `task_trace.c/h` and `test_utils.c/h`.
- CBS traces use the fixed task ID of the bound CBS worker/server task. Individual aperiodic jobs submitted to that server do not get their own task IDs.

# CBS Test Methods
- Task IDs are observed on the GPIO task-code pins and decoded using the task-switch strobe as a sampling clock. On the waveform, the strobe edge marks when the binary task-code output should be read.
- In the normal binary-trace setup, up to 7 GPIO bits can be used for task ID output. When a given test has fewer tasks and deadline-miss observation is important, the highest binary-output pin may instead be repurposed as a direct deadline-miss probe.
- When a test has very few tasks, one-hot encoding may be used instead of binary encoding to make the waveform easier to read directly.
- For precise timing analysis, the captured discussion in this document focuses on smaller windows that are sufficient to prove the intended scheduling or synchronization feature. We intentionally do not require full-hyperperiod inspection for every test when a shorter interval already demonstrates correctness.
- Compare GPIO task-order traces against the expected periodic/CBS interaction for the first several server periods.
- Use UART prints and in-test assertions to confirm server creation, submission success/failure, deadline updates, and job completion counts.
- For CBS-specific behavior, distinguish server deadline postponement from ordinary periodic deadline misses: server budget exhaustion should shift the server deadline, while periodic EDF misses should still raise the miss indicator and hook output.

# CBS Test Index
| Test # | Summary Name | File / Entry Point | Type | Task Count | Key Goal |
|---|---|---|---|---:|---|
| 1 | Single CBS Server with One Periodic Task | `cbs_tests/test_1.c`, `cbs_1_run()` | Baseline CBS integration | 3 | Validate one periodic task plus one CBS-managed worker and arrival source |
| 2 | Two Servers with Mixed Periodic Load | `cbs_tests/test_2.c`, `cbs_2_run()` | Multi-server scheduling | 6 | Validate concurrent CBS servers alongside two periodic EDF tasks |
| 3 | CBS Deadline Transition Rules | `cbs_tests/test_3.c`, `cbs_3_run()` | Server deadline update | 3 | Validate idle-arrival reset and budget-exhaustion deadline push |
| 4 | Equal-Deadline Tie Break | `cbs_tests/test_4.c`, `cbs_4_run()` | Tie handling | 3 | Validate CBS wins the configured equal-deadline tie case |
| 5 | Periodic Overrun with CBS Background Load | `cbs_tests/test_5.c`, `cbs_5_run()` | Periodic overrun / deadline miss | 3 | Validate periodic EDF overrun handling while a CBS server remains active |

# CBS Test Cases + Results

## Test 1 - Single CBS Server with One Periodic Task
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Role | First Release (ms) |
|---|---:|---:|---:|---|---:|
| Periodic EDF task | 300 | 1500 | 1500 | Baseline periodic load | 0 |
| CBS server/worker | 400 budget / 250 job work | 2000 | server-managed | Aperiodic worker bound to one server | arrival-driven |
| Arrival source | 20 | 100 | 100 | Periodically submits CBS jobs | 0 |

### Description of Test
One periodic EDF task executes alongside a single CBS-managed worker. A separate arrival task submits short aperiodic jobs to the server. The goal is to confirm the basic CBS plumbing without stressing deadline transitions.

### Test Implementation
- File: `cbs_tests/test_1.c`
- Entry point: `cbs_1_run()`

### Trace Task IDs
- `Periodic EDF task -> 1`
- `CBS server/worker task -> 2`
- `Arrival source` does not have its own trace task ID
- Each submitted CBS job reuses the worker/server trace ID rather than getting a new ID

### Expected Results and Results

#### Expected
- Periodic EDF execution should continue at its configured rate while the CBS worker services submitted jobs.
- CBS submissions may occasionally fail if a previous job is still outstanding, but the system should remain stable.
- Traces should show one fixed task ID for the CBS worker even though multiple aperiodic jobs are submitted over time.

#### Actual
-

#### Verified
- Basic CBS server creation and worker binding path works.
- Aperiodic jobs execute through the bound worker task under EDF scheduling.

## Test 2 - Two Servers with Mixed Periodic Load
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Role | First Release (ms) |
|---|---:|---:|---:|---|---:|
| P1 | 250 | 3000 | 3000 | Periodic EDF baseline | 0 |
| P2 | 350 | 5000 | 5000 | Periodic EDF baseline | 0 |
| CBS A1 server/worker | 1000 budget / 550 job work | 4000 | server-managed | First CBS reservation | arrival-driven |
| CBS A2 server/worker | 3000 budget / 400 job work | 6000 | server-managed | Second CBS reservation | arrival-driven |
| Arrival source A1 | 20 | 1000 | 1000 | Submits A1 jobs | 0 |
| Arrival source A2 | 20 | 1500 | 1500 | Submits A2 jobs | 0 |

### Description of Test
Two periodic EDF tasks run together with two independent CBS servers and their arrival sources. This checks that multiple servers can coexist and that each worker stays attached to its own reservation.

### Test Implementation
- File: `cbs_tests/test_2.c`
- Entry point: `cbs_2_run()`

### Trace Task IDs
- `P1 -> 1`
- `P2 -> 2`
- `CBS A1 server/worker -> 4`
- `CBS A2 server/worker -> 8`
- `Arrival source A1` and `Arrival source A2` do not have their own trace task IDs
- Individual aperiodic jobs reuse the fixed worker/server ID of their bound CBS server

### Expected Results and Results
Expected waveform:
![Test 2 CBS Answer](<../test_assets/CBS Tests/Test 2/Test 2 CBS Answer.jpg>)

Measured waveform:
![Test 2 CBS Waveform](<../test_assets/CBS Tests/Test 2/Test 2 CBS Waveform.png>)

<!-- Task Set
![Test 2 CBS Description](<../test_assets/CBS Tests/Test 2/Test 2 CBS Description.jpg>) -->

#### Expected
- Both periodic tasks should continue to release and complete normally.
- Each CBS worker should execute only jobs submitted to its bound server.
- The system should tolerate occasional submission failures when a worker already has an outstanding job.

#### Actual
-

#### Verified
- Multiple concurrent CBS servers remain schedulable.
- Server-specific worker binding and submission paths behave independently.

## Test 3 - CBS Deadline Transition Rules
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Role | First Release (ms) |
|---|---:|---:|---:|---|---:|
| Periodic EDF task | 2200 | 6000 | 6000 | Background periodic load | 0 |
| CBS server/worker | 1000 budget / 250 then 1200 job work | 8000 | server-managed | Observe deadline reset and push | sparse then frequent |
| Arrival source | 20 | 200 | 200 | Submits sparse then frequent jobs | phased |

### Description of Test
This test focuses on CBS deadline-update semantics. A sparse first arrival should reset the server deadline to `arrival + T`, while later frequent arrivals drain the budget and force a push to `current_deadline + T`.

### Test Implementation
- File: `cbs_tests/test_3.c`
- Entry point: `cbs_3_run()`

### Trace Task IDs
- `Periodic EDF task -> 1`
- `CBS server/worker task -> 2`
- `Arrival source` does not have its own trace task ID
- Individual aperiodic jobs reuse the fixed worker/server ID of the CBS server

### Expected Results and Results
Measured waveform:
![Test 3 CBS Waveform](<../test_assets/CBS Tests/Test 3/Test 3 CBS Waveform.png>)

#### Expected
- The first sparse submit should move the server deadline to `arrival + period`.
- Once the worker consumes the remaining budget, the server deadline should advance by one server period.
- These CBS deadline moves should not be confused with ordinary periodic deadline misses.

#### Actual
-

#### Verified
- Idle-arrival reset rule is implemented.
- Budget-exhaustion deadline postponement is implemented.

## Test 4 - Equal-Deadline Tie Break
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Role | First Release (ms) |
|---|---:|---:|---:|---|---:|
| Periodic EDF task | 1200 | 7000 | 7000 | Competes at equal deadlines | 0 |
| CBS server/worker | 2200 budget / 900 job work | 7000 | server-managed | Competes at equal deadlines | arrival-driven |
| Tie-arrival source | 20 | 7000 | 100 | Releases job to line up tie | anchored to periodic release |

### Description of Test
The periodic task and CBS worker are arranged so they become ready with matching absolute deadlines. The test checks the configured tie-break behavior in that equal-deadline window.

### Test Implementation
- File: `cbs_tests/test_4.c`
- Entry point: `cbs_4_run()`

### Trace Task IDs
- `Periodic EDF task -> 1`
- `CBS server/worker task -> 2`
- `Tie-arrival source -> 4`
- Individual submitted CBS jobs reuse the worker/server trace ID rather than getting new IDs

### Expected Results and Results
Measured waveform:
![Test 4 CBS Waveform](<../test_assets/CBS Tests/Test 4/Test 4 CBS Waveform.png>)

Console Output
![Test 4 CBS Console Output](<../test_assets/CBS Tests/Test 4/Test 4 CBS Console Output.png>)

#### Expected
- When the tie window opens, the CBS worker should win the equal-deadline scheduling decision.
- UART output should show repeated tie attempts and consistent CBS wins.
- The same CBS worker task ID should appear across repeated aperiodic jobs.

#### Actual
-

#### Verified
- Equal-deadline tie handling for CBS vs periodic EDF matches the implemented policy.

## Test 5 - Periodic Overrun with CBS Background Load
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Role | First Release (ms) |
|---|---:|---:|---:|---|---:|
| Periodic EDF overrun task | 700 configured / 2200 work | 6000 | 1800 | Intentionally overruns WCET and misses deadline | 0 |
| CBS server/worker | 1200 budget / 350 job work | 5000 | server-managed | Background CBS workload | offset arrival |
| CBS source task | 50 | 5000 | 100 | Periodically submits short CBS jobs | 1000 |

### Description of Test
One normal periodic EDF task deliberately executes longer than both its WCET and its relative deadline, while a CBS worker continues to service short jobs under its server reservation. The CBS source task has a much shorter deadline than the other work so it should run first at each release and submit the server job promptly. This keeps CBS active in the system without making CBS budget exhaustion the reason for the overrun behavior under test.

This test also flushes deferred WCET/deadline UART output from task context. Because those flushes happen close to the periodic `xTaskDelayUntil()` boundary, a late return can cause the task code to re-anchor its local `xLastWakeTime` to the current tick. When that happens, later printed release/deadline values may drift away from exact period multiples even though the behavior is still deterministic.

### Test Implementation
- File: `cbs_tests/test_5.c`
- Entry point: `cbs_5_run()`

### Trace Task IDs
- `Periodic EDF overrun task -> 1`
- `CBS server/worker task -> 2`
- `CBS source task -> 4`
- Individual CBS jobs reuse the fixed worker/server ID rather than getting separate IDs

### Expected Results and Results
Measured waveform:
![Test 5 CBS Waveform](<../test_assets/CBS Tests/Test 5/Test 5 CBS Waveform.png>)

Console Output
![Test 5 CBS Output](<../test_assets/CBS Tests/Test 5/Test 5 CBS Output.png>)

#### Expected
- The periodic EDF task should first report a WCET overrun and then a deadline miss.
- The short-deadline CBS source task should run first at each source release so the CBS job is submitted before the longer-running work dominates the CPU.
- The deadline-miss GPIO should pulse for the periodic task, not for the CBS worker.
- The CBS worker should keep completing short jobs with the same fixed trace task ID across submissions.
- UART prints should show server activity continuing even while the periodic task is being stopped and released by the normal EDF miss path.

#### Actual
-

#### Verified
- Periodic EDF WCET-overrun and deadline-miss handling still works in CBS-enabled builds.
- CBS background activity remains separate from the periodic overrun path.
