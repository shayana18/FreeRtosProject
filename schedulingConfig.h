
// Configure what scheduling algorithm is to be used. If all 0 then use default FreeRTOS scheduling
#define configUSE_EDF 0
#define configUSE_SRP 1
#define configUSE_CBS 0

/* SRP global resource table configuration.
 * Resource types are indexed from 0..(configSRP_RESOURCE_TYPE_COUNT-1).
 * Each table entry is the maximum available count for that resource type. */
#ifndef configSRP_RESOURCE_TYPE_COUNT
	#define configSRP_RESOURCE_TYPE_COUNT 0U
#endif

#ifndef configSRP_RESOURCE_CAPACITY_TABLE
	#define configSRP_RESOURCE_CAPACITY_TABLE { 1U }
#endif

/* Each table entry is the SRP ceiling for that resource type.
 * Ceiling values are in [0, configMAX_PRIORITIES-1].
 * A value of 0 means the resource does not raise system ceiling. */
#ifndef configSRP_RESOURCE_CEILING_TABLE
	#define configSRP_RESOURCE_CEILING_TABLE { 0U }
#endif

