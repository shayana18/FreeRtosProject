# SRP Design and Current Status

This document explains the SRP logic currently implemented on top of the EDF scheduler, the purpose of each major SRP component, the current limitations of the implementation, and the additional work still required to fully meet Task 2 of the assignment.

The important framing point is:

- `configUSE_EDF == 0`: use stock FreeRTOS fixed-priority scheduling.
- `configUSE_EDF == 1 && configUSE_SRP == 0`: use EDF only.
- `configUSE_EDF == 1 && configUSE_SRP == 1`: use EDF + SRP.

SRP is therefore treated as an extension of EDF, not as a standalone scheduler.

## 1. Why SRP Exists Here

EDF decides which ready task is most urgent by deadline. That solves CPU scheduling, but not shared-resource access. If two EDF tasks contend for a shared resource, a task may be released, start executing, and then block on a semaphore. That creates extra delay, makes timing analysis harder, and can break schedulability.

The goal of SRP in this project is to:

- restrict resource access to binary semaphores,
- prevent a task from starting unless it is safe to do so under the current system ceiling,
- keep blocking predictable,
- support EDF + SRP admission control later,
- eventually enable run-time stack sharing among tasks with the same preemption level.

FreeRTOS already uses priority inheritance for mutexes. This SRP work does not replace that mechanism globally. Instead, it adds a new EDF+SRP path for binary semaphores.

## 2. High-Level Structure

At a high level, the current design works like this:

```text
                          +----------------------+
                          |  Task creation API   |
                          |   xTaskCreate(...)   |
                          +----------+-----------+
                                     |
                                     v
                          +----------------------+
                          |  EDF task metadata   |
                          |  period / wcet / D   |
                          +----------+-----------+
                                     |
                                     v
                          +----------------------+
                          |  SRP task metadata   |
                          |  claims / held / PL  |
                          +----------+-----------+
                                     |
                                     v
                 +---------------------------------------------+
                 | EDF ordering + SRP eligibility check        |
                 | earliest deadline task that can legally run |
                 +---------------------------------------------+
                                     |
                                     v
                      +-------------------------------+
                      | SRP semaphore wrapper path    |
                      | take/give + ceiling updates   |
                      +-------------------------------+
```

SRP does not replace EDF ordering. EDF still orders ready tasks by absolute deadline. SRP only decides whether a task is eligible to start or continue execution under the current ceiling.

## 3. Build-Time and API Model

### 3.1 Compile-time configuration

The main configuration hooks are:

- `configUSE_EDF`
- `configUSE_SRP`
- `configSRP_RESOURCE_TYPE_COUNT`

`configSRP_RESOURCE_TYPE_COUNT` defines the compile-time resource ID space. Resource IDs are integers in:

```text
0 .. configSRP_RESOURCE_TYPE_COUNT - 1
```

The current implementation does not auto-create semaphores from these IDs. The IDs only define the SRP bookkeeping universe. The application is still responsible for creating the actual binary semaphores and then using the correct `(semaphore handle, resource ID)` pair.

### 3.2 Task creation API

In EDF+SRP mode, `xTaskCreate()` takes:

- task code
- stack depth
- task parameter
- task handle output
- period
- WCET
- relative deadline
- array of claimed semaphore IDs
- number of claimed semaphores

This means the task statically declares which binary semaphores it may lock.

If a task claims no semaphores:

- `puxClaimedSemaphores = NULL`
- `uxClaimedSemaphoreCount = 0`

This is the current replacement for the older, more general counted-resource claim model.

## 4. Current SRP Data Model

### 4.1 Per-task SRP state

Each SRP task currently stores:

- `uxPriorityCeiling`
- `uxSRPResourceClaimMax[]`
- `uxSRPResourceHeldCount[]`
- `xSRPTaskListItem`

What these mean today:

- `uxPriorityCeiling`
  This is actually the task's SRP preemption level. It is computed from the relative deadline at task creation. The current name is misleading and should eventually be changed to something like `uxPreemptionLevel`.

- `uxSRPResourceClaimMax[id]`
  In the current binary-semaphore design, this is effectively a boolean claim:
  - `0` means the task does not use resource `id`
  - `1` means the task may lock resource `id`

- `uxSRPResourceHeldCount[id]`
  In the current binary-semaphore design, this is effectively a boolean ownership state:
  - `0` means the task does not currently hold resource `id`
  - `1` means the task currently holds resource `id`

### 4.2 Global SRP state

The kernel currently keeps:

- `xReadySRPTasksList`
- `xSRPTaskRegistryList`
- `uxSRPResourceBlockingCeilingTable[]`
- `uxSRPResourceActiveCount[]`
- `uxSystemCeiling`

What these mean today:

- `xReadySRPTasksList`
  The ready list used in EDF+SRP mode. It is sorted by absolute deadline.

- `xSRPTaskRegistryList`
  The list of all SRP tasks currently known to the kernel. It is used when recomputing per-resource ceilings.

- `uxSRPResourceBlockingCeilingTable[id]`
  Despite the name, this now acts as the per-resource ceiling table.

- `uxSRPResourceActiveCount[id]`
  Despite the name, this now acts like a boolean lock state:
  - `0` means unlocked
  - `1` means locked

- `uxSystemCeiling`
  The current system ceiling, computed as the maximum ceiling of the resources that are currently locked.

## 5. How the Current Implementation Works

## 5.1 Task creation flow

The current SRP task-creation flow is:

```text
xTaskCreate(...) in EDF+SRP mode
    |
    +--> validate EDF timing parameters
    |
    +--> validate claimed semaphore IDs
    |     - IDs must be in range
    |     - no duplicates
    |
    +--> allocate and initialize TCB
    |
    +--> compute SRP preemption level from relative deadline
    |
    +--> mark each claimed semaphore ID in uxSRPResourceClaimMax[]
    |
    +--> initialize EDF release/deadline metadata
    |
    +--> insert task into ready list
    |
    +--> insert task into EDF registry
    |
    +--> insert task into SRP registry
    |
    +--> recompute system ceiling
```

This is how SRP is currently built directly on top of the EDF task model.

### 5.2 Preemption level derivation

The current implementation derives the task's SRP preemption level from the relative deadline:

```text
shorter relative deadline  -> higher preemption level
longer relative deadline   -> lower preemption level
```

This is done once at task creation. It is static. It does not change job-by-job with absolute deadlines.

That is the correct general idea for SRP on top of EDF.

### 5.3 Ready-list and scheduling flow

EDF+SRP scheduling currently works in two layers:

```text
Layer 1: EDF ordering
    - ready tasks are kept sorted by absolute deadline

Layer 2: SRP eligibility
    - scan the ready list from earliest deadline to latest
    - choose the first task that:
        a) has preemption level > current system ceiling
        or
        b) already holds a resource and must be allowed to continue
```

That gives the following runtime picture:

```text
xReadySRPTasksList (sorted by abs deadline)

    head --> T1 --> T2 --> T3 --> ...
              |      |      |
              |      |      +-- later deadline
              |      +--------- later deadline
              +---------------- earliest deadline

prvSRPSelectReadyTask()
    |
    +--> scan from head
    +--> pick first SRP-eligible task
```

This preserves EDF ordering while letting SRP control when a task is allowed to run.

### 5.4 Resource-ceiling and system-ceiling flow

The current implementation now computes the ceiling table as follows:

```text
for each resource id r:
    resource_ceiling[r] =
        max preemption level among tasks that claim r

system_ceiling =
    max resource_ceiling[r] over all resources currently locked
```

That gives the intended binary-semaphore SRP model:

```text
Task claims:
    Task A claims S0
    Task B claims S0, S1
    Task C claims S1

Resource ceilings:
    ceiling[S0] = max( PL(A), PL(B) )
    ceiling[S1] = max( PL(B), PL(C) )

Runtime:
    if S1 is locked:
        system_ceiling = ceiling[S1]
```

The current implementation recomputes this by scanning the SRP task registry whenever resource state changes. It does not use a stack of ceilings yet.

### 5.5 Resource acquisition path

The current take path is:

```text
xSemaphoreTakeSRP(...)
    |
    +--> xQueueSemaphoreTakeSRP(...)
            |
            +--> validate:
            |      - non-blocking take only
            |      - uxCount == 1
            |      - queue is a semaphore (item size 0)
            |      - length == 1
            |      - not a mutex
            |
            +--> xTaskSRPAcquireResource(resource_id, 1)
            |      - task must have claimed the resource
            |      - task must not already hold it
            |      - resource must be unlocked
            |      - task must be above system ceiling
            |        or already be continuing with a held resource
            |      - mark resource locked
            |      - mark task as holding it
            |      - recompute system ceiling
            |
            +--> xQueueSemaphoreTake(..., 0)
            |
            +--> if queue take fails:
                   rollback SRP release
```

This is how SRP bookkeeping is tied to semaphore acquisition without rewriting the whole semaphore subsystem.

### 5.6 Resource release path

The current give path is:

```text
xSemaphoreGiveSRP(...)
    |
    +--> xQueueSemaphoreGiveSRP(...)
            |
            +--> validate binary semaphore assumptions
            |
            +--> ensure semaphore is currently taken
            |
            +--> xTaskSRPReleaseResource(resource_id, 1)
            |      - current task must hold the resource
            |      - resource must be locked
            |      - clear task-held state
            |      - clear global locked state
            |      - recompute system ceiling
            |
            +--> xQueueGenericSend(..., 0)
```

This sequencing is safer than the earlier version that gave the semaphore first and only then updated SRP state.

### 5.7 Normal periodic completion

EDF tasks already used `xTaskDelayUntil()` to:

- advance the next release time,
- advance the next absolute deadline,
- reinsert the task into the periodic registry.

The SRP path now reuses that same EDF periodic helper and updates both:

- the EDF registry item
- the SRP registry item

This matters because SRP tasks are built on EDF periodic tasks, so they must continue to participate in the same job-release/deadline lifecycle.

### 5.8 Forced cleanup paths

The implementation already contains useful safety logic that should be kept:

- if a task is deleted, its SRP-held resources are force-released
- if a job is dropped/stopped on the EDF side, SRP-held resources are also force-released

This keeps global resource state from being permanently corrupted by a task that disappears while holding a resource.

## 6. Why This Design Is "Built on EDF"

The SRP implementation does not create a second independent scheduler. It reuses EDF in the following way:

```text
EDF provides:
    - periodic task model
    - absolute deadlines
    - deadline-sorted ready list
    - periodic release/deadline advancement

SRP adds:
    - static task preemption level
    - per-resource ceiling
    - current system ceiling
    - resource access eligibility rules
```

So the relationship is:

```text
EDF answers: "Who is most urgent?"
SRP answers: "Is that task allowed to start now?"
```

That is the core architectural idea of the current implementation.

## 7. Current Limitations

The current implementation is not yet a complete Task 2 solution. The main gaps are below.

### 7.1 Naming is still misleading

Several names still reflect the older design:

- `uxPriorityCeiling` is really a task preemption level
- `uxSRPResourceBlockingCeilingTable[]` is really a resource ceiling table
- `uxSRPResourceActiveCount[]` is really a lock-state table

These names do not currently match the binary-semaphore SRP meaning.

### 7.2 No binding between semaphore handle and resource ID

The application must manually pass both:

- the semaphore handle
- the SRP resource ID

The kernel does not currently prove that the handle and ID correspond to the same logical resource. A caller can still pass the wrong pair.

### 7.3 `uxCount` is still exposed even though the design is binary only

The runtime now enforces `uxCount == 1`, but the public SRP interfaces still expose a count parameter. This is a leftover generalization from counted resources.

### 7.4 `configSRP_RESOURCE_CEILING_TABLE` is unused

The config file still exposes `configSRP_RESOURCE_CEILING_TABLE`, but the kernel computes ceilings from task claims and task preemption levels instead of reading that table.

That means the configuration surface is not fully aligned with the implementation.

### 7.5 `xSrpCeilingTick` appears to be unused

The TCB and `StaticTask_t` still mirror `xSrpCeilingTick`, but the current logic does not meaningfully depend on it.

### 7.6 System ceiling is recomputed by full scan, not maintained by a stack

The assignment hint suggests using a ceiling stack. The current design instead recomputes ceilings by scanning:

- the SRP task registry
- the current locked-resource state

This can still work functionally, but it is not the stack-based implementation hinted at in the README and it is not optimized.

### 7.7 Blocking-time-aware admission control is missing

Task 2 explicitly requires extending admission control to include blocking times under EDF + SRP.

The current implementation does not yet:

- accept critical-section worst-case lengths as input,
- compute per-task blocking terms,
- include those blocking terms in admission control.

### 7.8 Run-time stack sharing is not implemented

Task 2 also explicitly requires stack sharing for tasks with the same preemption level.

The current implementation does not yet:

- group tasks by preemption level for stack reuse,
- allocate shared run-time stack storage,
- measure/report the quantitative memory savings.

### 7.9 No dedicated SRP test suite yet

The existing EDF tests are intentionally gated out when SRP is enabled. That is correct, but it means the project still needs:

- dedicated SRP functional tests,
- SRP + EDF admission tests with blocking,
- stack-sharing validation tests,
- the required quantitative study.

## 8. Improvements Needed to Fully Meet the Assignment

The remaining work should be done in roughly this order.

### 8.1 Clean up naming and public surface

- rename `uxPriorityCeiling` to `uxPreemptionLevel`
- rename `uxSRPResourceBlockingCeilingTable[]` to a resource-ceiling name
- rename `uxSRPResourceActiveCount[]` to a lock-state name
- remove `uxCount` from the SRP API, or keep it only as an internal assertion
- remove or replace `configSRP_RESOURCE_CEILING_TABLE`

### 8.2 Add blocking-time information to task creation

The kernel needs a way to know the worst-case length of each critical section, or at least the maximum blocking contribution of each resource claim. That should become part of the SRP task model so EDF + SRP admission control can be implemented correctly.

### 8.3 Implement EDF + SRP admission control with blocking

The existing EDF admission logic should be extended to account for SRP blocking time. This is still missing.

### 8.4 Decide whether to keep scan-based ceilings or implement a ceiling stack

The current scan-based recomputation is simple and works with the current data structures, but the assignment hint explicitly mentions a stack-based approach. If the goal is to align more closely with the expected SRP structure, this is the next architectural decision to make.

### 8.5 Add stack sharing

This is still the biggest missing Task 2 feature. The implementation needs:

- a way to detect tasks with equal preemption level,
- a shared run-time stack allocation strategy,
- proof/tests that such tasks never overlap on the stack,
- a quantitative comparison versus no stack sharing.

### 8.6 Add SRP tests and documentation artifacts

The project still needs:

- SRP functional tests
- SRP admission-control tests with blocking
- stack-sharing tests
- the required bugs/future/testing writeups specific to SRP

## 9. Current Bottom Line

The SRP implementation has moved from an older counted-resource design toward a binary-semaphore design that is actually layered on EDF:

- SRP is only enabled when EDF is enabled
- tasks declare claimed semaphore IDs
- task preemption levels are derived from relative deadlines
- the ready list remains deadline-ordered
- the system ceiling is derived from locked resources
- semaphore access goes through SRP-aware wrappers
- periodic completion now updates both EDF and SRP registries

So the current implementation captures the basic shape of EDF + SRP.

However, it is still not the full Task 2 solution because the following major requirements remain incomplete:

- blocking-time-aware admission control
- stack-based ceiling tracking if desired
- run-time stack sharing
- SRP-specific testing and quantitative evaluation

That is the current status of SRP in the codebase.
