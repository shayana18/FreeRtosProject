# Scheduling configuration guide

This project selects the active scheduler at compile time through `schedulingConfig.h`. The most important rule is that only one scheduling family should be active in a given build. Unused scheduler features should be set to `0U` so their guarded code paths compile out.

## Stock FreeRTOS fixed-priority scheduling

Use this when none of the EDF-based extensions should be compiled in.

```c
#define configUSE_EDF 0U
#define configUSE_UP  0U
#define configUSE_MP  0U
#define configUSE_SRP 0U
#define configUSE_CBS 0U
#define GLOBAL_EDF_ENABLE 0U
#define PARTITIONED_EDF_ENABLE 0U
#define configUSE_SRP_SHARED_STACKS 0U
```

With all scheduler-extension flags set to `0U`, the EDF/SRP/CBS/MP extension code compiles out and the kernel follows the base FreeRTOS fixed-priority scheduling path. In this mode, `configUSE_UP` and `configUSE_MP` are not used to select a custom scheduler; they should remain `0U` to make it clear that no EDF-based scheduling extension is active.

## Uniprocessor EDF

Use this for plain single-core EDF without SRP or CBS.

```c
#define configUSE_EDF 1U
#define configUSE_UP  1U
#define configUSE_MP  0U
#define configUSE_SRP 0U
#define configUSE_CBS 0U
#define GLOBAL_EDF_ENABLE 0U
#define PARTITIONED_EDF_ENABLE 0U
#define configUSE_SRP_SHARED_STACKS 0U
```

## Uniprocessor EDF + SRP

Use this for single-core EDF with the Stack Resource Policy enabled.

```c
#define configUSE_EDF 1U
#define configUSE_UP  1U
#define configUSE_MP  0U
#define configUSE_SRP 1U
#define configUSE_CBS 0U
#define GLOBAL_EDF_ENABLE 0U
#define PARTITIONED_EDF_ENABLE 0U
#define configUSE_SRP_SHARED_STACKS 0U
```

## Uniprocessor EDF + SRP + shared stacks

Use this for single-core EDF with SRP and the shared-stack backend.

```c
#define configUSE_EDF 1U
#define configUSE_UP  1U
#define configUSE_MP  0U
#define configUSE_SRP 1U
#define configUSE_CBS 0U
#define GLOBAL_EDF_ENABLE 0U
#define PARTITIONED_EDF_ENABLE 0U
#define configUSE_SRP_SHARED_STACKS 1U
```

## Uniprocessor EDF + CBS

Use this for single-core EDF with Constant Bandwidth Server support.

```c
#define configUSE_EDF 1U
#define configUSE_UP  1U
#define configUSE_MP  0U
#define configUSE_SRP 0U
#define configUSE_CBS 1U
#define GLOBAL_EDF_ENABLE 0U
#define PARTITIONED_EDF_ENABLE 0U
#define configUSE_SRP_SHARED_STACKS 0U
```

## Multiprocessor global EDF

Use this for multicore global EDF. Global EDF is the default MP EDF policy when partitioned EDF is not enabled.

```c
#define configUSE_EDF 1U
#define configUSE_UP  0U
#define configUSE_MP  1U
#define configUSE_SRP 0U
#define configUSE_CBS 0U
#define GLOBAL_EDF_ENABLE 1U
#define PARTITIONED_EDF_ENABLE 0U
#define configUSE_SRP_SHARED_STACKS 0U
```

Because global EDF is the default MP EDF mode, this is also valid:

```c
#define configUSE_EDF 1U
#define configUSE_UP  0U
#define configUSE_MP  1U
#define configUSE_SRP 0U
#define configUSE_CBS 0U
#define GLOBAL_EDF_ENABLE 0U
#define PARTITIONED_EDF_ENABLE 0U
#define configUSE_SRP_SHARED_STACKS 0U
```

## Multiprocessor partitioned EDF

Use this for multicore partitioned EDF with per-core EDF ready queues and best-fit task placement.

```c
#define configUSE_EDF 1U
#define configUSE_UP  0U
#define configUSE_MP  1U
#define configUSE_SRP 0U
#define configUSE_CBS 0U
#define GLOBAL_EDF_ENABLE 0U
#define PARTITIONED_EDF_ENABLE 1U
#define configUSE_SRP_SHARED_STACKS 0U
```

## Invalid combinations rejected at compile time

The configuration header intentionally rejects ambiguous scheduler combinations:

- `configUSE_UP == 1U` and `configUSE_MP == 1U` cannot both be enabled.
- `configUSE_EDF == 1U` requires exactly one of `configUSE_UP` or `configUSE_MP` to select the EDF execution model.
- `GLOBAL_EDF_ENABLE == 1U` and `PARTITIONED_EDF_ENABLE == 1U` cannot both be enabled.
- `GLOBAL_EDF_ENABLE` or `PARTITIONED_EDF_ENABLE` require `configUSE_MP == 1U` and `configUSE_EDF == 1U`.
- `configUSE_SRP == 1U` requires `configUSE_UP == 1U` and `configUSE_EDF == 1U`.
- `configUSE_CBS == 1U` requires `configUSE_UP == 1U` and `configUSE_EDF == 1U`.
- `configUSE_SRP == 1U` and `configUSE_CBS == 1U` cannot both be enabled.

These checks keep each compiled kernel image limited to one coherent scheduler mode.
