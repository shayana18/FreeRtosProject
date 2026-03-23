#include <stdio.h>
#include "schedulingConfig.h"
#include "pico/stdlib.h"

#if configUSE_EDF
#include "edf.h"
#include "FreeRTOS.h"
#include "task.h"

/* -----------------------------------------------------------------------
 * EDF task parameters
 *
 * Task 1: period=100 ms, deadline=100 ms, WCET=30 ms  → utilization=0.30
 * Task 2: period=200 ms, deadline=150 ms, WCET=50 ms  → utilization=0.25
 * Combined utilization=0.55 < 1.0 → feasible under EDF.
 * ----------------------------------------------------------------------- */
static EDFTaskParams_t xTask1Params = {
    .xPeriod           = pdMS_TO_TICKS( 100 ),
    .xRelativeDeadline = pdMS_TO_TICKS( 100 ),
    .xWCET             = pdMS_TO_TICKS( 30 ),
};

static EDFTaskParams_t xTask2Params = {
    .xPeriod           = pdMS_TO_TICKS( 200 ),
    .xRelativeDeadline = pdMS_TO_TICKS( 150 ),
    .xWCET             = pdMS_TO_TICKS( 50 ),
};

/* -----------------------------------------------------------------------
 * EDF task bodies
 *
 * Each task:
 *  1. Initialises its release time and first absolute deadline.
 *  2. Performs its work (simulated with a printf here).
 *  3. Calls edfPeriodicDelay() to sleep until the next period.
 *
 * edfPeriodicDelay() internally uses vTaskDelayUntil() — NOT vTaskDelay()
 * — so the period anchor is preserved.  It also detects deadline misses
 * and handles them by skipping the late job, not by calling vTaskDelay().
 * ----------------------------------------------------------------------- */
static void prvEDFTask1( void *pvParameters )
{
    EDFTaskParams_t *pxParams = ( EDFTaskParams_t * ) pvParameters;

    /* Initialise timing for the first job. */
    pxParams->xNextRelease = xTaskGetTickCount();
    pxParams->xAbsDeadline = pxParams->xNextRelease + pxParams->xRelativeDeadline;

    for( ; ; )
    {
        printf( "[EDF] Task1 running  now=%lu  abs_deadline=%lu\n",
                (unsigned long)xTaskGetTickCount(),
                (unsigned long)pxParams->xAbsDeadline );

        /*
         * End of job: check deadline then sleep until next period.
         *
         * edfPeriodicDelay() uses vTaskDelayUntil() so the period grid
         * stays aligned.  Deadline misses are handled inside without
         * resorting to vTaskDelay().
         */
        if( edfPeriodicDelay( pxParams ) == pdFALSE )
        {
            printf( "[EDF] Task1 MISSED its deadline!\n" );
        }
    }
}

static void prvEDFTask2( void *pvParameters )
{
    EDFTaskParams_t *pxParams = ( EDFTaskParams_t * ) pvParameters;

    pxParams->xNextRelease = xTaskGetTickCount();
    pxParams->xAbsDeadline = pxParams->xNextRelease + pxParams->xRelativeDeadline;

    for( ; ; )
    {
        printf( "[EDF] Task2 running  now=%lu  abs_deadline=%lu\n",
                (unsigned long)xTaskGetTickCount(),
                (unsigned long)pxParams->xAbsDeadline );

        if( edfPeriodicDelay( pxParams ) == pdFALSE )
        {
            printf( "[EDF] Task2 MISSED its deadline!\n" );
        }
    }
}
#endif /* configUSE_EDF */

int main( void )
{
    stdio_init_all();

#if configUSE_EDF
    /* Admission control – must succeed before creating tasks. */
    if( edfAdmitTask( &xTask1Params ) != EDF_ADMIT_OK )
    {
        printf( "[EDF] Task1 NOT admitted – utilization too high\n" );
    }
    if( edfAdmitTask( &xTask2Params ) != EDF_ADMIT_OK )
    {
        printf( "[EDF] Task2 NOT admitted – utilization too high\n" );
    }

    /*
     * Create FreeRTOS tasks.  The initial priority is a placeholder;
     * edfUpdatePriorities() will assign correct EDF priorities at runtime.
     */
    xTaskCreate( prvEDFTask1, "EDF1", 256, &xTask1Params, 1,
                 &xTask1Params.xHandle );
    xTaskCreate( prvEDFTask2, "EDF2", 256, &xTask2Params, 1,
                 &xTask2Params.xHandle );

    /* Set initial EDF priorities before the scheduler starts. */
    edfUpdatePriorities();

    vTaskStartScheduler();

    /* Should never reach here. */
    for( ; ; )
    {
    }
#else
    while( 1 )
    {
        printf( "Hello, world!\n" );
        sleep_ms( 1000 );
    }
#endif

    return 0;
}
