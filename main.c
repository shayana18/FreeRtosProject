#include <stdint.h>

#include "schedulingConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_trace.h"

#if ( configUSE_EDF == 1)
    #if(configUSE_CBS == 1)
        #include "cbs_tests/test_2.h"
    #elif(configUSE_SRP == 0)
        #include "edf_tests/test_1.h"
        #include "edf_tests/test_2.h"
        #include "edf_tests/test_3.h"
        #include "edf_tests/test_4.h"
        #include "edf_tests/test_5.h"
        #include "edf_tests/test_6.h"
        #include "edf_tests/test_7.h"
        #include "edf_tests/test_8.h"
    #elif (configUSE_SRP == 1 )
        #include "srp_tests/test_1.h"
    #endif
#endif

typedef struct xHARDFAULT_DEBUG_CONTEXT
{
    TaskDebugSnapshot_t xTaskSnapshot;
    uint32_t ulExcReturn;
    uint32_t ulFaultStackPointer;
    uint32_t ulPsp;
    uint32_t ulMsp;
    uint32_t ulIpsr;
    uint32_t ulIcsr;
    uint32_t ulR0;
    uint32_t ulR1;
    uint32_t ulR2;
    uint32_t ulR3;
    uint32_t ulR12;
    uint32_t ulLr;
    uint32_t ulPc;
    uint32_t ulXpsr;
} HardFaultDebugContext_t;

typedef struct xDEADLINE_MISS_DEBUG_CONTEXT
{
    TaskDebugSnapshot_t xTaskSnapshot;
    uint32_t ulMissCount;
    uint32_t ulFirstCaptured;
} DeadlineMissDebugContext_t;

volatile HardFaultDebugContext_t xHardFaultDebugContext;
volatile DeadlineMissDebugContext_t xDeadlineMissDebugContext;

void vHardFaultHandlerC( uint32_t * pulFaultStack,
                         uint32_t ulExcReturn ) __attribute__( ( noreturn ) );

void isr_hardfault( void ) __attribute__( ( naked ) );
static void vApplicationFirstDeadlineMissCaptured( void ) __attribute__( ( noinline ) );

int main( void )
{
    
    #if ( configUSE_EDF == 1)
        #if ( configUSE_CBS == 1 )
        // Simple cbs test with one periodic EDF task and one CBS-managed aperiodic task.
        // cbs_1_run();
        // Multiple CBS server test with two periodic tasks plus two CBS-managed aperiodic tasks.
        cbs_2_run();
        #elif (configUSE_SRP == 1 )
        // srp_1_run();
        // srp_2_run();
        // srp_3_run();
        // srp_4_run();
        // srp_5_run();
        // srp_6_run();
        #elif (configUSE_SRP == 0 && configUSE_CBS == 0 )
        // Simple edf implicit deadlinetest case with 3 tasks added at startup with a fairly low utilization.
        // edf_1_run(); 

        // Higher utilization (but still < 1.0) edf implicit deadline test with 4 tasks added at startup.
        // edf_2_run(); 

        // EDF admission control test, attempts to add unschedulable task at startup and after 10s, then adds a schedulable task.
        // edf_3_run(); 

        // Constrained deadline EDF test with 3 tasks (D != T).
        // edf_4_run();

        // Higher utilization Constrained deadline EDF test with 4 tasks (D < T).
        //  edf_5_run(); 

        // Constrained deadline admission control test (expected reject then accept).
        // edf_6_run();

        // Implicit deadline admission control test (utilization path: expected reject then accept).
        // edf_7_run();

        // Seven tasks with binary-encoded trace IDs and two intentional single deadline-miss events.
        // edf_8_run();
        #endif 
    #endif

    for( ;; )
    {
    }
}

void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                    char * pcTaskName )
{
    ( void ) xTask;
    ( void ) pcTaskName;

    taskDISABLE_INTERRUPTS();

    /* Put a distinctive visible pattern on the trace pins when overflow is detected. */
    vTraceWriteTaskCode( 0x7fu );
    vTraceSetTaskSwitchSignal();
    vTraceSignalDeadlineMiss();

    for( ;; )
    {
    }
}

void vApplicationDeadlineMissHook( void )
{
    xDeadlineMissDebugContext.ulMissCount++;

    if( xDeadlineMissDebugContext.ulFirstCaptured == 0u )
    {
        xDeadlineMissDebugContext.ulFirstCaptured = 1u;
        vTaskGetCurrentDebugSnapshot( ( TaskDebugSnapshot_t * ) &xDeadlineMissDebugContext.xTaskSnapshot );
        vApplicationFirstDeadlineMissCaptured();
    }
}

static void vApplicationFirstDeadlineMissCaptured( void )
{
    __asm volatile ( "" ::: "memory" );
}

void isr_hardfault( void )
{
    __asm volatile
    (
        "mov r2, lr                        \n"
        "movs r1, #4                       \n"
        "tst r2, r1                        \n"
        "beq 1f                            \n"
        "mrs r0, psp                       \n"
        "b 2f                              \n"
        "1:                                \n"
        "mrs r0, msp                       \n"
        "2:                                \n"
        "mov r1, r2                        \n"
        "b vHardFaultHandlerC              \n"
    );
}

void vHardFaultHandlerC( uint32_t * pulFaultStack,
                         uint32_t ulExcReturn )
{
    uint32_t ulPsp;
    uint32_t ulMsp;
    uint32_t ulIpsr;

    __asm volatile ( "mrs %0, psp" : "=r" ( ulPsp ) );
    __asm volatile ( "mrs %0, msp" : "=r" ( ulMsp ) );
    __asm volatile ( "mrs %0, ipsr" : "=r" ( ulIpsr ) );

    taskDISABLE_INTERRUPTS();

    xHardFaultDebugContext.ulExcReturn = ulExcReturn;
    xHardFaultDebugContext.ulFaultStackPointer = ( uint32_t ) pulFaultStack;
    xHardFaultDebugContext.ulPsp = ulPsp;
    xHardFaultDebugContext.ulMsp = ulMsp;
    xHardFaultDebugContext.ulIpsr = ulIpsr;
    xHardFaultDebugContext.ulIcsr = *( ( volatile uint32_t * ) 0xE000ED04u );

    if( pulFaultStack != NULL )
    {
        xHardFaultDebugContext.ulR0 = pulFaultStack[ 0 ];
        xHardFaultDebugContext.ulR1 = pulFaultStack[ 1 ];
        xHardFaultDebugContext.ulR2 = pulFaultStack[ 2 ];
        xHardFaultDebugContext.ulR3 = pulFaultStack[ 3 ];
        xHardFaultDebugContext.ulR12 = pulFaultStack[ 4 ];
        xHardFaultDebugContext.ulLr = pulFaultStack[ 5 ];
        xHardFaultDebugContext.ulPc = pulFaultStack[ 6 ];
        xHardFaultDebugContext.ulXpsr = pulFaultStack[ 7 ];
    }

    vTaskGetCurrentDebugSnapshot( ( TaskDebugSnapshot_t * ) &xHardFaultDebugContext.xTaskSnapshot );

    /* Distinct trace pattern for hard faults, separate from stack overflow. */
    vTraceWriteTaskCode( 0x3fu );
    vTraceSetTaskSwitchSignal();

    for( ;; )
    {
    }
}
