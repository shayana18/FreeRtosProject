# SRP Bug Notes

## Idle task caused infinite spin

### Symptom
The system would run the first few SRP jobs correctly, then appear to stop rescheduling useful work. On the logic analyzer this looked like the CPU falling into the idle task, and in the debugger the kernel got stuck inside `prvPeriodicTaskNextReleaseAfter()`.

### What was actually going wrong
The SRP tick-accounting path assumed that any task in the SRP registry list was a periodic SRP task. That assumption was false.

Internal zero-period tasks, including the idle task, were also being inserted into `xSRPTaskRegistryList`. For those tasks:

- `xPeriodTicks = 0`
- `xWcetTicks = 0`

When the tick interrupt fired while the idle task was running, the SRP tick path treated idle like a normal periodic SRP task:

1. `xJobExecTicks` was incremented.
2. The check `xJobExecTicks >= xWcetTicks` became immediately true, because idle had `xWcetTicks = 0`.
3. The kernel called `prvPeriodicTaskNextReleaseAfter()`.
4. That function tried to advance by `xPeriodTicks`, but idle had `xPeriodTicks = 0`.
5. The loop never made progress, so the scheduler effectively spun forever.

Intuitively: the kernel accidentally asked, "when is the idle task's next period?" but idle is not a periodic SRP task, so the answer made no sense and the bookkeeping loop could not advance.

### Fix
We made the SRP bookkeeping mirror the EDF bookkeeping:

1. Only periodic SRP tasks are inserted into `xSRPTaskRegistryList`.
2. The SRP tick path only performs periodic runtime accounting when `xPeriodTicks != 0`.
3. `prvPeriodicTaskNextReleaseAfter()` now asserts that it is only called for a real periodic task.

### Result
The idle task is still the scheduler fallback when no SRP job is ready, but it is no longer misclassified as a periodic SRP task. That removed the infinite loop and allowed normal rescheduling to continue.

## Deadline miss detection broke at tick wrap

### Symptom
The SRP schedule ran correctly for several minutes, then around the first large tick wrap boundary the kernel suddenly started reporting deadline misses and the misses kept cascading afterwards.

### What was actually going wrong
The absolute deadline is stored in `TickType_t`, so it wraps naturally with the kernel tick count. That is expected.

The bug was in the miss check itself. The EDF/SRP tick path used a plain numeric comparison:

`xConstTickCount > xAbsDeadline`

That is only valid before wrap. After wrap, a future deadline can look numerically smaller than the current tick value even though it is still in the future.

Example:

- release time: `63000`
- relative deadline: `3000`
- true absolute deadline: `66000`
- wrapped stored deadline: `464`

The old code compared `63001 > 464` and declared a miss immediately. But `464` was just the wrapped representation of a future time, not evidence that the job was late.

Intuitively: the kernel treated the tick counter like a straight line, but the tick counter is actually a circle. After wrap, "later" no longer means "numerically bigger".

### Fix
We changed deadline miss detection to use a wrap-safe tick comparison helper instead of a raw `>` comparison.

The helper treats tick `A` as "after" tick `B` only when the forward wrapped distance from `B` to `A` is non-zero and stays within half of the tick range. That keeps the fix small and consistent with the current kernel structure while removing the false miss at wrap.

### Result
The kernel no longer reports a deadline miss simply because a wrapped absolute deadline happens to have a smaller stored numeric value.

## Shared-stack SRP hard faulted while dedicated-stack SRP worked

### Symptom
With `configUSE_SRP_SHARED_STACKS = 1`, `srp_tests/test_1.c` did not run correctly. The first few task switches looked plausible, but the system eventually hard faulted when task 3 should start running. With shared stacks disabled, the same SRP scheduling logic worked.

The key observable pattern was:

1. `T1` ran.
2. `T2` ran.
3. The scheduler selected `T3`.
4. The system faulted before `T3` could run normally.

That made the failure look like an SRP shared-stack problem rather than a general scheduler problem.

### Why this bug is important
This was the strongest evidence that SRP scheduling policy and SRP shared-stack storage are two separate concerns.

When shared stacks were disabled, SRP scheduling behaved like expected.
When shared stacks were enabled, the same task set failed.

So the working conclusion is:

- SRP scheduling/resource logic can work.
- The shared-stack backend is the remaining unstable part.

### Strongest evidence we collected

#### 1. `T1 -> T2` worked, but `T2 -> T3` did not
This matters because it narrows the fault window.

If all restores were broken, `T1 -> T2` would also fail. But `T2` was able to start and run. That suggests:

- task creation completed
- initial task frames were at least partly valid
- the scheduler was able to pick the next task correctly

The more likely failing path is therefore:

- `T2` runs
- `T2` is switched out
- something during save/restore bookkeeping damages `T3`
- `T3` is selected next and then faults on restore or first entry

#### 2. A saved-frame word for `T3` contained garbage
At one point, inspecting:

`((uint32_t *) xHardFaultDebugContext.xTaskSnapshot.pxTopOfStack)[14]`

produced:

`1218967557 = 0x48A7F805`

That is not a sane instruction address for RP2040 task entry code. A valid task entry address should look like flash code, typically around `0x1000xxxx`.

This strongly suggests that `T3`'s saved frame contents were corrupted before `T3` was restored.

#### 3. Lower-boundary shared-stack guard corruption was never detected
We added a guard band below each shared-stack region and checked it:

- on switch-out of the current task
- before switching into the next SRP task

The guard check never fired.

That result is important because it weakens the simplest theory:

- not a straightforward "task overflowed below its region into the next lower region"

Instead, it suggests one of these:

- corruption happens inside another task's valid shared region
- the wrong saved stack pointer is used
- save/restore bookkeeping writes to the wrong place even though the region boundary itself is not crossed

### What we tried and what each result meant

#### A. Disabled shared stacks
Change:

- `configUSE_SRP_SHARED_STACKS = 0`

Result:

- SRP scheduling worked

Interpretation:

- the SRP scheduler itself is not the main blocker
- the shared-stack backend is the fault domain

#### B. Increased `configSRP_SHARED_STACK_SIZE`
Change:

- increased the total shared-stack pool size

Result:

- no meaningful change

Interpretation:

- the problem was not simply "the total pool is too small"
- increasing global pool capacity does not fix bad bookkeeping

#### C. Increased per-task `STACK_DEPTH`
Change:

- increased stack depth in the SRP test

Result:

- still failed

Interpretation:

- not enough evidence for a normal FreeRTOS-recognized stack overflow
- did not rule out corruption entirely, but made "plain stack too small" less convincing

#### D. Enabled FreeRTOS stack overflow checking
Change:

- enabled `configCHECK_FOR_STACK_OVERFLOW`
- added `vApplicationStackOverflowHook()`

Result:

- the overflow hook was not hit

Interpretation:

- not a normal overflow detected by FreeRTOS
- this still did not eliminate corruption, because dormant task-frame corruption can bypass the usual overflow checks

#### E. Replaced the SRP-local busy spin with the shared `spin_ms()`
Change:

- removed the local SRP spin implementation
- used `spin_ms()` from `test_utils`

Result:

- failure persisted

Interpretation:

- the fault was not just due to the SRP test's custom busy-wait implementation

#### F. Removed the local period-delay wrapper and used `xTaskDelayUntil()` directly
Change:

- made SRP test structure closer to the EDF test style

Result:

- failure persisted

Interpretation:

- the bug was not just a test-wrapper artifact

#### G. Instrumented hard faults and deadline misses
Change:

- added hard-fault snapshot capture
- added first-deadline-miss snapshot capture

Result:

- snapshots were useful for narrowing the problem

Interpretation:

- the hard-fault capture was especially valuable because it showed the selected task and saved-frame state at failure time

#### H. Fixed the idle-task SRP periodic-accounting bug
Change:

- kept zero-period tasks out of the SRP periodic registry

Result:

- removed one unrelated infinite-spin bug

Interpretation:

- this was a real bug, but not the shared-stack corruption bug

#### I. Added wrap-safe deadline miss detection
Change:

- changed raw deadline-miss comparison to a wrap-safe tick comparison

Result:

- resolved the long-run false deadline miss near tick wrap

Interpretation:

- another real bug, but again separate from the shared-stack failure

#### J. Added shared-stack padding / guard reservation only
Change:

- reserved space below each region

Result:

- no meaningful behavioral change by itself

Interpretation:

- extra space alone was not enough
- detection mattered more than just slack

#### K. Added guard fill/check around SRP shared regions
Change:

- filled guard words below each region
- checked them on switch-out and switch-in

Result:

- guard corruption was never hit

Interpretation:

- simple region-underflow into the next lower region is not currently the best explanation

### Current leading theory
The leading theory is now:

- shared-stack save/restore bookkeeping is wrong for at least one transition
- `T2 -> T3` is the first visible failing handoff
- `T3`'s saved frame gets corrupted before or during the first attempt to restore it

More concretely, the likely fault is one of:

1. the wrong `pxTopOfStack` is saved for a shared-stack task
2. the wrong `pxTopOfStack` is restored for `T3`
3. a dormant saved frame is being overwritten inside a valid region, not by crossing the region boundary

The fact that `T1 -> T2` works but `T2 -> T3` fails is the strongest reason to suspect a bookkeeping failure that occurs when saving out `T2`, rather than a universal restore bug.

### What we did not successfully finish
The next ideal step was to set a hardware write watchpoint on the exact saved-frame word for `T3` that later became corrupted, but that was not completed.

That still remains the best direct proof we do not yet have.

### Best next debug step
If this bug is revisited, the best next step is:

1. use shared stacks
2. watch the exact saved-frame word inside `T3`'s region that later becomes garbage
3. run until the first write happens
4. inspect whether the write comes from:
   - PendSV save/restore
   - task-switch bookkeeping
   - some unrelated write while another task is running

That would tell us whether the bug is:

- wrong saved stack pointer bookkeeping
- wrong restore address
- or an in-region overwrite that does not cross the lower boundary guard

### Current status
Unresolved.

What is known with high confidence:

- SRP scheduling works with dedicated stacks
- shared-stack mode is what breaks
- the lower shared-region boundary did not trip
- at least one saved-frame word for `T3` became invalid before `T3` could run correctly

So the remaining work is squarely in the SRP shared-stack save/restore path, not in the basic SRP scheduler policy.
