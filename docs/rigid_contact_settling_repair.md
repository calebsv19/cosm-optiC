# Contact geometry and angular-drift settling repair

The top-heavy six-brick rest, original strike and2cm offset strike now pass all
194 physical checks in normal and ASan/UBSan builds. The original six-second
scene and seconds4–6 settling window are unchanged. This closes the
[previous top-heavy offset settling hold](rigid_topheavy_qualification.md).

## Two numerical inconsistencies

The native scene adapter inherited a25mm static-support height band from the
legacy gravity-room presentation path. It could turn raised vertices of a
tilted block into unilateral contacts. During the failing quiet window, the
observer finds3,570 floor-witness samples above the existing0.5mm slop, with a
maximum24.166mm height. For the two upper bricks alone,1,165 such samples occur,
up to16.285mm above the floor. These are solver witnesses, not extra rendered
geometry. Full-solid recovery subsequently moves bodies and changes which
support witnesses appear on the next tick.

The adapter also discovered contacts at the predicted orientation and retained
that pre-impulse angular drift after the contact solver arrested or reversed
spin. Linear drift had already been reconciled to solved velocity. An independent
one-step quaternion oracle detects a maximum0.016782 component error between
accepted orientation and the rotation implied by solved angular velocity.

## Explicit native-scene behavior

`BallRigid3DCompoundWorldConfig.bound_static_support_to_contact_depth` selects a
support band of **measured contact depth plus existing position slop**. Native
rigid scenes enable it. Both face and point/edge support refreshes use that band;
raised corners outside it are excluded. The default remains false, preserving
the legacy gravity-room25mm behavior. This is a local Ball numerical profile;
shared schemas, material coefficients and pinned consumer donor lanes do not
change. Finite static-collider footprint clipping and general contact geometry
remain outside this floor/tower qualification.

Accepted orientation now follows the same semi-implicit convention as accepted
linear drift: after contact impulses change angular velocity, integrate the
previous accepted orientation once using the solved world-space angular
velocity. If spin is unchanged, retain the original free-flight bytes. Native
angular position correction is explicitly disabled, so it cannot be silently
mixed with this integration. Existing world position corrections and full-solid
recovery remain, and staged validation/rollback still precede acceptance.

The256-sweep recovery allowance, impulse solver passes, friction, restitution,
contact slop, source overlap gates, timing, initial states and all settling
limits are unchanged. No sleeping, pose pinning or extra damping is added.

## Results under the same gates

| Top-heavy case | Maximum drift, seconds4–6 | Physical checks |
| --- | ---: | ---: |
| Undisturbed | 4.99e-13m | 62/62 |
| Original strike | 4.58e-13m | 66/66 |
| 2cm offset strike | 9.00e-13m | 66/66 |

Previously the offset drift was2.08148mm; speed0.04071m/s, angular speed
0.19015rad/s and kinetic energy0.002800J also failed. All now pass the unchanged
2mm /0.01m/s /0.02rad/s /0.002J /0.01rad gates. Upper blocks still satisfy the
same60° source-up tilt and0.35m COM-drop test: both at tick210 in the primary,
and ticks210/213 in the offset. Contact-free ballistic intervals remain valid.
Full source overlap stays below0.501mm against the2mm gate.

There are now zero floor-witness samples above0.5mm in the same quiet window;
the highest is0.489797mm. Full-solid recovery no longer nudges the upper bricks
in that window. Stage observer values agree with exported accepted states within
2.23e-16 (full-precision diagnostic output versus JSON serialization).

The contact-band-only intermediate passed the top-heavy case but failed the
bottom-heavy offset (2.670mm drift). It is retained as a failed intermediate,
not qualified. Adding consistent angular drift passes both and the equal-mass
arrangement. The combined repair is the delivered checkpoint. Earlier dated
failures remain historical evidence; their windows were not relabeled or widened.

## Independent checks and presentation

- Normal/sanitizer/repeated replay/portable results match exactly across4,323
  snapshots,34,584 body poses and contact arrays. Leak detection is disabled.
- The independent quaternion oracle passes30,240 dynamic-body steps, with zero
  component error at the retained precision. Its pre-repair negative fails.
  A gravity-driven, spinning projectile has61 byte-identical free-flight states
  against the prior runtime.
- All nine six-brick cases across top-heavy, bottom-heavy and equal masses pass
  their existing gates. Six resting controls plus the finite-mass impact oracle
  also pass; together these add420 normal physical checks beyond the top-heavy194.
- Native runtime35 normally/sanitized, immutable result49, native rollback in
  both builds, seven surrounding contact/solver groups and the geometric
  support-band positive/negative test pass. The world/recovery contracts also
  pass ASan/UBSan. The geometric test finds an old witness19.599mm above an
  independent floor plane and excludes it under the native mode.
- Signed linear momentum closes within1.42e-10kg·m/s per step after gravity and
  fixed-floor reaction. This is not an exact energy/work or angular-momentum
  conservation claim, nor a gyroscopic integration qualification.
- Ordinary native optiC playback checks all4,323 ticks and1,245,024 source
  triangle-vertex samples. Thirteen views pass1,437,696 paired flattened/TLAS
  rays and134,784 independent source-ray/material pixels. Saved-result
  selection without resimulation, relocation/reopen and atomic wrong-revision
  refusal pass. Material edits affect only a review copy's presentation.

New rigid assets/results bind a new native engine identity. Re-prepare inputs
when producing a new simulation; preserved immutable results remain separate
accepted trajectories. Renderer/runtime/UI/package source is unchanged in optiC;
only its integration test observer and docs are added. No installed app,
canonical adoption, version edit, release, worker rollout or remote job occurs.
Broad UI/package gates are omitted because no such behavior changed; the native,
consumer and physical boundaries above have fresh direct proof.

## Reproduce and next boundary

Run `tests/qualify_rigid_topheavy_scene.py` with a fresh native backend, compatible
parent scene and fresh output, as described in the previous qualification.
Use `--cases impact --projectile-offset-y .02` for the offset. The existing
`tests/rigid_settling_limits.json` and `tests/rigid_topheavy_brick_masses.json`
remain the contracts. `tests/rigid_pose_consistency_probe.c` and its supplemental
Makefile provide the independent native angular-drift oracle.

This clears the settling hold for the declared240Hz/eight-body profile, including
floor and projectile. Larger structures require a separate capacity/design and
qualification slice. The recovery budget remains finite; arbitrary feasible
chains, piles, complex static support footprints, fracture, CCD and full rigid
body scientific validation are not established by this checkpoint.
