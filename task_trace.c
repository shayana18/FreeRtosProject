#include "FreeRTOS.h"
#include "task_trace.h"
#include "task.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "hardware/gpio.h"
#include "hardware/exception.h"
#include "pico/stdlib.h"
#if defined( LIB_PICO_STDIO_USB ) && ( LIB_PICO_STDIO_USB == 1 )
    #include "pico/stdio_usb.h"
#endif

#define TRACE_DEADLINE_MISS_HOLD_TICKS    ( ( configTICK_RATE_HZ > 0u ) ? configTICK_RATE_HZ : 1u )
#define TRACE_DEADLINE_MISS_EVENT_CAPACITY 8u
#define TRACE_WCET_OVERRUN_EVENT_CAPACITY 8u
#define TRACE_MP_OVERRUN_EVENT_CAPACITY   8u

static volatile uint32_t ulDeadlineMissHoldTicks = 0u;

typedef struct xTRACE_DEADLINE_MISS_EVENT
{
    BaseType_t xValid;
    uint32_t ulTaskId;
} TraceDeadlineMissEvent_t;

typedef struct xTRACE_WCET_OVERRUN_EVENT
{
    BaseType_t xValid;
    uint32_t ulTaskId;
} TraceWcetOverrunEvent_t;

static TraceDeadlineMissEvent_t xDeadlineMissEvents[ TRACE_DEADLINE_MISS_EVENT_CAPACITY ];
static volatile UBaseType_t uxDeadlineMissWriteIndex = 0u;
static volatile uint32_t ulDeadlineMissDroppedEvents = 0u;
static TraceWcetOverrunEvent_t xWcetOverrunEvents[ TRACE_WCET_OVERRUN_EVENT_CAPACITY ];
static volatile UBaseType_t uxWcetOverrunWriteIndex = 0u;
static volatile uint32_t ulWcetOverrunDroppedEvents = 0u;

static void prvTraceQueueDeadlineMissEvent( uint32_t ulTaskId )
{
    UBaseType_t uxSavedInterruptStatus;
    UBaseType_t uxSlot;

    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    {
        uxSlot = uxDeadlineMissWriteIndex;
        uxDeadlineMissWriteIndex = ( UBaseType_t ) ( ( uxDeadlineMissWriteIndex + 1u ) %
                                                    TRACE_DEADLINE_MISS_EVENT_CAPACITY );

        if( xDeadlineMissEvents[ uxSlot ].xValid != pdFALSE )
        {
            ulDeadlineMissDroppedEvents++;
        }

        xDeadlineMissEvents[ uxSlot ].ulTaskId = ulTaskId;
        xDeadlineMissEvents[ uxSlot ].xValid = pdTRUE;
    }
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );
}

static void prvTraceQueueWcetOverrunEvent( uint32_t ulTaskId )
{
    UBaseType_t uxSavedInterruptStatus;
    UBaseType_t uxSlot;

    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    {
        uxSlot = uxWcetOverrunWriteIndex;
        uxWcetOverrunWriteIndex = ( UBaseType_t ) ( ( uxWcetOverrunWriteIndex + 1u ) %
                                                   TRACE_WCET_OVERRUN_EVENT_CAPACITY );

        if( xWcetOverrunEvents[ uxSlot ].xValid != pdFALSE )
        {
            ulWcetOverrunDroppedEvents++;
        }

        xWcetOverrunEvents[ uxSlot ].ulTaskId = ulTaskId;
        xWcetOverrunEvents[ uxSlot ].xValid = pdTRUE;
    }
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );
}

#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
typedef struct xTRACE_MP_OVERRUN_EVENT
{
    BaseType_t xValid;
    const char * pcPolicy;
    const char * pcReason;
    char pcTaskName[ configMAX_TASK_NAME_LEN ];
    uint32_t ulTaskCode;
    uint32_t ulCoreID;
    uint32_t ulNowTick;
    uint32_t ulReleaseTick;
    uint32_t ulDeadlineTick;
    uint32_t ulExecTicks;
    uint32_t ulWcetTicks;
    uint32_t ulNextReleaseTick;
} TraceMPOverrunEvent_t;

static TraceMPOverrunEvent_t xMPOverrunEvents[ TRACE_MP_OVERRUN_EVENT_CAPACITY ];
static volatile UBaseType_t uxMPOverrunWriteIndex = 0u;
static volatile uint32_t ulMPOverrunDroppedEvents = 0u;

static void prvTraceCopyTaskName( char * pcDestination,
                                  const char * pcSource )
{
    UBaseType_t uxIndex;

    if( pcDestination == NULL )
    {
        return;
    }

    if( pcSource == NULL )
    {
        pcSource = "<unnamed>";
    }

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( configMAX_TASK_NAME_LEN - 1u ); uxIndex++ )
    {
        pcDestination[ uxIndex ] = pcSource[ uxIndex ];

        if( pcSource[ uxIndex ] == '\0' )
        {
            return;
        }
    }

    pcDestination[ configMAX_TASK_NAME_LEN - 1u ] = '\0';
}
#endif

void vTraceUsbSerialInit( uint32_t ulWaitForHostMs )
{
    stdio_init_all();

    if( ulWaitForHostMs > 0u )
    {
        sleep_ms( ulWaitForHostMs );
    }
}

void vTraceUsbPrint( const char * pcFormat,
                     ... )
{
    if( pcFormat == NULL )
    {
        return;
    }

    /* USB stdio paths are not ISR-safe and can deadlock/assert if called from an ISR. */
    if( __get_current_exception() != 0u )
    {
        return;
    }

#if defined( LIB_PICO_STDIO_USB ) && ( LIB_PICO_STDIO_USB == 1 )
    if( stdio_usb_connected() == false )
    {
        return;
    }
#endif

    va_list xArgs;

    va_start( xArgs, pcFormat );
    ( void ) vprintf( pcFormat, xArgs );
    va_end( xArgs );
}

void vTraceTaskPinsInit( void )
{
#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    gpio_init( TRACE_CORE0_TASK_PIN0 );
    gpio_init( TRACE_CORE0_TASK_PIN1 );
    gpio_init( TRACE_CORE0_TASK_PIN2 );
    gpio_init( TRACE_CORE0_TASK_SWITCH_PIN );
    gpio_init( TRACE_CORE1_TASK_PIN0 );
    gpio_init( TRACE_CORE1_TASK_PIN1 );
    gpio_init( TRACE_CORE1_TASK_PIN2 );
    gpio_init( TRACE_CORE1_TASK_SWITCH_PIN );
#else
    gpio_init( TRACE_TASK_PIN0 );
    gpio_init( TRACE_TASK_PIN1 );
    gpio_init( TRACE_TASK_PIN2 );
    gpio_init( TRACE_TASK_PIN3 );
    gpio_init( TRACE_TASK_PIN4 );
    gpio_init( TRACE_TASK_PIN5 );
    gpio_init( TRACE_TASK_PIN6 );
    gpio_init( TRACE_TASK_SWITCH_PIN );
#endif
    gpio_init( TRACE_DEADLINE_MISS_PIN );

#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    gpio_set_dir( TRACE_CORE0_TASK_PIN0, GPIO_OUT );
    gpio_set_dir( TRACE_CORE0_TASK_PIN1, GPIO_OUT );
    gpio_set_dir( TRACE_CORE0_TASK_PIN2, GPIO_OUT );
    gpio_set_dir( TRACE_CORE0_TASK_SWITCH_PIN, GPIO_OUT );
    gpio_set_dir( TRACE_CORE1_TASK_PIN0, GPIO_OUT );
    gpio_set_dir( TRACE_CORE1_TASK_PIN1, GPIO_OUT );
    gpio_set_dir( TRACE_CORE1_TASK_PIN2, GPIO_OUT );
    gpio_set_dir( TRACE_CORE1_TASK_SWITCH_PIN, GPIO_OUT );
#else
    gpio_set_dir( TRACE_TASK_PIN0, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN1, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN2, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN3, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN4, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN5, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN6, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_SWITCH_PIN, GPIO_OUT );
#endif
    gpio_set_dir( TRACE_DEADLINE_MISS_PIN, GPIO_OUT );

#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    gpio_put( TRACE_CORE0_TASK_PIN0, 0 );
    gpio_put( TRACE_CORE0_TASK_PIN1, 0 );
    gpio_put( TRACE_CORE0_TASK_PIN2, 0 );
    gpio_put( TRACE_CORE0_TASK_SWITCH_PIN, 0 );
    gpio_put( TRACE_CORE1_TASK_PIN0, 0 );
    gpio_put( TRACE_CORE1_TASK_PIN1, 0 );
    gpio_put( TRACE_CORE1_TASK_PIN2, 0 );
    gpio_put( TRACE_CORE1_TASK_SWITCH_PIN, 0 );
#else
    gpio_put( TRACE_TASK_PIN0, 0 );
    gpio_put( TRACE_TASK_PIN1, 0 );
    gpio_put( TRACE_TASK_PIN2, 0 );
    gpio_put( TRACE_TASK_PIN3, 0 );
    gpio_put( TRACE_TASK_PIN4, 0 );
    gpio_put( TRACE_TASK_PIN5, 0 );
    gpio_put( TRACE_TASK_PIN6, 0 );
    gpio_put( TRACE_TASK_SWITCH_PIN, 0 );
#endif
    gpio_put( TRACE_DEADLINE_MISS_PIN, 0 );
}

void vTraceWriteTaskCode( uint32_t ulTaskCode )
{
#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    const uint32_t ulCode = ulTaskCode & 0x7u;

    if( portGET_CORE_ID() == 0 )
    {
        gpio_put( TRACE_CORE0_TASK_PIN0, ( ulCode & 0x1u ) != 0u );
        gpio_put( TRACE_CORE0_TASK_PIN1, ( ulCode & 0x2u ) != 0u );
        gpio_put( TRACE_CORE0_TASK_PIN2, ( ulCode & 0x4u ) != 0u );
    }
    else
    {
        gpio_put( TRACE_CORE1_TASK_PIN0, ( ulCode & 0x1u ) != 0u );
        gpio_put( TRACE_CORE1_TASK_PIN1, ( ulCode & 0x2u ) != 0u );
        gpio_put( TRACE_CORE1_TASK_PIN2, ( ulCode & 0x4u ) != 0u );
    }
#else
    gpio_put( TRACE_TASK_PIN0, ( ulTaskCode & 0x1u ) != 0u );
    gpio_put( TRACE_TASK_PIN1, ( ulTaskCode & 0x2u ) != 0u );
    gpio_put( TRACE_TASK_PIN2, ( ulTaskCode & 0x4u ) != 0u );
    gpio_put( TRACE_TASK_PIN3, ( ulTaskCode & 0x8u ) != 0u );
    gpio_put( TRACE_TASK_PIN4, ( ulTaskCode & 0x10u ) != 0u );
    gpio_put( TRACE_TASK_PIN5, ( ulTaskCode & 0x20u ) != 0u );
    gpio_put( TRACE_TASK_PIN6, ( ulTaskCode & 0x40u ) != 0u );
#endif
}

void vTraceTaskSwitchedIn( uint32_t ulTaskCode )
{
    vTraceWriteTaskCode( ulTaskCode );
    vTraceSetTaskSwitchSignal();
}

void vTraceSetTaskSwitchSignal( void )
{
#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    if( portGET_CORE_ID() == 0 )
    {
        gpio_put( TRACE_CORE0_TASK_SWITCH_PIN, 1 );
    }
    else
    {
        gpio_put( TRACE_CORE1_TASK_SWITCH_PIN, 1 );
    }
#else
    gpio_put( TRACE_TASK_SWITCH_PIN, 1 );
#endif
}

void vTraceClearTaskSwitchSignal( void )
{
#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    if( portGET_CORE_ID() == 0 )
    {
        gpio_put( TRACE_CORE0_TASK_SWITCH_PIN, 0 );
    }
    else
    {
        gpio_put( TRACE_CORE1_TASK_SWITCH_PIN, 0 );
    }
#else
    gpio_put( TRACE_TASK_SWITCH_PIN, 0 );
#endif
}

void vTraceSignalDeadlineMiss( void )
{
    ulDeadlineMissHoldTicks = TRACE_DEADLINE_MISS_HOLD_TICKS;
    gpio_put( TRACE_DEADLINE_MISS_PIN, 1 );
}

void vTraceClearDeadlineMissSignal( void )
{
    ulDeadlineMissHoldTicks = 0u;
    gpio_put( TRACE_DEADLINE_MISS_PIN, 0 );
}

void vTraceDeadlineMissTick( void )
{
    if( ulDeadlineMissHoldTicks > 0u )
    {
        ulDeadlineMissHoldTicks--;

        if( ulDeadlineMissHoldTicks == 0u )
        {
            gpio_put( TRACE_DEADLINE_MISS_PIN, 0 );
        }
    }
}

void vTraceRecordDeadlineMiss( uint32_t ulTaskId )
{
    if( __get_current_exception() != 0u )
    {
        prvTraceQueueDeadlineMissEvent( ulTaskId );
    }
    else
    {
        vTraceUsbPrint( "Deadline miss: task id=%lu\r\n", ( unsigned long ) ulTaskId );
    }
}

void vTraceFlushDeadlineMissEvents( void )
{
    UBaseType_t uxIndex;
    uint32_t ulDroppedEvents;

    if( __get_current_exception() != 0u )
    {
        return;
    }

    taskENTER_CRITICAL();
    {
        ulDroppedEvents = ulDeadlineMissDroppedEvents;
        ulDeadlineMissDroppedEvents = 0u;
    }
    taskEXIT_CRITICAL();

    if( ulDroppedEvents != 0u )
    {
        vTraceUsbPrint( "[deadline miss] dropped %lu deferred trace events\r\n",
                        ( unsigned long ) ulDroppedEvents );
    }

    for( uxIndex = 0u; uxIndex < TRACE_DEADLINE_MISS_EVENT_CAPACITY; uxIndex++ )
    {
        TraceDeadlineMissEvent_t xEvent;
        BaseType_t xHasEvent = pdFALSE;

        taskENTER_CRITICAL();
        {
            if( xDeadlineMissEvents[ uxIndex ].xValid != pdFALSE )
            {
                xEvent = xDeadlineMissEvents[ uxIndex ];
                xDeadlineMissEvents[ uxIndex ].xValid = pdFALSE;
                xHasEvent = pdTRUE;
            }
        }
        taskEXIT_CRITICAL();

        if( xHasEvent != pdFALSE )
        {
            vTraceUsbPrint( "Deadline miss: task id=%lu\r\n",
                            ( unsigned long ) xEvent.ulTaskId );
        }
    }
}

void vApplicationWcetOverrunHook( uint32_t ulTaskId )
{
    if( __get_current_exception() != 0u )
    {
        prvTraceQueueWcetOverrunEvent( ulTaskId );
    }
    else
    {
        vTraceUsbPrint( "WCET overrun: task id=%lu\r\n", ( unsigned long ) ulTaskId );
    }
}

void vTraceFlushWcetOverrunEvents( void )
{
    UBaseType_t uxIndex;
    uint32_t ulDroppedEvents;

    if( __get_current_exception() != 0u )
    {
        return;
    }

    taskENTER_CRITICAL();
    {
        ulDroppedEvents = ulWcetOverrunDroppedEvents;
        ulWcetOverrunDroppedEvents = 0u;
    }
    taskEXIT_CRITICAL();

    if( ulDroppedEvents != 0u )
    {
        vTraceUsbPrint( "[WCET] dropped %lu deferred overrun trace events\r\n",
                        ( unsigned long ) ulDroppedEvents );
    }

    for( uxIndex = 0u; uxIndex < TRACE_WCET_OVERRUN_EVENT_CAPACITY; uxIndex++ )
    {
        TraceWcetOverrunEvent_t xEvent;
        BaseType_t xHasEvent = pdFALSE;

        taskENTER_CRITICAL();
        {
            if( xWcetOverrunEvents[ uxIndex ].xValid != pdFALSE )
            {
                xEvent = xWcetOverrunEvents[ uxIndex ];
                xWcetOverrunEvents[ uxIndex ].xValid = pdFALSE;
                xHasEvent = pdTRUE;
            }
        }
        taskEXIT_CRITICAL();

        if( xHasEvent != pdFALSE )
        {
            vTraceUsbPrint( "WCET overrun: task id=%lu\r\n",
                            ( unsigned long ) xEvent.ulTaskId );
        }
    }
}

#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
void vTraceRecordMPOverrunEvent( const char * pcPolicy,
                                 const char * pcReason,
                                 const char * pcTaskName,
                                 uint32_t ulTaskCode,
                                 uint32_t ulCoreID,
                                 uint32_t ulNowTick,
                                 uint32_t ulReleaseTick,
                                 uint32_t ulDeadlineTick,
                                 uint32_t ulExecTicks,
                                 uint32_t ulWcetTicks,
                                 uint32_t ulNextReleaseTick )
{
    UBaseType_t uxSavedInterruptStatus;
    UBaseType_t uxSlot;

    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    {
        uxSlot = uxMPOverrunWriteIndex;
        uxMPOverrunWriteIndex = ( UBaseType_t ) ( ( uxMPOverrunWriteIndex + 1u ) %
                                                 TRACE_MP_OVERRUN_EVENT_CAPACITY );

        if( xMPOverrunEvents[ uxSlot ].xValid != pdFALSE )
        {
            ulMPOverrunDroppedEvents++;
        }

        xMPOverrunEvents[ uxSlot ].pcPolicy = ( pcPolicy != NULL ) ? pcPolicy : "unknown";
        xMPOverrunEvents[ uxSlot ].pcReason = ( pcReason != NULL ) ? pcReason : "unknown";
        prvTraceCopyTaskName( xMPOverrunEvents[ uxSlot ].pcTaskName, pcTaskName );
        xMPOverrunEvents[ uxSlot ].ulTaskCode = ulTaskCode;
        xMPOverrunEvents[ uxSlot ].ulCoreID = ulCoreID;
        xMPOverrunEvents[ uxSlot ].ulNowTick = ulNowTick;
        xMPOverrunEvents[ uxSlot ].ulReleaseTick = ulReleaseTick;
        xMPOverrunEvents[ uxSlot ].ulDeadlineTick = ulDeadlineTick;
        xMPOverrunEvents[ uxSlot ].ulExecTicks = ulExecTicks;
        xMPOverrunEvents[ uxSlot ].ulWcetTicks = ulWcetTicks;
        xMPOverrunEvents[ uxSlot ].ulNextReleaseTick = ulNextReleaseTick;
        xMPOverrunEvents[ uxSlot ].xValid = pdTRUE;
    }
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );
}

void vTraceFlushMPOverrunEvents( void )
{
    UBaseType_t uxIndex;
    uint32_t ulDroppedEvents;

    taskENTER_CRITICAL();
    {
        ulDroppedEvents = ulMPOverrunDroppedEvents;
        ulMPOverrunDroppedEvents = 0u;
    }
    taskEXIT_CRITICAL();

    if( ulDroppedEvents != 0u )
    {
        vTraceUsbPrint( "[MP EDF] dropped %lu deferred overrun/deadline trace events\r\n",
                        ( unsigned long ) ulDroppedEvents );
    }

    for( uxIndex = 0u; uxIndex < TRACE_MP_OVERRUN_EVENT_CAPACITY; uxIndex++ )
    {
        TraceMPOverrunEvent_t xEvent;
        BaseType_t xHasEvent = pdFALSE;

        taskENTER_CRITICAL();
        {
            if( xMPOverrunEvents[ uxIndex ].xValid != pdFALSE )
            {
                xEvent = xMPOverrunEvents[ uxIndex ];
                xMPOverrunEvents[ uxIndex ].xValid = pdFALSE;
                xHasEvent = pdTRUE;
            }
        }
        taskEXIT_CRITICAL();

        if( xHasEvent != pdFALSE )
        {
            vTraceUsbPrint( "[MP %s EDF] %s: task=%s tag=%lu core=%lu now=%lu release=%lu deadline=%lu exec=%lu wcet=%lu next_release=%lu\r\n",
                            xEvent.pcPolicy,
                            xEvent.pcReason,
                            xEvent.pcTaskName,
                            ( unsigned long ) xEvent.ulTaskCode,
                            ( unsigned long ) xEvent.ulCoreID,
                            ( unsigned long ) xEvent.ulNowTick,
                            ( unsigned long ) xEvent.ulReleaseTick,
                            ( unsigned long ) xEvent.ulDeadlineTick,
                            ( unsigned long ) xEvent.ulExecTicks,
                            ( unsigned long ) xEvent.ulWcetTicks,
                            ( unsigned long ) xEvent.ulNextReleaseTick );
        }
    }

    vTraceFlushWcetOverrunEvents();
}
#endif
