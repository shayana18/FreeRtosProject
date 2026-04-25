# CBS future work

- Add automated trace checking for CBS tests so expected server deadline movements can be verified from decoded GPIO traces instead of manual waveform inspection only.
- Expand admission control beyond the current total-utilization check if the project needs stronger mixed periodic/CBS guarantees, especially for constrained-deadline periodic tasks.
- Add explicit tests for CBS server deletion, worker unbinding, invalid server handles, and stale outstanding-job recovery.
- Add a stress test that reaches `configCBS_MAX_SERVERS` and verifies clean rejection when the server table is full.
- Add richer UART/debug reporting for server deadline updates so future tests can assert exact deadline changes without relying on waveform timing alone.
- Consider a bounded pending-job queue per server if the assignment simplification is relaxed. The current implementation intentionally accepts only one outstanding job per worker.
- Revisit equal-deadline preemption policy if the CBS tie rule needs to be applied identically during both ready selection and unblock/preemption checks.
- Decide whether budget carryover should remain disabled or become a supported configuration path. The current test set assumes refreshed budget on deadline reset/postponement.
