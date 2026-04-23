# CBS design

## Scope and goals

CBS support is implemented on top of EDF to serve soft aperiodic work without breaking periodic hard real-time behavior.

Covered requirements from the assignment:
- Aperiodic soft real-time requests run alongside periodic EDF tasks.
- Reservation-based server model (`Q/T`) is used.
- Multiple CBS workers/servers can run concurrently.
- Applications can mix regular periodic EDF tasks and CBS-managed tasks.
- Aperiodic procedures can be submitted during runtime through job submission APIs.
- Scheduler remains EDF-based.
- For equal absolute deadlines in ready selection, CBS tasks are favored.
- One-job-at-a-time per CBS worker is enforced (no next release before completion).

## Why `cbs.c`/`cbs.h` is separated

CBS is kept in `cbs.c`/`cbs.h` because it behaves more like a reservation subsystem than a normal periodic task extension. EDF and SRP mainly change which ready task is selected, but CBS also has server state, submitted jobs, budget exhaustion, and deadline postponement rules. Keeping that logic in one module made the implementation easier to reason about.

The main benefits of this split are:

- Server rules such as reset, replenishment, admission, and budget exhaustion stay in one place.
- `tasks.c` only needs small hooks for binding a task to a server, checking outstanding work, and refreshing a server task's deadline.
- CBS can be tested and debugged without repeatedly changing the core scheduler paths.
- Application and test code gets one clear CBS header instead of several scattered kernel entry points.

## Public CBS API (`cbs.h`)
- `xCBSServerCreate(Q_ticks, T_ticks, name)`: validates params, allocates server, initializes budget/deadline.
- `vCBSServerDelete(server)`: unregisters and frees server.
- `xCBSSubmitJob(server, task)`: submits one aperiodic job to worker task, applies CBS reset/replenish rules on arrival, notifies worker.
- `xCBSWaitForJob(timeout)`: worker blocks waiting for a submitted job.
- `xCBSCompleteJob()`: marks current job finished and clears outstanding state.
- `xCBSConsumeBudget(server, ticks)`: budget accounting helper.
- `vCBSReplenishBudget(server)`: replenishes budget and postpones deadline by one period.
- `xCBSIsBudgetExhausted(server)`: budget exhaustion check.
- `xCBSAdmissionTest(Q_ticks, T_ticks)`: current utilization-based acceptance (`sum <= 1.0` in fixed-point).
- `uxCBSServerUtilization(server)`: utilization in fixed-point.
- `vCBSInit()`, `vCBSDeinit()`, `xCBSIsTaskManaged(task)`: subsystem lifecycle/query.
- `xTaskCreateCBSWorker(...)`, `xTaskCreateCBS(...)` (when SRP is off): create and bind CBS workers.

## Kernel integration points
Implemented through small task-layer APIs in `task.h`/`tasks.c`:
- `xTaskCBSBindToServer`, `xTaskCBSUnbindFromServer`, `xTaskCBSIsManaged`
- `xTaskCBSHasOutstandingJob`, `xTaskCBSSetOutstandingJob`, `pxTaskCBSGetServer`
- `xTaskCBSUpdateDeadline` (updates task deadline and refreshes ready-list position)

TCB extensions for CBS-enabled EDF builds:
- `pxCBSServer`
- `xCBSJobOutstanding`
- `uxCBSJobID`

## Runtime behavior summary

1. Create server with capacity/period and create a bound worker task.
2. Application code can add aperiodic work at runtime by calling `xCBSSubmitJob` on an existing CBS worker.
3. `xCBSSubmitJob` accepts one outstanding job per worker, updates server deadline/budget if CBS arrival rule triggers, and wakes worker.
4. Tick path decrements CBS budget while worker executes.
5. On budget exhaustion, deadline is postponed by one period and ready-list position is refreshed.
6. Worker calls `xCBSCompleteJob` when done, enabling next submission.

## Design assumptions

- Admission control uses a simple utilization test. This was chosen because the utilization test is easy to validate and fits the project scope.
- Each worker has at most one outstanding job. This matches the assignment simplification that a new job is not released until the previous job of the same task is complete.
- CBS is compiled only with EDF enabled because CBS relies on dynamic server deadlines to participate in EDF scheduling.
