# CBS Bugs Log

## Resolved: TickType/layout mismatch between CBS and kernel task code
Status: Fixed

### Symptom
- CBS server fields appeared corrupted at runtime (for example, budget unexpectedly reading as zero).
- The same `CBS_Server_t *` address showed different field offsets when inspected from `cbs.c` versus `tasks.c`.

### Investigation path
1. Added temporary cross-translation-unit sentinels (`sizeof`, `offsetof`, signatures) in both `cbs.c` and `tasks.c`.
2. Confirmed compile-time disagreement on `CBS_Server_t` layout.
3. Narrowed root cause to primitive-type width divergence (`TickType_t` path differed between translation units).
4. Traced divergence to include order in `tasks.c` (early `portmacro.h` include before normal FreeRTOS include flow).

### Root cause
- `tasks.c` was entering a different preprocessor/type path than `cbs.c`, producing inconsistent type widths and therefore different struct packing/offsets.

### Fix applied
- Removed the early `portmacro.h` include in `tasks.c` and restored normal `FreeRTOS.h`-first include ordering.
- Rebuilt and revalidated matching struct layout between translation units.
- Removed all temporary layout-debug instrumentation afterward.

### Current state
- Issue is fixed.
- CBS budget/deadline fields are interpreted consistently by both `cbs.c` and `tasks.c`.