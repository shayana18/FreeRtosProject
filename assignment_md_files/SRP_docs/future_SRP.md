# SRP future work

- Finish debugging the shared-stack hard fault, ideally by placing a watchpoint on the saved task-3 frame word that later becomes corrupted.
- Extend EDF + SRP admission control so declared SRP critical-section lengths contribute blocking terms during task acceptance.
- Add more SRP tests beyond `srp_tests/test_1.c`, including longer runs, more resource-sharing patterns, deletion/cleanup cases, and explicit shared-stack regression tests.
- Complete the assignment's quantitative stack-sharing study by running larger SRP task sets and comparing `vTaskGetSRPSharedStackUsage()` with shared stacks enabled and disabled.
- Relax or redesign the shared-stack allocator if runtime task creation or region growth after scheduler start becomes necessary.
- Consider replacing the current recompute-on-change ceiling logic with a more explicit ceiling-history structure if the SRP implementation is extended further.
