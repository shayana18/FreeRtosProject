# MP future improvements

- Add less conservative or exact admission analysis for global EDF and partitioned EDF. The current tests are intentionally sufficient rather than exact, which keeps admission safe but rejects some feasible task sets.
- Add automated trace checking for MP GPIO captures so the expected core/task schedule can be compared against recorded logic-analyzer data instead of relying only on manual inspection.
- Add longer-running stress tests for global EDF that repeatedly exercise simultaneous releases, migration, job migration after preemption, and cross-core preemption targeting.
- Add longer-running partitioned EDF tests that repeatedly change affinity at runtime to validate partition migration and registry movement over many jobs.
- Add instrumentation to measure MP scheduler overhead, especially shared ready-list scanning in global EDF and migration cost in partitioned EDF.
- Add optional debug reporting for admission decisions, including total utilization, maximum task utilization, selected partition, and rejection reason.
- Extend MP EDF beyond independent tasks by deciding how resource sharing should interact with global and partitioned EDF. The current MP mode intentionally does not integrate SRP or inter-task communication.
- Investigate more partition-placement heuristics, such as worst-fit, next-fit, and offline first-fit decreasing, and compare them against the current online best-fit implementation.
- Consider dynamic core hotplug only if the project is extended beyond the assignment. It would require port-level support and explicit scheduler semantics for stopping, restarting, and rebalancing cores.
