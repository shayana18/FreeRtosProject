# EDF design

This EDF implementation changes the scheduling decision from "run the highest-priority ready task" to "run the ready task with the earliest absolute deadline." The main kernel structure added for this is a ready list sorted by absolute deadline. A second EDF registry list stores all admitted EDF tasks so the kernel can run admission checks and update periodic release metadata.

The task control block was extended with the timing fields EDF needs at runtime:

- period
- WCET
- relative deadline
- absolute release time
- absolute deadline
- execution time used by the current job
- EDF registry list item

In EDF mode, `xTaskCreate()` is overloaded so the existing task creation entry point accepts timing parameters instead of a fixed priority. This was chosen to keep the user-facing API familiar: users still create tasks through `xTaskCreate()`, but the function signature changes based on the selected scheduler configuration. Task creation is also the right place to validate the task model, convert timing values to ticks, run admission control, and insert the task into the EDF data structures.

Periodic job management reuses the existing FreeRTOS timing paths instead of adding a separate scheduler loop. `xTaskDelayUntil()` was extended so that when an EDF task finishes a job and waits for its next period, the kernel also updates the next release time, refreshes the absolute deadline, and resets the execution counter. `xTaskIncrementTick()` was extended to charge execution time to the running EDF task. If a job is still running at its WCET boundary, the kernel reports a WCET overrun through a hook; if the job runs past its deadline, the kernel then stops that job and delays the task until its next release.

Admission control depends on the task set:

- implicit-deadline task sets use the standard EDF utilization test
- constrained-deadline task sets use processor-demand analysis

Runtime task creation uses the same admission checks as startup task creation. When a task is created after the scheduler has started, the scheduler is temporarily suspended so the admission test sees a stable snapshot of the existing EDF task registry. If the candidate is accepted, it is inserted into the registry and ready list with its release/deadline metadata initialized; if not, creation fails before the task can appear in the runtime trace.

WCET overruns and deadline misses are intentionally separated. A WCET overrun reports diagnostic output but does not by itself skip the job. A deadline miss is the stronger event: the miss hook and GPIO indicator fire, the current job is advanced to its next release, and the task is delayed so other ready work can continue.

This keeps the Task 1 implementation focused on the required EDF behavior: deadline-ordered dispatch, admission control, runtime task creation, and deadline-miss/overrun handling.
