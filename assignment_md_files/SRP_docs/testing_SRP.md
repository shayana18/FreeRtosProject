# SRP hardware testing setup

- SRP uses the same trace-pin setup as the EDF tests.
- On top of the debugger UART, task execution is exposed on GPIO trace pins so the currently running task can be read from a Saleae logic analyzer.
- A task-switch signal pin is used to tell the analyzer when to latch the task ID pins.
- A separate deadline-miss pin can also be wired to an LED so deadline misses are visible even when the analyzer is not connected.
- For SRP debugging, the same hardware setup was also paired with debugger snapshots captured by the hard-fault handler and the first-deadline-miss hook in `main.c`.

# SRP software testing setup

- SRP tests live in the `srp_tests` folder in the project root. Each test exposes a function that can be called from `main.c`.
- To run an SRP test, keep `configUSE_EDF == 1` and `configUSE_SRP == 1` in `schedulingConfig.h`, then uncomment the matching call in `main.c` before building and flashing.
- `configUSE_SRP_SHARED_STACKS` in `schedulingConfig.h` is the main switch used to compare SRP scheduling with ordinary per-task stacks versus the shared-stack backend.
- SRP tests reuse the common tracing and timing helpers in `test_utils.c/h` and `task_trace.c/h`.

# SRP test method

- First verify the SRP scheduling and resource protocol with `configUSE_SRP_SHARED_STACKS = 0`; this keeps the storage model simple and checks that EDF + SRP ordering and semaphore access behave correctly on hardware.
- For tests involving resource sharing, compare the observed trace against the expected synchronous-release schedule documented in the test source.
- Then rerun tests with `configUSE_SRP_SHARED_STACKS = 1` so the only major variable that changes is the shared-stack backend.
- Shared-stack guard checks and debugger snapshots are used when needed to separate scheduler-ordering bugs from stack save/restore bugs.

# SRP test cases

| Test | File / entry point | Scenario | Expected result | Status |
| --- | --- | --- | --- | --- |
| Basic SRP scheduling with binary semaphores | `srp_tests/test_1.c`, `srp_1_run()` | Three synchronous periodic tasks (T=4000/6000/12000 ms) sharing two binary semaphores. T1 claims R1, T2 claims R1 and R2, T3 claims R2. All tasks released simultaneously. | SRP ceiling prevents lower-preemption-level tasks from preempting while a higher-ceiling resource is held. No priority inversion; all tasks meet their deadlines over the 12 s hyperperiod. | Implemented. GPIO waveform verified against manually derived schedule. Tested with both `configUSE_SRP_SHARED_STACKS = 0` and `= 1`. |
| SRP ceiling ordering under same-deadline and shorter-deadline tasks | `srp_tests/test_2.c`, `srp_2_run()` | Five tasks with the same deadline (T=D=10000 ms) share R1 and R2. A sixth task (T6) has a shorter deadline (D=4000 ms) and is phased to become ready while T5 holds R1. | T6 is blocked by the system ceiling while T5 holds R1, even though T6 has an earlier absolute deadline. T6 runs only after T5 releases R1. Same-deadline tasks demonstrate correct shared-stack reuse. | Implemented. T6 release timing verified against the manual ceiling-block calculation. |
| Admission control with blocking terms | `srp_tests/test_3.c`, `srp_3_run()` | A long-deadline base task claims R1 for 4500 ms. A short-deadline bad candidate (D=5000 ms) passes a plain utilization check but fails once the R1 blocking term is included. A good candidate (D=8000 ms) passes. | Bad task returns `pdFAIL`; good task returns `pdPASS` and appears on GPIO. | Implemented. `volatile` result variables confirm accept/reject outcomes. Validates that the SRP admission test accounts for blocking time and not only raw utilization. |
| Shared-stack quantitative study | `srp_tests/test_4.c`, `srp_4_run()` | 12 tasks across three preemption levels (four tasks per level with varying stack depths). A reporter task periodically calls `vTaskGetSRPStackUsageRuntimeStats()` and asserts that the measured theoretical allocation matches the expected shared total. | Shared allocation = largest stack per level × three levels + reporter stack. Non-shared allocation = sum of all 12 task stacks + reporter stack. Reporter asserts equality and prints saved bytes to UART. | Implemented. `configASSERT` in the reporter verifies the theoretical shared bytes at every reporter period. Demonstrates the stack savings from the shared-stack backend. |
| WCET overrun inside a critical section | `srp_tests/test_5.c`, `srp_5_run()` | An overrun task holds R1 and runs for 1800 ms, which is well past its WCET of 1000 ms. An observer task also claims R1. | Kernel stops the overrun job at WCET exhaustion and force-releases R1. `ulSRP5ForcedReleaseObserved` flag is set when the observer successfully acquires R1 in the same period. Deadline-miss GPIO fires for the overrun task. | Implemented. `volatile` observer-acquire counter confirms R1 is accessible after the forced release. Validates the overrun management path for tasks holding resources at the time of the overrun. |

## Pass/fail interpretation

An SRP scheduling test is considered passing when the observed GPIO waveform matches the expected EDF + SRP ordering described in the test source and no unexpected deadline-miss GPIO activity occurs. An admission-control test is considered passing when the expected `pdPASS` / `pdFAIL` values are recorded in the `volatile` result variables and rejected tasks never appear on the GPIO task-code bank. For the stack-sharing test, passing requires the reporter's `configASSERT` to hold for every reporter period during a full run.
