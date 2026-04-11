# CBS Implementation Summary

## Files Created/Modified

### 1. **cbs.h** (NEW)
Complete header file defining CBS data types and API.

**Key Data Types:**
- `CBS_Server_t`: Each server tracks capacity, period, absolute deadline, remaining budget, and a queue of pending aperiodic tasks
- `CBS_Job_t`: Represents an aperiodic job waiting for service from a CBS server

**API Functions (stubs implemented):**
- Server management: `xCBSServerCreate()`, `vCBSServerDelete()`
- Job submission: `xCBSSubmitJob()`, `xCBSGetNextPendingJob()`
- Budget management: `xCBSConsumeBudget()`, `vCBSReplenishBudget()`, `xCBSIsBudgetExhausted()`
- Admission control: `xCBSAdmissionTest()`
- Scheduler integration: `vCBSInit()`, `vCBSDeinit()`, `vCBSTickUpdate()`

### 2. **cbs.c** (NEW)
Implementation of CBS subsystem with stub implementations.

**Global State:**
- Array of active CBS servers (`pxCBSServers[]`)
- Global pool of job descriptors (`xCBSJobPool[]`)
- Currently active server pointer (`pxActiveCBSServer`)

**Implemented Functions:**
- Server allocation/deallocation with ID tracking
- Job pool management (simple allocate/release)
- Basic budget replenishment logic
- Utilization calculation
- Simple admission control (sum of utilizations ≤ 100%)

### 3. **schedulingConfig.h** (MODIFIED)
Added CBS configuration guards:
- `configUSE_CBS`: Main enable/disable flag (gated by EDF requirement)
- `configCBS_MAX_SERVERS`: Max concurrent servers (default 4)
- `configCBS_MAX_PENDING_JOBS`: Max pending job descriptors (default 32)
- `configCBS_ALLOW_BUDGET_CARRYOVER`: Carryover flag (default disabled)

### 4. **FreeRTOS-Kernel/tasks.c** (MODIFIED)
Added minimal CBS fields to TCB for aperiodic task tracking:
```c
#if ( (configUSE_EDF == 1) && (configUSE_CBS == 1) )
    void * pxCBSServer;        // Link to CBS server
    UBaseType_t uxCBSJobID;    // Job sequence counter
#endif
```

### 5. **CMakeLists.txt** (MODIFIED)
Added `cbs.c` to build sources so CBS code is compiled.

## Design Decisions

### Separate Server Object (Not Embedded in TCB)
- Rationale: CBS is a scheduling policy, not core task state. Keeps TCB uncluttered.
- Each server is independently allocated and can manage multiple aperiodic tasks.
- Task links to server via pointer, not vice versa.

### Global Job Pool Pattern
- Simple allocate/free from fixed-size pool (no recycle for now).
- Jobs are descriptors that track task handle, arrival time, and queue node.
- Matches FreeRTOS pattern of pre-allocated structures for RTOS safety.

### Config Guards Follow EDF/SRP Convention
- All CBS code/structs guarded by `configUSE_CBS` which requires `configUSE_EDF == 1`.
- Mirrors existing EDF/SRP guard hierarchy.
- Allows coexistence with or without CBS in same kernel build.

### Simple Initial Admission Test
- Utilization-based check: Sum(Qs/Ts) ≤ 1.0.
- Can be enhanced later with deadline analysis if needed.

## How Aperiodic Tasks Will Work

1. **Task Creation**: Developer creates task managed by CBS server (via new API)
2. **Job Submission**: Task calls `xCBSSubmitJob(server, task_handle)` to submit itself
3. **Scheduling**: Kernel's EDF scheduler picks next job based on absolute deadline
4. **Execution**: When scheduled, CBS server's budget decrements each tick
5. **Budget Exhaustion**: When budget hits zero, deadline postponed to (current_time + Ts)
6. **Replenishment**: At start of next period, budget refills to Qs

## Testing Hooks (Next Steps)

Before implementing full body:
1. Create `cbs_tests/` directory with test cases
2. Test case 1: Single server, single aperiodic task
3. Test case 2: Single server, multiple aperiodic tasks
4. Test case 3: Mixed periodic (EDF) + aperiodic (CBS)
5. Test case 4: Budget exhaustion and deadline replenishment

## Summary of Data Types

| Type | Size | Purpose |
|------|------|---------|
| `CBS_Server_t` | ~48 bytes | Per-server state and budget tracking |
| `CBS_Job_t` | ~32 bytes | Per-job descriptor for pending queue |
| TCB extension | ~8 bytes | Pointer to server + job counter |

**Total per server**: ~100 bytes overhead (struct + list management)
**Total per job**: ~32 bytes (global pool shared)
**Example: 4 servers × 32 jobs**: 4×100 + 128×32 ≈ 4.5 KB RAM

---

**Next**: Implement scheduler integration hooks and test harness.
