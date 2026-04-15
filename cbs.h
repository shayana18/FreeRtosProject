#ifndef INC_CBS_H
#define INC_CBS_H

#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h must appear in source files before include cbs.h"
#endif

#include "schedulingConfig.h"

#if ( configUSE_CBS == 1 )

    #if ( configUSE_EDF != 1 )
        #error "CBS requires EDF to be enabled (configUSE_EDF == 1)"
    #endif

    #include "list.h"

    /* *INDENT-OFF* */
    #ifdef __cplusplus
        extern "C" {
    #endif
    /* *INDENT-ON* */

    /**
     * CBS_Server_t
     * 
     * Represents a constant bandwidth server with a reserved capacity and period.
     * The server manages a queue of aperiodic tasks, scheduling them according to CBS rules.
     * 
     * Key fields:
     * - xCapacityTicks: CPU budget per period (Qs)
     * - xPeriodTicks: Server replenishment period (Ts)
     * - xAbsDeadline: Absolute deadline of current server job (Ds)
     * - xRemainingBudget: Budget available in current period (cs)
     * - xLastReplenishTime: Tick count when budget was last replenished
     * - xPendingJobsList: Queue of aperiodic task handles waiting for service
     * - uxServerID: Unique server identifier
     */
    typedef struct xCBS_Server
    {
        /* Static server parameters (set at creation time) */
        TickType_t xCapacityTicks;          /**< Qs: CPU budget per period (ticks) */
        TickType_t xPeriodTicks;            /**< Ts: server period / replenishment interval (ticks) */

        /* Dynamic server state (updated at runtime) */
        TickType_t xAbsDeadline;            /**< Ds: absolute deadline of current job */
        TickType_t xRemainingBudget;        /**< cs: remaining budget in current period */
        TickType_t xLastReplenishTime;      /**< Tick count when budget was last replenished */

        /* Aperiodic job queue */
        List_t xPendingJobsList;            /**< Queue of aperiodic task handles awaiting service */

        /* Server identification and debugging */
        UBaseType_t uxServerID;             /**< Unique server identifier for tracking/debugging */
        char pcServerName[ configMAX_TASK_NAME_LEN ];  /**< Descriptive server name */

        /* Admission control and statistics */
        UBaseType_t uxTotalJobsSubmitted;   /**< Lifetime count of jobs submitted to this server */
        UBaseType_t uxTotalJobsCompleted;   /**< Lifetime count of completed jobs */

    } CBS_Server_t;

    /**
     * CBS_Job_t
     * 
     * Represents an aperiodic job submitted to a CBS server.
     * Tracks the task, arrival time, and position in the server's job queue.
     */
    typedef struct xCBS_Job
    {
        TaskHandle_t xTaskHandle;           /**< Task handle for the aperiodic task */
        TickType_t xArrivalTime;            /**< Tick count when this job was submitted */
        UBaseType_t uxJobID;                /**< Sequential job ID within this task */
        ListItem_t xJobListItem;            /**< List node for pending jobs queue */

    } CBS_Job_t;

    /**
     * Callback hook for CBS job completion.
     * Called by kernel when a CBS job completes or is evicted.
     */
    typedef void (*vCBSJobCompletionHook_t)( TaskHandle_t xTask, CBS_Server_t *pxServer );

    /* ============================================================================
     * CBS Kernel-Level API Functions (to be implemented in cbs.c)
     * ============================================================================ */

    /**
     * Create a new constant bandwidth server.
     * 
     * @param xCapacityTicks  CPU budget per period (Qs) in ticks
     * @param xPeriodTicks    Replenishment period (Ts) in ticks
     * @param pcServerName    Descriptive name for debugging
     * 
     * @return Pointer to new CBS_Server_t on success, NULL on failure
     */
    CBS_Server_t * xCBSServerCreate(
        TickType_t xCapacityTicks,
        TickType_t xPeriodTicks,
        const char *pcServerName
    );

    /**
     * Delete a CBS server and free its resources.
     * 
     * @param pxServer  Pointer to server to delete
     */
    void vCBSServerDelete( CBS_Server_t *pxServer );

    /**
     * Submit an aperiodic task to a CBS server for scheduling.
     * 
     * @param pxServer  CBS server to submit to
     * @param xTask     Task handle of aperiodic task
     * 
     * @return pdTRUE if job submitted successfully, pdFALSE if rejected
     */
    BaseType_t xCBSSubmitJob(
        CBS_Server_t *pxServer,
        TaskHandle_t xTask
    );

    /**
     * Retrieve the next aperiodic task to run from the server's pending queue.
     * Called by scheduler when current CBS job completes.
     * 
     * @param pxServer  CBS server to query
     * 
     * @return Next task handle from pending queue, NULL if none
     */
    TaskHandle_t xCBSGetNextPendingJob( CBS_Server_t *pxServer );

    /**
     * Consume budget from a CBS server due to CPU utilization.
     * Called by scheduler tick handler while a CBS task is running.
     * 
     * @param pxServer      CBS server managing the current task
     * @param ulTicksUsed   Number of ticks of budget to consume
     * 
     * @return pdTRUE if budget remains, pdFALSE if exhausted (desktop postponement needed)
     */
    BaseType_t xCBSConsumeBudget(
        CBS_Server_t *pxServer,
        TickType_t ulTicksUsed
    );

    /**
     * Replenish a CBS server's budget at the start of a new period.
     * Called by scheduler when xLastReplenishTime + xPeriodTicks is reached.
     * Updates xRemainingBudget and xAbsDeadline.
     * 
     * @param pxServer  CBS server to replenish
     */
    void vCBSReplenishBudget( CBS_Server_t *pxServer );

    /**
     * Check if a CBS server's budget for the current period has been exhausted.
     * 
     * @param pxServer  CBS server to check
     * 
     * @return pdTRUE if budget exhausted, pdFALSE if budget remains
     */
    BaseType_t xCBSIsBudgetExhausted( CBS_Server_t *pxServer );

    /**
     * Perform admission control test for CBS + existing EDF periodic tasks.
     * Determines if accepting a new CBS server would maintain schedulability.
     * 
     * @param xCapacityTicks  Capacity to test (Qs)
     * @param xPeriodTicks    Period to test (Ts)
     * 
     * @return pdTRUE if admissible, pdFALSE otherwise
     */
    BaseType_t xCBSAdmissionTest(
        TickType_t xCapacityTicks,
        TickType_t xPeriodTicks
    );

    /**
     * Get current CBS server utilization (Qs / Ts).
     * 
     * @param pxServer  CBS server to query
     * 
     * @return Utilization as fixed-point (e.g., 500 = 0.5 = 50%)
     */
    UBaseType_t uxCBSServerUtilization( CBS_Server_t *pxServer );

    /* ============================================================================
     * CBS Scheduler Integration Functions (called by kernel scheduler)
     * ============================================================================ */

    /**
     * Initialize CBS subsystem. Called once during kernel startup.
     */
    void vCBSInit( void );

    /**
     * Deinitialize CBS subsystem. Called during kernel shutdown.
     */
    void vCBSDeinit( void );

    /**
     * Called by scheduler on each tick while a CBS task is running.
     * Updates budget consumption and checks for budget exhaustion.
     */
    void vCBSTickUpdate( void );

    /**
     * Get pointer to currently active CBS server (if any).
     * Used by scheduler to track which server's budget to decrement.
     * 
     * @return Pointer to active CBS_Server_t, NULL if no CBS task running
     */
    CBS_Server_t * pxCBSGetActiveServer( void );

    /**
     * Determine if a given task is CBS-managed (vs. periodic EDF).
     * 
     * @param xTask  Task to query
     * 
     * @return pdTRUE if CBS-managed, pdFALSE otherwise
     */
    BaseType_t xCBSIsTaskManaged( TaskHandle_t xTask );

    #if ( configUSE_SRP == 0 )
        BaseType_t xTaskCreateCBS( TaskFunction_t pxTaskCode,
                                   const char * const pcName,
                                   const configSTACK_DEPTH_TYPE uxStackDepth,
                                   void * const pvParameters,
                                   TaskHandle_t * const pxCreatedTask,
                                   uint32_t ulServerPeriodMs,
                                   uint32_t ulServerBudgetMs,
                                   uint32_t ulRelDeadlineMs,
                                   CBS_Server_t * pxServer );
    #endif

    /* *INDENT-OFF* */
    #ifdef __cplusplus
        }
    #endif
    /* *INDENT-ON* */

#endif /* #if ( configUSE_CBS == 1 ) */

#endif /* INC_CBS_H */
