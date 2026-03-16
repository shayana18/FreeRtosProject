#ifndef TASK_TRACE_H
#define TASK_TRACE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRACE_TASK_PIN0    2u
#define TRACE_TASK_PIN1    3u
#define TRACE_TASK_PIN2    4u

void vTraceTaskPinsInit( void );
void vTraceWriteTaskCode( uint32_t ulTaskCode );

#define traceTASK_SWITCHED_IN()    vTraceWriteTaskCode( ( uint32_t ) ( uintptr_t ) pxCurrentTCB->pxTaskTag )

#ifdef __cplusplus
}
#endif

#endif /* TASK_TRACE_H */
