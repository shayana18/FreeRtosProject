#pragma once

#include "schedulingConfig.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )
/**
 * Run EDF test case 2 (higher total utilization, still schedulable).
 *
 * Intended usage: call once from main(). This function does not return.
 */
void edf_2_run(void);
#else
static inline void edf_2_run(void) {}
#endif
