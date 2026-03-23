#include "edf_tests/test_8.h"

#if ( configUSE_EDF == 1 )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

#define STACK_DEPTH                     256u
#define TEST8_TASK_COUNT                7u

#define T1_PERIOD_MS                    2000u
#define T1_WCET_MS                      300u
#define T1_DEADLINE_MS                  T1_PERIOD_MS

#define T2_PERIOD_MS                    3000u
#define T2_WCET_MS                      350u
#define T2_DEADLINE_MS                  T2_PERIOD_MS

#define T3_PERIOD_MS                    4000u
#define T3_WCET_MS                      500u
#define T3_DEADLINE_MS                  T3_PERIOD_MS

#define T4_PERIOD_MS                    5000u
#define T4_WCET_MS                      600u
#define T4_DEADLINE_MS                  T4_PERIOD_MS

#define T5_PERIOD_MS                    7000u
#define T5_WCET_MS                      650u
#define T5_DEADLINE_MS                  T5_PERIOD_MS

#define T6_PERIOD_MS                    9000u
#define T6_WCET_MS                      900u
#define T6_DEADLINE_MS                  2200u

#define T7_PERIOD_MS                    12000u
#define T7_WCET_MS                      1200u
#define T7_DEADLINE_MS                  2500u

typedef struct Test8TaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    uint32_t ulDeadlineMs;
    uint32_t ulLoopSliceMs;
    UBaseType_t uxLoopCount;
} Test8TaskConfig_t;

typedef struct Test8PeriodicMissTaskConfig
{
    Test8TaskConfig_t xBase;
    UBaseType_t uxMissEveryNJobs;
    uint32_t ulMissOverrunMs;
} Test8PeriodicMissTaskConfig_t;

static Test8TaskConfig_t xAlwaysMeetTaskConfigs[] =
{
    /* Always-meet tasks: bounded loop work stays below WCET. */
    { "Test8 T1", 1u, T1_PERIOD_MS, T1_WCET_MS, T1_DEADLINE_MS, 60u, 3u },
    { "Test8 T2", 2u, T2_PERIOD_MS, T2_WCET_MS, T2_DEADLINE_MS, 70u, 3u },
    { "Test8 T3", 3u, T3_PERIOD_MS, T3_WCET_MS, T3_DEADLINE_MS, 80u, 4u },
    { "Test8 T4", 4u, T4_PERIOD_MS, T4_WCET_MS, T4_DEADLINE_MS, 90u, 4u },
    { "Test8 T5", 5u, T5_PERIOD_MS, T5_WCET_MS, T5_DEADLINE_MS, 100u, 4u }
};

static Test8PeriodicMissTaskConfig_t xPeriodicMissTaskConfigs[] =
{
    /* Periodic-miss tasks: every N jobs they delay past deadline once each period. */
    { { "Test8 T6", 6u, T6_PERIOD_MS, T6_WCET_MS, T6_DEADLINE_MS, 120u, 4u }, 4u, 200u },
    { { "Test8 T7", 7u, T7_PERIOD_MS, T7_WCET_MS, T7_DEADLINE_MS, 140u, 4u }, 3u, 200u }
};

static void vTest8AlwaysMeetTask( void * pvParameters )
{
    const Test8TaskConfig_t * pxCfg = ( const Test8TaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    const TickType_t xPeriodTicks = pdMS_TO_TICKS( pxCfg->ulPeriodMs );

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        UBaseType_t uxLoop;

        /* Bounded inner-loop work that stays within configured WCET budget. */
        for( uxLoop = 0u; uxLoop < pxCfg->uxLoopCount; uxLoop++ )
        {
            spin_ms( pxCfg->ulLoopSliceMs );
        }

        /* Normal EDF job completion path: explicitly release at next period. */
        ( void ) xTaskDelayUntil( &xLastWakeTime, xPeriodTicks );
    }
}

static void vTest8PeriodicMissTask( void * pvParameters )
{
    const Test8PeriodicMissTaskConfig_t * pxCfg = ( const Test8PeriodicMissTaskConfig_t * ) pvParameters;
    UBaseType_t uxJobCount = 0u;
    TickType_t xLastWakeTime;
    const TickType_t xPeriodTicks = pdMS_TO_TICKS( pxCfg->xBase.ulPeriodMs );

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        UBaseType_t uxLoop;
        BaseType_t xMissThisJob = pdFALSE;
        BaseType_t xDelayResult;

        uxJobCount++;

        if( ( pxCfg->uxMissEveryNJobs > 0u ) &&
            ( ( uxJobCount % pxCfg->uxMissEveryNJobs ) == 0u ) )
        {
            xMissThisJob = pdTRUE;
        }

        for( uxLoop = 0u; uxLoop < pxCfg->xBase.uxLoopCount; uxLoop++ )
        {
            spin_ms( pxCfg->xBase.ulLoopSliceMs );
        }

        if( xMissThisJob != pdFALSE )
        {
            /* Intentionally overrun while running so the kernel tick-time
             * deadline logic catches a real execution overrun. */
            spin_ms( pxCfg->xBase.ulDeadlineMs + pxCfg->ulMissOverrunMs );
        }

        /* Always use an explicit periodic boundary from task code. */
        xDelayResult = xTaskDelayUntil( &xLastWakeTime, xPeriodTicks );

        if( xDelayResult == pdFALSE )
        {
            /* If we're already late, restart the local phase anchor instead of
             * repeatedly trying to catch up old release points. */
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void edf_8_run( void )
{
    UBaseType_t uxIndex = 0u;
    UBaseType_t uxHandleIndex = 0u;
    TaskHandle_t xTaskHandles[ TEST8_TASK_COUNT ] = { NULL };

    stdio_init_all();
    vTraceTaskPinsInit();

    for( uxIndex = 0u;
         uxIndex < ( UBaseType_t ) ( sizeof( xAlwaysMeetTaskConfigs ) / sizeof( xAlwaysMeetTaskConfigs[ 0 ] ) );
         uxIndex++ )
    {
        const Test8TaskConfig_t * pxCfg = &xAlwaysMeetTaskConfigs[ uxIndex ];

        ( void ) xTaskCreate( vTest8AlwaysMeetTask,
                              pxCfg->pcName,
                              STACK_DEPTH,
                              ( void * ) pxCfg,
                              &xTaskHandles[ uxHandleIndex ],
                              pxCfg->ulPeriodMs,
                              pxCfg->ulWcetMs,
                              pxCfg->ulDeadlineMs );

        if( xTaskHandles[ uxHandleIndex ] != NULL )
        {
            vTaskSetApplicationTaskTag( xTaskHandles[ uxHandleIndex ],
                                        ( TaskHookFunction_t ) ( uintptr_t ) pxCfg->ulTag );
        }

        uxHandleIndex++;
    }

    for( uxIndex = 0u;
         uxIndex < ( UBaseType_t ) ( sizeof( xPeriodicMissTaskConfigs ) / sizeof( xPeriodicMissTaskConfigs[ 0 ] ) );
         uxIndex++ )
    {
        const Test8PeriodicMissTaskConfig_t * pxCfg = &xPeriodicMissTaskConfigs[ uxIndex ];

        ( void ) xTaskCreate( vTest8PeriodicMissTask,
                              pxCfg->xBase.pcName,
                              STACK_DEPTH,
                              ( void * ) pxCfg,
                              &xTaskHandles[ uxHandleIndex ],
                              pxCfg->xBase.ulPeriodMs,
                              pxCfg->xBase.ulWcetMs,
                              pxCfg->xBase.ulDeadlineMs );

        if( xTaskHandles[ uxHandleIndex ] != NULL )
        {
            vTaskSetApplicationTaskTag( xTaskHandles[ uxHandleIndex ],
                                        ( TaskHookFunction_t ) ( uintptr_t ) pxCfg->xBase.ulTag );
        }

        uxHandleIndex++;
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif /* configUSE_EDF */
