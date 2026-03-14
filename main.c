#include <stdio.h>
#include "pico/stdlib.h"
#include "schedulingConfig.h"
#include "FreeRTOS.h"
#include "task.h"

static void Task1( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

static void Task2( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

int main( void )
{
    xTaskCreate( Task1, "Test Task 1", 256, NULL, NULL, 5, 2, 5 );
    xTaskCreate( Task2, "Test Task 2", 256, NULL, NULL, 7, 4, 7 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}
