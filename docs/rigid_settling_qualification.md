# Support-position creep repair and settling qualification

The local Ball scene adapter now reconciles predicted positions with the
contact-solved linear velocity before validating full solid separation. This
repairs sideways support creep without freezing bodies, changing friction,
removing collision correction or changing the impulse solver.

The original and +2 cm offset strikes both retain genuine upper-layer toppling.
Both pass the separately declared **seconds 4–6** settling window with the same
2 mm position, 0.01 m/s linear speed, 0.02 rad/s angular speed, 0.002 J kinetic
energy and 0.01 rad orientation-change limits.

**The original seconds-3-to-5 offset check remains failed.** After repair, one
brick is still rocking until approximately 3.3 seconds. Its window maximums are
5.282 mm displacement, 0.192 m/s speed, 0.869 rad/s angular speed and 0.050 J
kinetic energy. This is resolved dynamical motion in the model, unlike the old
millimetre position creep with nearly zero velocities. A longer observation is
not a retroactive pass of the original criterion.

## Cause and numerical boundary

The adapter already predicted a pose for contact discovery:

```text
v_force = velocity after external-force integration
x_predict = x_previous + dt * v_force
contact solve -> v_solved, plus existing world position corrections
```

At rest, contact impulses balance gravity and return velocity almost to zero,
but the old accepted position still contained the gravity-induced `dt²` sag.
Repeated full-geometry recovery of tilted contacts converted some of that sag
into sideways translation. A stage observer reproduces the earlier drift and
separates the world solve from final full-solid recovery. Disabling correction
instead refuses a tick; it is not the repair.

The adapter now adds `dt * (v_solved - v_force)` to the world-solved position,
then runs the existing coupled full-solid recovery and staged validation. Thus
linear drift uses solved velocity while contact discovery still uses predicted
geometry. Existing positional corrections are retained. Static state, angular
integration, restitution/friction, warm-start/cache behavior, solver iteration
budgets and failure rollback are unchanged. Free-flight semi-implicit gravity
remains identical. No sleeping, thresholds that pin motion, or scripted poses
were introduced. The change is local to Ball's `rigid_scene_runtime.c`; the
separate P5b kernel and shared contracts are unchanged.

## Results and independent checks

| Observation | Maximum brick position drift |
| --- | ---: |
| Earlier primary, seconds 3–5 | 2.303 mm, failed |
| Repaired primary, seconds 3–5 | 3.62e-10 m, passed |
| Repaired offset, seconds 3–5 | 5.282 mm, failed while rocking |
| Repaired primary, seconds 4–6 | 1.43e-13 m, passed |
| Repaired offset, seconds 4–6 | 8.20e-9 m, passed |

The undisturbed tower is stable too. All 140 six-second physical checks pass
normally and under ASan/UBSan. All 4,323 snapshots / 34,584 body poses and contact
arrays agree exactly between builds, replay and the portable results. Every
six-second result's first 1,201 frames is exactly its corresponding five-second
run; extending observation does not alter earlier motion. Leak detection was
disabled; no LSan claim is made.

Full source-mesh SAT stays below 0.501 mm against the unchanged 2 mm gate. Both
primary upper bricks satisfy the same 60° source-up tilt and 0.35 m COM-drop gates
at tick 215; offset upper bricks satisfy them at ticks 196/197. Independent
contact-free ballistic intervals and pre-impact gravity references remain valid.

All three six-second cases close linear momentum after gravity and signed
fixed-floor reaction within 1.65e-10 kg·m/s per step. The floor exchanges momentum
with its external support; moving-body momentum alone is not conserved. Exported
contact iteration-activity scalars are not signed net impulses. The test observer
uses accumulated normal/tangent vectors including warm starts. Total mechanical
energy never exceeds its initial value: primary 213.188→73.8096 J, offset
213.188→72.0359 J. Individual steps can gain up to 0.02396 J; this is a bounded
energy result, not exact energy/work or angular-momentum conservation.

## Timestep diagnostic, separate from delivery

A saved pre-repair quiet-contact pose was continued for two physical seconds
with a cold cache at three rates, measuring drift over seconds 0.5–2:

| Diagnostic rate | Before repair | After repair |
| --- | ---: | ---: |
| 120 Hz | 3.455 mm | 0.3011 mm |
| 240 Hz | 1.727 mm | 0.04038 mm |
| 480 Hz | 0.8635 mm | 0.002228 mm |

The old drift approximately halves as the timestep halves, consistent with the
repeated gravity-sag mechanism. The repaired 240 Hz continuation reduces drift
about 43×. All repaired continuations stay below the same 2 mm position gate;
maximum signed momentum residual across these diagnostics is below 1.55e-10
kg·m/s. Cold-cache transients remain timestep dependent. This is a local quiet
contact diagnostic, not full-impact trajectory convergence or arbitrary-rate
native delivery. The test observer explicitly labels diagnostic rates; the
production native adapter still admits 240 Hz only and its rate-refusal test passes.

## Replay, rendering and regression boundary

All 4,323 native tick poses and 1,245,024 source triangle-vertex samples agree;
maximum vertex error is 1.12e-14 m against 2e-12 m. Fourteen selected modest
native views pass 1,548,288 flattened/TLAS comparisons and 145,152 independent
source camera rays and material pixels. The accepted impact result also passes
selection without resimulation, relocation/reopening at four ticks and atomic
wrong-revision refusal. Human visual acceptance is separate.

Existing native runtime (35 checks per normal/sanitizer build), immutable results
(49 checks), exact rollback, seven contact/cache/friction/world/coupled-solver
groups, six prior resting/impact/offset controls with finite-mass reference, and
all 60 BR6 pillar/drop/throw checks pass. Nine toppling controls and a real
pre-repair cache regression verify that near-zero velocity cannot bypass the
quiet-position gate. The broad desktop/UI/package suite was not rerun: this
change affects the native scene producer, with no desktop source integration,
installation, package, shader or remote operation in this slice.

## Reproduce

Use the ordinary native/portable preparation commands in
[the toppling reference](rigid_toppling_qualification.md). The additional entry
point `tests/qualify_rigid_settling_scene.py` selects
`tests/rigid_settling_limits.json`: only observation end/start ticks change from
1200/720 to 1440/960; all error limits and the two-second window length remain.
Run its rest/impact cases and the same `--projectile-offset-y .02` control with
fresh normal and sanitizer assets. The original toppling entry point still
measures the original five-second criterion and records the offset failure.

Playback supports `--end-tick 1440 --rest-ticks 0 960 1440` and explicit
`--impact-ticks`. It requires the selected end tick to cover the whole cache.
`tests/test_rigid_settling_window.py` accepts the retained pre-repair cache and
metrics and verifies the expected quiet-position refusal. The optional rate in
`tests/rigid_toppling_momentum_probe.c` is diagnostic only and exports no result
bundle. General piles, CCD, concavity, gyroscopic qualification, larger body
capacity, installed adoption and release remain separate.

The subsequent [unequal-mass qualification](rigid_unequal_mass_qualification.md) passes the same six-second gates without a new runtime repair.
