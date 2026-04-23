# SRP design

This SRP implementation extends the EDF scheduler so shared binary semaphores are handled with the Stack Resource Policy instead of the original FreeRTOS fixed-priority resource behavior. Binary semaphores are the focus because that is the resource type required for Task 2. The goal was to keep EDF as the primary scheduling rule while adding the extra ceiling check SRP needs for safe resource access.

In EDF + SRP mode, the kernel keeps:

- an SRP ready list ordered by absolute deadline
- an SRP task registry
- each task's preemption level
- each task's declared resource claims
- each task's currently held resources
- optional shared-stack metadata

`xTaskCreate()` is overloaded again in SRP mode so the task's EDF timing parameters and SRP claim table are validated together. This makes task creation the place where the kernel can reject invalid resource IDs, reject duplicate claims, derive the task's preemption level from its relative deadline, and insert the task into the SRP data structures.

At runtime, EDF still gives the primary ordering: the scheduler scans ready tasks in deadline order. A ready task is allowed to run only if it also passes the SRP ceiling rule. In this implementation, that means its preemption level must be above the current system ceiling, or the task must already hold resources and be safe to resume.

Resource ceilings and the system ceiling are recomputed when SRP resources are acquired or released through the SRP semaphore wrappers. Recomputing is simple and defensible for this project because the resource set is statically declared at task creation time, and correctness is more important than saving a few cycles in the ceiling update path.

The optional shared-stack mode uses the SRP property that tasks at the same preemption level cannot require separate run-time stack regions at the same time. The kernel reserves one shared region per preemption level, sized for the largest task at that level. The current implementation uses a static layout, which matches the assignment's focus on measuring stack savings for a known task set.
