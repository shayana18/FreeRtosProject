
// Configure what scheduling algorithm is to be used. If all 0 then use default FreeRTOS scheduling
#define configUSE_EDF 1
#define configUSE_SRP 1
#define configUSE_CBS 0

#if ( configUSE_SRP == 1 ) && ( configUSE_EDF != 1 )
    #error "configUSE_SRP can only be enabled when configUSE_EDF == 1"
#endif


/* SRP global resource configuration.
 * Resource types are indexed from 0..(configSRP_RESOURCE_TYPE_COUNT-1). */
#ifndef configSRP_RESOURCE_TYPE_COUNT
	#define configSRP_RESOURCE_TYPE_COUNT 0U
#endif

/* Optional application hook called after SRP resources are released.
 * Set to 1 to provide vApplicationSRPResourceReleaseHook() in application code. */
#ifndef configUSE_SRP_RESOURCE_RELEASE_HOOK
	#define configUSE_SRP_RESOURCE_RELEASE_HOOK 0
#endif

/* Shared run-time stack pool used by EDF+SRP stack-sharing support.
 * Size is expressed in StackType_t entries (not bytes). */
#ifndef configSRP_SHARED_STACK_SIZE
	#define configSRP_SHARED_STACK_SIZE 1024U
#endif

/* Maximum number of distinct SRP preemption levels expected in the task set.
 * One shared stack region is reserved per unique preemption level. */
#ifndef configSRP_SHARED_STACK_MAX_LEVELS
	#define configSRP_SHARED_STACK_MAX_LEVELS 32U
#endif
