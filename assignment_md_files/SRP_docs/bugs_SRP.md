# SRP bugs and fixed issues

## Current bugs

- Blocking-aware admission control is limited. Task acceptance checks EDF timing, but it does not include the full SRP blocking term required for a complete EDF + SRP schedulability test.

## Bugs fixed during development

- Shared-stack initialization originally reset region counters after the first SRP task had already reserved shared-stack storage. That could make later tasks reuse overlapping stack space. The fix moved the reset to `vTaskResetState()`, so normal scheduler startup no longer erases shared-stack metadata created during task setup.
- Idle-task periodic accounting originally treated zero-period internal tasks like real periodic SRP tasks, which could cause an infinite next-release loop. The fix was to keep zero-period tasks out of the periodic SRP registry and guard the periodic accounting path.
- Deadline-miss detection originally used a raw numeric comparison against absolute deadlines. That could report false misses after tick wrap. The fix was to use a wrap-safe tick comparison.
