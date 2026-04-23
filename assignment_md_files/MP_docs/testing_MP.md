# MP EDF testing

The MP test strategy focuses on the behavior that is unique to multiprocessor scheduling: simultaneous dispatch on two cores, cross-core preemption, migration, partition ownership, admission control, and runtime task creation.

## Hardware trace setup

In MP mode, both cores may switch tasks close together, so a single shared task-code bus is not reliable. The trace setup assigns each core its own GPIO bank:

- core 0 task-code bank: GPIO `2`, `3`, `4`
- core 0 task-switch strobe: GPIO `5`
- core 1 task-code bank: GPIO `6`, `7`, `8`
- core 1 task-switch strobe: GPIO `9`
- shared deadline-miss indicator: GPIO `15`

Each test task sets its application task tag to a small binary value. With three task-code pins per core, task tags are kept in the range `0..7` so the logic analyzer can decode which task is running on each core.

## General method

- Use GPIO traces to verify dispatch, preemption, migration, and partition ownership.
- Use `volatile` result flags for admission-control tests, where the important result is whether `xTaskCreate()` returned `pdPASS` or `pdFAIL`.
- Keep global EDF and partitioned EDF tests separately guarded so only tests for the active MP policy compile into meaningful runtime code.
- For small task sets, derive the expected schedule manually and compare the first few releases against the observed core banks.
- For higher-utilization or admission-bound tests, focus on acceptance/rejection and the absence of unexpected deadline-miss signals rather than trying to hand-derive a long schedule.

## Test cases

| Test | File / entry point | Scenario | Expected result | Status |
| --- | --- | --- | --- | --- |
| Global EDF basic dispatch | `mp_tests/global_edf_tests/test_1.c`, `mp_global_edf_1_run()` | Several unrestricted tasks are released at startup. | The two earliest-deadline ready jobs occupy the two cores; later-deadline tasks wait. | Implemented. GPIO pass criterion documented in source comments. |
| Global EDF preemption | `mp_tests/global_edf_tests/test_2.c`, `mp_global_edf_2_run()` | An earlier-deadline job is released while worse jobs are running. | The kernel yields the correct core and replaces the worse running job. | Implemented. GPIO pass criterion documented in source comments. |
| Global EDF migration | `mp_tests/global_edf_tests/test_3.c`, `mp_global_edf_3_run()` | A globally runnable task is allowed to execute in different jobs under changing interference. | The same unrestricted task can appear on different core banks across jobs. | Implemented. GPIO pass criterion documented in source comments. |
| Global EDF affinity enforcement | `mp_tests/global_edf_tests/test_5.c`, `mp_global_edf_5_run()` | Four tasks: two pinned (one per core) and two unrestricted. All released simultaneously (hyperperiod 12 s). | Core-pinned tasks appear only on their assigned core's GPIO bank throughout the run; unrestricted tasks may appear on either core as EDF ordering demands. | Implemented. Confirms that per-task affinity masks are respected by the global EDF scheduler even when an unrestricted scheduling slot exists on the pinned core. |
| Partitioned EDF basic dispatch | `mp_tests/partitioned_edf_tests/test_1.c`, `mp_partitioned_edf_1_run()` | Tasks are created with explicit one-hot partition affinities. | Each task appears only on its assigned core's GPIO bank. | Implemented. GPIO pass criterion documented in source comments. |
| Partitioned EDF explicit migration | `mp_tests/partitioned_edf_tests/test_2.c`, `mp_partitioned_edf_2_run()` | A controller changes a task's affinity from one core to another. | Before migration, the task appears only on the old core; after migration, it appears only on the new core. | Implemented. GPIO pass criterion documented in source comments. |
| MP admission control | `mp_tests/test_1.c`, `mp_edf_admission_1_run()` | Runtime and startup task creation include both schedulable and unschedulable candidates. | Bad tasks return `pdFAIL`; good tasks return `pdPASS` and later appear on GPIO if admitted. | Implemented. Result flags stored in `volatile` variables. |
| MP runtime task creation | `mp_tests/test_2.c`, `mp_edf_runtime_create_1_run()` | A controller creates tasks after the scheduler has started. | The admitted runtime task is inserted into the active MP EDF policy and can preempt normally; rejected task does not appear. | Implemented. Result flags stored in `volatile` variables. |
| Global EDF demo | `mp_tests/test_compare_glob.c`, `mp_compare_glob_run()` | Small global EDF task set with total utilization below one core. | Tasks remain globally runnable; the two earliest-deadline jobs can occupy both cores even though the total utilization is less than one. | Implemented. Designed for presentation/demo waveform. |
| Partitioned EDF best-fit demo | `mp_tests/test_compare_part.c`, `mp_compare_part_run()` | Three tasks are created without explicit affinity. | Online best-fit places the tasks on the partition that gives the highest safe post-insertion utilization. | Implemented. Designed for presentation/demo waveform. |
| Conservative global admission demo | `mp_tests/global_edf_tests/test_dhall.c`, `mp_test_dhall_run()` | Task set illustrates the gap between total capacity and the global EDF sufficient bound. | The candidate that violates `U_total <= m - (m - 1) U_max` is rejected even though raw capacity may look available. | Implemented. Result flags stored in `volatile` variables. |

## Build verification

The MP tests are configuration-dependent. To verify coverage, the project should be built at least once in each of these modes:

### Global EDF

```c
#define configUSE_EDF 1U
#define configUSE_UP  0U
#define configUSE_MP  1U
#define configUSE_SRP 0U
#define configUSE_CBS 0U
#define GLOBAL_EDF_ENABLE 1U
#define PARTITIONED_EDF_ENABLE 0U
```

### Partitioned EDF

```c
#define configUSE_EDF 1U
#define configUSE_UP  0U
#define configUSE_MP  1U
#define configUSE_SRP 0U
#define configUSE_CBS 0U
#define GLOBAL_EDF_ENABLE 0U
#define PARTITIONED_EDF_ENABLE 1U
```

The documentation review build check was run in both MP modes:

- Global EDF: `cmake --build build` completed successfully with the global EDF configuration active.
- Partitioned EDF: `cmake --build build` completed successfully and linked `FreeRtosProject.elf` with the partitioned EDF configuration active.

These build checks only verify that each MP policy compiles and links. The GPIO waveform expectations above still require a board run and logic-analyzer capture for full runtime validation.

## Pass/fail interpretation

A scheduling test is considered passing when the observed GPIO waveform matches the expected EDF ordering and core ownership described by the test. An admission-control test is considered passing when the expected `pdPASS` / `pdFAIL` values are recorded in the volatile result variables and rejected tasks never appear on the GPIO task-code banks.
