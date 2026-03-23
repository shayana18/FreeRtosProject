#ifndef EDF_H
#define EDF_H

/*
 * EDF (Earliest Deadline First) scheduler support for FreeRTOS.
 *
 * Key design choice – deadline miss handling
 * ------------------------------------------
 * When a periodic job overruns its deadline, the naive fix of calling
 * vTaskDelay() is INCORRECT for EDF for two reasons:
 *
 *  1. vTaskDelay(N) suspends the task for N ticks measured from the current
 *     moment.  If the task already ran longer than expected, vTaskDelay()
 *     pushes the next release even further into the future, causing the
 *     period to drift and making the timing analysis invalid.
 *
 *  2. In EDF the running task is always the one with the earliest deadline.
 *     Voluntarily suspending it via vTaskDelay() just to let tasks with
 *     LATER deadlines execute does not help the real-time guarantee; it
 *     wastes the remaining budget of the current job without recovering the
 *     missed deadline.
 *
 * The correct primitives are:
 *  - vTaskDelayUntil() for periodic sleep: it blocks until an absolute
 *    tick count rather than a relative offset, so the period anchor is
 *    preserved even when a job finishes slightly late.
 *  - edfTaskDeadlineMissHandler() for miss recovery: it advances
 *    xNextRelease to the next future period boundary so that the task
 *    re-synchronises with its schedule instead of trying to catch up on
 *    every overdue job.
 */

#include "schedulingConfig.h"

#if configUSE_EDF

#include "FreeRTOS.h"
#include "task.h"

/* Maximum number of EDF tasks that can be admitted simultaneously. */
#define EDF_MAX_TASKS  16U

/*
 * Parameters describing a single EDF periodic task.
 * The application must fill in xPeriod, xRelativeDeadline and xWCET before
 * calling edfAdmitTask().  All other fields are managed by the EDF layer.
 */
typedef struct EDFTaskParams
{
    /* --- Set by the application before calling edfAdmitTask() --- */
    TickType_t   xPeriod;           /* Task period in ticks (T_i)             */
    TickType_t   xRelativeDeadline; /* Relative deadline in ticks (D_i <= T_i)*/
    TickType_t   xWCET;             /* Worst-case execution time in ticks      */

    /* --- Managed by the EDF layer --- */
    TaskHandle_t xHandle;           /* FreeRTOS task handle                    */
    TickType_t   xNextRelease;      /* Absolute tick of the next job release   */
    TickType_t   xAbsDeadline;      /* Absolute deadline of the current job    */
    uint32_t     ulMissedDeadlines; /* Running count of deadline misses        */
} EDFTaskParams_t;

/* Return value of edfAdmitTask(). */
typedef enum
{
    EDF_ADMIT_OK   = 0, /* Task admitted – utilization is feasible   */
    EDF_ADMIT_FAIL = 1  /* Task rejected – would make set infeasible */
} EDFAdmitResult_t;

/* --- Public API ---------------------------------------------------------- */

/*
 * edfAdmitTask()
 *
 * Admission control using the Liu & Layland utilization bound.  Returns
 * EDF_ADMIT_OK if the task can be added without exceeding U = 1, or
 * EDF_ADMIT_FAIL otherwise.  Call this BEFORE xTaskCreate().
 */
EDFAdmitResult_t edfAdmitTask( EDFTaskParams_t *pxParams );

/*
 * edfUpdatePriorities()
 *
 * Assign FreeRTOS priorities to all admitted tasks so that the task with
 * the earliest absolute deadline receives the highest priority (EDF rule).
 * Must be called from a task context, not an ISR.
 */
void edfUpdatePriorities( void );

/*
 * edfTaskDeadlineMissHandler()
 *
 * Called when a job has overrun its deadline.  Logs the miss and advances
 * xNextRelease to the next future period boundary so the task does NOT
 * attempt to execute every overdue job back-to-back (which would make
 * recovery impossible).
 *
 * Crucially, this function does NOT call vTaskDelay().  Using vTaskDelay()
 * to "handle" a miss would merely postpone the task by a fixed interval
 * from the current time, which does nothing to re-align the task with its
 * period grid and would let tasks with later deadlines run unnecessarily.
 */
void edfTaskDeadlineMissHandler( EDFTaskParams_t *pxParams );

/*
 * edfPeriodicDelay()
 *
 * Must be called at the end of each job (after all computation for the
 * current period is complete).  The function:
 *
 *  1. Detects whether the current job missed its deadline by comparing
 *     xTaskGetTickCount() against xAbsDeadline.
 *  2. On a miss, calls edfTaskDeadlineMissHandler() to skip the late job
 *     and advance the release schedule — NOT vTaskDelay(), which would
 *     only add a fixed offset from the current time and cause further drift.
 *  3. Sleeps using vTaskDelayUntil() until the next period boundary.
 *     vTaskDelayUntil() is the correct primitive because it targets an
 *     absolute wake time derived from the original release, keeping the
 *     period anchor stable over long running times.
 *  4. Updates xAbsDeadline for the next job.
 *  5. Calls edfUpdatePriorities() so EDF priority order is refreshed for
 *     the new set of active deadlines.
 *
 * Returns pdTRUE if the job met its deadline, pdFALSE on a miss.
 */
BaseType_t edfPeriodicDelay( EDFTaskParams_t *pxParams );

#endif /* configUSE_EDF */

#endif /* EDF_H */
