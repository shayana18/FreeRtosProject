# MP implementation limitations

- MP EDF in this repo assumes independent periodic tasks. Shared-resource protocols are not integrated into the MP scheduling path.
- Global EDF uses a sufficient admission test rather than a necessary-and-sufficient one, so some feasible task sets may still be rejected conservatively.
- Partitioned EDF uses conservative placement and admission logic, so acceptance depends on both the sufficient global screen and the existence of a fitting partition.
- Partitioned EDF treats affinity as a one-hot partition assignment. Multi-bit affinity masks are not part of the partitioned EDF model.
- Dynamic core hotplug is not implemented. Cores are started through the existing SMP startup path and remain online for the duration of the run.
- MP trace testing cannot safely reuse the old single shared GPIO task-code bus; per-core trace-pin ownership is required for clean waveform interpretation.
