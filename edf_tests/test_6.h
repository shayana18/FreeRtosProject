#pragma once

#include "schedulingConfig.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )
/**
 * Run EDF test case 6 (explicit deadlines + admission control):
 * - Start with a schedulable baseline set (explicit deadlines).
 * - Attempt to admit a task that should fail the DBF test (expected fail).
 * - After ~10s, retry the failing task (expected fail), then add a schedulable task (expected success).
 *
 * Intended usage: call once from main(). This function does not return.
 */
void edf_6_run(void);
#else
static inline void edf_6_run(void) {}
#endif
