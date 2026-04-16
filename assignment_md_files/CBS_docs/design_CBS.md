# CBS Design

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
Separating CBS into `cbs.c`/`cbs.h` made integration easier and safer:
- Isolated policy code: server rules (reset/replenish/admission) are in one module, not spread through kernel internals.
- Narrow kernel touch points: `tasks.c` only exposes small CBS hooks (`bind`, `outstanding job`, `deadline update`).
- Lower regression risk: CBS logic can evolve without repeatedly editing core scheduler paths.
- Cleaner API for tests/apps: CBS calls are discoverable in one header.
- Easier debugging: CBS state (`budget`, `deadline`, `running`) is centralized.

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

## Notes and current limits
- Admission control is intentionally simple (utilization test), not a full demand-bound CBS feasibility analysis.
- Current model is single outstanding job per worker task by design.
- CBS is compiled only with EDF enabled.