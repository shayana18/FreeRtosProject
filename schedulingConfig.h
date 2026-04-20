
// Uniprocessing vs multiprocessing selection
#define configUSE_UP  0U
#define configUSE_MP  1U
// EDF selection (if not use stock FP implementation for UP and MP)
#define configUSE_EDF 1U
// Uniprocessor scheduling config
#define configUSE_SRP 0
#define configUSE_CBS 0
// MP scheduling config
#define GLOBAL_EDF_ENABLE 1U
#define PARTITIONED_EDF_ENABLE 0

#if ( configUSE_UP == 1U ) && ( configUSE_MP == 1U )
    #error "configUSE_UP and configUSE_MP cannot both be enabled"
#endif

#if ( configUSE_EDF == 1U ) && ( configUSE_UP != 1U ) && ( configUSE_MP != 1U )
    #error "configUSE_EDF requires configUSE_UP == 1U or configUSE_MP == 1U"
#endif

#if ( configUSE_SRP == 1U ) && ( configUSE_CBS == 1U )
    #error "configUSE_SRP and configUSE_CBS cannot both be enabled"
#endif

#if ( ( configUSE_SRP == 1U ) || ( configUSE_CBS == 1U ) ) && \
    ( ( configUSE_UP != 1U ) || ( configUSE_EDF != 1U ) )
    #error "configUSE_SRP and configUSE_CBS require configUSE_UP == 1U and configUSE_EDF == 1U"
#endif

#if ( GLOBAL_EDF_ENABLE == 1U ) && ( PARTITIONED_EDF_ENABLE == 1U )
    #error "GLOBAL_EDF_ENABLE and PARTITIONED_EDF_ENABLE cannot both be enabled"
#endif

#if ( ( GLOBAL_EDF_ENABLE == 1U ) || ( PARTITIONED_EDF_ENABLE == 1U ) ) && \
    ( ( configUSE_MP != 1U ) || ( configUSE_EDF != 1U ) )
    #error "GLOBAL_EDF_ENABLE and PARTITIONED_EDF_ENABLE require configUSE_MP == 1U and configUSE_EDF == 1U"
#endif


/* CBS (Constant Bandwidth Server) global configuration.
 * Only relevant when configUSE_CBS == 1U. */
#if ( configUSE_CBS == 1U )

	/* Maximum number of concurrent CBS servers. */
	#ifndef configCBS_MAX_SERVERS
		#define configCBS_MAX_SERVERS 4U
	#endif

	/* Legacy CBS job-pool size. CBS currently allows only one active job per
	 * server and does not maintain a pending-job queue, so 1U is sufficient. */
	#ifndef configCBS_MAX_PENDING_JOBS
		#define configCBS_MAX_PENDING_JOBS 1U
	#endif

	/* Allow budget carryover from one period to the next (0 = no, 1U = yes).
	 * For simplicity, default is no carryover. */
	#ifndef configCBS_ALLOW_BUDGET_CARRYOVER
		#define configCBS_ALLOW_BUDGET_CARRYOVER 0
	#endif

#endif /* #if ( configUSE_CBS == 1U ) */ 


/* SRP global resource configuration.
 * Resource types are indexed from 0..(configSRP_RESOURCE_TYPE_COUNT-1U). */
#ifndef configSRP_RESOURCE_TYPE_COUNT
	// CHANGE PER TEST 
	#define configSRP_RESOURCE_TYPE_COUNT 2U
#endif

/* Optional application hook called after SRP resources are released.
 * Set to 1U to provide vApplicationSRPResourceReleaseHook() in application code. */
#ifndef configUSE_SRP_RESOURCE_RELEASE_HOOK
	#define configUSE_SRP_RESOURCE_RELEASE_HOOK 0
#endif

/* Controls whether SRP tasks use the shared stack backend or ordinary
 * per-task dynamic stacks. Set to 0 to keep SRP scheduling/resource logic
 * while disabling shared stack storage. */
#ifndef configUSE_SRP_SHARED_STACKS
	#define configUSE_SRP_SHARED_STACKS 0
#endif

/* Shared run-time stack pool used by EDF+SRP stack-sharing support.
 * Size is expressed in StackType_t entries (not bytes).
 * Only used when configUSE_SRP_SHARED_STACKS == 1U. */
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
 * Only used when configUSE_SRP_SHARED_STACKS == 1U. */
#ifndef configSRP_SHARED_STACK_MAX_LEVELS
	#define configSRP_SHARED_STACK_MAX_LEVELS 32U
#endif

#if ( configUSE_SRP_SHARED_STACKS != 0 ) && ( configUSE_SRP_SHARED_STACKS != 1U )
	#error "configUSE_SRP_SHARED_STACKS must be either 0 or 1U"
#endif
