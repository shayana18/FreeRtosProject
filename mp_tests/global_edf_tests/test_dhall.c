#include "mp_tests/global_edf_tests/test_dhall.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( GLOBAL_EDF_ENABLE == 1U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Demo test 2: Dhall-style admission-control rejection under global EDF.
 *
 * Task set:
 * - Heavy: T=2000 ms, C=1800 ms, D=T, unrestricted, tag 1   -> U = 0.90
 * - Light: T=2000 ms, C=200 ms,  D=T, unrestricted, tag 2   -> U = 0.10
 * - Bad  : T=4000 ms, C=3000 ms, D=3000 ms, unrestricted, tag 4 -> U = 0.75
 */

#define DEMO_DHALL_STACK_DEPTH    256u

typedef struct MPDemoDhallTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulDeadlineMs;
    uint32_t ulWcetMs;
    uint32_t ulWorkMs;
} MPDemoDhallTaskConfig_t;

static volatile BaseType_t xMpDemoDhallHeavyCreateResult = pdFAIL;
static volatile BaseType_t xMpDemoDhallLightCreateResult = pdFAIL;
static volatile BaseType_t xMpDemoDhallBadCreateResult = pdFAIL;
static TickType_t xMpDemoDhallSharedAnchorTick = 0u;

static void vMPDemoDhallTask( void * pvParameters )
{
    const MPDemoDhallTaskConfig_t * pxCfg = ( const MPDemoDhallTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xMpDemoDhallSharedAnchorTick;

    for( ;; )
    {
        spin_ms( pxCfg->ulWorkMs );

        xDelayResult = xTaskDelayUntil( &xLastWakeTime,
                                        pdMS_TO_TICKS( pxCfg->ulPeriodMs ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void mp_test_dhall_run( void )
{
    static const MPDemoDhallTaskConfig_t xHeavyCfg =
    {
        .pcName = "Dhall Heavy",
        .ulTag = 1u,
        .ulPeriodMs = 2000u,
        .ulDeadlineMs = 2000u,
        .ulWcetMs = 1800u,
        .ulWorkMs = 1600u
    };
    static const MPDemoDhallTaskConfig_t xLightCfg =
    {
        .pcName = "Dhall Light",
        .ulTag = 2u,
        .ulPeriodMs = 2000u,
        .ulDeadlineMs = 2000u,
        .ulWcetMs = 200u,
        .ulWorkMs = 100u
    };
    static const MPDemoDhallTaskConfig_t xBadCfg =
    {
        .pcName = "Dhall Bad",
        .ulTag = 4u,
        .ulPeriodMs = 4000u,
        .ulDeadlineMs = 3000u,
        .ulWcetMs = 3000u,
        .ulWorkMs = 3000u
    };
    TaskHandle_t xHeavyHandle = NULL;
    TaskHandle_t xLightHandle = NULL;
    TaskHandle_t xBadHandle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();
    xMpDemoDhallSharedAnchorTick = xTaskGetTickCount();

    xMpDemoDhallHeavyCreateResult =
        xTaskCreate( vMPDemoDhallTask,
                     xHeavyCfg.pcName,
                     DEMO_DHALL_STACK_DEPTH,
                     ( void * ) &xHeavyCfg,
                     &xHeavyHandle,
                     xHeavyCfg.ulPeriodMs,
                     xHeavyCfg.ulWcetMs,
                     xHeavyCfg.ulDeadlineMs,
                     tskNO_AFFINITY );

    xMpDemoDhallLightCreateResult =
        xTaskCreate( vMPDemoDhallTask,
                     xLightCfg.pcName,
                     DEMO_DHALL_STACK_DEPTH,
                     ( void * ) &xLightCfg,
                     &xLightHandle,
                     xLightCfg.ulPeriodMs,
                     xLightCfg.ulWcetMs,
                     xLightCfg.ulDeadlineMs,
                     tskNO_AFFINITY );

    xMpDemoDhallBadCreateResult =
        xTaskCreate( vMPDemoDhallTask,
                     xBadCfg.pcName,
                     DEMO_DHALL_STACK_DEPTH,
                     ( void * ) &xBadCfg,
                     &xBadHandle,
                     xBadCfg.ulPeriodMs,
                     xBadCfg.ulWcetMs,
                     xBadCfg.ulDeadlineMs,
                     tskNO_AFFINITY );

    if( ( xMpDemoDhallHeavyCreateResult == pdPASS ) && ( xHeavyHandle != NULL ) )
    {
        vTaskSetApplicationTaskTag( xHeavyHandle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xHeavyCfg.ulTag );
    }

    if( ( xMpDemoDhallLightCreateResult == pdPASS ) && ( xLightHandle != NULL ) )
    {
        vTaskSetApplicationTaskTag( xLightHandle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xLightCfg.ulTag );
    }

    if( ( xMpDemoDhallBadCreateResult == pdPASS ) && ( xBadHandle != NULL ) )
    {
        vTaskSetApplicationTaskTag( xBadHandle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xBadCfg.ulTag );
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif /* demo Dhall test */
