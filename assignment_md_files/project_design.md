# Project design methodology

## User-facing design

This project extends FreeRTOS with EDF, SRP, CBS, and multiprocessor EDF scheduling while keeping the user-facing API as close to normal FreeRTOS as possible. The main design choice was to preserve the same API entry points and use configuration-based function overloading behind them. For example, task creation still goes through `xTaskCreate()`, but the prototype and implementation selected for `xTaskCreate()` depend on the active scheduler configuration. This avoids adding algorithm-specific public functions such as `xTaskCreateEDF()`, `xTaskCreateSRP()`, or `xTaskCreateMPEDF()`. The benefit is that users select the scheduling algorithm by setting configuration flags, not by learning a separate API for each scheduler. When EDF is disabled, the standard FreeRTOS fixed-priority `xTaskCreate()` interface remains active, preserving compatibility with ordinary FreeRTOS applications.

## Compile-time specialization

The kernel implementation follows the same configuration-driven approach internally. EDF, SRP, CBS, and multiprocessor-specific metadata, ready lists, admission tests, and dispatch paths are wrapped in preprocessor guards such as `configUSE_EDF`, `configUSE_UP`, `configUSE_MP`, `configUSE_SRP`, `configUSE_CBS`, `GLOBAL_EDF_ENABLE`, and `PARTITIONED_EDF_ENABLE`. This keeps the selected scheduling mode explicit and prevents inactive features from being compiled into the final image. That is important for FreeRTOS because the kernel is expected to remain lightweight and predictable on embedded targets. A build that only uses base FreeRTOS does not carry EDF/SRP/CBS/MP scheduling code, while a build that enables one of the real-time scheduling modes includes only the code required for that selected mode.

Note that while this approach may make the code look more "intimidating" the API and binary size benefits make it worth while when caring and end user centeric approach to the developement of this problem

## Configuration safety

The project also uses compile-time checks in `schedulingConfig.h` to reject invalid scheduler combinations early. The main checks are:

- `configUSE_UP` and `configUSE_MP` cannot both be enabled.
- `GLOBAL_EDF_ENABLE` and `PARTITIONED_EDF_ENABLE` cannot both be enabled.
- EDF-based scheduling modes require `configUSE_EDF`.
- SRP and CBS require uniprocessor EDF in this project.
- SRP and CBS cannot run at the same time (this is a limitation imposed by us as developers for simplicity).

These restrictions keep each compiled kernel image in one coherent scheduling mode. Invalid combinations fail at compile time instead of producing ambiguous scheduler behavior at runtime.

## Resource-sharing scope

Resource-sharing behavior was intentionally restricted to SRP for this project. Base EDF, CBS, and MP EDF focus on their required scheduling behavior, while SRP is the mode responsible for reasoning about shared binary semaphores, resource ceilings, and resource-related blocking. This would not be ideal for a production RTOS, where resource sharing should be considered consistently across every scheduler configuration. For this assignment, keeping resource-sharing support inside SRP was a defensible scope decision because it matched the task breakdown and avoided spreading partial resource-protocol logic across scheduler modes that were not required to handle shared resources.

## CBS integration

CBS was implemented as a slightly separate entity because its algorithm is not just a different ordering rule for periodic tasks. EDF and SRP can be integrated mostly by changing task metadata, admission checks, ready-list ordering, and preemption eligibility. CBS, however, introduces a server abstraction with its own budget, period, deadline, remaining capacity, and aperiodic job state. The kernel still schedules CBS-managed work through EDF by assigning the server's current deadline to its worker task, but the server must also track budget consumption, replenish capacity, advance server deadlines, and accept external aperiodic job arrivals. For that reason, CBS support is split into guarded CBS server code plus small EDF hooks inside the scheduler, rather than being treated as only another `xTaskCreate()` variant.

## Stock FreeRTOS fallback

If all scheduler-extension flags are disabled, including `configUSE_EDF`, `configUSE_UP`, `configUSE_MP`, `configUSE_SRP`, `configUSE_CBS`, `GLOBAL_EDF_ENABLE`, `PARTITIONED_EDF_ENABLE`, and `configUSE_SRP_SHARED_STACKS`, the extension code compiles out and the system follows the base FreeRTOS fixed-priority scheduler. This fallback was verified by building the project with all extension flags set to `0U`. The base kernel path remains intact: the normal priority-based task creation API is selected, EDF-specific TCB fields and scheduler logic are omitted, and the standard tick and ready-list behavior is used.

Configuration details and supported flag combinations are documented in [configuration_guide.md](configuration_guide.md).

# Testing Methodology
For testing, we use an 8-channel Saleae logic analyzer. Each task in a test case is assigned a task ID, and that ID is broadcast over GPIO pins so the running task can be identified directly from the waveform capture. In Saleae single-parallel mode, the GPIO lines are decoded as a decimal value, which lets the trace show both which task was running and how long it ran for. With 8 channels, one line is reserved for the task-switch clock, leaving 7 lines for task-ID output and allowing up to 127 distinct task IDs in the general trace format. In MP tests, the channels are split across the two cores so each core can be observed independently. The GPIO assignment used by the tracing system is defined in `task_trace.h`.

To emulate computation time, the tests use a spin-based work function that keeps a task active for a controlled duration. This gives us a simple and repeatable way to model WCET-sized execution windows in the hardware traces. The implementation of this helper is in `test_utils.c`.

The saved images in the `test_assets` directory should also be interpreted carefully. The tests themselves were run over one hyperperiod, but the waveform captures included in the documentation are often shorter windows, typically up to about 20 seconds, so the relevant timing behaviour is easier to see and discuss clearly.
