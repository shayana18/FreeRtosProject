
// Configure what scheduling algorithm is to be used. If all 0 then use default FreeRTOS scheduling
#define configUSE_EDF 1
#define configUSE_SRP 1
#define configUSE_CBS 0

#if ( configUSE_SRP == 1 ) && ( configUSE_EDF != 1 )
    #error "configUSE_SRP can only be enabled when configUSE_EDF == 1"
#endif

#if (configUSE_CBS == 1) && ( configUSE_EDF != 1)
	#error "configUSE_CBS can only be enabled when configUSE_EDF == 1"
#endif


/* CBS (Constant Bandwidth Server) global configuration.
 * Only relevant when configUSE_CBS == 1. */
#if ( configUSE_CBS == 1 )

	/* Maximum number of concurrent CBS servers. */
	#ifndef configCBS_MAX_SERVERS
		#define configCBS_MAX_SERVERS 4U
	#endif

	/* Maximum number of pending aperiodic jobs across all CBS servers. */
	#ifndef configCBS_MAX_PENDING_JOBS
		#define configCBS_MAX_PENDING_JOBS 32U
	#endif

	/* Allow budget carryover from one period to the next (0 = no, 1 = yes).
	 * For simplicity, default is no carryover. */
	#ifndef configCBS_ALLOW_BUDGET_CARRYOVER
		#define configCBS_ALLOW_BUDGET_CARRYOVER 0
	#endif

#endif /* #if ( configUSE_CBS == 1 ) */ 


/* SRP global resource configuration.
 * Resource types are indexed from 0..(configSRP_RESOURCE_TYPE_COUNT-1). */
#ifndef configSRP_RESOURCE_TYPE_COUNT
	// CHANGE PER TEST 
	#define configSRP_RESOURCE_TYPE_COUNT 2U
#endif

/* Optional application hook called after SRP resources are released.
 * Set to 1 to provide vApplicationSRPResourceReleaseHook() in application code. */
#ifndef configUSE_SRP_RESOURCE_RELEASE_HOOK
	#define configUSE_SRP_RESOURCE_RELEASE_HOOK 0
#endif

/* Controls whether SRP tasks use the shared stack backend or ordinary
 * per-task dynamic stacks. Set to 0 to keep SRP scheduling/resource logic
 * while disabling shared stack storage. */
#ifndef configUSE_SRP_SHARED_STACKS
	#define configUSE_SRP_SHARED_STACKS 1
#endif

/* Shared run-time stack pool used by EDF+SRP stack-sharing support.
 * Size is expressed in StackType_t entries (not bytes).
 * Only used when configUSE_SRP_SHARED_STACKS == 1. */
#ifndef configSRP_SHARED_STACK_SIZE
	#define configSRP_SHARED_STACK_SIZE 2048U
#endif

/* Guard words reserved below each SRP shared-stack region.
 * For downward-growing stacks this gives a canary area that can detect one
 * task writing below its assigned region into a lower shared region. */
#ifndef configSRP_SHARED_STACK_GUARD_WORDS
	#define configSRP_SHARED_STACK_GUARD_WORDS 16U
#endif

/* Maximum number of distinct SRP preemption levels expected in the task set.
 * One shared stack region is reserved per unique preemption level.
 * Only used when configUSE_SRP_SHARED_STACKS == 1. */
#ifndef configSRP_SHARED_STACK_MAX_LEVELS
	#define configSRP_SHARED_STACK_MAX_LEVELS 32U
#endif

#if ( configUSE_SRP_SHARED_STACKS != 0 ) && ( configUSE_SRP_SHARED_STACKS != 1 )
	#error "configUSE_SRP_SHARED_STACKS must be either 0 or 1"
#endif
