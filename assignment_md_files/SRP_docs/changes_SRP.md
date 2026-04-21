# SRP kernel changes

- Added SRP configuration flags in `schedulingConfig.h`, including `configUSE_SRP`, `configSRP_RESOURCE_TYPE_COUNT`, `configUSE_SRP_SHARED_STACKS`, `configSRP_SHARED_STACK_SIZE`, `configSRP_SHARED_STACK_GUARD_WORDS`, and `configSRP_SHARED_STACK_MAX_LEVELS`.
- Extended `TCB_t` and `StaticTask_t` with SRP metadata such as task preemption level, requested stack depth, SRP registry list item, declared resource claims, and currently held resources.
- Added `xReadySRPTasksList_UP` as an EDF-ordered ready list for SRP tasks and `xSRPTaskRegistryList_UP` as the kernel registry for SRP task metadata and ceiling recomputation.
- Overloaded the EDF `xTaskCreate()` path again in EDF + SRP mode so the caller can provide an SRP claim table and the kernel can validate claim IDs, reject duplicates, and derive the task's preemption level from its relative deadline.
- Added SRP ready-task selection logic so the scheduler scans the SRP ready list in deadline order and only chooses tasks that satisfy the current system-ceiling rule or are safe to resume while already holding resources.
- Added `prvSRPRecomputeSystemCeiling()` so the per-resource blocking ceilings and the global `uxSystemCeiling` are recomputed whenever SRP task or resource state changes.
- Added `xTaskSRPAcquireResource()` and `xTaskSRPReleaseResource()` in the kernel task layer to maintain per-task held resources and global resource activity.
- Added `xQueueSemaphoreTakeSRP()` and `xQueueSemaphoreGiveSRP()` in the queue/semaphore path so binary semaphores can participate in SRP accounting.
- Added delete-time cleanup so any SRP resources still held by a task are released before the task leaves the system.
- Added a kernel-owned SRP shared-stack buffer, per-level region bookkeeping, and guard-band support so tasks with the same preemption level can share run-time stack storage.
- Added `vTaskGetSRPSharedStackUsage()` to report reserved shared-stack bytes and the equivalent private-stack total for comparison.
- Fixed the SRP periodic accounting path so zero-period internal tasks such as the idle task are not inserted into the periodic SRP registry and cannot trigger an infinite next-release loop.
- Replaced the raw deadline-miss comparison with a wrap-safe tick comparison so long SRP runs do not report false deadline misses when the tick counter wraps.
