# CBS (Constant Bandwidth Server) Design Plan

## Overview
CBS allows aperiodic tasks to be scheduled alongside periodic EDF tasks using a reservation-based approach. Each CBS server reserves a fraction of CPU time (bandwidth) that can be used by aperiodic jobs.

## Key CBS Concepts

### Server Parameters
- **Qs (Capacity)**: Budget per period in ticks (e.g., 50ms budget per 100ms period)
- **Ts (Period)**: Server replenishment period in ticks (e.g., 100ms)
- **Ds (Deadline)**: Absolute deadline of current server job (dynamically adjusted)
- **cs (Remaining Budget)**: Budget remaining in current period

### CBS Scheduling Rules (from Liu chapter 7)
1. When a CBS job arrives and budget > 0: accept immediately with deadline = current_time + Ts
2. When server's budget exhausts: replenish at time = last_replenish_time + Ts with new deadline
3. If budget > 0 at replenishment, it carries over (to be simplified: no carryover for simplicity)
4. EDF schedules all (periodic + CBS) jobs; ties broken in favor of CBS

## Data Structure Design

### CBS_Server_t
Per-server structure (NOT embedded in TCB, kept separate):
```c
typedef struct xCBS_Server
{
    // Server parameters (static, set at creation)
    TickType_t xCapacityTicks;        // Qs: budget per period
    TickType_t xPeriodTicks;          // Ts: server period
    
    // Server state (dynamic, updated at runtime)
    TickType_t xAbsDeadline;          // Ds: absolute deadline
    TickType_t xRemainingBudget;      // cs: remaining budget in period
    TickType_t xLastReplenishTime;    // When budget was last replenished
    
    // Aperiodic job queue (list of tasks waiting on this server)
    List_t xPendingJobsList;          // List of aperiodic task handles waiting
    
    // Server identification
    UBaseType_t uxServerID;           // Unique server identifier
    char pcServerName[configMAX_TASK_NAME_LEN];
    
} CBS_Server_t;
```

### Link from task to server (minimal TCB extension)
For CBS-managed aperiodic tasks, add to TCB (guarded by configUSE_CBS):
```c
#if ( (configUSE_EDF == 1) && (configUSE_CBS == 1) )
    CBS_Server_t * pxCBSServer;       // NULL if not CBS-managed, else points to server
    UBaseType_t uxCBSJobID;           // Job counter within this CBS task
#endif
```

OR: Keep it entirely separate and use a mapping table (cleaner, no TCB pollution).

### Aperiodic Job Descriptor
When an aperiodic task wants service from CBS:
```c
typedef struct xCBS_Job
{
    TaskHandle_t xTaskHandle;         // The task that submitted this job
    TickType_t xArrivalTime;          // When the job arrived
    UBaseType_t uxJobID;              // Job sequence number
    ListItem_t xJobListItem;          // Node for pending jobs list
} CBS_Job_t;
```

## Configuration Guards

In schedulingConfig.h:
```c
#define configUSE_CBS 0

#if (configUSE_CBS == 1) && (configUSE_EDF != 1)
    #error "CBS requires EDF"
#endif

#ifndef configCBS_MAX_SERVERS
    #define configCBS_MAX_SERVERS 4U
#endif

#ifndef configCBS_MAX_PENDING_JOBS
    #define configCBS_MAX_PENDING_JOBS 32U
#endif
```

## API Functions (to be implemented)

### Server Management (kernel)
```c
CBS_Server_t * xCBSCreateServer(TickType_t xCapacity, TickType_t xPeriod, const char *pcName);
void vCBSDeleteServer(CBS_Server_t *pxServer);
BaseType_t xCBSSubmitJob(CBS_Server_t *pxServer, TaskHandle_t xTask);

// Admission control
BaseType_t xCBSAdmissionTest(CBS_Server_t *pxServer);

// Scheduler integration
void vCBSReplenishBudget(CBS_Server_t *pxServer);
CBS_Server_t * pxCBSGetNextJobServer(void);
```

### Task API (application)
```c
TaskHandle_t xTaskCreateWithCBS(
    TaskFunction_t pvTaskCode,
    const char *pcName,
    configSTACK_DEPTH_TYPE uxStackDepth,
    void *pvParameters,
    CBS_Server_t *pxServer
);
```

## Aperiodic Task Behavior

When an aperiodic task is created "with CBS":
1. It is managed by the specified CBS server
2. On each invocation, it submits a job to the server
3. The job waits in the server's pending queue
4. When scheduled, the job runs and consumes budget
5. If budget exhausts mid-job, deadline is postponed and budget replenished

For simplicity: assume one job per task at a time (new job not released until previous completes).

## Scheduler Integration Points

In vTaskSwitchContext() and xTaskIncrementTick():
- Check if running task is CBS-managed
- If so, decrement remaining budget
- If budget exhausts, call vCBSReplenishBudget()
- Recalculate deadlines for EDF ordering

## Testing Strategy

### Test Scenario 1: Single CBS with one aperiodic task
- Create CBS server: Qs=50, Ts=100
- Create aperiodic task, submit to server
- Verify it runs, deadline is set to current_time + Ts

### Test Scenario 2: Multiple aperiodic tasks on one server
- Same server, submit 3 aperiodic task jobs
- Verify they queue and run in order
- Check budget sharing

### Test Scenario 3: Mixed periodic + aperiodic
- Create 2 periodic EDF tasks
- Create 1 CBS server with 2 aperiodic task instances
- Verify EDF scheduling respects CBS deadlines
- Verify aperiodic jobs interleave correctly

### Test Scenario 4: Budget exhaustion and replenishment
- Create high-utilization aperiodic task that will exceed budget
- Verify deadline postponed, new budget allocated

## Notes on Simplifications

1. **No budget carryover**: Budget not carried over to next period
2. **Single job per task**: Task can't release next job until previous completes
3. **No polling period**: Assume CPU-intensive aperiodic tasks (no sleep/block)
4. **No complex admission**: Basic utilization test (sum Qs/Ts <= 1.0)

## Implementation Order

1. Add config guards to schedulingConfig.h
2. Create cbs.h with data type definitions
3. Create cbs.c with core functions (stub implementations)
4. Add CBS field to TCB (guarded)
5. Add CBS handling in scheduler tick/switch
6. Add test harness
7. Implement body of each function

---
