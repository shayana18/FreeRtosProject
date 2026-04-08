#pragma once

#include "schedulingConfig.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )
/**
 * Run EDF test case 4 (explicit deadlines):
 * - 3 periodic tasks, all created at startup.
 * - Uses D != T for each task.
 *
 * Intended usage: call once from main(). This function does not return.
 */
void edf_4_run(void);
#else
static inline void edf_4_run(void) {}
#endif
