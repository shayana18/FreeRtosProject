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

    #ifndef CBS_SERVER_INTEGRITY_TAG
        #define CBS_SERVER_INTEGRITY_TAG    ( ( UBaseType_t ) 0xC0B5007u )
    #endif

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

        /* Server identification and debugging */
        UBaseType_t uxServerID;             /**< Unique server identifier for tracking/debugging */
        char pcServerName[ configMAX_TASK_NAME_LEN ];  /**< Descriptive server name */
        TaskHandle_t xWorkerTaskHandle;     /**< CBS worker task currently bound to this server. */

        /* Admission control and statistics */
        UBaseType_t uxTotalJobsSubmitted;   /**< Lifetime count of jobs submitted to this server */
        UBaseType_t uxTotalJobsCompleted;   /**< Lifetime count of completed jobs */
        BaseType_t xJobRunning;             /**< pdTRUE iff one job is currently running on this server */
        UBaseType_t uxIntegrityTag;         /**< Integrity tag used to detect server memory corruption. */

    } CBS_Server_t;

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
     * Mark the current CBS-managed job as complete.
     *
     * Workers should call this after finishing their work so the same task can
     * accept the next submitted job.
     */
    BaseType_t xCBSCompleteJob( void );

    /**
     * Block inside a CBS-managed worker routine until a submitted job arrives.
     *
     * @param xTicksToWait  Maximum time to wait for a pending CBS job
     *
     * @return pdTRUE if a job was received, pdFALSE on timeout
     */
    BaseType_t xCBSWaitForJob( TickType_t xTicksToWait );

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
     * Determine if a given task is CBS-managed (vs. periodic EDF).
     * 
     * @param xTask  Task to query
     * 
     * @return pdTRUE if CBS-managed, pdFALSE otherwise
     */
    BaseType_t xCBSIsTaskManaged( TaskHandle_t xTask );

    #if ( configUSE_SRP == 0 )
        /**
         * Create a CBS-managed worker bound to an existing server.
         *
         * This avoids repeating server period/budget at each task creation call.
         * The worker's EDF timing parameters are derived from the server.
         */
        BaseType_t xTaskCreateCBSWorker( TaskFunction_t pxTaskCode,
                                         const char * const pcName,
                                         const configSTACK_DEPTH_TYPE uxStackDepth,
                                         void * const pvParameters,
                                         TaskHandle_t * const pxCreatedTask,
                                         CBS_Server_t * pxServer );

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
