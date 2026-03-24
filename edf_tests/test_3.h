#pragma once

#include "schedulingConfig.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )
/**
 * Run EDF test case 3 (admission control):
 * - Attempt to add an unschedulable task (U_total would exceed 1).
 * - After ~10s, attempt to add it again.
 * - Then add a schedulable task (U_total < 1).
 *
 * Intended usage: call once from main(). This function does not return.
 */
void edf_3_run(void);
#else
static inline void edf_3_run(void) {}
#endif
