<<<<<<< HEAD
# CBS bugs and fixed issues
=======
# CBS Bugs Fixed During Development 
>>>>>>> b44896a (added edf testing template and some bug info for cbs)

## Fixed issue: CBS/kernel layout mismatch

During CBS testing, server fields looked corrupted at runtime. For example, the same `CBS_Server_t *` appeared to have different field offsets when inspected from `cbs.c` and from `tasks.c`. That pointed to a cross-translation-unit layout mismatch rather than a normal scheduling bug.

The root cause was include order. `tasks.c` included `portmacro.h` too early, which sent it through a different FreeRTOS type path than `cbs.c`. As a result, primitive types such as `TickType_t` could be interpreted differently, which changed the layout of `CBS_Server_t`.

The fix was to remove the early `portmacro.h` include and restore the normal `FreeRTOS.h`-first include order. After that, `cbs.c` and `tasks.c` agreed on the CBS server layout, and the temporary layout-debug checks were removed.

## Current bugs

No current CBS bugs are documented within the Task 3 scope.
