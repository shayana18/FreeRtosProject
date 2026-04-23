# SRP hardware testing setup

- SRP uses the same trace-pin setup as the EDF tests.
- On top of the debugger UART, task execution is exposed on GPIO trace pins so the currently running task can be read from a Saleae logic analyzer.
- A task-switch signal pin is used to tell the analyzer when to latch the task ID pins.
- A separate deadline-miss pin can also be wired to an LED so deadline misses are visible even when the analyzer is not connected.
- For SRP debugging, the same hardware setup was also paired with debugger snapshots captured by the hard-fault handler and the first-deadline-miss hook in `main.c`.

# SRP software testing setup

- SRP tests live in the `srp_tests` folder in the project root.
- The current repo only contains one SRP test, `srp_tests/test_1.c`, which creates three synchronous periodic tasks that share two binary semaphores through the SRP wrappers.
- To run the SRP test, keep `configUSE_EDF == 1` and `configUSE_SRP == 1` in `schedulingConfig.h`, then uncomment `srp_1_run()` in `main.c` before building and flashing.
- `configUSE_SRP_SHARED_STACKS` in `schedulingConfig.h` is the main switch used to compare SRP scheduling with ordinary per-task stacks versus the shared-stack backend.
- SRP tests reuse the common tracing and timing helpers in `test_utils.c/h` and `task_trace.c/h`.

# SRP test method

- First verify the SRP scheduling and resource protocol with `configUSE_SRP_SHARED_STACKS = 0`; this keeps the storage model simple and checks that EDF + SRP ordering and semaphore access behave correctly on hardware.
- For `srp_tests/test_1.c`, compare the observed trace against the expected synchronous-release schedule documented in the test source over the 12 s hyperperiod.
- Then rerun the same test with `configUSE_SRP_SHARED_STACKS = 1` so the only major variable that changes is the shared-stack backend.
- Shared-stack guard checks and debugger snapshots are used when needed to separate scheduler-ordering bugs from stack save/restore bugs.
- A larger quantitative stack-sharing experiment can be done by creating a larger SRP task set and comparing `vTaskGetSRPSharedStackUsage()` output with shared stacks enabled and disabled.
