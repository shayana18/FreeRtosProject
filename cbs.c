/*
 * FreeRTOS Constant Bandwidth Server (CBS) Implementation
 * 
 * Implements reservation-based dynamic server scheduling for aperiodic tasks
 * on top of EDF scheduling. CBS allows soft real-time aperiodic tasks to coexist
 * with hard real-time periodic EDF tasks.
 */

#include "schedulingConfig.h"

#if ( configUSE_CBS == 1 )

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cbs.h"

/* ============================================================================
 * CBS Global State
 * ============================================================================ */

/* Array of active CBS servers managed by the system */
static CBS_Server_t *pxCBSServers[ configCBS_MAX_SERVERS ];
static UBaseType_t uxCBSServerCount = 0;

static BaseType_t xCBSInitialised = pdFALSE;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static void prvCBSEnsureInitialised( void )
{
    if( xCBSInitialised == pdFALSE )
    {
        vCBSInit();
    }
}

static BaseType_t prvCBSServerIsValid( const CBS_Server_t * pxServer )
{
    return ( ( pxServer != NULL ) && ( pxServer->uxIntegrityTag == CBS_SERVER_INTEGRITY_TAG ) ) ? pdTRUE : pdFALSE;
}

static BaseType_t prvCBSShouldResetOnIdleArrival( CBS_Server_t * pxServer,
                                                  TickType_t xArrivalTime )
{
    const TickType_t xDeadline = pxServer->xAbsDeadline;

    if( xArrivalTime >= xDeadline )
    {
        return pdTRUE;
    }

    {
        const TickType_t xDelta = xDeadline - xArrivalTime;
        const uint64_t ullLeft = ( uint64_t ) pxServer->xRemainingBudget * ( uint64_t ) pxServer->xPeriodTicks;
        const uint64_t ullRight = ( uint64_t ) xDelta * ( uint64_t ) pxServer->xCapacityTicks;

        return ( ullLeft >= ullRight ) ? pdTRUE : pdFALSE;
    }
}

/* ============================================================================
 * CBS Server Management
 * ============================================================================ */

CBS_Server_t * xCBSServerCreate(
    TickType_t xCapacityTicks,
    TickType_t xPeriodTicks,
    const char *pcServerName
)
{
    prvCBSEnsureInitialised();

    /* Reject if capacity >= period (invalid CBS parameters) */
    if( ( xCapacityTicks == 0 ) || ( xPeriodTicks == 0 ) || 
        ( xCapacityTicks > xPeriodTicks ) )
    {
        return NULL;
    }

    /* Check if server limit reached */
    if( uxCBSServerCount >= configCBS_MAX_SERVERS )
    {
        return NULL;
    }

    if( xCBSAdmissionTest( xCapacityTicks, xPeriodTicks ) == pdFALSE )
    {
        return NULL;
    }

    /* Allocate server struct */
    CBS_Server_t *pxNewServer = (CBS_Server_t *)pvPortMalloc( sizeof( CBS_Server_t ) );
    if( pxNewServer == NULL )
    {
        return NULL;
    }

    ( void ) memset( pxNewServer, 0, sizeof( CBS_Server_t ) );

    /* Initialize server state */
    pxNewServer->xCapacityTicks = xCapacityTicks;
    pxNewServer->xPeriodTicks = xPeriodTicks;
    pxNewServer->xAbsDeadline = xTaskGetTickCount() + xPeriodTicks;
    pxNewServer->xRemainingBudget = xCapacityTicks;
    pxNewServer->uxServerID = uxCBSServerCount;
    pxNewServer->xJobRunning = pdFALSE;
    pxNewServer->uxIntegrityTag = CBS_SERVER_INTEGRITY_TAG;

    /* Copy server name */
    if( pcServerName != NULL )
    {
        strncpy( pxNewServer->pcServerName, pcServerName, configMAX_TASK_NAME_LEN - 1 );
        pxNewServer->pcServerName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';
    }
    else
    {
        snprintf( pxNewServer->pcServerName, configMAX_TASK_NAME_LEN, "CBS_%u", pxNewServer->uxServerID );
    }

    /* Add to active servers list */
    pxCBSServers[ uxCBSServerCount ] = pxNewServer;
    uxCBSServerCount++;

    return pxNewServer;
}

/* ============================================================================
 * CBS Job Management
 * ============================================================================ */

BaseType_t xCBSSubmitJob(
    CBS_Server_t *pxServer,
    TaskHandle_t xTask
)
{
    TickType_t xArrivalTime;
    BaseType_t xDeadlineUpdated = pdFALSE;

    prvCBSEnsureInitialised();

    if( ( prvCBSServerIsValid( pxServer ) == pdFALSE ) || ( xTask == NULL ) )
    {
        return pdFALSE;
    }

    if( xTaskCBSHasOutstandingJob( xTask ) != pdFALSE )
    {
        return pdFAIL;
    }

    if( pxServer->xJobRunning != pdFALSE )
    {
        /* Recover from a stale running flag if no job is outstanding for
         * this worker. */
        if( xTaskCBSHasOutstandingJob( xTask ) == pdFALSE )
        {
            pxServer->xJobRunning = pdFALSE;
        }
        else
        {
            return pdFAIL;
        }
    }

    // Ensure that the task is subscribed to this sever. 
    if( pxTaskCBSGetServer( xTask ) != pxServer )
    {
        return pdFAIL;
    }

    if( ( pxServer->xWorkerTaskHandle != NULL ) && ( pxServer->xWorkerTaskHandle != xTask ) )
    {
        return pdFAIL;
    }

    xArrivalTime = xTaskGetTickCount();

    if( prvCBSShouldResetOnIdleArrival( pxServer, xArrivalTime ) != pdFALSE )
    {
        // when the server is sitting idle and gets a job all of a sudden 
        pxServer->xRemainingBudget = pxServer->xCapacityTicks;
        pxServer->xAbsDeadline = xArrivalTime + pxServer->xPeriodTicks;
        xDeadlineUpdated = pdTRUE;
    }

    if( ( pxServer->xRemainingBudget == ( TickType_t ) 0U ) ||
        ( xArrivalTime >= pxServer->xAbsDeadline ) )
    {
        vCBSReplenishBudget( pxServer );
        xDeadlineUpdated = pdTRUE;
    }

    if( xDeadlineUpdated != pdFALSE )
    {
        ( void ) xTaskCBSUpdateDeadline( xTask, pxServer->xAbsDeadline );
    }

    if( xTaskCBSSetOutstandingJob( xTask, pdTRUE ) != pdPASS )
    {
        return pdFAIL;
    }

    pxServer->xJobRunning = pdTRUE;

    /* Dispatch immediately by notifying the worker to run one job. */
    ( void ) xTaskNotifyGive( xTask );

    return pdPASS;
}

BaseType_t xCBSWaitForJob( TickType_t xTicksToWait )
{
    // how an a periodic job can "submit" a job to the server so the server just waits until a job notifies it. 
    return ( ulTaskNotifyTake( pdTRUE, xTicksToWait ) > 0U ) ? pdTRUE : pdFALSE;
}

BaseType_t xCBSCompleteJob( void )
{
    TaskHandle_t xTask = xTaskGetCurrentTaskHandle();
    CBS_Server_t * pxServer = pxTaskCBSGetServer( xTask );

    if( prvCBSServerIsValid( pxServer ) == pdFALSE )
    {
        return pdFAIL;
    }

    if( xTaskCBSHasOutstandingJob( xTask ) == pdFALSE )
    {
        /* Keep server/task state consistent even if completion arrives with a
         * stale outstanding flag. */
        pxServer->xJobRunning = pdFALSE;
        return pdFAIL;
    }

    ( void ) xTaskCBSSetOutstandingJob( xTask, pdFALSE );
    pxServer->xJobRunning = pdFALSE;

    return pdPASS;
}

/* ============================================================================
 * CBS Budget Management
 * ============================================================================ */

void vCBSReplenishBudget( CBS_Server_t *pxServer )
{
    if( pxServer == NULL )
    {
        return;
    }

    configASSERT( pxServer->uxIntegrityTag == CBS_SERVER_INTEGRITY_TAG );

    /* Replenish budget and update deadline */
    pxServer->xRemainingBudget = pxServer->xCapacityTicks;
    pxServer->xAbsDeadline = pxServer->xAbsDeadline + pxServer->xPeriodTicks;
}

UBaseType_t uxCBSServerUtilization( CBS_Server_t *pxServer )
{
    if( pxServer == NULL || pxServer->xPeriodTicks == 0 )
    {
        return 0;
    }

    /* Return utilization as fixed-point (e.g., 500 = 50%) */
    return ( pxServer->xCapacityTicks * 1000 ) / pxServer->xPeriodTicks;
}

/* ============================================================================
 * CBS Admission Control
 * ============================================================================ */

BaseType_t xCBSAdmissionTest(
    TickType_t xCapacityTicks,
    TickType_t xPeriodTicks
)
{
    /* Stub: simple utilization sum <= 1.0 test
     * To be enhanced with deadline-based tests */

    TickType_t ulTotalUtilization = 0;

    /* Sum existing CBS utilizations */
    for( UBaseType_t ux = 0; ux < uxCBSServerCount; ux++ )
    {
        ulTotalUtilization += uxCBSServerUtilization( pxCBSServers[ ux ] );
    }

    /* Add new server's utilization */
    ulTotalUtilization += ( xCapacityTicks * 1000 ) / xPeriodTicks;

    /* Accept if total <= 100% (1000 in fixed-point) */
    return ( ulTotalUtilization <= 1000 ) ? pdTRUE : pdFALSE;
}

/* ============================================================================
 * CBS Scheduler Integration
 * ============================================================================ */

void vCBSInit( void )
{
    uxCBSServerCount = 0;
    xCBSInitialised = pdTRUE;

    memset( pxCBSServers, 0, sizeof( pxCBSServers ) );
}

#if ( configUSE_SRP == 0 )

BaseType_t xTaskCreateCBSWorker( TaskFunction_t pxTaskCode,
                                 const char * const pcName,
                                 const configSTACK_DEPTH_TYPE uxStackDepth,
                                 void * const pvParameters,
                                 TaskHandle_t * const pxCreatedTask,
                                 CBS_Server_t * pxServer )
{
    TickType_t xPeriodTicks;
    TickType_t xBudgetTicks;
    uint32_t ulPeriodMs;
    uint32_t ulBudgetMs;

    if( ( pxServer == NULL ) || ( pxCreatedTask == NULL ) )
    {
        return pdFAIL;
    }

    xPeriodTicks = pxServer->xPeriodTicks;
    xBudgetTicks = pxServer->xCapacityTicks;
    ulPeriodMs = ( uint32_t ) ( xPeriodTicks * portTICK_PERIOD_MS );
    ulBudgetMs = ( uint32_t ) ( xBudgetTicks * portTICK_PERIOD_MS );

    return xTaskCreateCBS( pxTaskCode,
                           pcName,
                           uxStackDepth,
                           pvParameters,
                           pxCreatedTask,
                           ulPeriodMs,
                           ulBudgetMs,
                           ulPeriodMs,
                           pxServer );
}

BaseType_t xTaskCreateCBS( TaskFunction_t pxTaskCode,
                           const char * const pcName,
                           const configSTACK_DEPTH_TYPE uxStackDepth,
                           void * const pvParameters,
                           TaskHandle_t * const pxCreatedTask,
                           uint32_t ulServerPeriodMs,
                           uint32_t ulServerBudgetMs,
                           uint32_t ulRelDeadlineMs,
                           CBS_Server_t * pxServer )
{
    BaseType_t xReturn;

    if( ( pxCreatedTask == NULL ) || ( pxServer == NULL ) )
    {
        return pdFAIL;
    }

    if( ( pdMS_TO_TICKS( ulServerPeriodMs ) != pxServer->xPeriodTicks ) ||
        ( pdMS_TO_TICKS( ulServerBudgetMs ) != pxServer->xCapacityTicks ) )
    {
        return pdFAIL;
    }

    if( pdMS_TO_TICKS( ulRelDeadlineMs ) > pdMS_TO_TICKS( ulServerPeriodMs ) )
    {
        return pdFAIL;
    }

    xReturn = xTaskCreate( pxTaskCode,
                           pcName,
                           uxStackDepth,
                           pvParameters,
                           pxCreatedTask,
                           ulServerPeriodMs,
                           ulServerBudgetMs,
                           ulRelDeadlineMs );

    if( xReturn != pdPASS )
    {
        return xReturn;
    }

    if( xTaskCBSBindToServer( *pxCreatedTask, pxServer ) != pdPASS )
    {
        vTaskDelete( *pxCreatedTask );
        *pxCreatedTask = NULL;
        return pdFAIL;
    }

    return pdPASS;
}

#endif /* configUSE_SRP == 0 */

#endif /* #if ( configUSE_CBS == 1 ) */
