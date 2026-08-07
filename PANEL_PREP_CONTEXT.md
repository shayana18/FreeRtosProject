# Panel Prep — Presentation Brief & Session Handoff

**What this file is:** the governing document for a 20-minute technical presentation to the
Tesla Optimus firmware team. §2–§6 define *what the talk is*. §7–§10 are the technical
reference that supplies evidence for it. Point every new session at this file first.

**Rule for future sessions:** §3 (the spine) is the constitution. If a request or suggestion
doesn't serve the spine, say so before doing it. The failure mode of the last 20 hours of work
was **breadth** — a tour of features instead of a deep dive into one problem. Do not help
re-expand the scope. Optimize for **decisions made and slides finished, not options surveyed.**

---

## 1. The brief (verbatim from the recruiter)

> Please prepare a technical presentation to the team, on a topic of your choice, that is
> relevant to the position you are interviewing for. The purpose of this is to give you an
> opportunity to **teach the team, in depth, about a problem you've solved and are
> particularly proud of.** You can assume your audience will be technical — engineers and
> engineering leaders. It is best to have a slideshow presentation for the team to follow
> along with. The best presentations are ones where the team **walks away feeling like they've
> learned something new**, and gotten **insight into the way that you solve, and think about,
> complex problems.**
>
> Presentation should be 20 minutes long with a short amount of time at the end for Q&A.
> You are welcome to give a brief introduction of your background and experience, but please
> spend no more than 2 minutes on this.
>
> Hint: Discuss **why the technical problem was important to solve**, including **tradeoffs**
> that had to be made and the **solution's impact.**

### What the brief demands (decoded)

| Phrase | What it rules out |
|---|---|
| "a problem you've solved" — **singular** | A tour of four features. One problem, chased down. |
| "in depth" | Breadth is the enemy. Depth on one thing beats coverage of everything. |
| "learned something new" | Textbook EDF is not new to this room. **What separates "an RTOS" from "a real-time system"** is. |
| "how you think about complex problems" | The debugging story and the honest self-critique are **required**, not optional. |
| "why the problem was important" | Every section must open with the *gap*, never with the algorithm's name. |
| "tradeoffs" | Every capability needs a "what this cost me" line. |
| "impact" | Numbers: 13,200→4,000 B, U=0.936 admitted, the non-preemption at t≈15 s, 55%→25%. |

**Audience:** Tesla Optimus firmware engineers and engineering leaders. Assume they know what
an RTOS is, what a context switch is, and roughly what EDF is. Do **not** assume they have
audited what their RTOS actually guarantees them — that's the gap the talk fills.

**Role:** Embedded Software Engineer, new grad. They are screening for correct mental models,
root-cause instinct, communication, and the ability to evaluate one's own work against
production constraints.

---

## 2. What a good version of this talk looks like

**A) As a presentation:** visually driven, low text density, one idea per slide, logical causal
flow, no distractions. Diagrams and traces carry the meaning; the speaker carries the detail.
Code appears only where the *shape of the code is the point*.

**B) As a technical deep dive:** rigorous without being intimidating. Every mechanism is
introduced by **the capability the kernel is missing**, never by its name first. The audience
should be able to predict the next section before it arrives.

**The single test for every slide:** *does this advance the spine in §3?* If it's true,
impressive, and doesn't advance the spine — appendix.

---

## 3. THE SPINE (the constitution)

### Thesis

> **FreeRTOS gives you a scheduler. It does not give you a real-time system.**
>
> It will faithfully run the highest-priority ready task — and that is all it promises. It
> cannot express a deadline, will not tell you whether your task set is even schedulable,
> cannot absorb work that doesn't arrive on a schedule, cannot bound how long you'll wait on a
> lock, and makes every task pay for its own worst-case stack.
>
> **I added those five capabilities to the kernel. This talk is how each one was built inside
> a shipping kernel, and what each one cost.**

This is the teach. Most of the room ships on FreeRTOS. The word "real-time" in the name is
doing a lot of unexamined work, and naming precisely what the kernel does *not* guarantee is
genuinely new to most engineers who use it. That framing is the hook, the agenda, and the
thesis simultaneously.

**The contribution is the implementation, not the theory.** None of these algorithms are the
candidate's invention, and the talk must not pretend otherwise. What's his is: where each one
hooks into a real kernel, what data structures it needed, what it broke, and what it cost.
That is the thing the room hasn't done. **See §5 — code is central to this deck, not an
appendix to a theory lecture.**

### The five gaps — this table IS the agenda slide

| # | What a hard real-time system needs | What FreeRTOS gives you | What I built |
|---|---|---|---|
| 1 | **Express urgency as a deadline** | A static priority number, assigned offline. **No TCB field means "due at."** Urgency can't change at runtime. | **EDF** — the kernel orders by absolute deadline |
| 2 | **Refuse a task set that can't meet its deadlines** | **Nothing — for any policy.** Create tasks until infeasible; the kernel runs it and you find out in the field. | **Admission control** — an offline-analysis hook in `xTaskCreate` |
| 3 | **Absorb work that doesn't arrive on a schedule** | Nothing. An aperiodic task either starves or steals from the hard set. | **CBS** — a bandwidth reservation that gives aperiodic work an *enforced* deadline |
| 4 | **Bound how long you wait on a lock** | Priority inheritance — bounds inversion, does **not** prevent deadlock, and becomes a **no-op** once tasks share a priority level | **SRP** — preemption-level ceilings; deadlock-free by construction |
| 5 | **Pay for stack once, not per task** | Every task allocates its own worst-case stack, resident for the life of the system | **SRP shared stacks** — one region per preemption level |

### Framing notes that must not drift

**Gap 1 is dynamic priority, NOT utilization.** The headline is: *priority is assigned offline
and cannot express "due at"; EDF makes urgency a runtime property.* Higher achievable
utilization is a **consequence worth one sentence**, not the pitch. And keep the honest caveat
— it reads as maturity, not hedging:

> *"EDF isn't automatically the right answer for hard real-time. Under overload, fixed priority
> degrades predictably — you know in advance who suffers. EDF's overload behavior is global and
> can cascade. You trade determinism-under-overload for expressiveness."*

That caveat is what sets up Gap 2 as the thing that buys the determinism back.

**Gap 2 is independent and is arguably the most universally relevant contribution.** Do NOT
present admission control as a corollary of EDF. The gap is that **FreeRTOS performs no
schedulability analysis for any policy, fixed priority included** — every engineer in the room
has shipped a task set nobody proved. The scheduling policy determines *which test runs*, not
*whether the capability exists*. EDF's contribution is that it makes the test **necessary and
sufficient** rather than a conservative bound — a quality argument about the test, not the
reason to build it.

⚠ **Verified constraint on how to word this.** `tasks.c:4568` and `:4574` are the only admission
call sites, and they run `prvTestEDFUtilPerCoreWithNewQ32` / `prvEDFDemandTestWithNew`. **There
is no fixed-priority / response-time test in the tree**, and "Fixed Priority Admission Control"
is on the V2 list (slide 24). So "be it EDF or fixed priority" **dies to one follow-up.** The
version that survives and is barely weaker:

> *"FreeRTOS does no schedulability analysis for any policy — fixed priority included. I put an
> admission hook in `xTaskCreate` and implemented the EDF tests behind it: utilization for
> implicit deadlines, processor-demand analysis for constrained ones. The hook is
> policy-agnostic; adding fixed priority is response-time analysis in the same slot."*

### Two real dependencies — say these out loud

Not every gap chains. **Only these two links are real**, and asserting more than this is
overclaiming:

```
(1) EDF collapses all EDF tasks onto ONE FreeRTOS priority level
        └──► xTaskPriorityInherit has nothing left to raise ──► (4) SRP becomes REQUIRED
                                                                      │
(4) gives every task a static PREEMPTION LEVEL ───────────────► (5) STACK SHARING
    tasks at the same level provably cannot be live                 falls out free
    simultaneously
```

- **1→4:** *"Every EDF task lives at the same FreeRTOS priority level. So
  `xTaskPriorityInherit` has nothing to raise — mutexes still compile, still run, and silently
  stop bounding inversion. Dynamic priority scheduling needs a resource protocol that doesn't
  key on priority at all."*
- **4→5:** *"SRP gives each task a static preemption level, and the ceiling rule guarantees a
  task at the same level can never preempt one already running. Two tasks that can't be
  simultaneously live don't need two stacks. The memory win isn't a feature I added — it's a
  corollary of the protocol."*

Gaps 2 and 3 stand on their own. Gap 3's only link is that CBS's periodic *registration*
(C=Qs, T=Ts) is what lets aperiodic work pass the Gap-2 admission test — worth one sentence in
the CBS section, not a structural claim.

### The recurring visual (the spine made visible)

**The five-gap table from the agenda returns as a scoreboard** — one row lit per section. The
audience always knows where they are and how much is left. Cheapest possible navigation aid
for a 20-minute talk, and it makes five sections feel like one build rather than five topics.

**Second recurring visual: one ready-list diagram, five states.** Same frame each time, one
element added:

| Beat | The picture |
|---|---|
| FreeRTOS today | Array of priority ready lists; selector walks down to the first non-empty |
| + EDF | Collapses to **one** list sorted by absolute deadline; head = next to run |
| + Admission | A gate *before* the list — tasks that don't fit never get in |
| + CBS | A synthetic-deadline feeder *into* the list |
| + SRP | A ceiling gate *at dispatch* — head of list can be held back |

Building these two visuals is the highest-leverage work left in the deck.

---

## 4. Slide plan and time budget

**19 minutes of content + 1 minutes intro. ~22 slides.** (Current deck is 37 — §6.)

**Slide count and time are decoupled here** — see §5.1. A "stop" slide adds one code panel to a
diagram already on screen and costs **30–40 s**; a slide carrying a new idea costs 60–70 s. 22
slides is not 22 ideas, it's ~9 ideas with their implementations revealed in place.

| Time | Beat | Slides | Notes |
|---|---|---|---|
| 0:00–1:00 | Title + intro | 2 | **Hard cap 1 min.** Grad date, Bedrock, UBC FE, Rivian/Microchip. Brief is "no more than 2" — coming in at 1 buys a minute of technical content and signals you know what they came for. |
| 1:00–3:00 | **How FreeRTOS picks a task** → **the five-gap table** | 2 | Ready-list array + selector, then the gap table (§3). End on *"none of these are bugs — they're the boundary of what the kernel promises."* Agenda + thesis in one slide. |
| 3:00–4:00 | Scope, assumptions, **how the kernel was modified** | 2 | **"Constrained deadlines, D ≤ T"** — not "implicit" (§9.7). WCET known. Board returned → some results reasoned. Then §5.3: config guards, overloaded `xTaskCreate`, `#error` — *and* the "I'm eliding these from here on" line (§5.2). |
| 4:00–8:00 | **Gap 1 — express a deadline: EDF** | 5 | Flow frame + **3 stops** (list-value assignment, sorted insert, deadline compare on yield) + **Test 4 trace**. |
| 8:00–10:30 | **Gap 2 — refuse an infeasible set: admission** | 3 | Frame + **1 stop** (the Q32 test) + the *no analysis for any policy* framing. Cascade wording (§9.1). |
| 10:30–13:30 | **Gap 3 — absorb aperiodic work: CBS** | 4 | Open with *starve or steal*. Frame + **2 stops** (arrival rule, exhaustion rule) + Test 1 payoff. |
| 13:30–18:00 | **Gaps 4 & 5 — bound blocking, share stacks: SRP** | 5 | Frame + **3 stops** (preemption level, ceiling recompute, the selection filter) + **13,200→4,000** + **60 s debug beat** + test. |
| 18:00–19:00 | **What it cost / where it's weak** | 1 | See below. |
| 19:00–20:00 | Buffer + transition to Q&A | — | Do **not** plan to use the full 20. Overrunning is the most common failure. |

Gaps 4 and 5 share a section because 5 is a corollary of 4 — separating them breaks the
dependency argument (§3).

**If it runs long, cut in this order:** one EDF stop → one SRP stop → the CBS test slide (keep
the payoff line, say it over the rules slide). Never cut the debug beat or Test 4.

### The close (replaces "Future Improvements" + "Practical Applications")

Not a roadmap — a roadmap reads as an unfinished project. **Honest cost accounting** reads as
an engineer who knows where the bodies are, which is what a panel is screening for. Three
lines, all already evidenced in §7:

- **What it cost:** +N bytes/TCB; O(1) bitmap select → sorted insert; all EDF tasks collapse to
  one priority level, so the fixed-priority bitmap now only separates "EDF set" from "not."
- **Where it's weak:** six preemption sites still compare priority, not deadline (bounded 4 ms
  latency; ordering still correct); event lists still release FIFO, not earliest-deadline;
  the ready-list sort isn't tick-wrap-safe (~49 days).
- **What I'd change:** the CBS server object is pure indirection at 1:1 — it caused a real bug
  (§7.5). It only earns its keep at N:1.

Then **one spoken sentence** of practical application: *"the admission test is the piece I'd
reach for first in production — it turns an infeasible task set from a field failure into a CI
gate."*

---

## 5. Editorial rules (these are what make it fast to build)

### 5.1 Code is central — the FLOW-AND-STOP pattern

The contribution is the implementation (§3), so **code appears in every capability section.**
The constraint is never *how much* code but *whether each panel has one job.*

**The deck already contains both the right and wrong answer, and the contrast is the lesson:**
- `ppt/media/image3.png` (slide 11) — 7 lines, large font, one idea:
  `if( pxCurrentTCB->xAbsDeadline > ( pxTCB )->xAbsDeadline )`. That single comparison *is*
  dynamic priority. **This is the target.**
- `ppt/media/image13.png` (slide 21) — ~150 lines at ~6pt: config block, TCB extension,
  globals, `prvSRPComputePreemptionLevel`, `prvSRPRecomputeSystemCeiling`, and
  `prvSRPSelectReadyTask`, all on one slide. Unreadable on a video call. **This is the failure.**

Same author, same deck. image3 has one job; image13 has six.

**The pattern: one persistent flow diagram per capability, revealed in stops.**

Build a call-flow diagram for the capability (e.g. "what happens inside `xTaskCreate` under
this config") as 4–6 blocks. Then hold that diagram **fixed in the same screen position** across
several slides, lighting one block at a time with its code beside it.

```
┌──────────────────────────┬────────────────────────────────────────┐
│  FLOW  (≈40% width)      │  CODE PANEL  (≈60% width)              │
│                          │                                        │
│  ┌────────────────┐      │   /* prvAddTaskToReadyList — EDF */    │
│  │ validate args  │ dim  │                                        │
│  └────────────────┘      │   listSET_LIST_ITEM_VALUE(             │
│  ┌────────────────┐      │       &pxTCB->xStateListItem,          │
│  │ admission test │ dim  │       pxTCB->xAbsDeadline );           │
│  └────────────────┘      │                                        │
│  ┌────────────────┐      │►  vListInsert( &xReadyEDFTasksList_UP, │
│  │ INSERT SORTED  │ LIT ─┼──────  &pxTCB->xStateListItem );       │
│  └────────────────┘      │                                        │
│  ┌────────────────┐      │   /* was: vListInsertEnd() — FIFO */   │
│  │ yield if newer │ dim  │                                        │
│  └────────────────┘      │                                        │
└──────────────────────────┴────────────────────────────────────────┘
```

Why this works: the audience builds the mental model **once**, then never has to relocate their
eyes. Each new slide adds one code panel to a picture they already hold. It reads as a single
slide animating rather than five separate slides, so **slide count and talk time decouple** — a
stop slide costs 30–40 s, not the 60–70 s a new idea costs.

**Mechanics:**
- **Dim non-active blocks, never remove them.** Removing loses the map; dimming keeps it.
- **One connector line** from the lit block to the code panel. Not a bundle of arrows.
- **≤ 12 visible lines of code, ≥ 18 pt.** If it doesn't fit at 18 pt, it's two stops.
- **Highlight one line inside the panel**, even though the panel is already the focus.
- Open each capability with the **diagram alone, no code** (~20 s) so the path lands before any
  syntax appears.
- Number of stops scales with how surprising the mechanism is: EDF 3, admission 1, CBS 2,
  SRP 3. Same template throughout so it's learnable; varying depth so it isn't monotonous.
- **Build one frame template properly, then copy it.** This is what makes the remaining build
  fast: 5 frames of copy-paste, not 20 unique authoring decisions.

### 5.2 Code fidelity — simplified is correct, dishonest is not

Slide code does **not** have to be a byte-exact replica. Rules:

- **Keep real:** function names, type names, field names, control flow, and the data structures.
  An engineer who later opens the repo must recognize what they saw.
- **Elide freely:** `#if` config guards, `taskENTER_CRITICAL()` wrappers, `configASSERT`s,
  error-return plumbing, MISRA casts.
- **Label it once, on the first code slide:** *"Simplified — config guards and critical sections
  elided."* That single line reads as rigor and immunizes every later panel.
- **Show the config guards exactly once** — on the development-approach slide, where they're the
  *point* (non-invasive kernel modification). Then say *"I'm dropping these from here on; they
  wrap every change you're about to see."* Now the guards have been credited **and** removed.
- Never show: full struct definitions, the CBS admission stub (§7.7), the accessors, the
  equal-deadline tie-break code (§7.7).

### 5.3 The kernel-modification story is the differentiator — promote it

Slide 8 ("Development Approach") is currently a throwaway process slide. Under the §3 framing
it is **core evidence**: no new API surface, `xTaskCreate` overloaded by config, `#error` on
illegal combinations, ~60 conditional-compilation sites, zero impact on the stock kernel path.
Anyone can implement EDF in userspace; **modifying a shipping kernel in place without breaking
its contract** is the practical contribution and it's the part no other candidate's talk will
have. Give it real airtime, and let it justify the elision rule above.

### 5.4 Everything else

**Visuals.**
- Waveforms full-bleed and annotated. Box the region of interest. **Give the room a beat of
  silence** after showing a trace — let them find it before you say it.
- Include photos of the hand-drawn expected schedules. Predicting on paper and matching on
  hardware **is** the validation story, and it's a "how I think" artifact.
- Every test slide: hand-derived expected schedule → measured trace, in that order.

**Openings.** Every section opens with **the capability that's missing**, never the algorithm's
name. "An aperiodic task has no period, so EDF has nothing to sort it by" *then* the letters
C-B-S. Naming first makes it a lecture; naming second makes it a solution.

**Claims.**
- Distinguish **measured** from **reasoned** every single time. "The trace shows" vs "I'd
  expect." The board has been returned; anything not on a captured trace is *implemented, not
  hardware-validated* and must be labelled so.
- Never say "the assignment only required."
- Route Q&A to appendix slides **by name**. Have a one-sentence version of every appendix slide.

**Bullets.** Current deck's bullets are notes-to-self (function documentation). Slide text
should be the **claim**; the explanation is spoken.

---

## 6. Current deck state and diagnosis

**File:** `/Users/shayan.ajmal/Downloads/Untitled presentation-3.pptx` (37 slides).
(The Google Slides deck is on a different Google account than the connected Drive connector, so
MCP can't read it — File → Download → PPTX/PDF and point at the local path. Poppler is
installed if page images are needed for layout feedback.)

**How to read this deck properly — text alone is not enough.** Unzip the pptx, then:
- text: parse `<a:t>` runs out of `ppt/slides/slideN.xml`
- **images: `ppt/media/` + map them per slide via `ppt/slides/_rels/slideN.xml.rels`** — much of
  this deck's real content is code screenshots, invisible to a text extraction
- notes: `ppt/notesSlides/` — **currently empty; there are no speaker notes anywhere**

### The numbers that explain why it feels bad

- **37 slides / 19 min ≈ 31 s per slide.** Structurally impossible as built.
- **8 slides carry images** (2, 8, 9, 11, 13, 14, 17, 21) — mostly code screenshots.
- **The five test slides are genuinely empty: 12, 15, 18, 22, 23.** No text beyond the title and
  no image. **No waveform is in this deck yet.** These are the payoff slides.
- **Slides 4–9 are six slides of preamble** before any of the candidate's own work appears:
  roughly 7–8 minutes, ~40% of the talk, spent on background.
- **The code that is present is uncalibrated** — image3 (7 readable lines) and image13 (~150
  lines at ~6pt) sit in the same deck. See §5.1; this is the single most fixable problem.

**The encouraging read:** this isn't bad work, it's a skeleton that's too wide. What's missing
is exactly the good stuff. Every slide currently feels incomplete because the structure gives
it no single job — not because the content is weak. Narrowing the spine makes the remaining
build small and obvious.

### Disposition of every current slide

| # | Slide | Verdict |
|---|---|---|
| 1–2 | Title, Intro | **Keep.** 2 min hard cap. |
| 3 | Overview/agenda | **Replace with the five-gap table** (§3). The agenda *is* the thesis. |
| 4–5 | FreeRTOS Plumbing I & II | **Merge to 1–2 slides, diagram-led.** Currently function documentation. Keep only what's needed to land *"there is no field in the TCB that means 'due at'."* |
| 6 | Limitations & Motivations | **Merges into the gap table.** Its content is the "What FreeRTOS gives you" column. |
| 7 | Assumptions/Algorithms | **Split.** Assumptions stay (fix wording — §9.6, §9.7). The algorithm definitions move into their own sections; defining all three up front spoils each section's opening. |
| 8 | Development Approach | **Compress to 2 lines** on the scope slide, or appendix. Best line: *"invalid configurations are a compile error, not undefined behavior."* |
| 9 | Testing Methodology | **Fold into the first trace slide, ~20 s.** A standalone slide here delays the payoff. Full version → appendix. |
| 10–11 | EDF divider + Implementation | **Keep, rebuild.** Bullets → the ready-list transform diagram + ≤8 lines of sorted insert. |
| 12 | EDF Test *(empty)* | **BUILD — highest-priority slide in the deck.** Test 4 non-preemption (§7.1). |
| 13 | Admission Control & Miss Handling | **Keep, build out.** Necessary-and-sufficient, Q32 tradeoff, cascade framing. |
| 14–15 | SMP MP EDF Impl + Test | **→ APPENDIX.** See §6.1. Biggest single cut; buys ~3–4 min. |
| 16–18 | CBS divider, Impl, Test *(test empty)* | **Keep, rebuild + BUILD the test.** Lead with *starve or steal*, not the rules. |
| 19–20 | SRP divider + Background | **Keep, compress.** 15 bullets → inheritance-no-op + the ceiling rule. |
| 21 | SRP Implementation *(one ~150-line code image)* | **SPLIT into a frame + 3 stops** (§5.1). The content is good — see §8, recovered from `image13.png` — it's the packaging that fails. This slide is the clearest illustration of the whole problem. |
| 22 | SRP Stack Sharing + Debugging *(empty)* | **BUILD — second-highest priority.** 13,200→4,000 + the 60 s debug beat. |
| 23 | SRP Test *(empty)* | **BUILD.** |
| 24 | SRP+CBS V2 Improvements | **→ APPENDIX.** Replaced by the honest-cost close (§4). |
| 25 | Practical Applications | **CUT the slide.** One spoken sentence per section + one in the close. |
| 26–37 | Appendix (12, mostly empty) | **Keep as a bank.** Build the top 4 only: PIP vs SRP, EDF optimality proof, how timing guarantees are made, constrained-deadline analysis. Titles alone suffice for the rest — they signal depth and give Q&A somewhere to land. |

### 6.1 Why SMP MP EDF goes to the appendix

Real work, and it will feel painful to cut. Correct anyway:
1. **It isn't one of the five gaps.** It's a different axis (one core → two cores). Including it
   turns a capability argument back into a feature tour.
2. **It's the least defensible section.** Two known findings against it (§7.2): the `(m+1)/2`
   gate rejects perfectly schedulable sets, and MP admission ignores relative deadline while
   `test_dhall.c` uses a constrained-deadline task. Both are great *answers* and bad *claims*.
3. **It costs 3–4 minutes** — exactly the budget SRP and CBS need to stop being rushed.

**Preserve the credit with one spoken sentence** on the scope slide: *"I also built global and
partitioned SMP variants with online best-fit partitioning — there are appendix slides, happy
to go there in Q&A."* Full credit, zero minutes, and it invites a question you answer very well.

### 6.2 The three doubts, resolved

- **Debug story — KEEP, as 60 seconds inside SRP, not a section.** The single best "how I
  think" artifact in the project, and the brief explicitly asks for it. It fails only when
  given three slides. One visual, three sentences: *first hypothesis was the ceiling logic and
  it was wrong; the reframe was noticing guard-band damage correlated with **task creation
  order**, not runtime preemption pattern — which pointed at init, not scheduling; root cause
  was shared-stack region counters resetting during scheduler startup, after tasks had already
  reserved regions.* The transferable lesson — *"the correlation that doesn't fit your
  hypothesis is the data"* — is what they'll remember.
- **Future Improvements — CUT to appendix.** Replaced by "what it cost / where it's weak" (§4).
  A roadmap says "unfinished." A cost accounting says "I know exactly where the bodies are."
- **Practical Applications — CUT the slide.** Correct instinct. One sentence per section,
  spoken at the moment the mechanism is fresh. Keep an appendix slide as a Q&A landing spot.

---

## 7. Technical reference — evidence for the talk

All verified against code. **Line numbers drift — re-grep before quoting on a slide.**
Tags: **[SLIDE]** on a slide · **[SPOKEN]** say it, don't slide it · **[Q&A]** answer only ·
**[APX]** appendix.

### 7.1 Gap 1 — EDF core

**[SLIDE] The ready-list mechanism.** `prvAddTaskToReadyList` is overridden to set
`xStateListItem.xItemValue = xAbsDeadline` and call `vListInsert` (sorted) instead of
`vListInsertEnd` (FIFO). `vListInsert` (`list.c:192`) maintains **ascending** item value, so
head = earliest deadline. Ties are FIFO (`<=` in the loop). `vTaskSwitchContext` uses
`listGET_OWNER_OF_HEAD_ENTRY` — **not** `NEXT_ENTRY`, which would rotate `pxIndex` and break
the min-deadline property. Sentinel `xListEnd.xItemValue = portMAX_DELAY` (`list.c:62`) makes
the ordering total, and is why `portMAX_DELAY` deadlines (idle, non-EDF tasks) need the special
insert branch.

**[SLIDE — load-bearing for Gap 4]** All EDF tasks share one FreeRTOS priority level
(`configMAX_PRIORITIES - 1`); the fixed-priority bitmap now only distinguishes "EDF set" from
"non-EDF set." **Plant this fact here and collect it in the SRP section** — it's the 1→4 link
in the dependency chain.

**[SLIDE] EDF Test 4 — the money trace.** T1 (C=1000, T=5000, D=3000), T2 (C=1500, T=7000,
D=3500), T3 (C=1000, T=10000, D=8000). At t≈15 s T1 releases while T2 is mid-job. T1 has the
shorter *period*, so rate-monotonic would preempt. **It doesn't** — because T2's absolute
deadline (17.5 s) beats T1's (18 s). *"That single non-preemption is the whole capability."*
Box it on the trace. Silence for a beat.

**[SPOKEN, close slide] Six preemption sites were never converted to deadline comparisons.**
Because every EDF task is created at `configMAX_PRIORITIES - 1`,
`pxTCB->uxPriority > pxCurrentTCB->uxPriority` is always false between EDF tasks, so no yield is
requested — the task lands in the correct sorted position but doesn't preempt until the next
tick (**up to 4 ms latency; ordering stays correct**).
Sites: `xTaskRemoveFromEventList` (**the important one** — queue/semaphore unblock path),
`vTaskRemoveFromUnorderedEventList`, `xTaskResumeFromISR`, `xTaskAbortDelay`,
`xTaskGenericNotifyFromISR`, `vTaskGenericNotifyGiveFromISR`.
Converted correctly: `taskYIELD_ANY_CORE_IF_USING_PREEMPTION` (`tasks.c:100-111`),
`prvAddNewTaskToReadyList`, `xTaskResumeAll`, `xTaskIncrementTick` unblock path,
`xTaskGenericNotify` (task context — the one CBS depends on).

**[Q&A] Event lists are still fixed-priority sorted.** `xEventListItem`'s value is
`configMAX_PRIORITIES - uxPriority`, so multiple EDF tasks blocked on one semaphore are
released FIFO, not earliest-deadline-first.

**[SLIDE, in the SRP section] Mutex priority inheritance still operates on `uxPriority`**
(`xTaskPriorityInherit`), which is a no-op when all EDF tasks share one priority. **This is the
1→4 dependency — this is *why* SRP exists.**

**[Q&A] Tick wraparound.** `prvTickTimeIsAfter` (`tasks.c:574`) is wrap-safe (half-range
compare). The **ready-list sort is not** — absolute unsigned comparison, so a wrapped deadline
sorts before an unwrapped one. ~49 days at 32-bit ticks. Stock FreeRTOS solves this for delayed
lists with the overflow-list swap; there's no equivalent here. Good "where it's weak" line.

### 7.2 Gap 2 — admission control

**[SLIDE] Frame the gap first, the test second.** FreeRTOS runs **no schedulability analysis for
any policy**. Nothing stops a user creating a task set that provably cannot meet its deadlines;
the kernel schedules it and the failure shows up in the field. The contribution is an
**offline-analysis hook at `xTaskCreate` that can refuse the set.** See §3 for the exact wording
that survives a follow-up — **do not claim a fixed-priority test exists; it doesn't** (verified:
`tasks.c:4568`/`:4574` are the only call sites and both run EDF tests).

**[SLIDE] Then the quality argument.** Rate-monotonic's utilization bound (~69%) is *sufficient
only* — a set above it may still be schedulable, so the test has to be conservative. EDF on a
uniprocessor with implicit deadlines has a bound of 100% that is **necessary and sufficient** —
the test is a decision procedure, not a heuristic. That is the strongest formal statement in the
project; **say those words.** This is a claim about how good the test can be, **not** the reason
admission control was built.

**[SLIDE] Q32 fixed point** (`tasks.c:2272`, `prvTestEDFUtilPerCoreWithNewQ32`). `(C << 32) / T`
encodes C/T with 32 fractional bits; compare the sum against `1ULL << 32`. **This is the
tradeoff slide.** Reasons in order of strength:
(a) RP2040 is Cortex-M0+ with **no FPU** — `double` means libgcc soft-float inside a
scheduler-suspended region; (b) **determinism** — FP addition isn't associative and
round-to-nearest can flip a set sitting exactly at U=1.0 depending on registry order, whereas
integer addition is order-independent; (c) kernel code touching FP registers is a portability
hazard (`configUSE_TASK_FPU_SUPPORT`); (d) exact enough — 2⁻³² against a 4 ms tick quantization
that already dominates by ~8 orders of magnitude.
**[Q&A]** Floor division makes each term underestimate by <2⁻³², so the test is very slightly
*optimistic*, bounded by N·2⁻³². Round up per term for strict conservatism.

**[SPOKEN] Constrained deadlines (D<T)** use processor-demand / DBF analysis
(`prvEDFDemandTestWithNew`) on uniprocessor — utilization alone is not sufficient when D<T.
Scope this claim to uniprocessor (§7.3 has no MP equivalent).

**[SPOKEN] Deadline-miss mitigation pairs with admission.** On a miss the kernel abandons the
job and re-releases at the next period, which **damps** the cascade — textbook EDF domino
assumes the late job keeps running. Turns the slide into guarantee + defense-in-depth.

### 7.3 MP EDF — **[APX]** (see §6.1)

Kept in full because it's excellent Q&A material.

**Topology** (`tasks.c:688-692`): global = `xReadyEDFTasksList_Glob_MP` +
`xEDFTaskRegistryList_Glob_MP`; partitioned = `xReadyEDFTasksLists_Part_MP[N]` +
`xEDFTaskRegistryLists_Part_MP[N]`.

**Selection is O(n), not O(1) — the most MP-specific fact.** On uniprocessor "earliest deadline
ready task" is `listGET_OWNER_OF_HEAD_ENTRY`. On two cores the head may already be running
elsewhere, so `prvSelectGlobEDFTaskForCore` / `prvSelectPartEDFTaskForCore` walk from the head
skipping `xTaskRunState != taskTASK_NOT_RUNNING`. Wired into `vTaskSwitchContext` with fallback
to `taskSELECT_HIGHEST_PRIORITY_TASK( xCoreID )`.

**Cross-core preemption** (`prvYieldForTask`): partitioned looks only at the task's assigned
core; global scans all cores and targets the **worst** victim — the core running the *latest*
deadline — preserving the invariant that the m earliest deadlines are executing.

**Affinity has two meanings.** Global: an eligibility *restriction*; `tskNO_AFFINITY` = fully
migratable, mask filters both the selection scan and the victim scan. Partitioned: the
assignment *itself* — `prvPartEDFCoreFromAffinityMask` returns −1 unless **exactly one** bit is
set. So `vTaskCoreAffinitySet` becomes a migration operation
(`prvPartEDFMoveTaskToAssignedCore` moves the task across *both* ready and registry lists), and
idle tasks must be pinned one-per-core. Line if ever slided: *"Same `uxCoreAffinityMask` field,
two contracts."*

**Partitioning = online Best-Fit bin packing.** `prvPartEDFUtilTestWithNew` returns a *core
index*, not a boolean — admission and placement are the same decision. Bins = cores, capacity =
U 1.0 (Liu & Layland), items = tasks of size C/T. Among cores where post-insertion U ≤ 1, pick
the **highest** post-insertion utilization. Ties → lowest core index. Caller-supplied
single-bit affinity overrides the packer entirely.
FF-vs-BF arithmetic, if shown, goes **on the slide, not narrated**:
```
cores at 0.3 / 0.5, then U=0.2 and U=0.6 arrive
First-Fit:  0.2 → core 0  → 0.5 / 0.5  →  0.6 REJECTED
Best-Fit:   0.2 → core 1  → 0.3 / 0.7  →  0.6 → core 0 = 0.9  ADMITTED
```

**FINDING — the `(m+1)/2` total-utilization gate rejects perfectly schedulable sets.** m=2,
four tasks at U=0.5: best-fit packs 2+2 for per-core U=1.0/1.0, exactly schedulable. But
`ullSufficientBoundQ32` caps U_total at 1.5, so the fourth task is rejected at U_total=2.0 —
even though the loop *did* select core 1 as a valid home. A pinned set is m independent
uniprocessor EDF instances where per-core U ≤ 1 is necessary *and* sufficient, so the per-core
check is already exact and the total gate only subtracts. Also, `(m+1)/2` is the classic bound
for **decreasing-order** allocation (FFD/BFD), whose precondition (offline, sorted) this online
unsorted packer doesn't meet — **don't cite it as theoretical backing.**

**FINDING — MP admission never uses relative deadline.** `prvPartEDFUtilTestWithNew(C, T, mask)`
and `prvGlobEDFUtilTestWithNew(C, T)` take only C and T. `xRelDeadlineTicks` is validated
(D ≤ T) and stored in the TCB — so the *scheduler* honors it — but there's no MP equivalent of
`prvEDFDemandTestWithNew`. MP admission is sound for implicit deadlines and **optimistic for
constrained ones.** And `test_dhall.c`'s "Bad" task is **T=4000, D=3000** — constrained. The
hand-derived miss analysis reasons about D; the code's test doesn't.

**Global admission** = `U_total ≤ m − (m−1)·U_max` (the U_max term is Dhall's effect — global
EDF can fail just above U=1 if one task has utilization near 1). Dhall test
(`mp_tests/global_edf_tests/test_dhall.c`): Heavy U=0.90, Light U=0.10, Bad U=0.75 →
U_total 1.75; bound = 2 − 1×0.90 = 1.10 → rejected.

**Online best-fit is arrival-order dependent and can't re-partition.** m=2 with
U = 0.2, 0.6, 0.4, 0.4, 0.4 in that order → rejected, though 0.2+0.4+0.4 / 0.6+0.4 = 1.0/1.0 is
valid. `prvPartEDFMoveTaskToAssignedCore` exists but nothing calls it for re-packing.

**`GLOBAL_EDF_ENABLE` doesn't select global EDF.** Every branch in `tasks.c` keys off
`PARTITIONED_EDF_ENABLE`; global is the fall-through `#else`. The flag appears only in
`schedulingConfig.h` guards and `main.c` test selection, so MP+EDF with both flags at 0 silently
compiles to global EDF rather than erroring.

**RP2040 makes the global-vs-partitioned comparison unusually clean.** Both cores are
Cortex-M0+ with **no per-core data cache** and uniform shared SRAM; the only cache is the shared
flash XIP cache. The standard objection to global EDF (migration destroys cache warmth)
essentially doesn't apply, so `test_compare_*` isolates scheduling behavior from migration cost
in a way it wouldn't on a Cortex-A. Genuine strength of the platform choice.

**Verified sound, no action:** cross-core locking. `xPortSysTickHandler` wraps
`xTaskIncrementTick` in `taskENTER_CRITICAL_FROM_ISR()` (`port.c:742`), taking the SMP ISR
spinlock, so the per-core tick loop's reads of `pxCurrentTCBs[other]` and its `prvYieldCore`
calls are serialized against the other core's `vTaskSwitchContext`.

**Minor:** the packer only sees periodic EDF tasks, but the timer service task is pinned to
core 0 (`configTIMER_SERVICE_TASK_CORE_AFFINITY (1U << 0U)`) and the tick handler runs on one
core — so core 0 carries unaccounted kernel load while the packer treats both bins as equal.

### 7.4 Gap 3 — CBS mental model **[SLIDE, heavily compressed]**

**Open the section with the gap, not the name:** an aperiodic task has no period, so EDF has
nothing to sort it by. Give it an optimistic fake deadline and it steals from the hard set;
give it a pessimistic one and it starves. **CBS bounds its bandwidth and hands it a deadline
that is enforced** — which is what lets aperiodic work enter a hard real-time analysis at all.

The vocabulary is where CBS confuses people: **CBS theory is written in terms of *jobs*, and
FreeRTOS only has *tasks*.** This table is worth a slide on its own.

| CBS theory | This implementation |
|---|---|
| server (Qs, Ts, cs, Ds) | `CBS_Server_t` — pure data, no stack, **never runs** |
| aperiodic job | one pass through the worker's `for(;;)` body |
| job arrival at time r | `xCBSSubmitJob()` → task notification |
| the thing that executes | the **worker** task |

**[SPOKEN] Clarifications that took a while to land:**
- **The worker *is* the aperiodic task.** There is no second task. The work function lives in
  exactly one place — the worker's body (`spin_ms( A1_WORK_MS )` in tests).
- **The worker is *registered* as periodic but doesn't *behave* periodically.**
  `xTaskCreateCBS` calls plain `xTaskCreate(..., T=Ts, C=Qs, D=Ts)`. That registration is an
  **upper-bound contract**, made honest by the two budget rules — that's the theorem, not a
  hack. **This is the 2→3 dependency: it's what lets aperiodic work pass the admission test.**
- **The server never runs anything — it only decides how urgent the worker is allowed to be.**
  Its sole output is a number written into the worker's `xAbsDeadline`.
- **A "job" is a doorbell, not a package.** `xCBSSubmitJob(server, worker)` carries no payload.
- **1:1 permanent bind, not a subscription.** One server, one worker, one outstanding job. A
  second submit while one is outstanding is **rejected** (`pdFAIL`), not queued.
- **Three roles**, obscured by the test names: `vA1ArrivalTask` = producer (itself a periodic
  EDF task, pure test scaffolding); `pxA1ServerRef` = server; `vAperiodicTask1` = worker.

**[Q&A] Of the periodic EDF fields, only two do real work for a CBS worker:** admission
(C=Qs, T=Ts) and registry membership (`xPeriodTicks != 0` → in registry → tick charges budget,
gated at `tasks.c:8171`). `xWcetTicks`, `xRelDeadline`, `xAbsJobReleaseTime` are inert at runtime
because the miss/overrun branches exclude CBS tasks and the worker blocks on a notification
rather than `xTaskDelayUntil`.

### 7.5 Gap 3 — CBS run path and budget rules

**[SPOKEN] The thing that was hardest to see: there is no CBS dispatch code.**

```
worker: xCBSWaitForJob(MAX) → ucNotifyState = WAITING → xSuspendedTaskList  (tasks.c:11698)
producer: xCBSSubmitJob(srv, worker)                                          (cbs.c:136)
   ├ RULE A: arrival rule → maybe cs=Qs, Ds=r+Ts                             (cbs.c:47, 183)
   ├ (a) xTaskCBSUpdateDeadline → writes TCB.xAbsDeadline                     (cbs.c:200)
   └ (b) xTaskNotifyGive                                                      (cbs.c:211)
         └→ xTaskGenericNotify:
              listREMOVE_ITEM  →  prvAddTaskToReadyList   ← EDF macro         (tasks.c:11969)
              taskYIELD_ANY_CORE_IF_USING_PREEMPTION      ← deadline compare  (tasks.c:11992)
         └→ vTaskSwitchContext takes head of list → WORKER RUNS
tick: cs--                                                                    (tasks.c:8188)
      cs == 0 → RULE B: cs=Qs, Ds+=Ts, sync TCB, re-sort                      (tasks.c:8190-8195)
      miss + WCET-overrun hooks SKIPPED for CBS tasks                         (tasks.c:8205, 8216)
worker: xCBSCompleteJob() → loops → blocks again
```

**[SPOKEN] Order is load-bearing:** deadline (a) is written *before* the notify (b), because
`prvAddTaskToReadyList` keys the insert on `xAbsDeadline`. Reverse them and the worker is
inserted at a stale position. Good "detail that bites you" beat.

**[SLIDE] The two rules — present by failure mode, not mechanism:**

| | RULE A (arrival) | RULE B (exhaustion) |
|---|---|---|
| Fires when | work *arrives* | work *overruns* |
| New deadline | `r + Ts` (**absolute** — anchored to now) | `Ds + Ts` (**relative** — stays on the period grid) |
| Prevents | front-loading old bandwidth at inflated urgency | spending more than Qs per Ts |
| Worker is | suspended → insert via `prvAddTaskToReadyList` | already ready → `prvCBSRefreshReadyListPosition` |

Together they make Qs/Ts an **enforced bound** rather than an average. That's what licenses the
periodic declaration to the admission test.

**[SLIDE] The arrival rule's cross-multiply.** The rule is `cs ≥ (Ds − r) · Qs/Ts`. `Qs/Ts` is a
fraction <1 that truncates to 0 in integer math (no FPU), so cross-multiply by Ts:
**`cs·Ts ≥ (Ds−r)·Qs`** → `ullLeft ≥ ullRight`. `uint64_t` because both products can exceed 32
bits; the `r >= Ds` early return guards the unsigned `xDelta` subtraction from wrapping.
Semantically the right side is *"how much budget a server at bandwidth Qs/Ts is entitled to
spend before its deadline"* — so the test asks **"is my budget larger than my entitlement?"** If
yes, the deadline is stale and would grant unearned urgency → take a fresh one anchored at
arrival. Put the cross-multiplied math above the code; highlight `ullLeft >= ullRight`.

**[Q&A] Five budget-write sites, four situations:**

| Situation | `cs` → | `Ds` → | Where |
|---|---|---|---|
| Server created | `Qs` | `now + Ts` | `cbs.c:109` |
| **RULE A** — arrival, budget too big for time left | `Qs` | `r + Ts` | `cbs.c:186` |
| Arrival fallback (`cs==0` or `r>=Ds`) | `Qs` | `Ds + Ts` | `cbs.c:191` → `:260` |
| **Worker runs one tick** | `cs − 1` | — | `tasks.c:8188` |
| **RULE B** — budget hits zero | `Qs` | `Ds + Ts` | `tasks.c:8192` |

**[SPOKEN] Exhaustion is not a failure.** Both the deadline-miss check (`tasks.c:8202,8205`) and
the WCET-overrun check (`tasks.c:8213,8216`) carry `&& prvTaskIsCBSManaged(...) == pdFALSE`. No
hook, no GPIO. The job is **demoted, not killed.**

**[SLIDE] CBS Test 1** (`cbs_tests/test_1.c`) — the asymmetry is deliberate:

| Task | C | T | U |
|---|---:|---:|---:|
| PERIODIC_1 | 250 | 3000 | 0.083 |
| PERIODIC_2 | 350 | 5000 | 0.070 |
| CBS_APER_1 (worker) | 1000 = Qs | 4000 = Ts | 0.250 |
| CBS_APER_2 (worker) | 3000 = Qs | 6000 = Ts | 0.500 |
| APER_A1_SRC (producer) | 20 | 1000 | 0.020 |
| APER_A2_SRC (producer) | 20 | 1500 | 0.013 |
| | | | **0.936** → admitted |

- **A1 reserves 25%, its producer demands 550/1000 = 55%** → deliberately oversubscribed.
  Exercises budget exhaustion, deadline postponement, submit rejections (`ulA1SubmitFailures`).
- **A2 reserves 50%, demands 400/1500 = 27%** → served normally.
- **The payoff line:** *"A1's producer wants 55% of the CPU and gets 25%, and both periodic
  tasks still meet every deadline. An over-eager aperiodic producer cannot steal from the hard
  real-time set."* That's the entire capability, in one trace.

**[Q&A] `xCBSSubmitJob` is task-context only, not ISR-safe.** Calls `xTaskGetTickCount()`
(`cbs.c:181`), `xTaskNotifyGive()` (`cbs.c:211`), and the accessors use `taskENTER_CRITICAL()`.
Realistic wiring is the deferred-interrupt pattern: ISR → `vTaskNotifyGiveFromISR` → handler
task → `xCBSSubmitJob`. An ISR variant would also hit the unconverted
`xTaskGenericNotifyFromISR` priority compare (§7.1), so the worker would wait up to a tick.

### 7.6 CBS — architecture critique **[SLIDE, on the close]**

**The strongest self-evaluation available. At 1:1, the server object is pure indirection.**

1. **The deadline exists twice and is hand-synced.** `xAbsDeadline` is a field in both
   `CBS_Server_t` and `TCB_t`; `tasks.c:8194` copies server→TCB, and `xTaskCBSUpdateDeadline`
   exists solely to perform that copy.
2. **The exhaustion rule is duplicated across the module boundary.** `tasks.c:8193` and
   `cbs.c:261` implement the same `Ds += Ts`; the kernel doesn't call the module's version.
3. **The integrity tag exists because of a cross-TU `void *`.** Fold the state into the TCB and
   there's nothing to validate.

Folding Qs/Ts/cs into the TCB (+12 B, −4 B for `pxCBSServer` = **+8 B/TCB**) would delete:
`CBS_Server_t`, the malloc/pool question, `xTaskCBSBindToServer`, `xWorkerTaskHandle` and the
1:1 checks, `uxIntegrityTag`, and all four accessors that exist only so `cbs.c` can reach into a
TCB it can't see — arguably `cbs.c` entirely.

**And this architecture caused bug B1.** `TickType_t` resolved differently between translation
units, changing `CBS_Server_t`'s field offsets — same pointer, different layout. **If the state
lived in the TCB there is no cross-TU struct to disagree about.** This is the sentence to say.

**The honest counter (say it — it's what makes the critique credible):** a separate object earns
its keep at **N:1** — one reservation serving multiple aperiodic sources with a pending-job
queue, the classical CBS model. Keeping the object is a **bet on that feature, not a
justification.**

**[Q&A] The `cbs.c`-as-separate-module defense needs rewording.** Currently defended as "server
state isn't a scheduling rule," but tell #2 shows the boundary doesn't hold: the exhaustion rule
is in `tasks.c` and only the arrival rule is in `cbs.c`. The split isn't policy-vs-mechanism,
it's **policy cut in half.**

### 7.7 CBS — smaller findings **[Q&A only]**

- **`xCBSAdmissionTest` (`cbs.c:348`) is a self-labelled stub that sums only CBS servers**,
  ignoring the periodic set, so it can pass a server infeasible against the full task set. The
  *real* gate is the worker's `xTaskCreate`, which runs full EDF admission with (C=Qs, T=Ts) and
  lands the worker in the EDF registry so subsequent admissions count it. **A CBS server with
  (Qs, Ts) consumes exactly the bandwidth of a periodic task with C=Qs, T=Ts — that equivalence
  is why CBS composes with EDF, so the EDF test *is* the CBS test.** Don't show this function;
  if asked, say the server-level test is a redundant pre-filter you'd delete. Also uses
  per-mille truncation instead of the Q32 math used everywhere else.
- **Equal-deadline tie-breaks are contradictory.** `vTaskSwitchContext` (`tasks.c:~8790-8815`)
  scans the equal-deadline run and **prefers CBS** ("as required by CBS policy");
  `prvEDFShouldPreempt` (`tasks.c:~10353-10360`) **prefers periodic** ("to avoid starving
  periodic EDF tasks"). Net: a newly-ready CBS task can't *preempt* an equal-deadline periodic
  task, but once a switch happens for any other reason, selection hands it the CPU. Not a crash;
  equal deadlines are rare in the tests — but the comments make it self-evident. "Prefer
  periodic consistently" is the defensible fix. **Do not show this code.**
- **`cbs.c:191` arrival fallback appears unreachable.** `r >= Ds` is already consumed by the
  arrival rule's early return (`cbs.c:52`), and `cs == 0` can never be *observed* from task
  context because `cs--` and the `if( cs == 0 ) cs = Qs` refill sit in the same critical section
  (`tasks.c:8188-8192`). Invariant: **`cs ∈ [1, Qs]` at all times from task context.** Also
  makes `vCBSReplenishBudget`'s only caller unreachable. This is reachability reasoning, not a
  grep — **do not delete before the panel.**
- **Servers are `pvPortMalloc`'d with a compile-time cap and never freed.**
  `static CBS_Server_t *pxCBSServers[configCBS_MAX_SERVERS]` (`cbs.c:25`) is a static array of
  *pointers to heap objects*, and nothing frees a server. That's static allocation with extra
  steps: an unhandleable OOM path, heap_4 header overhead (48 B struct → ~56 B allocated;
  4 servers ≈ 240 B vs 192 B static), and a MISRA/certification smell. Fix is ~10 lines to an
  internal pool with no call-site impact; more FreeRTOS-idiomatic is an `xCBSServerCreateStatic`
  variant taking a caller buffer. **Note the premise "we statically allocate tasks" is false** —
  all app tasks are `xTaskCreate` (dynamic).

---

## 8. Gaps 4 & 5 — SRP: **KNOWN GAP, highest-value research left**

SRP now carries **4 minutes and 4 slides** — two of the five capabilities, the best number in
the deck, the debug story, and a PIP-vs-SRP appendix slide. **Yet §7 contains zero SRP mechanism
findings.** Every other feature got an audit; SRP did not. This is the biggest risk in the deck:
the section with the best story has the least verified substance behind it.

**Section opening (the two gaps, stated as gaps):**
- *Gap 4:* "FreeRTOS gives you priority inheritance. That bounds inversion but **does not
  prevent deadlock** — two tasks taking two mutexes in opposite orders still deadlock. And
  under EDF it does nothing at all, because there's no priority left to inherit."
- *Gap 5:* "Every FreeRTOS task allocates its own worst-case stack, resident for the life of the
  system, even though most of them can never be live at the same time."

**Recovered from `ppt/media/image13.png` (slide 21) — real substance, currently unreadable on
the slide but excellent material once split into stops (§5.1):**

- `prvSRPComputePreemptionLevel( TickType_t xRelDeadlineTicks )` returns
  **`portMAX_DELAY - xRelDeadlineTicks`** — preemption level is *inversely* ordered with respect
  to relative deadline, so a shorter deadline yields a higher level. One line, one idea:
  **an excellent stop panel.**
- `prvSRPRecomputeSystemCeiling()` — nested loop rebuilding `uxSRPResourceCeilingTable[]` per
  resource from the SRP registry, then `uxSystemCeiling = max` over resources **currently
  locked** (`uxSRPResourceActiveCount[] != 0`). Binary semaphores make `ceil_R(v)` two-valued
  (free → 0, locked → the computed ceiling), so "is it locked?" is the whole selector.
- `prvSRPSelectReadyTask()` — the protocol in one function, and the natural 3rd stop:
  - **fast path:** `uxSystemCeiling == 0` → nothing held → `listGET_OWNER_OF_HEAD_ENTRY` (plain EDF)
  - walks `xReadyEDFTasksList_UP`, already sorted by absolute deadline
  - admits a candidate if `uxPreemptionLevel > uxSystemCeiling` **or** it already holds a
    resource — the exemption exists because a task that raised the ceiling *by its own lock*
    would otherwise deadlock against itself; SRP gates the **start** of a job, not a resume
  - returns `NULL` when every ready task is ceiling-blocked
- **The best sentence in the whole project is already in that file's comments:**
  *"EDF selection is implicit in the traversal order — SRP is only a filter layered on top of an
  EDF-ordered list."* Eleven words, whole protocol. Put it on the SRP frame slide.
- Also in the comments: *"the price of deadlock freedom + stack sharing"* — good tradeoff line.
- TCB additions visible: `uxPreemptionLevel`, `uxStackDepthWords`, `xSRPTaskListItem`,
  `uxSRPResourceClaimMax[]`, `xSRPResourceMaxCriticalSectionTicks[]`, `uxSRPResourceHeldCount[]`.
  Globals: `xReadySRPTasksList_UP`, `xSRPTaskRegistryList_UP`, `uxSystemCeiling`,
  `uxSRPResourceCeilingTable[]`, `uxSRPResourceActiveCount[]`.

**Also known:**
- Preemption levels are **statically** derived from relative deadline — counterintuitive for a
  dynamic-priority protocol, and worth a slide on its own.
- Ceiling model: per-resource ceiling = highest preemption level of any task that can be blocked
  by it; system ceiling = max over held resources. Rule: a task may only *start* if its
  preemption level exceeds the system ceiling. **Once started it cannot be blocked** — blocking
  moves from execution time to preemption time, which is what makes it analyzable.
- `prvSRPRecomputeSystemCeiling()` recomputes rather than maintaining a ceiling stack — chosen
  for clarity and validatability at project task counts. Good deliberate-tradeoff beat.
- **Shared-stack context save/restore** is real and undocumented here:
  `pxSRPSavedContextBuffer` / `uxSRPSavedContextCapacityWords` / `uxSRPSavedContextWords`
  (`tasks.c:473-475`, `FreeRTOS.h:3216`); `prvSRPSaveTaskContextImage` (`tasks.c:3318`, called
  at `:3438` and `:3483`) copies the live stack out to a buffer when a shared-stack task goes
  dormant; restore path around `tasks.c:3505+`; frees at `:3357`, `:4499`, `:10209`. Guard
  bands: `prvSRPCheckSharedStackGuard` / `taskSRP_SHARED_STACK_GUARD_PATTERN`.
  **This mechanism is what makes stack sharing actually work, and it answers the most likely
  question in the room** — *"how do two tasks share a stack without clobbering each other?"*
  There is currently no written answer.
- Kernel surface: `configUSE_SRP` appears at 53 sites in `tasks.c`. Change log:
  `assignment_md_files/SRP_docs/changes_SRP.md`.

**What to produce next:** a §7-quality audit of the SRP path — the ceiling gate in selection,
the save/restore mechanism, where preemption level is computed, how 13,200 → 4,000 is actually
derived and under what task set, plus the same honest critique §7.6 gives CBS. Two things to
check: whether save/restore uses `pvPortMalloc` on a scheduling-adjacent path (same smell as
§7.7's servers, potentially worse), and whether the 13,200→4,000 figure came from a config
that's currently built (`configUSE_SRP_SHARED_STACKS` is presently **0**).

---

## 9. Wording that must change

1. **"Admission control avoids starvation"** — use *starvation* only for the fixed-priority
   side. EDF's overload failure mode is the **cascade / domino effect**. Correct framing:
   *"under fixed priority an over-budget task mostly damages itself and what's below it — the
   harm is localized and predictable. Under EDF there is no 'below': one infeasible task can
   push the whole set into cascading misses. EDF's failure mode is global, so the check has to
   happen before anything runs."* Say **can** cascade, not will.
2. **Retire the 100% claim properly.** The limitations slide claims EDF buys the full processor
   vs RM's ~69%; admission control is what makes that **enforced rather than aspirational**.
   *"The infeasible task set becomes a startup error instead of a field failure."*
3. **Say "necessary and sufficient"** (uniprocessor, implicit deadlines) — strongest formal
   statement in the project, and the reason Gap 1 unlocks Gap 2.
4. **Never say best-fit is "ideal" or "maximizes utilization."** FF and BF share the same 1.7
   asymptotic worst-case ratio; bin packing is NP-hard so every online heuristic is provably
   non-optimal. Defensible: *"best-fit packs into the tightest core, preserving large contiguous
   headroom for a heavy task that arrives late."*
5. **Don't cite `(m+1)/2` as theoretical backing** (§7.3).
6. **Fix "subscribe"** (slide 7) — CBS is a 1:1 permanent bind with one outstanding job.
   "Subscribe" implies many-to-one and accidentally claims the V2 feature.
7. **Fix the assumptions slide** (slide 7) — "implicit deadlines" → **"constrained deadlines,
   D ≤ T."** EDF Test 4 uses T=5000/D=3000 and the Dhall task T=4000/D=3000, and the UP path
   *does* implement demand analysis for D<T. Current wording is inconsistent with the tests and
   gives away credit.
8. **Scope the demand-analysis claim to uniprocessor**, then flag the MP gap deliberately (§7.3)
   rather than being caught by it.
9. **Typos on live slides:** `taskSELEC_HIGHEST_PRIORITY_TASK` (slide 5, ×2), "Addeds"
   (slide 4), "Maintance" / "Neglible" (slide 8), "Salee" (slide 9), "A periodic" where
   "aperiodic" is meant (slides 7, 17), "Prinicples" (slide 20).

---

## 10. Project facts (reference)

**What it is.** A replacement scheduler for the FreeRTOS kernel on a Raspberry Pi Pico (RP2040).
Started as a university course project, continued afterward. Replaces fixed-priority scheduling
with dynamic-priority (deadline-based) scheduling, plus a resource access protocol and an
aperiodic-task server.

**Constraint that matters: the board has been returned.** No new hardware validation is
possible. Anything not already on a captured logic-analyzer trace is *implemented, not
hardware-validated* and must be labelled as such.

| Feature | Flag | What it does |
|---|---|---|
| **EDF** | `configUSE_EDF` | Dynamic priority by absolute deadline. UP + MP. |
| **MP EDF — Global** | `configUSE_MP` + `PARTITIONED_EDF_ENABLE 0` | One shared deadline-sorted ready list; both cores pull; migration allowed. |
| **MP EDF — Partitioned** | `configUSE_MP` + `PARTITIONED_EDF_ENABLE 1` | One ready list + registry per core; pinned at admission by online best-fit. |
| **CBS** | `configUSE_CBS` | Aperiodic tasks get a bandwidth reservation (Qs/Ts) and join EDF via a dynamic server deadline. |
| **SRP** | `configUSE_SRP` | Preemption-level ceiling protocol. Deadlock freedom, bounded blocking, shared stacks. |
| **SRP shared stacks** | `configUSE_SRP_SHARED_STACKS` | One stack region per preemption level → 13,200 B → 4,000 B across 12 tasks. |

Also built: admission control (utilization test for D==T; processor-demand/DBF analysis for D<T
on uniprocessor), WCET-overrun instrumentation split from deadline-miss handling, GPIO task-ID
tracing.

**Hardware/config.** RP2040 — dual Cortex-M0+, **no FPU**, **no per-core data cache**, shared
SRAM, shared flash XIP cache (these facts drive several design decisions).
`configTICK_RATE_HZ 250` → **4 ms tick**; `configCPU_CLOCK_HZ 60000000`;
`configMAX_PRIORITIES 5`; `configNUMBER_OF_CORES` 2 when `configUSE_MP == 1` else 1; static +
dynamic allocation both on, `configKERNEL_PROVIDED_STATIC_MEMORY 1`;
`configTOTAL_HEAP_SIZE 120000`, heap_4. **All application tasks are created dynamically**
(`xTaskCreate`, 43 call sites in tests; zero `xTaskCreateStatic`). Only idle/timer are static.

**Configuration system.** `schedulingConfig.h` selects the feature set at compile time with
`#error` guards for invalid combinations (UP+MP, SRP+CBS, SRP/CBS without UP+EDF, both MP
policies). `xTaskCreate` is **overloaded by config** — different signatures under different
modes, so no new API surface except CBS. ~60 conditional-compilation sites in `tasks.c`.

**⚠ Current flag state (verified 2026-08-07):** `configUSE_UP 1, configUSE_MP 0, configUSE_EDF 1,
configUSE_SRP 1, configUSE_CBS 0, configUSE_SRP_SHARED_STACKS 0`. Note `configUSE_SRP` is **1**
— either deliberate (SRP work in progress) or a validation build left flipped. **Never leave
flags flipped after a validation build.**

**Build.**
```bash
cd build && ~/.pico-sdk/ninja/v1.12.1/ninja
~/.pico-sdk/toolchain/13_2_Rel1/bin/arm-none-eabi-size build/FreeRtosProject.elf
```
Already cmake-configured; `PICO_SDK_PATH=/Users/shayan.ajmal/.pico-sdk/sdk/2.2.0`.

**Tests.** `edf_tests/` (9), `cbs_tests/` (4), `srp_tests/`, `mp_tests/` (+ `global_edf_tests/`
incl. `test_dhall.c`, `partitioned_edf_tests/`, `test_compare_glob.c`, `test_compare_part.c`),
`regression_tests/`. Selected by config in `main.c`.

**Testing methodology.** Tasks get an ID at creation; on context-switch-in the ID is driven onto
7 GPIO pins; one line strobes on context switch, one pulses on deadline miss. A Saleae logic
analyzer parallel-decodes to a decimal task ID, so a capture shows *which* task ran and *for how
long*. WCET simulated with a calibrated `spin_ms()` loop. MP mode splits channels into per-core
banks (core 0 → GPIO 2–5, core 1 → GPIO 6–9). Correctness today = **hand-derived expected
schedule matched against measured trace**; no automated trace assertion (future-work item).

**Git state:** modified but uncommitted — `FreeRTOS.h`, `task.h`, `tasks.c`, `changes_SRP.md`,
`cbs.c`, `cbs.h`, `schedulingConfig.h`, `task_trace.c`. Last commit `53a0593`. A prior session
removed 14 verified-unused CBS items (`cbs.c` 492 → 403 lines), build-verified in both
`configUSE_CBS` states; flash only −24 B because the Pico SDK builds with
`-ffunction-sections --gc-sections`. **Useful line:** *"dead code cost me nothing in flash
because the linker already collected it — it cost me reviewability."*

Clang diagnostics on `cbs.h`/`task.h` about unknown types are a pre-existing standalone-parse
artifact; both headers deliberately `#error` unless `FreeRTOS.h` is included first.

---

## 11. How to help in the next session

**Priority order of work remaining:**
1. **Rebuild the spine** — slides 3, 6, 7 into the five-gap table (§3). One hour. It turns every
   later slide from an hour's decision into a ten-minute job.
2. **Build ONE flow-and-stop frame template** (§5.1) — geometry, dim/lit states, code panel type
   size, connector. Get it right once for EDF, then the other four are copy-and-fill. This is
   the difference between 20 more hours and an afternoon.
3. **Build the five test slides** (EDF Test 4, MP if kept, CBS, SRP stack sharing + debug, SRP
   test). Genuinely empty today — no waveform is in the deck at all. These are the payoff.
4. **Audit SRP** (§8) — most airtime, least verified substance. Much is now recovered from
   `image13.png`; what's still unwritten is the shared-stack save/restore mechanism and the
   derivation of 13,200→4,000.
5. **Appendix Tier 1 only:** PIP vs SRP, EDF optimality proof, how timing guarantees are made,
   constrained-deadline analysis. Leave the rest as titles.

**What worked in past sessions:**
- Verify claims against code before asserting them — several §7 findings only surfaced because a
  grep contradicted an assumption.
- After explaining a mechanism: ask **"am I missing anything?"** then **"what's worth
  presenting?"** The audit step produced most of §7.
- Give the **time cost** of any recommended content and say what to trade for it — the talk is
  clock-bound.
- Flag claims that wouldn't survive one follow-up question (§9 exists because of this).

**Standing asks:**
- Slide-ready snippets (≤8 lines, one highlighted) + speaker beat + failure mode.
- Diagram generation as inline-SVG Artifacts. Candidates: the five-state ready list; the gap
  scoreboard; SRP stack sharing / save-restore; CBS server-vs-worker; best-fit-vs-first-fit bin
  bars. (Lucid MCP is connected; Figma and Excalidraw are not.)
- Rehearsal: given a section, predict the hardest follow-up and draft the answer.

**Candidate's constraints:** four further interviews to prepare for after this one. Presentation
prep has already consumed ~20 hours. Recommend, don't enumerate.
