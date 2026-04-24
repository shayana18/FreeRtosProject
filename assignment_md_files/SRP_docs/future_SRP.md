# SRP future work

- Extend EDF + SRP admission control so declared SRP critical-section lengths contribute blocking terms during task acceptance.
- Add more SRP tests beyond `srp_tests/test_1.c`, including longer runs, more resource-sharing patterns, deletion/cleanup cases, and explicit shared-stack regression tests.
- Complete the assignment's quantitative stack-sharing study by running larger SRP task sets and comparing `vTaskGetSRPSharedStackUsage()` with shared stacks enabled and disabled.
- Relax or redesign the shared-stack allocator if runtime task creation or region growth after scheduler start becomes necessary.
- Consider replacing the current recompute-on-change ceiling logic with a more explicit ceiling-history structure only if larger task sets make ceiling recomputation too expensive.
- Extend SRP support beyond binary semaphores only if the project is expanded past the Task 2 requirements.
- Add a stronger production-style recovery policy for deadline misses inside SRP critical sections. The current kernel cleanup releases SRP bookkeeping state so the scheduler does not deadlock, but it does not guarantee that the interrupted application-level critical section completed safely.
