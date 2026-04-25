# SRP bugs and fixed issues

## Current bugs

No current SRP bugs are documented within the SRP scope.

## Bugs fixed during development

- Shared-stack initialization originally reset region counters after the first SRP task had already reserved shared-stack storage. That could make later tasks reuse overlapping stack space. The fix moved the reset to `vTaskResetState()`, so normal scheduler startup no longer erases shared-stack metadata created during task setup.
- Idle-task periodic accounting originally treated zero-period internal tasks like real periodic SRP tasks, which could cause an infinite next-release loop. The fix was to keep zero-period tasks out of the periodic SRP registry and guard the periodic accounting path.
- Deadline-miss detection originally used a raw numeric comparison against absolute deadlines. That could report false misses after tick wrap. The fix was to use a wrap-safe tick comparison.
- SRP admission control originally behaved too much like plain EDF admission. It now incorporates declared maximum critical-section lengths as blocking terms, which is why the SRP admission test can reject a task that would otherwise pass a simple utilization check.
