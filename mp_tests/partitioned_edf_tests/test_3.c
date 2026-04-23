#include "mp_tests/partitioned_edf_tests/test_3.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 1U ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Partitioned EDF WCET-overrun trace test.
 *
 * Each core gets one intentional overrun task and one normal task. Deferred
 * UART messages should identify MP partitioned EDF and include task/core
 * timing details for each overrun.
 */

#define PART3_STACK_DEPTH           256u

#define PART3_C0_OVR_PERIOD_MS     5000u
#define PART3_C0_OVR_WCET_MS       1000u
#define PART3_C0_OVR_WORK_MS       1300u

#define PART3_C0_N_PERIOD_MS      10000u
#define PART3_C0_N_WCET_MS         1000u
#define PART3_C0_N_WORK_MS          800u

#define PART3_C1_OVR_PERIOD_MS     8000u
#define PART3_C1_OVR_WCET_MS       1000u
#define PART3_C1_OVR_WORK_MS       1300u

#define PART3_C1_N_PERIOD_MS      10000u
#define PART3_C1_N_WCET_MS         1000u
#define PART3_C1_N_WORK_MS          800u

typedef struct MPPartOverrunTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    uint32_t ulWorkMs;
    UBaseType_t uxAffinityMask;
} MPPartOverrunTaskConfig_t;

static TickType_t xPart3AnchorTick = 0u;

static void vMPPartOverrunTask( void * pvParameters )
{
    const MPPartOverrunTaskConfig_t * pxCfg = ( const MPPartOverrunTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime = xPart3AnchorTick;

    configASSERT( pxCfg != NULL );

    for( ;; )
    {
        spin_ms( pxCfg->ulWorkMs );
        vTraceFlushMPOverrunEvents();

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( pxCfg->ulPeriodMs ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }

        vTraceFlushMPOverrunEvents();
    }
}

void mp_partitioned_edf_3_run( void )
{
    static const MPPartOverrunTaskConfig_t xTaskCfgs[] =
    {
        { "P3 C0O", 1u, PART3_C0_OVR_PERIOD_MS, PART3_C0_OVR_WCET_MS, PART3_C0_OVR_WORK_MS, ( UBaseType_t ) 1u << 0u },
        { "P3 C0N", 2u, PART3_C0_N_PERIOD_MS,   PART3_C0_N_WCET_MS,   PART3_C0_N_WORK_MS,   ( UBaseType_t ) 1u << 0u },
        { "P3 C1O", 4u, PART3_C1_OVR_PERIOD_MS, PART3_C1_OVR_WCET_MS, PART3_C1_OVR_WORK_MS, ( UBaseType_t ) 1u << 1u },
        { "P3 C1N", 3u, PART3_C1_N_PERIOD_MS,   PART3_C1_N_WCET_MS,   PART3_C1_N_WORK_MS,   ( UBaseType_t ) 1u << 1u }
    };
    UBaseType_t uxIndex;

    stdio_init_all();
    vTraceTaskPinsInit();

    xPart3AnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        configASSERT( xTaskCreate( vMPPartOverrunTask,
                                   xTaskCfgs[ uxIndex ].pcName,
                                   PART3_STACK_DEPTH,
                                   ( void * ) &xTaskCfgs[ uxIndex ],
                                   &xHandle,
                                   xTaskCfgs[ uxIndex ].ulPeriodMs,
                                   xTaskCfgs[ uxIndex ].ulWcetMs,
                                   xTaskCfgs[ uxIndex ].ulPeriodMs,
                                   xTaskCfgs[ uxIndex ].uxAffinityMask ) == pdPASS );
        configASSERT( xHandle != NULL );

        vTaskSetApplicationTaskTag( xHandle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xTaskCfgs[ uxIndex ].ulTag );
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif
