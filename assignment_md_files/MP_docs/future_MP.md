# MP future work

- Add a dedicated MP test suite in the same style as `edf_tests`, with separate global EDF and partitioned EDF cases selectable from `main.c`.
- Update `task_trace.c/h` for MP-specific per-core GPIO trace banks so both cores can be observed on hardware without shared-pin races.
- Expand the partitioned EDF placement study beyond the current online first-fit-decreasing style placement and compare it against alternative heuristics such as best-fit.
- Add longer-running global EDF stress tests that intentionally exercise migration, simultaneous releases, and repeated cross-core preemption.
- Add longer-running partitioned EDF tests that repeatedly change task affinity at runtime to validate partition migration and registry movement over time.
- Extend the MP admission study with additional edge cases near the sufficient bounds so the conservative nature of the implemented tests is clearly characterized.
- If the project is extended beyond independent periodic tasks, decide whether shared-resource protocols should be integrated into MP EDF as well; the current MP design assumes tasks are independent.
- Add a clearer user-facing description of the MP trace and test workflow to the top-level README once the first batch of MP tests has been committed.
- Dynamic core hotplug is intentionally out of scope for the assignment, but would require port-level support and explicit scheduler semantics if the project were extended further.
