/*
 * FreeRTOS Constant Bandwidth Server (CBS) Implementation
 * 
 * Implements reservation-based dynamic server scheduling for aperiodic tasks
 * on top of EDF scheduling. CBS allows soft real-time aperiodic tasks to coexist
 * with hard real-time periodic EDF tasks.
 */

#include "schedulingConfig.h"

#if ( configUSE_CBS == 1 )

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

/* Global job pool for aperiodic jobs */
static CBS_Job_t xCBSJobPool[ configCBS_MAX_PENDING_JOBS ];

/* Pointer to currently active CBS server (for tick accounting) */
static CBS_Server_t *pxActiveCBSServer = NULL;
static BaseType_t xCBSInitialised = pdFALSE;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Find a CBS server by ID.
 */
static CBS_Server_t * prvCBSFindServerByID( UBaseType_t uxServerID )
{
    for( UBaseType_t ux = 0; ux < uxCBSServerCount; ux++ )
    {
        if( pxCBSServers[ ux ]->uxServerID == uxServerID )
        {
            return pxCBSServers[ ux ];
        }
    }
    return NULL;
}

/**
 * Allocate a job from the global pool.
 */
static CBS_Job_t * prvCBSAllocateJob( void )
{
    for( UBaseType_t ux = 0; ux < ( UBaseType_t ) configCBS_MAX_PENDING_JOBS; ux++ )
    {
        if( xCBSJobPool[ ux ].xTaskHandle == NULL )
        {
            return &xCBSJobPool[ ux ];
        }
    }

    return NULL;
}

/**
 * Release a job back to the pool.
 */
static void prvCBSReleaseJob( CBS_Job_t *pxJob )
{
    if( pxJob != NULL )
    {
        ( void ) memset( pxJob, 0, sizeof( *pxJob ) );
    }
}

static void prvCBSEnsureInitialised( void )
{
    if( xCBSInitialised == pdFALSE )
    {
        vCBSInit();
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

    /* Allocate server struct */
    CBS_Server_t *pxNewServer = (CBS_Server_t *)pvPortMalloc( sizeof( CBS_Server_t ) );
    if( pxNewServer == NULL )
    {
        return NULL;
    }

    /* Initialize server state */
    pxNewServer->xCapacityTicks = xCapacityTicks;
    pxNewServer->xPeriodTicks = xPeriodTicks;
    pxNewServer->xAbsDeadline = xTaskGetTickCount() + xPeriodTicks;
    pxNewServer->xRemainingBudget = xCapacityTicks;
    pxNewServer->xLastReplenishTime = xTaskGetTickCount();
    pxNewServer->uxServerID = uxCBSServerCount;
    pxNewServer->uxTotalJobsSubmitted = 0;
    pxNewServer->uxTotalJobsCompleted = 0;

    /* Initialize pending jobs list */
    vListInitialise( &pxNewServer->xPendingJobsList );

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

void vCBSServerDelete( CBS_Server_t *pxServer )
{
    if( pxServer == NULL )
    {
        return;
    }

    /* Remove from active servers list */
    for( UBaseType_t ux = 0; ux < uxCBSServerCount; ux++ )
    {
        if( pxCBSServers[ ux ] == pxServer )
        {
            /* Shift remaining servers down */
            for( UBaseType_t uy = ux; uy < ( uxCBSServerCount - 1 ); uy++ )
            {
                pxCBSServers[ uy ] = pxCBSServers[ uy + 1 ];
            }
            uxCBSServerCount--;
            break;
        }
    }

    /* Free pending jobs from this server */
    while( listCURRENT_LIST_LENGTH( &pxServer->xPendingJobsList ) > 0 )
    {
        ListItem_t *pxItem = listGET_HEAD_ENTRY( &pxServer->xPendingJobsList );
        CBS_Job_t *pxJob = ( CBS_Job_t * ) listGET_LIST_ITEM_OWNER( pxItem );
        uxListRemove( pxItem );
        prvCBSReleaseJob( pxJob );
    }

    if( pxActiveCBSServer == pxServer )
    {
        pxActiveCBSServer = NULL;
    }

    vPortFree( pxServer );
}

/* ============================================================================
 * CBS Job Management
 * ============================================================================ */

BaseType_t xCBSSubmitJob(
    CBS_Server_t *pxServer,
    TaskHandle_t xTask
)
{
    prvCBSEnsureInitialised();

    if( pxServer == NULL || xTask == NULL )
    {
        return pdFALSE;
    }

    if( ( pxServer->xRemainingBudget == ( TickType_t ) 0U ) ||
        ( xTaskGetTickCount() >= pxServer->xAbsDeadline ) )
    {
        vCBSReplenishBudget( pxServer );
    }

    /* Allocate a job descriptor */
    CBS_Job_t *pxJob = prvCBSAllocateJob();
    if( pxJob == NULL )
    {
        return pdFALSE;
    }

    /* Initialize job */
    pxJob->xTaskHandle = xTask;
    pxJob->xArrivalTime = xTaskGetTickCount();
    pxJob->uxJobID = pxServer->uxTotalJobsSubmitted++;
    listSET_LIST_ITEM_OWNER( &pxJob->xJobListItem, pxJob );
    listSET_LIST_ITEM_VALUE( &pxJob->xJobListItem, pxJob->xArrivalTime );

    /* Add job to server's pending queue */
    vListInsertEnd( &pxServer->xPendingJobsList, &pxJob->xJobListItem );

    return pdTRUE;
}

TaskHandle_t xCBSGetNextPendingJob( CBS_Server_t *pxServer )
{
    if( pxServer == NULL )
    {
        return NULL;
    }

    if( listCURRENT_LIST_LENGTH( &pxServer->xPendingJobsList ) == 0 )
    {
        return NULL;
    }

    /* Get first pending job */
    ListItem_t *pxItem = listGET_HEAD_ENTRY( &pxServer->xPendingJobsList );
    CBS_Job_t *pxJob = (CBS_Job_t *)listGET_LIST_ITEM_OWNER( pxItem );

    /* Remove from queue */
    uxListRemove( pxItem );

    TaskHandle_t xTask = pxJob->xTaskHandle;
    prvCBSReleaseJob( pxJob );

    return xTask;
}

/* ============================================================================
 * CBS Budget Management
 * ============================================================================ */

BaseType_t xCBSConsumeBudget(
    CBS_Server_t *pxServer,
    TickType_t ulTicksUsed
)
{
    if( pxServer == NULL )
    {
        return pdFALSE;
    }

    /* Check if budget is sufficient */
    if( pxServer->xRemainingBudget < ulTicksUsed )
    {
        /* Budget exhausted or insufficient */
        pxServer->xRemainingBudget = ( TickType_t ) 0U;
        return pdFALSE;
    }

    pxServer->xRemainingBudget -= ulTicksUsed;

    return pdTRUE;
}

void vCBSReplenishBudget( CBS_Server_t *pxServer )
{
    if( pxServer == NULL )
    {
        return;
    }

    /* Replenish budget and update deadline */
    pxServer->xLastReplenishTime = xTaskGetTickCount();
    pxServer->xRemainingBudget = pxServer->xCapacityTicks;
    pxServer->xAbsDeadline = pxServer->xLastReplenishTime + pxServer->xPeriodTicks;
}

BaseType_t xCBSIsBudgetExhausted( CBS_Server_t *pxServer )
{
    if( pxServer == NULL )
    {
        return pdTRUE;
    }

    return ( pxServer->xRemainingBudget == 0 ) ? pdTRUE : pdFALSE;
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
    pxActiveCBSServer = NULL;
    xCBSInitialised = pdTRUE;

    memset( pxCBSServers, 0, sizeof( pxCBSServers ) );
    memset( xCBSJobPool, 0, sizeof( xCBSJobPool ) );
}

void vCBSDeinit( void )
{
    /* Clean up all servers */
    for( UBaseType_t ux = 0; ux < uxCBSServerCount; ux++ )
    {
        if( pxCBSServers[ ux ] != NULL )
        {
            vCBSServerDelete( pxCBSServers[ ux ] );
        }
    }

    uxCBSServerCount = 0;
    pxActiveCBSServer = NULL;
    xCBSInitialised = pdFALSE;
}

void vCBSTickUpdate( void )
{
    TaskHandle_t xTask = xTaskGetCurrentTaskHandle();
    CBS_Server_t *pxServer = pxTaskCBSGetServer( xTask );

    if( pxServer != NULL )
    {
        pxActiveCBSServer = pxServer;

        if( xCBSConsumeBudget( pxServer, ( TickType_t ) 1U ) == pdFALSE )
        {
            vCBSReplenishBudget( pxServer );
        }
    }
    else
    {
        pxActiveCBSServer = NULL;
    }
}

CBS_Server_t * pxCBSGetActiveServer( void )
{
    return pxActiveCBSServer;
}

BaseType_t xCBSIsTaskManaged( TaskHandle_t xTask )
{
    return xTaskCBSIsManaged( xTask );
}

#if ( configUSE_SRP == 0 )

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
