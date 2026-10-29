# Top-heavy six-brick qualification checkpoint

**Follow-up:** the [contact geometry/angular-drift repair](rigid_contact_settling_repair.md) now passes the same fixed settling window. The failed checkpoint below is retained as dated evidence.

**The complete qualification does not pass yet.** The undisturbed tower and
original strike pass; the 2 cm lateral-offset strike still fails the unchanged
seconds-4-to-6 settling window. Larger structures remain on hold.

The brick masses are the exact reverse of the [previous arrangement](rigid_unequal_mass_qualification.md):
1.25, 1.5, 1.75, 2, 2.5 and 3 kg from the base upward. Total tower mass remains
12 kg, projectile mass 8 kg. Geometry, materials, initial poses, launch, native
240 Hz rate, six-second duration and all physical tolerances are unchanged.
Only the mass ordering and corresponding uniform-density box inertias change.
A native observer verifies actual admitted masses/inertias, with substitution
negative controls. All six bricks remain dynamic from tick zero.

## Blocking refusal repaired

The earlier scene runtime refuses the offset strike at tick98 and the primary
strike at tick118. Full-solid inverse-mass position recovery exhausts its64
sweeps: at the offset refusal, floor/brick overlap is0.504055mm against the
unchanged0.501mm convergence target. This is a recovery convergence-budget
failure with a heavier coupled load on lighter supports.

The scene-only bounded recovery allowance is now256 sweeps, retaining the same
position correction fraction, inverse-mass weighting, maximum correction,
0.5mm slop plus1µm query tolerance, and final validation. Early exit is unchanged;
scenes already converging in64 sweeps keep their exact trajectories. Static
bytes, contact impulses, velocities, orientations and staged rollback are
unchanged. Unconverged/impossible cases still refuse. This is a finite local
budget increase, not a general convergence guarantee or a new impulse solver.

A focused1.25kg support/10.75kg load regression fails under64 sweeps and passes
under256 normally and sanitized. An exploratory deeper six-collinear-cube chain
still exhausts256; its source/log is retained as a limitation, rather than being
claimed qualified. The actual crossed six-brick scene now completes all1440
ticks. Accepted body-pose prefixes exactly match the old runtime before its
refused ticks. P5b and shared collision contracts are unchanged.

The qualification harness also records a terminal `failed` status when the native
producer refuses, preserving the completed cases and error. Previously such an
exception left the status file saying `running`. The old failing backend is a
negative control for the repaired status behavior.

## Physical results, with the failed gates retained

| Case | Physical checks | Maximum drift during seconds4–6 |
| --- | --- | ---: |
| Undisturbed tower | 62/62 pass | 1.04e-12m |
| Original strike | 66/66 pass | 7.69e-13m |
| 2cm offset strike | 60/66 pass | 2.08148mm; limit2mm |

Both upper bricks satisfy simultaneous source-up tilt≥60° and COM drop≥0.35m:
primary at ticks207/210, offset at211 for both. Independent contact-free ballistic
intervals verify support loss and gravity. Maximum full source overlap remains
below0.501mm against the existing2mm physical gate.

Six offset settling checks fail, identically in normal and ASan/UBSan builds:
brick4 drift, linear speed, angular speed and kinetic energy; brick5 linear speed
and kinetic energy. Across the4–6s window their maxima are2.08148mm,
0.040711m/s,0.190148rad/s and0.0027996J, versus2mm,0.01m/s,0.02rad/s and0.002J.
Orientation-change gates pass. The upper block continues small motion in the
model; this has not been established as physically correct rocking rather than
contact-solver/recovery jitter. A still frame cannot resolve that question.
No threshold, observation window, mass, contact law or initial pose was adjusted
to hide the failure. Later observation is not a pass of this declared window.

## Verification layers

Fresh native/renderer builds pass. All4,323 accepted snapshots/34,584 body poses
and contacts agree exactly between normal and ASan/UBSan, repeated native replay
and portable results. Physical acceptance is188/194 checks per build. Leak
detection is disabled. Signed linear momentum closes after gravity and fixed
floor reaction within1.52e-10kg·m/s per step; this is not an energy/work or angular
momentum conservation claim.

All4,323 ordinary native optiC pose/geometry samples and1,245,024 source
triangle-vertex samples pass. Thirteen selected views pass1,437,696 paired
flattened/TLAS rays and134,784 independent source-ray/material pixels. These
prove presentation of the accepted trajectory, including the failing case;
they do not turn it into a settled qualification. Immutable selection without
resimulation, relocation/reopen and atomic wrong-revision refusal pass.

Surrounding checks pass: native admission/runtime35 normally and sanitized,
immutable result49, native owner rollback in both builds and seven contact/solver
groups including the new recovery load regression. The previous bottom-heavy
three-case arrangement passes again with all4,323 frames/contact arrays exactly
unchanged. This bounds the budget repair's effect on the accepted predecessor.
Broad UI/package suites were not run: no UI, renderer runtime or package code
changed, and the local native/result/contact boundaries have fresh direct proof.

## Reproduce and next boundary

```sh
python3 tests/qualify_rigid_topheavy_scene.py \
  --backend <ball-native-backend> --parent <parent-scene.json> \
  --out <fresh-output>
python3 tests/qualify_rigid_topheavy_scene.py \
  --backend <ball-native-backend> --parent <parent-scene.json> \
  --out <fresh-offset-output> --cases impact --projectile-offset-y .02
python3 tests/test_rigid_topheavy_contract.py
```

The first command passes; the second returns a physical qualification failure
with the declared masses in `tests/rigid_topheavy_brick_masses.json` and unchanged
`tests/rigid_settling_limits.json`. Keep that negative result visible.

Next: isolate the offset case's small post-impact rocking/contact jitter using
the retained trajectory before any capacity increase. This checkpoint does not
qualify larger structures, arbitrary mass ratios or general piles. Installed
apps, canonical adoption, versions, production and remote jobs are unchanged.
