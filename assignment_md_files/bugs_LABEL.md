# EDF Task 1 limitations

- Shared synchronization resources are not supported as part of the Task 1 EDF model. Mutexes, semaphores, queues, and other event-based blocking primitives still use the original FreeRTOS fixed-priority ordering and wakeup logic rather than absolute-deadline ordering.
- As a result, EDF tasks that block on shared resources may still block and unblock mechanically, but the ordering of waiters, preemption decisions after unblock, and priority inheritance behavior are not EDF-correct.
- This limitation was left intentionally for Task 1 to avoid intrusive changes across the synchronization subsystem; EDF + resource sharing is deferred to the SRP implementation in Task 2.
