# Multiprocessor EDF Development Plan

## Scope

This document replaces the earlier rough notes with a concrete implementation
plan for Task 4:

- implement both global EDF and partitioned EDF
- target the RP2040 dual-core SMP platform already present in this repo
- support only implicit-deadline periodic tasks for multiprocessing
- exclude SRP/resource sharing and CBS in MP mode, per the assignment
- keep both implementations in the code base under config guards
- default to global EDF when MP is enabled and no specific MP policy is chosen

This plan is written to be consistent with the Task 4 requirements in
`assignment_md_files/FreeRTOS-README.md`.

## High-Level Assessment Of The Current Approach

The general direction in the earlier plan is correct:

- reuse existing FreeRTOS SMP infrastructure instead of building multicore
  support from scratch
- keep the implementation low-intrusion and config-guarded
- reuse the existing EDF metadata already added to the TCB

However, a few important corrections are needed.

### 1. `configUSE_MP` should not conceptually replace EDF

Task 4 is still EDF scheduling. Right now the config structure suggests:

- uniprocessor EDF if `configUSE_EDF == 1`
- multiprocessing if `configUSE_MP == 1`

and the current compile-time checks even forbid both together.

That is too coarse. For Task 4, multiprocessing is the execution model and EDF
is still the policy. If `configUSE_MP` disables the EDF path entirely, we end up
duplicating EDF logic or accidentally falling back to the stock SMP priority
scheduler.

Recommended interpretation:

- `configUSE_UP == 1`, `configUSE_MP == 0`, `configUSE_EDF == 0`:
  stock FreeRTOS uniprocessor fixed-priority scheduling
- `configUSE_UP == 1`, `configUSE_MP == 0`, `configUSE_EDF == 1`:
  uniprocessor EDF
- `configUSE_UP == 0`, `configUSE_MP == 1`, `configUSE_EDF == 0`:
  stock FreeRTOS SMP fixed-priority scheduling
- `configUSE_UP == 0`, `configUSE_MP == 1`, `configUSE_EDF == 1`,
  `PARTITIONED_EDF_ENABLE == 0`:
  global EDF on SMP
- `configUSE_UP == 0`, `configUSE_MP == 1`, `configUSE_EDF == 1`,
  `PARTITIONED_EDF_ENABLE == 1`:
  partitioned EDF on SMP

### 2. The main implementation point is `tasks.c`, not `queue.c` or `list.c`

The current MP plan mentions looking at `tasks.c`, `queue.c`, and `list.c`.
For Task 4, the real work is almost entirely in:

- `FreeRTOS-Kernel/tasks.c`
- `FreeRTOS-Kernel/include/task.h`
- `FreeRTOS-Kernel/include/FreeRTOS.h`
- `FreeRTOSConfig.h`
- `schedulingConfig.h`

`queue.c` is only relevant for SRP/resource sharing, which Task 4 explicitly
does not require. `list.c` already provides the ordered insertion behavior we
need for EDF queues.

### 3. The RP2040 port already solves most of the "raw SMP" questions

This repository is already using the RP2040 SMP-capable FreeRTOS port. That
port already provides:

- dual-core startup through `multicore_launch_core1()`
- one designated tick core via `configTICK_CORE`
- cross-core reschedule interrupts via `portYIELD_CORE`
- task/core affinity masks through `uxCoreAffinityMask`
- per-core idle tasks and per-core current TCBs

So the correct design is:

- keep the existing port and SMP runtime
- replace the scheduling policy layered on top of it

not:

- re-implement interrupt routing, core launch, or low-level SMP machinery

### 4. Partitioned EDF should not use the proposed global `Ub = (m + 1) / 2` test

For partitioned EDF, the meaningful schedulability decision is:

- did the task fit into some partition according to the chosen bin-packing
  algorithm?
- is each partition's utilization still `<= 1` after assignment?

So the correct partitioned acceptance logic is:

1. optionally early-reject if total utilization exceeds `m`
2. pick a target core using bin packing
3. verify that target core remains at utilization `<= 1`

For this project, with implicit deadlines and a simple implementation goal,
that is the right test.

### 5. Global EDF needs more than "one global queue"

The earlier notes correctly identify the need for one global bucket of tasks,
but the non-trivial part is the preemption rule:

- when a new task with an earlier deadline becomes ready,
  global EDF must identify whether it should displace one of the currently
  running jobs
- if yes, it should preempt the running job with the latest deadline among the
  jobs that are currently executing and are eligible to be displaced

Without that, the implementation will have a sorted queue but still not behave
like global EDF.

## Core Design Decisions

### Config Guarding Strategy

Keep the low-intrusion, config-guarded approach, but reshape it slightly.

The simpler public configuration model you suggested is reasonable:

- let `configUSE_UP` and `configUSE_MP` choose the execution model
- let `configUSE_EDF` independently choose EDF vs stock scheduler behavior
- if `configUSE_EDF == 0`, fall back to the stock scheduler for either UP or MP

That is a cleaner user-facing model than making `configUSE_MP` implicitly mean
"EDF MP mode."

Recommended public config shape:

```c
#define configUSE_UP 1
#define configUSE_MP 0

#if ( ( configUSE_UP == 1 ) && ( configUSE_MP == 1 ) )
    #error "configUSE_UP and configUSE_MP cannot both be enabled."
#endif

#if ( ( configUSE_UP == 0 ) && ( configUSE_MP == 0 ) )
    #error "Exactly one of configUSE_UP or configUSE_MP must be enabled."
#endif
```

However, even if the public knobs are just:

- `configUSE_UP`
- `configUSE_MP`
- `configUSE_EDF`

the source code should still use internal derived mode macros for readability
and safety.

Recommended internal derived macros:

```c
#define configUSE_UP_FIXED_PRIORITY \
    ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 0 ) )

#define configUSE_UP_EDF \
    ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 1 ) )

#define configUSE_GLOBAL_EDF_MP \
    ( ( configUSE_EDF == 1 ) && ( configUSE_MP == 1 ) && \
      ( PARTITIONED_EDF_ENABLE == 0 ) )

#define configUSE_PARTITIONED_EDF_MP \
    ( ( configUSE_EDF == 1 ) && ( configUSE_MP == 1 ) && \
      ( PARTITIONED_EDF_ENABLE == 1 ) )

#define configUSE_EDF_MP \
    ( ( configUSE_GLOBAL_EDF_MP == 1 ) || \
      ( configUSE_PARTITIONED_EDF_MP == 1 ) )

#define configUSE_MP_FIXED_PRIORITY \
    ( ( configUSE_MP == 1 ) && ( configUSE_EDF == 0 ) )
```

Additional recommended restrictions:

```c
#if ( configUSE_MP == 0 ) && ( GLOBAL_EDF_ENABLE == 1 )
    #error "GLOBAL_EDF_ENABLE requires MP mode."
#endif

#if ( configUSE_MP == 0 ) && ( PARTITIONED_EDF_ENABLE == 1 )
    #error "PARTITIONED_EDF_ENABLE requires MP mode."
#endif

#if ( GLOBAL_EDF_ENABLE == 1 ) && ( PARTITIONED_EDF_ENABLE == 1 )
    #error "Only one MP EDF policy may be enabled."
#endif

#if ( configUSE_MP == 1 ) && ( configUSE_CBS == 1 )
    #error "CBS is not supported in MP mode for Task 4."
#endif

#if ( configUSE_MP == 1 ) && ( configUSE_SRP == 1 )
    #error "SRP is not supported in MP mode for Task 4."
#endif
```

Why:

- MP Task 4 excludes resource sharing and server scheduling
- the code path should remain explicit and mechanically easy to review
- it avoids accidental interaction with the current uniprocessor EDF+CBS/SRP
  implementation
- it preserves stock FreeRTOS behavior in both UP and MP whenever EDF is off

### Safe Refactor Strategy For Derived Mode Macros

The safe way to introduce derived mode macros is:

- keep `configUSE_EDF` as the broad "EDF family is enabled" capability flag
- use `configUSE_UP` and `configUSE_MP` only as public execution-model knobs
- use the derived mode macros only to choose which EDF behavior is active
- do not perform a blind global replacement of every
  `#if ( configUSE_EDF == 1 )` in the tree

This matters because the current code already mixes three different kinds of EDF
logic under the same `configUSE_EDF` guard:

1. shared EDF capability state
2. uniprocessor-only EDF policy behavior
3. extensions layered on uniprocessor EDF, namely CBS and SRP
4. stock fixed-priority behavior in UP or MP when EDF is disabled

If all of that is converted mechanically to the derived mode macros, regressions
are very likely.

#### The Correct Guarding Model

When refactoring, think in terms of three guard categories.

##### 1. Shared EDF Capability Guards

These should stay under the broad EDF capability guard:

```c
#if ( configUSE_EDF == 1 )
    /* shared EDF fields, shared release/deadline metadata, shared helpers */
#endif
```

These are pieces of state or logic that are needed by:

- uniprocessor EDF
- global EDF MP
- partitioned EDF MP

Examples from the current code base:

- EDF fields in `TCB_t`
- EDF fields mirrored in `StaticTask_t`
- `xEDFTaskRegistryList`
- `prvTickTimeIsAfter()`
- `prvPeriodicTaskNextReleaseAfter()`
- `prvPeriodicTaskAdvanceAndReinsert()`
- EDF cleanup in `vTaskDelete()`
- EDF fields captured by debug snapshot helpers

These are not where the policy difference lives, so changing them to
`configUSE_UP_EDF` would break MP EDF immediately.

##### 2. Uniprocessor-Only EDF Policy Guards

These should be narrowed to:

```c
#if ( configUSE_UP_EDF == 1 )
    /* legacy uniprocessor EDF behavior */
#endif
```

These are behaviors whose semantics are explicitly single-core:

- exact constrained-deadline demand analysis helpers
- the current uniprocessor EDF admission path
- the single-core EDF branch inside `vTaskSwitchContext()`
- the single-core EDF job accounting path in `xTaskIncrementTick()`
- all CBS logic
- all SRP logic

Examples that should move from broad EDF guards to UP EDF guards:

- `prvEDFUtilTestWithNew()` if kept with its current `U <= 1` meaning
- `prvEDFDemandTestWithNew()`
- `prvTaskIsCBSManaged()`
- `prvEDFShouldPreempt()` in its current single-core form
- `prvCBSRefreshReadyListPosition()`
- SRP resource and ceiling helpers
- SRP shared stack helpers
- the EDF/CBS/SRP overloads of `xTaskCreate()`

##### 3. MP EDF Policy Guards

These should branch explicitly:

```c
#if ( configUSE_GLOBAL_EDF_MP == 1 )
    /* global EDF MP behavior */
#elif ( configUSE_PARTITIONED_EDF_MP == 1 )
    /* partitioned EDF MP behavior */
#else
    /* existing fixed-priority SMP behavior */
#endif
```

These are the places where the scheduling policy actually differs in MP mode:

- SMP ready-list insertion behavior
- SMP task selection
- SMP cross-core preemption decisions
- SMP tick accounting for all running cores
- MP admission control
- MP task creation APIs

#### Refactor Rule: Do Not Replace, Reclassify

The safest workflow is:

1. add the derived macros first
2. classify each existing `configUSE_EDF`-guarded region as:
   - shared EDF capability
   - UP-only EDF
   - MP-specific EDF
3. only then adjust its guard

This means the question to ask at each site is not:

- "Should this use the new macros?"

It is:

- "Is this shared EDF infrastructure, UP EDF behavior, or MP EDF behavior?"

#### Safe Refactor Table

Use the following mapping during the refactor.

| Current code kind | Safe new guard |
| --- | --- |
| EDF metadata fields or registry state | `configUSE_EDF == 1` |
| Single-core EDF scheduler logic | `configUSE_UP_EDF == 1` |
| SRP logic | `configUSE_UP_EDF == 1 && configUSE_SRP == 1` |
| CBS logic | `configUSE_UP_EDF == 1 && configUSE_CBS == 1` |
| SMP global EDF selector/yield/accounting | `configUSE_GLOBAL_EDF_MP == 1` |
| SMP partitioned EDF selector/yield/accounting | `configUSE_PARTITIONED_EDF_MP == 1` |
| UP fixed-priority fallback | `configUSE_UP_FIXED_PRIORITY == 1` or final `#else` in UP-only code |
| SMP fixed-priority fallback | `configUSE_MP_FIXED_PRIORITY == 1` or final `#else` in SMP-only code |

#### Concrete Examples In This Repo

##### Example A: `TCB_t` EDF fields

Current shape:

```c
#if ( configUSE_EDF == 1 )
    TickType_t xAbsJobReleaseTime;
    TickType_t xPeriodTicks;
    TickType_t xWcetTicks;
    TickType_t xJobExecTicks;
    TickType_t xRelDeadline;
    TickType_t xAbsDeadline;
    ListItem_t xEDFTaskListItem;
#endif
```

Safe result:

- leave this exactly under the broad EDF guard

Why:

- all EDF modes need these fields

##### Example B: current uniprocessor `vTaskSwitchContext()` EDF branch

Current shape:

```c
#if ( configUSE_EDF == 1 )
    /* choose from xReadyEDFTasksList */
#else
    taskSELECT_HIGHEST_PRIORITY_TASK();
#endif
```

Safe result:

```c
#if ( configUSE_UP_EDF == 1 )
    /* existing single-core EDF behavior preserved */
#else
    taskSELECT_HIGHEST_PRIORITY_TASK();
#endif
```

Why:

- this exact branch is single-core logic and must not silently become the MP
  implementation

##### Example C: current SMP `prvSelectHighestPriorityTask()`

Current shape:

- fixed-priority SMP selector only

Safe result:

```c
#if ( configUSE_GLOBAL_EDF_MP == 1 )
    /* global EDF MP selector */
#elif ( configUSE_PARTITIONED_EDF_MP == 1 )
    /* partitioned EDF MP selector */
#else
    /* existing fixed-priority SMP selector unchanged */
#endif
```

Why:

- this lets the old SMP fallback survive intact while MP EDF is introduced

##### Example D: current `xTaskIncrementTick()` EDF accounting

Current shape:

- broad EDF guard
- updates only `pxCurrentTCB`

Safe result:

```c
#if ( configUSE_UP_EDF == 1 )
    /* preserve current single-core accounting */
#elif ( configUSE_EDF_MP == 1 )
    /* new MP accounting across pxCurrentTCBs[core] */
#endif
```

Why:

- the broad guard is currently hiding two very different semantics

#### Recommended Refactor Sequence

To minimize regression risk, apply the migration in controlled stages.

##### Stage 0: Add Derived Macros Without Behavior Change

Only update `schedulingConfig.h`:

- add `configUSE_UP`
- add the derived mode macros
- remove the current compile-time error that forbids EDF+MP together
- keep all existing code untouched

At the end of this stage:

- the current uniprocessor EDF, CBS, and SRP code should still compile and run
  unchanged

##### Stage 1: Narrow Only The Clearly UP-Only Guards

Convert only the obviously UP-only regions:

- single-core EDF branch in `vTaskSwitchContext()`
- UP-only exact admission helpers
- CBS helpers and CBS create/bind logic
- SRP helpers and SRP semaphore/resource logic

Do not touch:

- TCB EDF fields
- EDF registry state
- shared release/deadline helpers

At the end of this stage:

- uniprocessor EDF behavior should be identical to today
- MP EDF mode still does nothing new yet

##### Stage 2: Introduce MP EDF Helpers As New Code

Add new MP-only helpers under the derived mode guards:

- global EDF selector
- partitioned EDF selector
- global EDF yield helper
- partitioned EDF yield helper
- global EDF admission helper
- partition assignment helper

Do not wire them into the active path yet.

At the end of this stage:

- old code paths still dominate
- new MP EDF code exists in isolation and is reviewable

##### Stage 3: Switch SMP Dispatch To The New EDF Selectors

Only after the helpers exist:

- route SMP `prvSelectHighestPriorityTask()` through the new mode branches
- leave the existing fixed-priority SMP logic as the final fallback

At the end of this stage:

- MP EDF starts affecting dispatch
- stock SMP still works when `configUSE_MP == 1 && configUSE_EDF == 0`

##### Stage 4: Switch Tick Accounting

Then update `xTaskIncrementTick()`:

- preserve the current UP EDF path under `configUSE_UP_EDF`
- add a separate MP EDF path that loops over all running cores

This stage should be kept separate from Stage 3 because it is one of the
highest-risk correctness changes.

##### Stage 5: Add MP EDF Task Creation APIs

After dispatch and accounting are stable:

- add `xTaskCreateGlobalEDF()`
- add `xTaskCreatePartitionedEDF()`
- add `xTaskMigrateToCore()`

Do not remove the current uniprocessor `xTaskCreate()` overloads.

At the end of this stage:

- old user code for UP EDF still works unchanged
- MP EDF gets explicit entry points

#### Safe Verification Matrix

After each stage, verify at least the following configurations.

1. `configUSE_EDF=0`, `configUSE_MP=0`
   Expected result: invalid if `configUSE_UP` is introduced and exactly one of
   UP/MP must be enabled.
2. `configUSE_UP=1`, `configUSE_MP=0`, `configUSE_EDF=0`
   Expected result: stock FreeRTOS UP fixed-priority unchanged.
3. `configUSE_UP=1`, `configUSE_MP=0`, `configUSE_EDF=1`, `configUSE_CBS=0`, `configUSE_SRP=0`
   Expected result: current uniprocessor EDF unchanged.
4. `configUSE_UP=1`, `configUSE_MP=0`, `configUSE_EDF=1`, `configUSE_CBS=1`
   Expected result: current EDF+CBS unchanged.
5. `configUSE_UP=1`, `configUSE_MP=0`, `configUSE_EDF=1`, `configUSE_SRP=1`
   Expected result: current EDF+SRP unchanged.
6. `configUSE_UP=0`, `configUSE_MP=1`, `configUSE_EDF=1`, `PARTITIONED_EDF_ENABLE=0`
   Expected result: global EDF MP active.
7. `configUSE_UP=0`, `configUSE_MP=1`, `configUSE_EDF=1`, `PARTITIONED_EDF_ENABLE=1`
   Expected result: partitioned EDF MP active.
8. `configUSE_UP=0`, `configUSE_MP=1`, `configUSE_EDF=0`
   Expected result: stock SMP fixed-priority unchanged.

This matrix is what keeps the derived-macro refactor safe. The point is not
just to make MP EDF work, but to prove that the older working modes continue to
behave exactly as before.

#### Final Refactor Principle

The public mode knobs should stay simple, and the derived mode macros should be
used to split behavior, not to erase the current architecture.

In practice that means:

- user-facing config can be:
  - `configUSE_UP`
  - `configUSE_MP`
  - `configUSE_EDF`
- shared EDF state remains broadly guarded
- existing uniprocessor EDF/CBS/SRP behavior is preserved under narrower guards
- MP EDF is added as a new branch beside, not inside, the old UP logic

That is the safest way to refactor this code without introducing subtle
regressions in the already-working uniprocessor implementation.

### Admission Control Policy

#### Global EDF

Use a simple sufficient test, not an exact one:

- for `m = configNUMBER_OF_CORES`
- accept the task set if
  `U_total <= m - (m - 1) * U_max`

This is a sufficient global EDF test on identical processors. It is not
necessary, but it is simple, safe, and consistent with the assignment's request
for "a simple admission control test of your choice."

#### Partitioned EDF

Use:

- online First-Fit Decreasing if task creation is staged and can be sorted
- or plain First-Fit for dynamic arrivals

For this code base, the pragmatic choice is:

- use First-Fit for runtime task creation
- maintain per-core utilization
- accept only if the chosen core remains at `U_core <= 1`

Why not WFD:

- WFD balances load, but admission success is the primary goal
- FFD/FF are simpler, standard, and easier to justify in a report

### Migration Policy

#### Global EDF

Allow both:

- task migration
- job migration

Meaning:

- a task is generally allowed on both cores
- a preempted job may resume on a different core

That matches the assignment text.

#### Partitioned EDF

Default behavior:

- no automatic migration during scheduling
- each task belongs to exactly one partition

But the assignment also asks for the ability to change the processor upon which
a task is running. So for partitioned EDF we should still expose:

- explicit migration API

with the semantics:

- remove the task's utilization from the old core
- attempt to add it to the new core
- update affinity and core-local queueing state

### Scheduler Structure

#### Partitioned EDF

Conceptually: yes, one scheduler per core.

Practically:

- still one FreeRTOS kernel
- still one delayed list / one global tick timeline
- but each core chooses from its own EDF-ready queue

This means:

- per-core ready queue
- per-core utilization bookkeeping
- per-core dispatch

#### Global EDF

Conceptually: one scheduler for the whole machine.

Practically:

- one global EDF-ready queue
- every core dispatches from that same global queue
- the running set consists of the best `m` eligible jobs

## Direct Answers To The Four Assignment Questions

### 1. Interrupt handling and timer functionality

A single periodic kernel tick source is sufficient, but per-core reschedule
interrupts are still required.

This repository's RP2040 port already uses exactly the correct model:

- one designated tick core via `configTICK_CORE`
- cross-core yield requests using `portYIELD_CORE`

That should be retained.

Recommended design decision:

- keep one shared kernel tick maintained by the tick core
- do not create an additional periodic timer per core
- keep cross-core software/FIFO interrupts for remote rescheduling

Why:

- the kernel already uses a single global tick count
- tasks are not independent kernel instances
- per-core timers would add complexity without improving correctness here

### 2. How to stop/start each core

Starting each core is already handled by the RP2040 SMP port:

- core 0 enters `xPortStartScheduler()`
- core 1 is launched via `multicore_launch_core1()`

For this project, do not implement hardware-level core stop/start as part of
the scheduling feature.

Recommended behavior:

- start both cores normally through the existing port
- if a core has no eligible work, let it run its passive idle task
- if a "disable core" experiment is desired later, implement it logically by
  making that core unavailable to EDF dispatch, not by shutting the core down

Why:

- the assignment asks for scheduling support, not power management
- the existing idle-task model already provides a clean "no work on this core"
  behavior

### 3. Dispatching tasks to certain cores, and migration

Use the existing FreeRTOS SMP affinity system as the primary control surface.

Partitioned EDF:

- each task gets a one-bit affinity mask
- dispatch only on its assigned core
- migration happens only through an explicit migration API

Global EDF:

- each task defaults to all-core affinity unless explicitly pinned
- dispatch the earliest-deadline eligible job to any available core
- allow resumed execution on another core after preemption

Why:

- affinity already exists in the kernel
- it maps directly to the assignment's "specify core", "remove from processor",
  and "change processor" requirements

### 4. Separate scheduler per core?

Partitioned EDF:

- yes, conceptually yes
- each core has its own EDF ready queue and local admission accounting

Global EDF:

- no
- one global queue and one global policy, with per-core dispatch points

In both cases:

- the system remains one kernel
- one global tick
- one delayed-task mechanism

## Detailed Development Plan

## Phase 1: Configuration And Mode Separation

### File: `schedulingConfig.h`

#### Changes

- remove the current compile-time error that forbids `configUSE_MP == 1` with
  `configUSE_EDF == 1`
- keep the "cannot enable both global and partitioned at once" guard
- add derived mode macros
- explicitly block CBS and SRP when MP mode is enabled

#### Why

The current guard:

```c
#if (configUSE_MP == 1 && configUSE_EDF == 1)
    #error "Multiprocessing and uniprocessing scheduling cannot be enabled simultaneously ..."
#endif
```

prevents Task 4 from reusing EDF scheduling logic. That is the wrong axis of
separation.

#### Code To Add

```c
#define configUSE_UP 1
#define configUSE_MP 0

#if ( ( configUSE_UP == 1 ) && ( configUSE_MP == 1 ) )
    #error "configUSE_UP and configUSE_MP cannot both be enabled."
#endif

#if ( ( configUSE_UP == 0 ) && ( configUSE_MP == 0 ) )
    #error "Exactly one of configUSE_UP or configUSE_MP must be enabled."
#endif

#define configUSE_UP_FIXED_PRIORITY \
    ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 0 ) )

#define configUSE_UP_EDF \
    ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 1 ) )

#define configUSE_GLOBAL_EDF_MP \
    ( ( configUSE_EDF == 1 ) && ( configUSE_MP == 1 ) && \
      ( PARTITIONED_EDF_ENABLE == 0 ) )

#define configUSE_PARTITIONED_EDF_MP \
    ( ( configUSE_EDF == 1 ) && ( configUSE_MP == 1 ) && \
      ( PARTITIONED_EDF_ENABLE == 1 ) )

#define configUSE_EDF_MP \
    ( ( configUSE_GLOBAL_EDF_MP == 1 ) || \
      ( configUSE_PARTITIONED_EDF_MP == 1 ) )

#define configUSE_MP_FIXED_PRIORITY \
    ( ( configUSE_MP == 1 ) && ( configUSE_EDF == 0 ) )

#if ( configUSE_MP == 0 ) && ( GLOBAL_EDF_ENABLE == 1 )
    #error "GLOBAL_EDF_ENABLE requires MP mode."
#endif

#if ( configUSE_MP == 0 ) && ( PARTITIONED_EDF_ENABLE == 1 )
    #error "PARTITIONED_EDF_ENABLE requires MP mode."
#endif

#if ( GLOBAL_EDF_ENABLE == 1 ) && ( PARTITIONED_EDF_ENABLE == 1 )
    #error "Only one MP EDF policy may be enabled."
#endif

#if ( configUSE_MP == 1 ) && ( configUSE_CBS == 1 )
    #error "CBS is not supported in MP mode."
#endif

#if ( configUSE_MP == 1 ) && ( configUSE_SRP == 1 )
    #error "SRP is not supported in MP mode."
#endif
```

### File: `FreeRTOSConfig.h`

#### Changes

- keep `configNUMBER_OF_CORES == 2` when MP is enabled
- keep `configUSE_CORE_AFFINITY == 1`
- explicitly define `configTICK_CORE 0`
- keep time slicing disabled

#### Why

The RP2040 SMP port already defaults the tick to a single core. Making that
explicit improves clarity and keeps the design aligned with the report.

#### Code To Add

```c
#if ( configUSE_MP == 1 )
    #define configUSE_CORE_AFFINITY 1
    #define configNUMBER_OF_CORES   2
    #define configTICK_CORE         0
#else
    #define configNUMBER_OF_CORES   1
#endif
```

## Phase 2: Public MP EDF Interface

### File: `FreeRTOS-Kernel/include/task.h`

#### Sections To Change

- the EDF `xTaskCreate()` declarations
- the multicore API section near the affinity APIs

#### Changes

Keep the current uniprocessor EDF `xTaskCreate()` intact for non-MP EDF.
For MP EDF add explicit APIs instead of trying to overload the fixed-priority
`xTaskCreateAffinitySet()`.

#### Why

Right now:

- EDF builds redefine `xTaskCreate()` with timing parameters
- SMP affinity creation APIs still use the fixed-priority signature

That makes the interface inconsistent in MP EDF mode.

#### Code To Add

```c
#if ( configUSE_GLOBAL_EDF_MP == 1 )
BaseType_t xTaskCreateGlobalEDF( TaskFunction_t pxTaskCode,
                                 const char * const pcName,
                                 configSTACK_DEPTH_TYPE uxStackDepth,
                                 void * const pvParameters,
                                 TaskHandle_t * const pxCreatedTask,
                                 uint32_t ulPeriodMs,
                                 uint32_t ulWcetMs ) PRIVILEGED_FUNCTION;
#endif

#if ( configUSE_PARTITIONED_EDF_MP == 1 )
BaseType_t xTaskCreatePartitionedEDF( TaskFunction_t pxTaskCode,
                                      const char * const pcName,
                                      configSTACK_DEPTH_TYPE uxStackDepth,
                                      void * const pvParameters,
                                      TaskHandle_t * const pxCreatedTask,
                                      uint32_t ulPeriodMs,
                                      uint32_t ulWcetMs,
                                      BaseType_t xRequestedCore ) PRIVILEGED_FUNCTION;

BaseType_t xTaskMigrateToCore( TaskHandle_t xTask,
                               BaseType_t xNewCore ) PRIVILEGED_FUNCTION;
#endif
```

Note:

- `ulRelDeadlineMs` is omitted in MP EDF because Task 4 only requires
  implicit-deadline tasks, so `D = T`
- if the implementation wants interface symmetry, it is acceptable to keep the
  deadline parameter and assert `D == T`

## Phase 3: TCB And Static TCB Support

### File: `FreeRTOS-Kernel/tasks.c`

#### Section To Change

- `TCB_t` definition

#### Changes

Add MP EDF bookkeeping fields:

```c
#if ( configUSE_MP == 1 )
    BaseType_t xAssignedCore;   /* -1 for global EDF, 0..N-1 for partitioned EDF */
    BaseType_t xLastCore;       /* optional migration hint/debug aid */
#endif
```

#### Why

Partitioned EDF needs to know the owner's partition even when the task is
blocked. Relying only on affinity is possible, but explicit assignment is
cleaner and easier to use for load accounting and migration APIs.

### File: `FreeRTOS-Kernel/include/FreeRTOS.h`

#### Section To Change

- `StaticTask_t`

#### Changes

Mirror the new TCB fields so static task allocation remains layout-compatible.

#### Code To Add

```c
#if ( configUSE_MP == 1 )
    BaseType_t xAssignedCore;
    BaseType_t xLastCore;
#endif
```

#### Why

Any field added to `TCB_t` must be reflected in `StaticTask_t`.

## Phase 4: Shared MP EDF Kernel Helpers

### File: `FreeRTOS-Kernel/tasks.c`

#### New State To Add

```c
#if ( configUSE_PARTITIONED_EDF_MP == 1 )
    PRIVILEGED_DATA static List_t xReadyPartitionedEDFTasksLists[ configNUMBER_OF_CORES ];
    PRIVILEGED_DATA static uint64_t ullPartitionUtilQ32[ configNUMBER_OF_CORES ] = { 0 };
#endif
```

#### New Helpers To Add

```c
static BaseType_t prvTaskAllowedOnCore( const TCB_t * pxTCB,
                                        BaseType_t xCoreID );

static BaseType_t prvTaskIsRealtimePeriodic( const TCB_t * pxTCB );
```

Example:

```c
static BaseType_t prvTaskAllowedOnCore( const TCB_t * pxTCB,
                                        BaseType_t xCoreID )
{
    #if ( configUSE_CORE_AFFINITY == 1 )
        if( ( pxTCB->uxCoreAffinityMask & ( ( UBaseType_t ) 1U << ( UBaseType_t ) xCoreID ) ) == 0U )
        {
            return pdFALSE;
        }
    #else
        ( void ) xCoreID;
    #endif

    return pdTRUE;
}
```

#### Why

These helpers centralize policy checks and avoid repeating affinity and
periodicity logic throughout the scheduler.

## Phase 5: Ready Queue Insertion Refactor

### File: `FreeRTOS-Kernel/tasks.c`

#### Section To Change

- current `prvAddTaskToReadyList` macro block

Current issue:

- the existing EDF path inserts everything into one `xReadyEDFTasksList`
- that works for uniprocessor EDF
- it does not distinguish global EDF MP from partitioned EDF MP

#### Recommended Change

Keep the existing macro for:

- stock fixed-priority mode
- uniprocessor EDF

But in MP EDF mode, replace macro-only insertion with helpers:

```c
static void prvAddTaskToReadyListEDFGlobal( TCB_t * pxTCB );
static void prvAddTaskToReadyListEDFPartitioned( TCB_t * pxTCB );
```

Example:

```c
static void prvAddTaskToReadyListEDFGlobal( TCB_t * pxTCB )
{
    listSET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ), pxTCB->xAbsDeadline );
    vListInsert( &xReadyEDFTasksList, &( pxTCB->xStateListItem ) );
}

static void prvAddTaskToReadyListEDFPartitioned( TCB_t * pxTCB )
{
    BaseType_t xCore = pxTCB->xAssignedCore;
    configASSERT( taskVALID_CORE_ID( xCore ) != pdFALSE );

    listSET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ), pxTCB->xAbsDeadline );
    vListInsert( &xReadyPartitionedEDFTasksLists[ xCore ],
                 &( pxTCB->xStateListItem ) );
}
```

Then route all EDF insertions through a wrapper:

```c
static void prvAddTaskToReadyListEDF( TCB_t * pxTCB )
{
    #if ( configUSE_PARTITIONED_EDF_MP == 1 )
        prvAddTaskToReadyListEDFPartitioned( pxTCB );
    #else
        prvAddTaskToReadyListEDFGlobal( pxTCB );
    #endif
}
```

#### Why

Partitioned EDF fundamentally needs per-core ready queues. Trying to encode both
policies with the current single macro will get messy quickly.

## Phase 6: Scheduler Selection Logic

### File: `FreeRTOS-Kernel/tasks.c`

#### Section To Change

- `prvSelectHighestPriorityTask()` in SMP mode

This is the central change for Task 4.

Right now the SMP path still selects tasks from fixed-priority ready lists.
That is incompatible with both global EDF MP and partitioned EDF MP.

### Global EDF Selector

Add:

```c
static TCB_t * prvGlobalEDFSelectTaskForCore( BaseType_t xCoreID )
{
    ListItem_t * pxIterator;

    if( listLIST_IS_EMPTY( &xReadyEDFTasksList ) != pdFALSE )
    {
        return NULL;
    }

    for( pxIterator = listGET_HEAD_ENTRY( &xReadyEDFTasksList );
         pxIterator != listGET_END_MARKER( &xReadyEDFTasksList );
         pxIterator = listGET_NEXT( pxIterator ) )
    {
        TCB_t * pxTCB = ( TCB_t * ) listGET_LIST_ITEM_OWNER( pxIterator );

        if( prvTaskAllowedOnCore( pxTCB, xCoreID ) == pdFALSE )
        {
            continue;
        }

        if( ( pxTCB->xTaskRunState == taskTASK_NOT_RUNNING ) ||
            ( pxTCB == pxCurrentTCBs[ xCoreID ] ) )
        {
            return pxTCB;
        }
    }

    return NULL;
}
```

Why:

- one global deadline-ordered queue
- each core takes the earliest eligible task not already running elsewhere

### Partitioned EDF Selector

Add:

```c
static TCB_t * prvPartitionedEDFSelectTaskForCore( BaseType_t xCoreID )
{
    if( listLIST_IS_EMPTY( &xReadyPartitionedEDFTasksLists[ xCoreID ] ) == pdFALSE )
    {
        return ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY(
            &xReadyPartitionedEDFTasksLists[ xCoreID ] );
    }

    return NULL;
}
```

Why:

- each core owns a local EDF queue
- selection is local

### SMP `prvSelectHighestPriorityTask()` Refactor

In SMP mode, branch early:

```c
#if ( configUSE_GLOBAL_EDF_MP == 1 )
    pxTCB = prvGlobalEDFSelectTaskForCore( xCoreID );
#elif ( configUSE_PARTITIONED_EDF_MP == 1 )
    pxTCB = prvPartitionedEDFSelectTaskForCore( xCoreID );
#else
    /* existing fixed-priority SMP logic */
#endif
```

If `pxTCB == NULL`, fall back to the core's idle task.

Why:

- this cleanly overlays EDF policy onto the existing SMP machinery
- the low-level context-switch code does not need to be rewritten

## Phase 7: Cross-Core Preemption Logic

### File: `FreeRTOS-Kernel/tasks.c`

#### Section To Change

- `prvYieldForTask()`

This function currently compares fixed priorities across cores. In MP EDF mode,
that is wrong.

### Global EDF Preemption Rule

Add a separate helper:

```c
static void prvYieldForGlobalEDFTask( const TCB_t * pxReadyTCB )
{
    BaseType_t xVictimCore = -1;
    TickType_t xWorstDeadline = 0;

    for( BaseType_t xCoreID = 0; xCoreID < ( BaseType_t ) configNUMBER_OF_CORES; xCoreID++ )
    {
        TCB_t * pxRunning = pxCurrentTCBs[ xCoreID ];

        if( ( pxRunning->uxTaskAttributes & taskATTRIBUTE_IS_IDLE ) != 0U )
        {
            xVictimCore = xCoreID;
            break;
        }

        if( prvTaskAllowedOnCore( pxReadyTCB, xCoreID ) == pdFALSE )
        {
            continue;
        }

        if( pxRunning->xAbsDeadline > pxReadyTCB->xAbsDeadline )
        {
            if( ( xVictimCore < 0 ) || ( pxRunning->xAbsDeadline > xWorstDeadline ) )
            {
                xVictimCore = xCoreID;
                xWorstDeadline = pxRunning->xAbsDeadline;
            }
        }
    }

    if( xVictimCore >= 0 )
    {
        prvYieldCore( xVictimCore );
    }
}
```

Why:

- under global EDF, the new task should displace the currently running task with
  the latest deadline among the running set, not the one with the lowest fixed
  priority

### Partitioned EDF Preemption Rule

Add:

```c
static void prvYieldForPartitionedEDFTask( const TCB_t * pxReadyTCB )
{
    BaseType_t xCoreID = pxReadyTCB->xAssignedCore;
    TCB_t * pxRunning = pxCurrentTCBs[ xCoreID ];

    if( ( pxRunning->uxTaskAttributes & taskATTRIBUTE_IS_IDLE ) != 0U )
    {
        prvYieldCore( xCoreID );
    }
    else if( pxReadyTCB->xAbsDeadline < pxRunning->xAbsDeadline )
    {
        prvYieldCore( xCoreID );
    }
}
```

Why:

- partitioned EDF only preempts within a partition

## Phase 8: Tick Accounting And Job Release

### File: `FreeRTOS-Kernel/tasks.c`

#### Section To Change

- `xTaskIncrementTick()`

This is the most important correctness change.

Current problem:

- the EDF accounting currently increments `pxCurrentTCB->xJobExecTicks`
- in SMP mode, `pxCurrentTCB` is just the TCB of the core that is currently
  executing the tick path
- therefore the non-tick core's currently running EDF task is not accounted for

That would undercount execution time and break MP EDF completely.

### Required MP EDF Behavior

When the global tick advances:

- every running realtime periodic task on every core must have its execution
  counter updated
- if a job completes its WCET budget or is dropped after a deadline miss, it
  must be moved to its next release

### Code Pattern To Add

```c
#if ( configUSE_MP == 1 )
for( BaseType_t xCoreID = 0; xCoreID < ( BaseType_t ) configNUMBER_OF_CORES; xCoreID++ )
{
    TCB_t * pxRunning = pxCurrentTCBs[ xCoreID ];

    if( prvTaskIsRealtimePeriodic( pxRunning ) != pdFALSE )
    {
        BaseType_t xStopCurrentJob = pdFALSE;
        pxRunning->xJobExecTicks++;

        if( pxRunning->xJobExecTicks >= pxRunning->xWcetTicks )
        {
            xStopCurrentJob = pdTRUE;
        }
        else if( prvTickTimeIsAfter( xConstTickCount, pxRunning->xAbsDeadline ) != pdFALSE )
        {
            traceTASK_DEADLINE_MISSED();
            xStopCurrentJob = pdTRUE;
        }

        if( xStopCurrentJob != pdFALSE )
        {
            const TickType_t xNextReleaseTime =
                prvPeriodicTaskNextReleaseAfter( pxRunning, xConstTickCount );

            prvPeriodicTaskAdvanceAndReinsert( pxRunning,
                                               &( pxRunning->xEDFTaskListItem ),
                                               &xEDFTaskRegistryList,
                                               xNextReleaseTime );

            /* move the task to delayed until next release */
            /* request reschedule on this core */
        }
    }
}
#endif
```

#### Additional Note

For partitioned EDF, the registry can still be global. The important thing is:

- ready queues are partitioned
- release/deadline metadata is still per task and can live in one registry

## Phase 9: Admission Control

### File: `FreeRTOS-Kernel/tasks.c`

#### Global EDF Admission Helper

Add:

```c
static BaseType_t prvGlobalEDFAdmissionWithNew( TickType_t xNewC,
                                                TickType_t xNewT )
{
    uint64_t ullTotalQ32 = 0ULL;
    uint64_t ullUMaxQ32 = 0ULL;
    uint64_t ullNewQ32 = ( ( uint64_t ) xNewC << 32U ) / ( uint64_t ) xNewT;

    ullTotalQ32 += ullNewQ32;
    ullUMaxQ32 = ullNewQ32;

    for( ListItem_t * pxItem = listGET_HEAD_ENTRY( &xEDFTaskRegistryList );
         pxItem != listGET_END_MARKER( &xEDFTaskRegistryList );
         pxItem = listGET_NEXT( pxItem ) )
    {
        TCB_t * pxTCB = ( TCB_t * ) listGET_LIST_ITEM_OWNER( pxItem );
        uint64_t ullTaskQ32 =
            ( ( uint64_t ) pxTCB->xWcetTicks << 32U ) / ( uint64_t ) pxTCB->xPeriodTicks;

        ullTotalQ32 += ullTaskQ32;

        if( ullTaskQ32 > ullUMaxQ32 )
        {
            ullUMaxQ32 = ullTaskQ32;
        }
    }

    return ( ullTotalQ32 <=
             ( ( ( uint64_t ) configNUMBER_OF_CORES << 32U ) -
               ( ( ( uint64_t ) configNUMBER_OF_CORES - 1ULL ) * ullUMaxQ32 ) ) ) ? pdTRUE : pdFALSE;
}
```

#### Why

- simple, sufficient, and defensible
- easy to explain in the report

### Partitioned EDF Assignment Helper

Add:

```c
static BaseType_t prvPartitionedAssignCoreFF( TickType_t xNewC,
                                              TickType_t xNewT,
                                              BaseType_t xRequestedCore )
{
    uint64_t ullTaskUtilQ32 =
        ( ( uint64_t ) xNewC << 32U ) / ( uint64_t ) xNewT;

    if( taskVALID_CORE_ID( xRequestedCore ) != pdFALSE )
    {
        if( ullPartitionUtilQ32[ xRequestedCore ] + ullTaskUtilQ32 <= ( 1ULL << 32U ) )
        {
            return xRequestedCore;
        }

        return -1;
    }

    for( BaseType_t xCoreID = 0; xCoreID < ( BaseType_t ) configNUMBER_OF_CORES; xCoreID++ )
    {
        if( ullPartitionUtilQ32[ xCoreID ] + ullTaskUtilQ32 <= ( 1ULL << 32U ) )
        {
            return xCoreID;
        }
    }

    return -1;
}
```

#### Why

- simple online first-fit
- directly compatible with dynamic task creation
- each partition only needs the implicit-deadline EDF utilization test

### Optional FFD Variant

If the implementation later wants a more classical offline packer:

- gather task descriptors before scheduler start
- sort by decreasing utilization
- assign using first-fit

For the current code base, online first-fit is the better initial choice.

## Phase 10: Task Creation Paths

### File: `FreeRTOS-Kernel/tasks.c`

#### Section To Change

- EDF `xTaskCreate()` path
- new MP EDF API implementations

### Global EDF Task Creation

Implement `xTaskCreateGlobalEDF()`:

1. convert period/WCET to ticks
2. validate `C > 0`, `T > 0`, and implicit deadline
3. suspend scheduler if already running
4. run `prvGlobalEDFAdmissionWithNew()`
5. create TCB using existing task creation path
6. set:
   - `xPeriodTicks`
   - `xWcetTicks`
   - `xRelDeadline = xPeriodTicks`
   - `xAbsDeadline = now + xPeriodTicks`
   - `xAssignedCore = -1`
   - `uxCoreAffinityMask = all cores`
7. add to ready list

Code skeleton:

```c
BaseType_t xTaskCreateGlobalEDF( ... )
{
    TickType_t xPeriodTicks = pdMS_TO_TICKS( ulPeriodMs );
    TickType_t xWcetTicks   = pdMS_TO_TICKS( ulWcetMs );

    if( prvGlobalEDFAdmissionWithNew( xWcetTicks, xPeriodTicks ) == pdFALSE )
    {
        return pdFAIL;
    }

    pxNewTCB->xAssignedCore = -1;
    pxNewTCB->uxCoreAffinityMask = ( ( 1U << configNUMBER_OF_CORES ) - 1U );
    pxNewTCB->xRelDeadline = xPeriodTicks;
    ...
}
```

### Partitioned EDF Task Creation

Implement `xTaskCreatePartitionedEDF()`:

1. convert period/WCET to ticks
2. run `prvPartitionedAssignCoreFF()`
3. reject if no core fits
4. create TCB
5. set:
   - `xAssignedCore = target core`
   - `uxCoreAffinityMask = 1 << target core`
   - `xRelDeadline = xPeriodTicks`
6. increment that partition's utilization
7. add to that core's EDF ready queue

Code skeleton:

```c
BaseType_t xTaskCreatePartitionedEDF( ... )
{
    BaseType_t xCoreID;
    uint64_t ullTaskUtilQ32;

    xCoreID = prvPartitionedAssignCoreFF( xWcetTicks, xPeriodTicks, xRequestedCore );

    if( xCoreID < 0 )
    {
        return pdFAIL;
    }

    pxNewTCB->xAssignedCore = xCoreID;
    pxNewTCB->uxCoreAffinityMask = ( ( UBaseType_t ) 1U << ( UBaseType_t ) xCoreID );

    ullTaskUtilQ32 = ( ( uint64_t ) xWcetTicks << 32U ) / ( uint64_t ) xPeriodTicks;
    ullPartitionUtilQ32[ xCoreID ] += ullTaskUtilQ32;
    ...
}
```

#### Why

This separates the APIs cleanly and makes tests explicit.

## Phase 11: Task Migration Support

### File: `FreeRTOS-Kernel/tasks.c`

#### New Function

```c
BaseType_t xTaskMigrateToCore( TaskHandle_t xTask,
                               BaseType_t xNewCore )
{
    TCB_t * pxTCB = prvGetTCBFromHandle( xTask );
    BaseType_t xOldCore = pxTCB->xAssignedCore;
    uint64_t ullTaskUtilQ32;

    if( taskVALID_CORE_ID( xNewCore ) == pdFALSE )
    {
        return pdFAIL;
    }

    ullTaskUtilQ32 =
        ( ( uint64_t ) pxTCB->xWcetTicks << 32U ) / ( uint64_t ) pxTCB->xPeriodTicks;

    taskENTER_CRITICAL();
    {
        if( ullPartitionUtilQ32[ xNewCore ] + ullTaskUtilQ32 > ( 1ULL << 32U ) )
        {
            taskEXIT_CRITICAL();
            return pdFAIL;
        }

        ullPartitionUtilQ32[ xOldCore ] -= ullTaskUtilQ32;
        ullPartitionUtilQ32[ xNewCore ] += ullTaskUtilQ32;

        pxTCB->xAssignedCore = xNewCore;
        pxTCB->xLastCore = xOldCore;
        vTaskCoreAffinitySet( xTask, ( ( UBaseType_t ) 1U << ( UBaseType_t ) xNewCore ) );
    }
    taskEXIT_CRITICAL();

    return pdPASS;
}
```

#### Why

This directly addresses the assignment requirement to change the processor upon
which a task is running.

## Phase 12: Delete And Cleanup Paths

### File: `FreeRTOS-Kernel/tasks.c`

#### Section To Change

- `vTaskDelete()`

#### Changes

When MP partitioned EDF is active and the task is periodic:

- subtract its utilization from `ullPartitionUtilQ32[xAssignedCore]`

Code pattern:

```c
#if ( configUSE_PARTITIONED_EDF_MP == 1 )
if( pxTCB->xPeriodTicks != ( TickType_t ) 0U )
{
    uint64_t ullTaskUtilQ32 =
        ( ( uint64_t ) pxTCB->xWcetTicks << 32U ) / ( uint64_t ) pxTCB->xPeriodTicks;

    if( taskVALID_CORE_ID( pxTCB->xAssignedCore ) != pdFALSE )
    {
        ullPartitionUtilQ32[ pxTCB->xAssignedCore ] -= ullTaskUtilQ32;
    }
}
#endif
```

#### Why

Partition load accounting must remain correct after deletes.

## Phase 13: Initialization

### File: `FreeRTOS-Kernel/tasks.c`

#### Section To Change

- `prvInitialiseTaskLists()`

#### Changes

Initialize partition-local EDF ready queues when partitioned EDF MP is enabled:

```c
#if ( configUSE_PARTITIONED_EDF_MP == 1 )
for( UBaseType_t uxCoreID = 0; uxCoreID < configNUMBER_OF_CORES; uxCoreID++ )
{
    vListInitialise( &xReadyPartitionedEDFTasksLists[ uxCoreID ] );
    ullPartitionUtilQ32[ uxCoreID ] = 0ULL;
}
#endif
```

#### Why

Partition queues are kernel structures and must be initialized alongside the
standard task lists.

## Phase 14: `vTaskSwitchContext()` Integration

### File: `FreeRTOS-Kernel/tasks.c`

#### Section To Change

- SMP `vTaskSwitchContext( BaseType_t xCoreID )`

#### Changes

Keep the existing low-level switch body, but ensure:

- it dispatches through the EDF-aware SMP selector
- the selected TCB for each core comes from the correct EDF policy

The good news is that most of the function can stay untouched if
`taskSELECT_HIGHEST_PRIORITY_TASK( xCoreID )` is routed to the new EDF-aware
selector in MP EDF mode.

#### Why

This minimizes intrusion and preserves the stability of the RP2040 SMP context
switch path.

## Phase 15: Testing Hooks And Instrumentation

### File: `task_trace.h`

#### Changes

Extend trace hooks to include core ID for MP tests.

Example:

```c
void vTraceTaskSwitchedInOnCore( uint32_t ulTaskCode,
                                 BaseType_t xCoreID );

#define traceTASK_SWITCHED_IN() \
    vTraceTaskSwitchedInOnCore( ( uint32_t ) ( uintptr_t ) pxCurrentTCB->pxTaskTag, \
                                portGET_CORE_ID() )
```

#### Why

For Task 4, proving that:

- two tasks run concurrently on different cores
- migration occurs in global EDF
- tasks stay pinned in partitioned EDF

is much easier if the trace explicitly captures the core.

### File: `task_trace.c`

#### Changes

- emit core-specific patterns or GPIO combinations
- optionally dedicate one GPIO to core identity

#### Why

This gives visible proof of multicore behavior during demo/testing.

## Phase 16: MP Tests

### File: `CMakeLists.txt`

#### Changes

Add:

```cmake
file(GLOB MP_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/mp_tests/*.c
)
```

and include `${MP_TEST_SOURCES}` in the executable sources.

### File: `main.c`

#### Changes

Add MP-mode test selection:

```c
#if ( configUSE_MP == 1 )
    #include "mp_tests/test_global_basic.h"
    #include "mp_tests/test_global_migration.h"
    #include "mp_tests/test_partitioned_basic.h"
    #include "mp_tests/test_partitioned_migrate.h"
#endif
```

### New Test Files To Add

#### `mp_tests/test_global_basic.c`

Goal:

- 3 implicit-deadline tasks on 2 cores
- verify that the two earliest deadlines run first

#### `mp_tests/test_global_migration.c`

Goal:

- create a preemption event where a preempted task resumes on the other core

#### `mp_tests/test_partitioned_basic.c`

Goal:

- create tasks that are assigned one per partition
- verify local EDF on each core

#### `mp_tests/test_partitioned_manual_migrate.c`

Goal:

- create a task on core 0
- invoke `xTaskMigrateToCore()`
- verify it now runs only on core 1

#### `mp_tests/test_mp_admission.c`

Goal:

- exercise both:
  - global EDF sufficient-bound rejection
  - partitioned EDF packing rejection

## File-By-File Summary

### Must Change

- `schedulingConfig.h`
- `FreeRTOSConfig.h`
- `FreeRTOS-Kernel/include/task.h`
- `FreeRTOS-Kernel/include/FreeRTOS.h`
- `FreeRTOS-Kernel/tasks.c`
- `task_trace.h`
- `task_trace.c`
- `main.c`
- `CMakeLists.txt`

### New Files

- `mp_tests/test_global_basic.c`
- `mp_tests/test_global_basic.h`
- `mp_tests/test_global_migration.c`
- `mp_tests/test_global_migration.h`
- `mp_tests/test_partitioned_basic.c`
- `mp_tests/test_partitioned_basic.h`
- `mp_tests/test_partitioned_manual_migrate.c`
- `mp_tests/test_partitioned_manual_migrate.h`
- `mp_tests/test_mp_admission.c`
- `mp_tests/test_mp_admission.h`

### Should Not Change For Task 4

- `FreeRTOS-Kernel/queue.c`
- `FreeRTOS-Kernel/list.c`

unless a small cleanup is needed incidentally.

## Recommended Implementation Order

1. Fix config guarding so MP EDF mode is representable.
2. Add TCB/static-TCB fields and MP EDF state.
3. Refactor EDF ready insertion into helper functions.
4. Implement partitioned EDF selector and ready queues first.
5. Implement global EDF selector and EDF-aware cross-core preemption.
6. Fix MP tick accounting for all running cores.
7. Add explicit MP EDF task creation APIs.
8. Add explicit partition migration API.
9. Add tests and trace extensions.
10. Write `design_MP.md`, `changes_MP.md`, and `testing_MP.md` from the final
    implementation.

## Final Recommendation

For the cleanest and most defensible implementation:

- build on top of the existing RP2040 SMP port
- keep one global tick owner
- use per-core reschedule interrupts already provided by the port
- use a single global ready queue for global EDF
- use one ready queue per core for partitioned EDF
- use explicit affinity and explicit migration APIs instead of trying to hide
  partition assignment implicitly
- use a sufficient bound for global EDF admission
- use first-fit plus per-core `U <= 1` for partitioned EDF

That design is aligned with the assignment, matches the existing code base, and
keeps the implementation scope reasonable.
