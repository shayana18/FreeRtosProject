# SRP Design (Current Implementation)

This document describes the current SRP implementation in this repository as of March 31, 2026, including EDF integration, resource protocol behavior, and SRP stack sharing.

## 1. Configuration Model

SRP is only available on top of EDF.

- `configUSE_EDF == 0`: stock FreeRTOS scheduling.
- `configUSE_EDF == 1 && configUSE_SRP == 0`: EDF only.
- `configUSE_EDF == 1 && configUSE_SRP == 1`: EDF + SRP.

Relevant SRP-related configuration:

- `configSRP_RESOURCE_TYPE_COUNT`: number of resource IDs in the SRP claim universe.
- `configUSE_SRP_RESOURCE_RELEASE_HOOK`: optional application release callback.
- `configSRP_SHARED_STACK_SIZE`: size of SRP shared stack pool in `StackType_t` words.
- `configSRP_SHARED_STACK_MAX_LEVELS`: maximum number of unique SRP preemption levels tracked for shared-stack regions.

## 2. SRP Task Metadata

When EDF+SRP is enabled, each task TCB stores:

- `uxPriorityCeiling`: task preemption level (derived from relative deadline).
- `uxStackDepthWords`: requested task stack depth in words (used for shared-stack sizing and reporting).
- `xSRPTaskListItem`: registry/list node for SRP task tracking.
- `uxSRPResourceClaimMax[]`: binary claim map per resource type.
- `uxSRPResourceHeldCount[]`: binary held map per resource type.

Global SRP state includes:

- `xReadySRPTasksList`: SRP ready list sorted by absolute deadline.
- `xSRPTaskRegistryList`: registry of SRP tasks.
- `uxSRPResourceBlockingCeilingTable[]`: per-resource computed static ceiling.
- `uxSRPResourceActiveCount[]`: lock-state table (0/1 in binary use).
- `uxSystemCeiling`: current system ceiling from active resources.

## 3. Task Creation API (EDF+SRP)

`xTaskCreate(...)` in EDF+SRP mode accepts:

- `period`, `wcet`, `relative deadline`
- claimed resource ID array + count

Validation currently performed:

- EDF parameter validation (`T > 0`, `C > 0`, `D > 0`, `D <= T`)
- SRP claim validation (ID bounds + duplicate detection)

### 3.1 Preemption Level

Task preemption level is derived once at create-time:

- `preemption_level = portMAX_DELAY - relative_deadline_ticks`

So shorter relative deadlines map to higher preemption level values.

## 4. SRP Shared Runtime Stack

### 4.1 Buffer and region model

A single global shared stack pool is declared:

- `xSRPSharedStackBuffer[configSRP_SHARED_STACK_SIZE]`

It is partitioned into contiguous regions, one per unique SRP preemption level.

Each region tracks:

- preemption level
- region depth (words)
- base pointer in the shared pool

### 4.2 Region sizing rule

At SRP task creation, region depth for the task's preemption level is:

- `max(stackDepth)` across existing tasks in `xSRPTaskRegistryList` with that level
- compared against the new task's requested depth

The larger value is used.

### 4.3 Region allocation constraints

Current implementation intentionally uses fixed layout, no reorganization:

- Regions are appended in preemption-level order.
- Existing regions can only grow if they are the last region.
- If a new task would require reordering or middle-region growth, creation fails.

This matches the project assumption that relevant SRP tasks are created before scheduler start and layout is fixed thereafter.

### 4.4 Stack pointer assignment path

In EDF+SRP mode, dynamic task creation uses:

- dynamic TCB allocation
- shared stack assignment from the SRP region allocator

It does **not** use per-task `pvPortMallocStack` for SRP tasks.

Non-SRP paths are unchanged:

- EDF-only and stock FreeRTOS continue to use existing per-task stack allocation logic.

### 4.5 Deletion and memory ownership

SRP shared stack is kernel-owned shared storage, so per-task delete must not free it.

SRP-created tasks are tagged as:

- `tskSTATICALLY_ALLOCATED_STACK_ONLY`

Result:

- `prvDeleteTCB()` frees only the TCB for those tasks.

## 5. Scheduler/Runtime Behavior with SRP

### 5.1 Ready ordering

Tasks remain EDF-ordered (by absolute deadline).

### 5.2 Eligibility under ceiling

SRP selection scans ready tasks in EDF order and picks first eligible task:

- preemption level above current `uxSystemCeiling`, or
- task already holds a resource (must be allowed to continue)

### 5.3 Ceiling computation

For each resource ID:

- static ceiling = max preemption level among tasks that claim it

System ceiling:

- max static ceiling among currently active resources

### 5.4 Resource acquisition and release

Acquisition path checks:

- binary-only assumptions (`uxCount == 1`, valid resource ID)
- task claimed resource
- task not already holding resource
- resource currently unlocked
- ceiling eligibility

On success:

- mark resource active
- mark task holding
- recompute system ceiling

Release path checks ownership and active state, then clears both and recomputes system ceiling.

### 5.5 Cleanup safety

When tasks are deleted, SRP-held resources are force-released to prevent stale resource state.

## 6. Quantitative Stack Tracking API

A utility API is provided:

- `vTaskGetSRPSharedStackUsage(size_t *sharedBytes, size_t *perTaskBytes)`

It reports:

- `sharedBytes`: bytes reserved by current SRP preemption-level regions
- `perTaskBytes`: bytes that would be used if each SRP task had its own private stack (sum of requested SRP task depths)

## 7. Intentional Guardrails

The implementation currently enforces:

- SRP shared stack create-time path rejects task creation once scheduler is running.
- Shared-stack support requires mixed static/dynamic allocation bookkeeping support (`tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0`).

## 8. Current Known Gaps

The following are still pending relative to full Task 2 expectations:

- Blocking-aware EDF+SRP admission control is not yet integrated into the EDF+SRP `xTaskCreate` branch.
- A formal ceiling-stack data structure (instead of recompute-on-change) is not implemented.
- End-to-end SRP stack-sharing quantitative experiments and dedicated SRP test suite documentation still need to be finalized in testing docs.

## 9. Removed Legacy/Unused Pieces

The project no longer uses a separate static SRP ceiling table configuration knob.

- `configSRP_RESOURCE_CEILING_TABLE` has been removed from scheduling config.

The previously carried SRP tick metadata field that had no behavioral effect is also removed from TCB/static mirror structures.
