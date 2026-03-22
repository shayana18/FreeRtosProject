#pragma once

#include "schedulingConfig.h"

#if ( configUSE_EDF == 1 )
/**
 * Run EDF test case 5 (explicit deadlines, higher load):
 * - 4 periodic tasks, all created at startup.
 * - Uses constrained deadlines (D < T) to make the DBF test meaningful.
 *
 * Intended usage: call once from main(). This function does not return.
 */
void edf_5_run(void);
#else
static inline void edf_5_run(void) {}
#endif
