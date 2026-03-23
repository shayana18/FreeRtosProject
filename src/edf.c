/*
 * edf.c – Earliest Deadline First (EDF) scheduler layer for FreeRTOS.
 *
 * Background
 * ----------
 * FreeRTOS uses fixed priorities.  EDF requires dynamic priorities: the task
 * with the earliest absolute deadline must always run next.  This module
 * achieves EDF behaviour by calling vTaskPrioritySet() whenever the set of
 * active deadlines changes (i.e., at every new job release).
 *
 * Deadline miss handling – why NOT vTaskDelay()
 * ---------------------------------------------
 * A common mistake when implementing deadline miss recovery is to call
 * vTaskDelay() on the offending task.  This is wrong in an EDF context:
 *
 *  • vTaskDelay(N) suspends the caller for N ticks from *right now*.  If the
 *    task already overran, the next release is pushed even further out.  The
 *    period drifts, utilization calculations become invalid, and subsequent
 *    jobs inherit the accumulated error — the opposite of recovery.
 *
 *  • In EDF, the running task is by definition the one with the earliest
 *    deadline.  Deliberately suspending it with vTaskDelay() allows tasks
 *    with *later* deadlines to execute, which achieves nothing useful: those
 *    tasks were already schedulable and the current task still needs to
 *    finish its work.
 *
 * The correct approach used here:
 *  • vTaskDelayUntil() for periodic sleep – blocks until an absolute future
 *    tick rather than a relative offset, so the period grid is preserved.
 *  • edfTaskDeadlineMissHandler() for miss recovery – fast-forwards
 *    xNextRelease to the next future period boundary, dropping the late job
 *    rather than trying to catch up.
 */

#include "edf.h"

#if configUSE_EDF

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Module-private state
 * ---------------------------------------------------------------------- */

static EDFTaskParams_t *xEDFRegistry[ EDF_MAX_TASKS ];
static UBaseType_t      uxEDFCount = 0U;

/* -------------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */

/*
 * edfAdmitTask()
 *
 * Admission control based on the Liu & Layland utilization bound (U <= 1).
 * Sufficient and necessary for implicit-deadline tasks; used here as a
 * conservative check for constrained-deadline tasks as well.
 *
 * Utilization is computed in fixed-point (scaled by 1 000) to avoid
 * floating-point arithmetic on embedded targets.
 */
EDFAdmitResult_t edfAdmitTask( EDFTaskParams_t *pxParams )
{
    uint32_t ulTotalUtil;
    UBaseType_t i;

    /* Validate parameters. */
    if( ( pxParams == NULL )
        || ( pxParams->xPeriod == 0U )
        || ( pxParams->xRelativeDeadline == 0U )
        || ( pxParams->xRelativeDeadline > pxParams->xPeriod )
        || ( pxParams->xWCET == 0U )
        || ( pxParams->xWCET > pxParams->xRelativeDeadline ) )
    {
        return EDF_ADMIT_FAIL;
    }

    if( uxEDFCount >= EDF_MAX_TASKS )
    {
        return EDF_ADMIT_FAIL;
    }

    /* Sum existing utilization (fixed-point, × 1 000). */
    ulTotalUtil = 0U;
    for( i = 0U; i < uxEDFCount; i++ )
    {
        ulTotalUtil += ( xEDFRegistry[ i ]->xWCET * 1000U )
                       / xEDFRegistry[ i ]->xPeriod;
    }
    /* Add the new task's utilization. */
    ulTotalUtil += ( pxParams->xWCET * 1000U ) / pxParams->xPeriod;

    /* Reject if total utilization would exceed 1.000 (represented as 1 000). */
    if( ulTotalUtil > 1000U )
    {
        printf( "[EDF] Admission REJECTED: utilization %lu.%03lu > 1\n",
                (unsigned long)( ulTotalUtil / 1000U ),
                (unsigned long)( ulTotalUtil % 1000U ) );
        return EDF_ADMIT_FAIL;
    }

    /* Admit the task. */
    xEDFRegistry[ uxEDFCount ] = pxParams;
    uxEDFCount++;

    printf( "[EDF] Task admitted. Total utilization: %lu.%03lu\n",
            (unsigned long)( ulTotalUtil / 1000U ),
            (unsigned long)( ulTotalUtil % 1000U ) );

    return EDF_ADMIT_OK;
}

/*
 * edfUpdatePriorities()
 *
 * Reassign FreeRTOS task priorities so the task with the earliest absolute
 * deadline runs at the highest priority (EDF invariant).
 *
 * Priority mapping:
 *   rank 0 (earliest deadline) → configMAX_PRIORITIES - 1 (highest)
 *   rank n-1 (latest deadline) → 1 (lowest non-idle priority)
 *
 * Ties in deadline are broken arbitrarily (stable relative to registry
 * insertion order) which is acceptable for EDF.
 */
void edfUpdatePriorities( void )
{
    UBaseType_t i, j, uxRank, uxPriority;

    for( i = 0U; i < uxEDFCount; i++ )
    {
        if( xEDFRegistry[ i ]->xHandle == NULL )
        {
            continue;
        }

        /* Count how many other tasks have a strictly earlier deadline. */
        uxRank = 0U;
        for( j = 0U; j < uxEDFCount; j++ )
        {
            if( ( j != i ) &&
                ( xEDFRegistry[ j ]->xAbsDeadline <
                  xEDFRegistry[ i ]->xAbsDeadline ) )
            {
                uxRank++;
            }
        }

        /*
         * Map rank → priority.
         * rank 0  (earliest deadline) → configMAX_PRIORITIES - 1
         * rank n-1 (latest deadline)  → 1
         * Clamp to [1, configMAX_PRIORITIES - 1] so we never touch the
         * idle task priority (0).
         */
        if( uxRank >= ( UBaseType_t )( configMAX_PRIORITIES - 1U ) )
        {
            uxPriority = 1U;
        }
        else
        {
            uxPriority = ( UBaseType_t )configMAX_PRIORITIES - 1U - uxRank;
        }

        vTaskPrioritySet( xEDFRegistry[ i ]->xHandle, uxPriority );
    }
}

/*
 * edfTaskDeadlineMissHandler()
 *
 * Handles a detected deadline miss for *pxParams*.
 *
 * Strategy: drop the late job and fast-forward to the next future period
 * boundary.  This prevents the task from queuing up every missed job and
 * saturating the CPU trying to execute them all back-to-back.
 *
 * Why NOT vTaskDelay()
 * --------------------
 * Calling vTaskDelay(xPeriod) here would suspend the task for one more
 * period measured from *right now* (i.e., from the overrun point).  That
 * compounds the drift instead of correcting it, and temporarily removes the
 * highest-urgency task from the ready queue for no benefit.
 *
 * Instead, xNextRelease is advanced to the first period boundary that lies
 * strictly in the future.  edfPeriodicDelay() then calls vTaskDelayUntil()
 * with that anchor, which correctly re-aligns the task on its period grid.
 */
void edfTaskDeadlineMissHandler( EDFTaskParams_t *pxParams )
{
    TickType_t xNow;

    pxParams->ulMissedDeadlines++;
    xNow = xTaskGetTickCount();

    printf( "[EDF] DEADLINE MISS – handle %p  missed=%lu  "
            "abs_deadline=%lu  now=%lu\n",
            (void *)pxParams->xHandle,
            (unsigned long)pxParams->ulMissedDeadlines,
            (unsigned long)pxParams->xAbsDeadline,
            (unsigned long)xNow );

    /*
     * Advance xNextRelease to the next period boundary strictly after xNow
     * by stepping forward in whole periods.  This is the re-synchronisation
     * step; it replaces any use of vTaskDelay() for "deadline miss handling".
     */
    while( pxParams->xNextRelease <= xNow )
    {
        pxParams->xNextRelease += pxParams->xPeriod;
    }
}

/*
 * edfPeriodicDelay()
 *
 * Called at the end of each job.  Sequence of operations:
 *
 *  1. Compare current tick count with xAbsDeadline.
 *  2. If a miss is detected → call edfTaskDeadlineMissHandler() (which
 *     advances the period anchor without using vTaskDelay()).
 *  3. Compute the absolute deadline for the NEXT job.
 *  4. Sleep with vTaskDelayUntil() until xNextRelease.
 *     vTaskDelayUntil() targets an absolute tick, so the period grid
 *     stays aligned even if the previous job ran slightly over.
 *     Contrast with vTaskDelay(xPeriod): that would delay relative to
 *     NOW, accumulating any overrun into the next period.
 *  5. After waking, refresh xAbsDeadline (vTaskDelayUntil updated
 *     xNextRelease to the actual wake time).
 *  6. Update EDF priorities for the freshly released jobs.
 *
 * Returns pdTRUE on time, pdFALSE if the job missed its deadline.
 */
BaseType_t edfPeriodicDelay( EDFTaskParams_t *pxParams )
{
    BaseType_t xOnTime = pdTRUE;

    /* Step 1-2: deadline miss detection and recovery. */
    if( xTaskGetTickCount() > pxParams->xAbsDeadline )
    {
        xOnTime = pdFALSE;
        edfTaskDeadlineMissHandler( pxParams );
    }

    /*
     * Step 3: absolute deadline for the next job.
     *
     * xNextRelease already points to the next release time (either the
     * natural next period or the fast-forwarded one after a miss).
     */
    pxParams->xAbsDeadline = pxParams->xNextRelease
                             + pxParams->xRelativeDeadline;

    /*
     * Step 4: sleep until xNextRelease using vTaskDelayUntil().
     *
     * vTaskDelayUntil() blocks until the absolute tick stored in
     * *pxNextRelease, then updates *pxNextRelease to that tick value.
     * This keeps the period anchor fixed regardless of when the task
     * actually woke up.
     *
     * Using vTaskDelay(xPeriod) instead would be WRONG here: it would
     * sleep for xPeriod ticks starting from the current overrun point,
     * shifting every future release by the overrun amount and making
     * deadline analysis incorrect.
     */
    vTaskDelayUntil( &pxParams->xNextRelease, pxParams->xPeriod );

    /*
     * Step 5: refresh absolute deadline now that vTaskDelayUntil() has
     * updated xNextRelease to the actual wake time.
     */
    pxParams->xAbsDeadline = pxParams->xNextRelease
                             + pxParams->xRelativeDeadline;

    /* Step 6: re-assign FreeRTOS priorities for the new job release. */
    edfUpdatePriorities();

    return xOnTime;
}

#endif /* configUSE_EDF */
