#include "cbs_tests/test_5.h"

#include "FreeRTOS.h"

#if ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * CBS rejection tests:
 * - server admission rejects a reservation that would push total CBS bandwidth
 *   above 100%.
 * - one worker cannot accept a second outstanding job before completing the
 *   first one.
 */

#define CBS5_STACK_DEPTH                256u

#define CBS5_SERVER_A_PERIOD_MS       5000u
#define CBS5_SERVER_A_BUDGET_MS       2000u
#define CBS5_SERVER_B_PERIOD_MS       5000u
#define CBS5_SERVER_B_BUDGET_MS       2500u
#define CBS5_SERVER_BAD_PERIOD_MS     5000u
#define CBS5_SERVER_BAD_BUDGET_MS     1000u

#define CBS5_CONTROLLER_PERIOD_MS    10000u
#define CBS5_CONTROLLER_WCET_MS        250u
#define CBS5_WORKER_WORK_MS            300u

static CBS_Server_t * pxCBS5ServerA = NULL;
static CBS_Server_t * pxCBS5ServerB = NULL;
static CBS_Server_t * pxCBS5RejectedServer = NULL;
static TaskHandle_t xCBS5WorkerHandle = NULL;
static volatile uint32_t ulCBS5WorkerJobs = 0u;
static volatile BaseType_t xCBS5AdmissionRejected = pdFALSE;
static volatile BaseType_t xCBS5SecondJobRejected = pdFALSE;
static volatile BaseType_t xCBS5ThirdJobAccepted = pdFALSE;

static void vCBS5WorkerTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            spin_ms( CBS5_WORKER_WORK_MS );
            ulCBS5WorkerJobs++;
            configASSERT( xCBSCompleteJob() == pdPASS );
        }
    }
}

static void vCBS5ControllerTask( void * pvParameters )
{
    BaseType_t xFirstSubmit;
    BaseType_t xSecondSubmit;
    BaseType_t xThirdSubmit;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    ( void ) pvParameters;

    vTaskDelay( pdMS_TO_TICKS( 500u ) );

    xFirstSubmit = xCBSSubmitJob( pxCBS5ServerA, xCBS5WorkerHandle );
    xSecondSubmit = xCBSSubmitJob( pxCBS5ServerA, xCBS5WorkerHandle );

    xCBS5SecondJobRejected =
        ( ( xFirstSubmit == pdPASS ) && ( xSecondSubmit == pdFAIL ) ) ? pdTRUE : pdFALSE;
    configASSERT( xCBS5SecondJobRejected == pdTRUE );

    vTaskDelay( pdMS_TO_TICKS( 1000u ) );
    xThirdSubmit = xCBSSubmitJob( pxCBS5ServerA, xCBS5WorkerHandle );
    xCBS5ThirdJobAccepted = ( xThirdSubmit == pdPASS ) ? pdTRUE : pdFALSE;
    configASSERT( xCBS5ThirdJobAccepted == pdTRUE );

    vTraceUsbPrint( "[CBS5] admission_rejected=%ld second_job_rejected=%ld third_job_accepted=%ld completed_jobs=%lu\r\n",
                    ( long ) xCBS5AdmissionRejected,
                    ( long ) xCBS5SecondJobRejected,
                    ( long ) xCBS5ThirdJobAccepted,
                    ( unsigned long ) ulCBS5WorkerJobs );

    for( ;; )
    {
        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( CBS5_CONTROLLER_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void cbs_5_run( void )
{
    TaskHandle_t xControllerHandle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();

    pxCBS5ServerA = xCBSServerCreate( pdMS_TO_TICKS( CBS5_SERVER_A_BUDGET_MS ),
                                      pdMS_TO_TICKS( CBS5_SERVER_A_PERIOD_MS ),
                                      "CBS5_A" );
    pxCBS5ServerB = xCBSServerCreate( pdMS_TO_TICKS( CBS5_SERVER_B_BUDGET_MS ),
                                      pdMS_TO_TICKS( CBS5_SERVER_B_PERIOD_MS ),
                                      "CBS5_B" );
    pxCBS5RejectedServer = xCBSServerCreate( pdMS_TO_TICKS( CBS5_SERVER_BAD_BUDGET_MS ),
                                             pdMS_TO_TICKS( CBS5_SERVER_BAD_PERIOD_MS ),
                                             "CBS5_BAD" );

    configASSERT( pxCBS5ServerA != NULL );
    configASSERT( pxCBS5ServerB != NULL );
    xCBS5AdmissionRejected = ( pxCBS5RejectedServer == NULL ) ? pdTRUE : pdFALSE;
    configASSERT( xCBS5AdmissionRejected == pdTRUE );

    configASSERT( xTaskCreateCBSWorker( vCBS5WorkerTask,
                                        "CBS5 WORK",
                                        CBS5_STACK_DEPTH,
                                        NULL,
                                        &xCBS5WorkerHandle,
                                        pxCBS5ServerA ) == pdPASS );
    configASSERT( xCBS5WorkerHandle != NULL );

    configASSERT( xTaskCreate( vCBS5ControllerTask,
                               "CBS5 CTRL",
                               CBS5_STACK_DEPTH,
                               NULL,
                               &xControllerHandle,
                               CBS5_CONTROLLER_PERIOD_MS,
                               CBS5_CONTROLLER_WCET_MS,
                               CBS5_CONTROLLER_PERIOD_MS ) == pdPASS );
    configASSERT( xControllerHandle != NULL );

    vTaskSetApplicationTaskTag( xCBS5WorkerHandle, ( TaskHookFunction_t ) 1 );
    vTaskSetApplicationTaskTag( xControllerHandle, ( TaskHookFunction_t ) 2 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void cbs_5_run( void )
{
}

#endif
