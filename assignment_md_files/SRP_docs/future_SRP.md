# SRP future work

- Further validate and tighten EDF + SRP admission control against more formal blocking-analysis examples, especially mixed implicit/constrained-deadline task sets.
- Add more SRP tests beyond the current five-test set, especially longer runs, more resource-sharing patterns, task deletion/cleanup cases, and additional shared-stack regression cases.
- Extend the quantitative stack-sharing study with larger task sets and more preemption-level distributions, then compare `vTaskGetSRPSharedStackUsage()` against equivalent private-stack totals.
- Relax or redesign the shared-stack allocator if runtime task creation or region growth after scheduler start becomes necessary.
- Consider replacing the current recompute-on-change ceiling logic with a more explicit ceiling-history structure only if larger task sets make ceiling recomputation too expensive.
- Extend SRP support beyond binary semaphores only if the project is expanded past the Task 2 requirements.
- Add a stronger production-style recovery policy for deadline misses inside SRP critical sections. The current kernel cleanup releases SRP bookkeeping state so the scheduler does not deadlock, but it does not guarantee that the interrupted application-level critical section completed safely.
