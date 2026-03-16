#include <stdio.h>
#include "pico/stdlib.h"
#include "schedulingConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_trace.h"

static void Task1( void * pvParameters )
{
    ( void ) pvParameters;

    vTaskSetApplicationTaskTag( NULL, ( TaskHookFunction_t ) 1 );

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

static void Task2( void * pvParameters )
{
    ( void ) pvParameters;

    vTaskSetApplicationTaskTag( NULL, ( TaskHookFunction_t ) 2 );

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

static void Task3( void * pvParameters )
{
    ( void ) pvParameters;

    vTaskSetApplicationTaskTag( NULL, ( TaskHookFunction_t ) 4 );

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

int main( void )
{
    stdio_init_all();
    vTraceTaskPinsInit();

    xTaskCreate( Task1, "Test Task 1", 256, NULL, NULL, 5000, 1500, 5000 );
    xTaskCreate( Task2, "Test Task 2", 256, NULL, NULL, 7000, 1500, 7000 );
    xTaskCreate( Task3, "Test Task 3", 256, NULL, NULL, 8000, 2000, 8000 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}
