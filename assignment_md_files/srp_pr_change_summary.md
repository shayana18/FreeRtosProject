# SRP Changes Made (Concrete Summary)

I changed the SRP ready-task selection logic in:

- `FreeRTOS-Kernel/tasks.c`

## Main Behavior Change

In `prvSRPSelectReadyTask()` (`FreeRTOS-Kernel/tasks.c`), I added an explicit **resume exception** for tasks that already hold resources.

Before:

- A ready task was selected only if:
	- `task_priority_ceiling > uxSystemCeiling`

After:

- A ready task is selected if either:
	- `task_priority_ceiling > uxSystemCeiling`, or
	- the task already holds at least one resource **and** for every claimed resource:
		- `claim <= available + held_by_this_task`

This second condition is the runtime-safe version requested in review.
It lets a preempted holder continue, but only when its declared claim still fits current dynamic availability plus what it already owns.

## Functions, Files, and What Each One Does

### `prvSRPSelectReadyTask()`

- File: `FreeRTOS-Kernel/tasks.c`
- Type: `static` internal scheduler helper
- What it does:
	- scans the SRP ready list (`xReadySRPTasksList`) in deadline order,
	- chooses the first task that is SRP-eligible to run,
	- uses `uxSystemCeiling` as the normal gate,
	- now includes the holder-resume + feasibility check (`claim <= available + held`).

### `prvSRPRecomputeSystemCeiling()`

- File: `FreeRTOS-Kernel/tasks.c`
- Type: `static` internal SRP helper
- What it does:
	- recomputes per-resource runtime blocking ceilings,
	- recomputes global `uxSystemCeiling` as the max of those per-resource ceilings,
	- called after SRP resource state changes and when SRP task registry changes.

### `xTaskSRPAcquireResource()`

- File: `FreeRTOS-Kernel/tasks.c`
- Type: public SRP API
- What it does:
	- validates request (`resource type`, `count`, claim bounds, capacity bounds),
	- increments:
		- global active count for that resource,
		- current task's held count for that resource,
	- calls `prvSRPRecomputeSystemCeiling()` after successful acquire.

### `xTaskSRPReleaseResource()`

- File: `FreeRTOS-Kernel/tasks.c`
- Type: public SRP API
- What it does:
	- validates release (`count <= held`, `count <= active`),
	- decrements:
		- global active count for that resource,
		- current task's held count for that resource,
	- calls `prvSRPRecomputeSystemCeiling()` after successful release.

### `xQueueSemaphoreTakeSRP()`

- File: `FreeRTOS-Kernel/queue.c`
- Type: SRP queue/semaphore wrapper
- What it does:
	- calls `xTaskSRPAcquireResource()` before semaphore take,
	- if semaphore take fails, rolls back via `xTaskSRPReleaseResource()`,
	- therefore system ceiling is recomputed on both success path and rollback path.

### `xQueueSemaphoreGiveSRP()`

- File: `FreeRTOS-Kernel/queue.c`
- Type: SRP queue/semaphore wrapper
- What it does:
	- gives semaphore,
	- calls `xTaskSRPReleaseResource()` to release SRP accounting,
	- system ceiling recomputation happens through that release call.

## Static Arrays (Exactly What They Hold)

All SRP static arrays below are in `FreeRTOS-Kernel/tasks.c` (inside `configUSE_SRP == 1` and resource-count guards).

### `uxSRPResourceCapacityTable[]` (const)

- Source: `configSRP_RESOURCE_CAPACITY_TABLE`
- Meaning per index `r`:
	- total system capacity (max units) of resource type `r`.
- Example:
	- if index `2` is value `4`, resource type 2 has 4 total units in system.

### `uxSRPResourceBlockingCeilingTable[]`

- Runtime-computed per resource type.
- Meaning per index `r`:
	- current blocking ceiling contributed by resource type `r`,
	- highest preemption level among tasks that could be blocked by current availability of `r`.
- Updated by `prvSRPRecomputeSystemCeiling()`.

### `uxSRPResourceActiveCount[]`

- Runtime global usage per resource type.
- Meaning per index `r`:
	- how many units of resource type `r` are currently acquired (active) system-wide.
- Updated by acquire/release APIs.

## Per-Task Arrays (Not Static, But Important)

These are fields in each task's TCB (`FreeRTOS-Kernel/tasks.c`, struct `tskTCB`).

### `uxSRPResourceClaimMax[]`

- Meaning per task, per resource type `r`:
	- declared maximum units that task may need of resource `r` during a job.

### `uxSRPResourceHeldCount[]`

- Meaning per task, per resource type `r`:
	- currently held units by that task for resource `r` at runtime.

## Eligibility Formula Used in the New Selection Path

For each claimed resource `r` of a candidate task:

```text
available[r] = capacity[r] - active[r]
feasible if claimMax[r] <= available[r] + heldByCandidate[r]
```

A candidate can pass via holder-exception only if:

- it holds at least one resource now, and
- the feasibility check above passes for all claimed resources.

## Practical Effect of the Change

- Prevents starvation of a preempted task that already owns resources.
- Still enforces a runtime safety condition, so this is not a blanket bypass of system ceiling.
- Keeps SRP admission tied to actual dynamic availability, not only static declarations.
