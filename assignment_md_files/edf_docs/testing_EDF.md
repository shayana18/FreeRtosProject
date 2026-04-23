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
