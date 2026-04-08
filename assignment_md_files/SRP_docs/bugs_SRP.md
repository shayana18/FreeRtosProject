# SRP blocker status

- The earlier shared-stack SRP hard fault is no longer the current blocker. Commit `2cca8a809f547c3e17c688583139badcc72d3c67` fixed the initialization bug behind the failure described in the first version of this document.
- The root cause was that shared-stack metadata for the first SRP task could be assigned during task creation and then partially reset later when `prvInitialiseTaskLists()` ran.
- In practice, `uxSRPSharedStackRegionCount` and `uxSRPSharedStackUsedDepthWords` were being cleared during list initialization even though the first task had already consumed and recorded shared-stack state.
- That meant later SRP shared-stack bookkeeping could proceed with inconsistent region metadata, which matched the observed symptom where `srp_tests/test_1.c` faulted once execution reached the later handoff to task 3.
- The fix moved that reset logic out of `prvInitialiseTaskLists()` and into `vTaskResetState()`, which is the correct path for resetting kernel-global scheduler state.
- After that change, normal startup no longer wipes previously assigned SRP shared-stack metadata, so this specific shared-stack initialization bug should be considered resolved.

# SRP implementation limitations

- SRP in this repo is only supported on top of EDF.
- Only binary semaphores are integrated into the SRP resource path. Other FreeRTOS blocking primitives are not SRP-aware.
- Resource claims are static and user-supplied, so the user must provide correct resource IDs and critical-section information at task creation time.
- Blocking-aware admission control for EDF + SRP is not yet integrated, so task acceptance still does not include the full SRP blocking term expected by the assignment.
- In shared-stack mode, SRP tasks must be created in non-decreasing preemption-level order. The allocator appends regions and does not reorder them later, so creating a task with a lower preemption level after a higher-level region already exists will fail.
- The current shared-stack allocator assumes the task set is created before the scheduler starts and uses a fixed layout, so it cannot freely reorder or resize middle regions once execution has begun.

# Bugs fixed during development

- Shared-stack SRP originally had a first-task initialization bug where startup list initialization reset shared-stack region counters after task creation had already assigned them. In practice, the first task could successfully reserve a shared-stack region, then `prvInitialiseTaskLists()` would clear `uxSRPSharedStackRegionCount` and `uxSRPSharedStackUsedDepthWords`, making the allocator think the shared stack was empty again. The next task could then be assigned overlapping shared-stack storage and overwrite the first task's saved frame. Commit `2cca8a809f547c3e17c688583139badcc72d3c67` fixed this by moving the reset into `vTaskResetState()`, so normal startup no longer erases already assigned SRP shared-stack metadata.
- Idle-task periodic accounting originally treated zero-period internal tasks like real periodic SRP tasks, which caused an infinite loop in next-release computation when the idle task was running. The fix was to keep zero-period tasks out of the periodic SRP registry and guard the periodic accounting path.
- Deadline miss detection originally used a raw numeric comparison against absolute deadlines, which broke after tick wrap and produced false misses on long runs. The fix was to switch to a wrap-safe tick comparison.
