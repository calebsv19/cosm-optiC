# Sequence clock foundation (Stages 0–1)

This documents the `ray_sequence_clock_v1` pure contract introduced in Stages 0–1. Stage 2 now supplies saved JSON, authenticated rigid-result playback, snapshot provenance and uniform video. See [Stage 2 usage and qualification](sequence_playback.md). Legacy scenes without a sequence retain their prior clock behavior.

## Ownership and scope

`include/animation/sequence_clock.h` owns the public bounded data/API contract.
`src/animation/sequence_time.c` owns checked nonnegative rational arithmetic;
`sequence_clock.c` owns validation and sample evaluation. There is no allocation,
IO, solver, wall clock, global state, authoring mutation or render dependency.
The existing rational `TimelineRate`, `TimelineRange`, `TimelineSample` and
canonical legacy context builder are reused. An evaluation keeps that context
and separate exact presentation, segment-local and simulation times.

Reuse decision: reuse the app timeline; defer shared extraction. `core_time`
measures monotonic runtime time, `core_sim` owns simulation orchestration,
`core_scene` owns source/result meaning, and `core_math` generic math. None owns
this authored edit policy. No new shared library or vendored change is needed.

## Stage 0 decisions

- One contiguous, exact-tick source envelope per sequence in v1. Source ID and
  64-hex revision identify the intended immutable result; this module validates
  the declaration, not the file, its qualification or availability. A later
  adapter must authenticate the result and prove contiguous tick coverage.
- One rational output rate and absolute frame range. Encoded FPS and spatial/
  temporal render-sample counts are not alternative clocks. Output frame offset
  `i` has exact presentation PTS `i / output_rate`, irrespective of range origin.
  A duration not representable by an integer frame count must be explicitly
  resolved by the author; do not round it silently.
- Up to 64 uniquely identified play/hold segments partition the entire range by
  local frame offset. Positive counts, no holes, overlap or unused tail.
  Intervals are half-open. At a join, the segment on the right owns the sample.
- Play has positive rational source-seconds/presentation-second speed. Hold has
  canonical speed zero. Source starts must equal the preceding segment's exact
  end limit: no implicit jump, replay, reverse, wrap or source change.
- The complete source mapping, including its unrendered right-hand end limit,
  must fit the declared inclusive tick envelope. Do not infer a state beyond the
  last tick. To display the last state for an output-frame interval, author a
  hold there. Last image timestamp and encoded duration are different quantities.
- Integer output samples must map to exact source ticks. Validation checks the
  first sample and arithmetic progression for each segment without scanning all
  output frames. No rounding, nearest-frame fallback or interpolation.
- Rational subframes are evaluated exactly and refused if they require a
  nonintegral source tick. A source hold accepts subframes because its source
  tick remains fixed. Subframes may occupy the final output frame's interval;
  they may not cross the sequence's exclusive end.
- Rational numerator/denominator components are at most `INT64_MAX`, normalized
  by GCD in results. Zero denominators and overflow/unrepresentable arithmetic
  refuse. Finite integer arithmetic is authoritative, not floating tolerance.
- Every public output is committed only on success. Documents must remain
  immutable during evaluation. Pure evaluation validates each request, so there
  is no stale compiled-plan cache or lifecycle to reconcile in Stage 1.

## Evaluation and future clock bindings

For a segment starting at local frame `a`, source time `s`, and speed `r`:

`presentation = (local_frame + rational_subframe) / output_rate`

`segment_local = presentation - a / output_rate`

`simulation = s + r * segment_local`

Hold uses `r = 0`. Source tick position is `simulation * source_rate`; only
integral positions are returned. `SequenceClockDuration` returns exact frame-count/rate duration. Readback includes
segment index, exact PTS,
segment-local time, simulation time, source tick and declared source identity.
Motion-time selection exposes presentation, simulation and segment-local clocks.
It does not implement camera bindings or pause-while-unselected behavior.

Example: 30 FPS, 300 frames, play 120 frames from source0 at speed1, hold90 at
source4, play90 from source4 at speed1. PTS0..4 maps source0..4; PTS4..7 holds4;
PTS7..10 maps4..7. There are300 output frames, not301. Frame120 is the first hold
sample; frame210 is the resumed play. At240 source ticks/sec the envelope must
include tick1680, the final source limit. This is a synthetic contract fixture,
not authority to extend the Kinetic Spill qualified prefix.

## Verification contract

`make test-sequence-clock` builds strict C11 tests and runs an independent Python
`fractions.Fraction` oracle against the C probe. `test-sequence-clock-sanitize`
rebuilds the C tests with ASan/UBSan. Use a fresh `BUILD_DIR_BASE`; neither target
cleans or touches existing output. Tests cover the example, all frame boundaries,
nonzero/negative origins, rational rates/subframes, chunk and arbitrary seeking,
source envelopes, exact-grid refusal, capacities, identities, arithmetic range
and unchanged failure outputs. Neighboring timeline/motion tests remain gates.

## Remaining stages and stop boundary

2. Completed: authenticated rigid-result bindings, evaluated snapshot provenance,
   one-camera Preview/headless playback, versioned saved JSON and uniform video.
   See [Stage 2 boundaries](sequence_playback.md).
3. Completed: stable named camera identities, detached independent views and legacy
   compatibility. See [Stages 3–4](camera_sequence.md).
4. Completed: complete shot selection and continuous, selection-paused and explicit
   clip clocks derived from the declared activation schedule. Cuts retain output
   duration and source-state mapping.
5. Completed: exact Hold-boundary camera/recorded-target inspection orbits reuse
   M5 scheduling with dependency locks and feasibility checks.
6. Completed: retained native/MCP authoring, shared Undo/Redo, Save/reopen and
   separate clock UI. See [Stages 5–6](inspection_sequence_authoring.md).
7. Completed for one additional source: pinned complete VF3D v1 frames with exact
   Hold/Resume and native provenance proof; other families remain bounded follow-ons.
8. Completed: synchronized angle/edit/matrix exports, exact timing validation and
   explicit temporal/visual review. See [Stages 7–8](sequence_sources_and_multiview.md).

No part of Stage1 promises camera selection, physical interpolation, serializer,
video smoothing, improved pixels, GUI behavior or installed-app changes. The
next integration stage must not reuse legacy `normalized_t` as a second clock.

## Historical Stage 1 verified closeout

Strict C11 and ASan/UBSan each pass2757checks. The independent Python Fraction
oracle passes2799samples. Existing straight timing, waypoint scheduling, curved
geometry, curved scheduling, motion orientation, timeline entity and timeline
view gates pass from fresh contained output. An expanded-test fixture initially
retained an invalid document for the chunk-overflow check; that fixture is fixed
and the failed attempt remains recorded. No production evaluator defect was
masked or tolerance relaxed.

A full app/headless image build and GUI acceptance are deliberately not run for
this unconsumed pure module. They belong to Stage2 integration; no current pixels
or playback can change through this API until a consumer is added.
