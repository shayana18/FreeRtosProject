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
- Note that a CBS server only accepts a submission once the current job is complete. If a new job arrives while the previous one is still active, the submission will fail.

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
| 1 | Two Servers with Mixed Periodic Load | `cbs_tests/test_1.c`, `cbs_1_run()` | Multi-server scheduling | 6 | Validate concurrent CBS servers alongside two periodic EDF tasks |
| 2 | CBS Deadline Transition Rules | `cbs_tests/test_2.c`, `cbs_2_run()` | Server deadline update | 3 | Validate idle-arrival reset and budget-exhaustion deadline push |
| 3 | Equal-Deadline Tie Break | `cbs_tests/test_3.c`, `cbs_3_run()` | Tie handling | 3 | Validate CBS wins the configured equal-deadline tie case |
| 4 | Periodic Overrun with CBS Background Load | `cbs_tests/test_4.c`, `cbs_4_run()` | Periodic overrun / deadline miss | 3 | Validate periodic EDF overrun handling while a CBS server remains active |

# CBS Test Cases + Results

## Test 1 - Two Servers with Mixed Periodic Load
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
- File: `cbs_tests/test_1.c`
- Entry point: `cbs_1_run()`

### Trace Task IDs
- `P1 -> 1`
- `P2 -> 2`
- `CBS A1 server/worker -> 4`
- `CBS A2 server/worker -> 8`
- `Arrival source A1` and `Arrival source A2` do not have their own trace task IDs
- Individual aperiodic jobs reuse the fixed worker/server ID of their bound CBS server

### Expected Results and Results
Expected waveform:
<img src="../test_assets/CBS Tests/Test 1/Test 1 CBS Answer.jpg" alt="Test 1 CBS Answer" width="650">

Measured waveform:
![Test 1 CBS Waveform](<../test_assets/CBS Tests/Test 1/Test 1 CBS Waveform.png>)

<!-- Task Set
![Test 1 CBS Description](<../test_assets/CBS Tests/Test 1/Test 1 CBS Description.jpg>) -->

#### Expected
- Both periodic tasks should continue to release and complete normally.
- Each CBS worker should execute only jobs submitted to its bound server.
- The system should tolerate occasional submission failures when a worker already has an outstanding job.
- At marker `1` on the hand-drawn expected waveform, CBS server 1 initially has an absolute deadline of `t=4s`. It exhausts its budget before finishing its aperiodic job, so CBS pushes its deadline to `t=8s` and refreshes its budget. With that later deadline, periodic task 2, with deadline `t=5s`, can preempt CBS server 1. After that, CBS server 2, with deadline `t=6s`, can run its own aperiodic job.
- At marker `2`, periodic task 1 and aperiodic jobs for CBS servers 1 and 2 arrive at the same time. CBS server 2 would otherwise have the earliest deadline because it did not consume all of its previous budget (`t=6s`). However, the arrival-time CBS check `C_cbs >= (deadline - arrival) * U_cbs` is triggered, so CBS server 2 has its deadline pushed to `arrival + period = 3s + 6s = 9s` and its budget is refreshed. With this new deadline, periodic task 1's deadline of `t=6s` is earliest. Before this deadline push, CBS server 2 would have run first because the tie-breaking clause favors CBS on equal deadlines. After periodic task 1, CBS server 1 runs because its deadline is `t=8s`, though it still does not finish its job.
- Marker `3` shows the same budget-exhaustion behavior for CBS server 1: it uses up its budget, gets its deadline pushed to `t=12s`, and CBS server 2 is then able to preempt.

#### Actual
- The measured waveform follows this behavior exactly for the first `4-5s`, matching the expected interactions at the marked points.
- This shows that the two main CBS governing clauses are functional: budget exhaustion pushes the server deadline by one server period, and the arrival-time check can push a server deadline to `arrival + period` when the remaining budget is too large for the old deadline.
- Small dips inside tasks that otherwise look continuous in the expected schedule are caused by the short periodic source tasks that release aperiodic jobs to the CBS servers.

#### Verified
- Multiple concurrent CBS servers remain schedulable.
- Server-specific worker binding and submission paths behave independently.

## Test 2 - CBS Deadline Transition Rules
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Role | First Release (ms) |
|---|---:|---:|---:|---|---:|
| Periodic EDF task | 2200 | 6000 | 6000 | Background periodic load | 0 |
| CBS server/worker | 1000 budget / 250 then 1200 job work | 8000 | server-managed | Observe deadline reset and push | sparse then frequent |
| Arrival source | 20 | 200 | 200 | Submits sparse then frequent jobs | phased |

### Description of Test
This test focuses on CBS deadline-update semantics. A sparse first arrival should reset the server deadline to `arrival + T`, while later frequent arrivals drain the budget and force a push to `current_deadline + T`.

### Test Implementation
- File: `cbs_tests/test_2.c`
- Entry point: `cbs_2_run()`

### Trace Task IDs
- `Periodic EDF task -> 1`
- `CBS server/worker task -> 2`
- `Arrival source` does not have its own trace task ID
- Individual aperiodic jobs reuse the fixed worker/server ID of the CBS server

### Expected Results and Results
Measured waveform:
![Test 2 CBS Waveform](<../test_assets/CBS Tests/Test 2/Test 2 CBS Waveform.png>)

#### Expected
- The first sparse submit should move the server deadline to `arrival + period`.
- Once the worker consumes the remaining budget, the server deadline should advance by one server period.
- These CBS deadline moves should not be confused with ordinary periodic deadline misses.
- The source task is intentionally delayed by `3.5s` before submitting the first CBS job.
- The first submitted job is short (`250ms`), so the initial server behavior is dominated by the arrival-time check `C_cbs >= (deadline - arrival) * U_cbs`. This should push the server deadline to `arrival + period`.
- After that light first job, the test switches to a heavier workload: `1200ms` jobs are attempted every `200ms`, which repeatedly drains the server budget and forces deadline postponement.
- The test includes `configASSERT`s for both CBS deadline rules: the first checks that the arrival-time rule moves the deadline to `arrival + period`, and later checks confirm that budget exhaustion moves the deadline to `current_deadline + period`.
- Visually, the waveform should show the CBS server running in roughly `1.2s` increments with about `200ms` gaps between repeated submission attempts.
- The periodic task should always preempt the CBS server because the server deadline is repeatedly pushed far enough into the future that the periodic task has the earlier deadline.

#### Actual
- The measured waveform matches the expected pattern: the CBS worker runs in repeated long bursts, separated by short pauses from the source task's repeated submission attempts.
- The periodic task preempts the CBS server as expected, because the server deadline is repeatedly postponed beyond the periodic task's active deadline.
- As in Test 1, the small dips in otherwise continuous task regions are caused by the short task that attempts to submit jobs to the CBS server.

#### Verified
- Idle-arrival reset rule is implemented.
- Budget-exhaustion deadline postponement is implemented.

## Test 3 - Equal-Deadline Tie Break
### Task Table
| Task/Class | WCET C (ms) | Period T (ms) | Relative Deadline D (ms) | Role | First Release (ms) |
|---|---:|---:|---:|---|---:|
| Periodic EDF task | 1200 | 7000 | 7000 | Competes at equal deadlines | 0 |
| CBS server/worker | 2200 budget / 900 job work | 7000 | server-managed | Competes at equal deadlines | arrival-driven |
| Tie-arrival source | 20 | 7000 | 100 | Releases job to line up tie | anchored to periodic release |

### Description of Test
The periodic task and CBS worker are arranged so they become ready with matching absolute deadlines. The test checks the configured tie-break behavior in that equal-deadline window.

### Test Implementation
- File: `cbs_tests/test_3.c`
- Entry point: `cbs_3_run()`

### Trace Task IDs
- `Periodic EDF task -> 1`
- `CBS server/worker task -> 2`
- `Tie-arrival source -> 4`
- Individual submitted CBS jobs reuse the worker/server trace ID rather than getting new IDs

### Expected Results and Results
Measured waveform:
![Test 3 CBS Waveform](<../test_assets/CBS Tests/Test 3/Test 3 CBS Waveform.png>)

Console Output
![Test 3 CBS Console Output](<../test_assets/CBS Tests/Test 3/Test 3 CBS Console Output.png>)

#### Expected
- When the tie window opens, the CBS worker should win the equal-deadline scheduling decision.
- UART output should show repeated tie attempts and consistent CBS wins.
- The same CBS worker task ID should appear across repeated aperiodic jobs.
- The CBS server deadline and the periodic task deadline are manually aligned on repeated releases.
- Each time the aligned-deadline window is created, the CBS server is expected to run and finish its aperiodic job before the periodic task runs.
- The program also asserts after releasing the CBS job and adjusting the server deadline that the aperiodic task was the first task to run before the periodic task. Therefore, the program should continue running without halting.

#### Actual
- As expected, the CBS server runs first in the equal-deadline window and completes before the periodic job.
- The small dips are again from the submission/check task. In particular, the dip shortly after the CBS server starts running each job is the check from the submission task that the aperiodic job ran first.
- The console output verifies that the relevant deadlines were set to the same value during the tie attempts, and the continued execution confirms that the in-test assertions passed.

#### Verified
- Equal-deadline tie handling for CBS vs periodic EDF matches the implemented policy.

## Test 4 - Periodic Overrun with CBS Background Load
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
- File: `cbs_tests/test_4.c`
- Entry point: `cbs_4_run()`

### Trace Task IDs
- `Periodic EDF overrun task -> 1`
- `CBS server/worker task -> 2`
- `CBS source task -> 4`
- Individual CBS jobs reuse the fixed worker/server ID rather than getting separate IDs

### Expected Results and Results
Measured waveform:
![Test 4 CBS Waveform](<../test_assets/CBS Tests/Test 4/Test 4 CBS Waveform.png>)

Console Output
![Test 4 CBS Output](<../test_assets/CBS Tests/Test 4/Test 4 CBS Output.png>)

#### Expected
- The periodic EDF task should first report a WCET overrun and then a deadline miss.
- The short-deadline CBS source task should run first at each source release so the CBS job is submitted before the longer-running work dominates the CPU.
- The deadline-miss GPIO should pulse for the periodic task, not for the CBS worker.
- The CBS worker should keep completing short jobs with the same fixed trace task ID across submissions.
- UART prints should show server activity continuing even while the periodic task is being stopped and released by the normal EDF miss path.
- This should look similar to the EDF overrun/deadline-miss handling tests: the periodic task exceeds its configured WCET, misses its deadline, and is then handled by the normal deadline-miss path rather than by CBS budget logic.
- Channel 6 monitors the deadline-miss pin for this test.

#### Actual
- The waveform shows the long-running periodic task occupying the CPU until it overruns and misses its deadline. Channel 6 pulses during the miss window, matching the expected deadline-miss indication.
- The console output reports `WCET overrun: task id=1` followed by `Deadline miss: task id=1`, confirming that the periodic task, not the CBS worker, is the task being reported by the EDF miss path.
- The CBS source and worker continue to appear after the miss, and the console shows server job counts increasing. This confirms that CBS background work remains active while the periodic overrun is handled separately.

#### Verified
- Periodic EDF WCET-overrun and deadline-miss handling still works in CBS-enabled builds.
- CBS background activity remains separate from the periodic overrun path.
