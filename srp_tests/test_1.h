#pragma once

#include "schedulingConfig.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 1 ) )

void srp_1_run( void );

#else
static inline void srp_1_run( void ) {}

#endif
