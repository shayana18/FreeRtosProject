# CBS kernel changes

- Added CBS configuration support through `configUSE_CBS`, `configCBS_MAX_SERVERS`, `configCBS_MAX_PENDING_JOBS`, and `configCBS_ALLOW_BUDGET_CARRYOVER` in `schedulingConfig.h`.
- Added a separate CBS module (`cbs.c`/`cbs.h`) that owns server allocation, server validation, budget/deadline state, admission checks, job submission, worker waiting, and job completion.
- Added `CBS_Server_t` with capacity, period, absolute deadline, remaining budget, worker binding, job counters, and an integrity tag used to catch server layout/corruption issues during testing.
- Added CBS task-layer hooks in `task.h`/`tasks.c`: `xTaskCBSBindToServer`, `xTaskCBSUnbindFromServer`, `xTaskCBSIsManaged`, `xTaskCBSHasOutstandingJob`, `xTaskCBSSetOutstandingJob`, `pxTaskCBSGetServer`, and `xTaskCBSUpdateDeadline`.
- Extended `TCB_t` for CBS-enabled EDF builds with `pxCBSServer`, `xCBSJobOutstanding`, and `uxCBSJobID`.
- Added `xTaskCreateCBSWorker()` and `xTaskCreateCBS()` helpers so a worker task can be created with EDF timing derived from its server and then bound to that server.
- Added one-job-at-a-time enforcement for CBS workers. A submission fails if the worker already has an outstanding job or is bound to a different server.
- Implemented the CBS arrival rule in `xCBSSubmitJob()`: if remaining budget is too large for the time left before the current deadline, the server budget is refreshed and the deadline becomes `arrival + period`.
- Added scheduler-tick budget charging for CBS-managed tasks. When budget reaches zero, the server budget is refreshed, the server deadline is pushed by one period, and the worker task's EDF ready-list position is refreshed.
- Adjusted uniprocessor EDF ready selection so equal-deadline ready selection can prefer CBS-managed tasks while still using the normal EDF ready list.
- Kept periodic WCET/deadline-miss handling separate from CBS budget exhaustion. CBS budget exhaustion postpones the server deadline; ordinary periodic deadline misses still use the deadline-miss hook and GPIO trace.
- Added CBS tests under `cbs_tests/` and included them through the existing CMake globbed test source list.
