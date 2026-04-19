#pragma once

#include "schedulingConfig.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )
/**
 * Run EDF test case 7 (implicit deadlines + admission control):
 * - Start with a schedulable baseline set where all tasks have D = T.
 * - Attempt to admit a task that should fail the utilization test (expected fail).
 * - Then admit a task that should pass the utilization test (expected success).
 *
 * Intended usage: call once from main(). This function does not return.
 */
void edf_7_run(void);
#else
static inline void edf_7_run(void) {}
#endif
