# EDF hardware testing setup

- The debugger UART is kept available, and GPIO trace pins are used to show which task is currently running.
- Seven task-ID pins encode the running task ID. The idle task uses ID `0`.
- One task-switch strobe pin tells the Saleae logic analyzer when to latch the task-ID pins.
- The combined seven-bit task ID lets us observe up to 127 task IDs in one test. Smaller tests sometimes use one-hot task IDs because those waveforms are easier to read by eye.
- A separate deadline-miss GPIO is raised for a short time when a deadline miss is detected. On the breadboard this pin can also drive an LED, so misses are visible even without the logic analyzer connected.

# EDF software testing setup

- EDF tests are in the `edf_tests` folder at the project root. Each test exposes a function that can be called from `main.c`.
- `main.c` lists the test entry points with short comments. To run a test, uncomment the matching call, build, and flash the Raspberry Pi Pico.
- EDF tests reuse the common helpers in `test_utils.c/h` and `task_trace.c/h`.

# EDF test method

- For small task sets, we manually derive the expected EDF schedule for the first part of the run. This makes it easy to check cases such as preemption by an earlier-deadline task, no preemption when the current task still has the earlier deadline, and deadline-miss behavior.
- The hardware waveform is then compared against that manual schedule.
- For larger task sets, especially tests near the assignment's 100-task requirement, deriving the whole schedule by hand is not practical. In those cases, we first check admission/utilization and then use the waveform mainly to confirm that tasks continue to run and that the deadline-miss pin does not fire unexpectedly.
- Runtime admission tests can have slightly different creation times than expected because the controller task is itself scheduled by EDF. For those tests, `volatile` result variables are used to confirm whether task creation returned `pdPASS` or `pdFAIL`.

# EDF test cases

| Test | File / entry point | Scenario | Expected result | Status |
| --- | --- | --- | --- | --- |
| Basic implicit-deadline dispatch | `edf_tests/test_1.c`, `edf_1_run()` | Three implicit-deadline tasks (T=5000/7000/8000 ms, U≈0.76) released simultaneously at startup. | Tasks run in EDF order throughout; earliest absolute deadline always executes first; no deadline misses. | Implemented. GPIO waveform verified against manually derived schedule. |
| Higher-utilization implicit-deadline dispatch | `edf_tests/test_2.c`, `edf_2_run()` | Four harmonic implicit-deadline tasks (T=2/4/8/16 s, all C=1 s, U=15/16=0.9375). | EDF ordering preserved at every release; no deadline misses despite utilization near 1.0. | Implemented. Harmonic periods make each release boundary verifiable by hand. |
| Runtime admission control — implicit deadline | `edf_tests/test_3.c`, `edf_3_run()` | Baseline U=0.85. A controller task first tries to add a bad candidate (U would exceed 1.0), then a good candidate (U stays below 1.0). | Bad task returns `pdFAIL`; `volatile` flag records the rejection. Good task returns `pdPASS` and appears on GPIO. | Implemented. `volatile` result variables checked in addition to GPIO. |
| Constrained-deadline dispatch | `edf_tests/test_4.c`, `edf_4_run()` | Three constrained-deadline tasks (D<T): T1=5000/1000/3000 ms, T2=7000/1500/3500 ms, T3=10000/1000/8000 ms. Kernel uses processor-demand analysis at admission. | All three tasks are admitted and run without deadline misses. | Implemented. DBF check points documented in source comments; waveform validated. |
| Higher-utilization constrained-deadline dispatch | `edf_tests/test_5.c`, `edf_5_run()` | Four constrained-deadline tasks (D<T) with higher combined utilization. | All four tasks are admitted via DBF; no deadline misses over the hyperperiod. | Implemented. GPIO pass criterion documented in source comments. |
| Constrained-deadline admission control | `edf_tests/test_6.c`, `edf_6_run()` | Baseline of two constrained-deadline tasks. Bad candidate (C=3200 ms, D=3600 ms): DBF exceeds available time at t=3600 ms. Good candidate (same C, D=6400 ms): DBF passes. | Bad task returns `pdFAIL`; good task returns `pdPASS` and runs. | Implemented. DBF sanity check documented in source comments. |
| Implicit-deadline admission control — utilization path | `edf_tests/test_7.c`, `edf_7_run()` | Baseline of two implicit-deadline tasks (U≈0.472). Bad candidate would push U>1.0; good candidate keeps U<1.0. Kernel takes the utilization path because D=T for all tasks. | Bad task returns `pdFAIL`; good task returns `pdPASS` and runs. | Implemented. Confirms the kernel selects the simpler utilization test when all deadlines are implicit. |
| Deadline-miss and overrun management | `edf_tests/test_8.c`, `edf_8_run()` | Five tasks that always meet their deadlines plus two tasks (T6, T7) that intentionally overrun their budget periodically (every 4 and every 3 jobs respectively). | Five clean tasks show no deadline-miss GPIO activity. T6 and T7 fire the deadline-miss GPIO at the expected job intervals; the kernel delays each late job to the next release and resumes normal scheduling. | Implemented. Periodic miss cadence verified against the configured miss intervals. |
| Stress test — 100 periodic tasks | `edf_tests/test_9.c`, `edf_9_run()` | 100 implicit-deadline tasks (T=11000 ms, C=95 ms each) with varying relative deadlines from 100 ms to 10000 ms (i×100 ms for task i). | All 100 tasks are admitted via the utilization test (U_total≈0.864). No unexpected deadline misses. Tasks with shorter deadlines are consistently observed earlier in each period on the GPIO trace. | Implemented. Validates that admission control and the EDF ready-list scale to a large task set without unexpected rejections or misses. |

## Pass/fail interpretation

A scheduling test is considered passing when the observed GPIO waveform matches the expected EDF ordering described in the test source. An admission-control test is considered passing when the expected `pdPASS` / `pdFAIL` values are stored in the `volatile` result variables and rejected tasks never appear on the GPIO task-code bank.
