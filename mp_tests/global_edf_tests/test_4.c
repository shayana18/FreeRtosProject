#include "mp_tests/global_edf_tests/test_4.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( GLOBAL_EDF_ENABLE == 1U ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Global EDF WCET-overrun + deadline-miss trace test.
 *
 * The miss task exceeds WCET first and then also misses a constrained relative
 * deadline. Deferred WCET/deadline print callbacks are flushed from task
 * context so both events can be observed in a global EDF MP run.
 */

#define GLOB4_STACK_DEPTH        256u

#define GLOB4_OVR_PERIOD_MS     5000u
#define GLOB4_OVR_DEADLINE_MS   1800u
#define GLOB4_OVR_WCET_MS       1000u
#define GLOB4_OVR_WORK_MS       2200u

#define GLOB4_N1_PERIOD_MS      8000u
#define GLOB4_N1_DEADLINE_MS    8000u
#define GLOB4_N1_WCET_MS        1000u
#define GLOB4_N1_WORK_MS         800u

#define GLOB4_N2_PERIOD_MS     10000u
#define GLOB4_N2_DEADLINE_MS   10000u
#define GLOB4_N2_WCET_MS        1000u
#define GLOB4_N2_WORK_MS         800u

typedef struct MPGlobOverrunTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulDeadlineMs;
    uint32_t ulWcetMs;
    uint32_t ulWorkMs;
} MPGlobOverrunTaskConfig_t;

static TickType_t xGlob4AnchorTick = 0u;

static void vMPGlobOverrunTask( void * pvParameters )
{
    const MPGlobOverrunTaskConfig_t * pxCfg = ( const MPGlobOverrunTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime = xGlob4AnchorTick;

    configASSERT( pxCfg != NULL );

    for( ;; )
    {
        spin_ms( pxCfg->ulWorkMs );
        vTraceFlushWcetOverrunEvents();
        vTraceFlushDeadlineMissEvents();
        vTraceFlushMPOverrunEvents();

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( pxCfg->ulPeriodMs ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }

        vTraceFlushWcetOverrunEvents();
        vTraceFlushDeadlineMissEvents();
        vTraceFlushMPOverrunEvents();
    }
}

void mp_global_edf_4_run( void )
{
    static const MPGlobOverrunTaskConfig_t xTaskCfgs[] =
    {
        { "G4 OVR", 1u, GLOB4_OVR_PERIOD_MS, GLOB4_OVR_DEADLINE_MS, GLOB4_OVR_WCET_MS, GLOB4_OVR_WORK_MS },
        { "G4 N1",  2u, GLOB4_N1_PERIOD_MS,  GLOB4_N1_DEADLINE_MS,  GLOB4_N1_WCET_MS,  GLOB4_N1_WORK_MS },
        { "G4 N2",  4u, GLOB4_N2_PERIOD_MS,  GLOB4_N2_DEADLINE_MS,  GLOB4_N2_WCET_MS,  GLOB4_N2_WORK_MS }
    };
    UBaseType_t uxIndex;

    stdio_init_all();
    vTraceTaskPinsInit();

    xGlob4AnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        configASSERT( xTaskCreate( vMPGlobOverrunTask,
                                   xTaskCfgs[ uxIndex ].pcName,
                                   GLOB4_STACK_DEPTH,
                                   ( void * ) &xTaskCfgs[ uxIndex ],
                                   &xHandle,
                                   xTaskCfgs[ uxIndex ].ulPeriodMs,
                                   xTaskCfgs[ uxIndex ].ulWcetMs,
                                   xTaskCfgs[ uxIndex ].ulDeadlineMs,
                                   tskNO_AFFINITY ) == pdPASS );
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
