# Bounded fast/thin-feature motion diagnostic

October 22, 2026 — Main Edit diagnostic complete. **The existing adapter accepts
three missed crossings. General fast/thin-feature collision and CCD remain
unqualified.** This slice changes diagnostic tests/build routing and docs only;
it does not repair collision behavior or relax any existing limit.

## Results

Twenty-two controls use the existing native 240 Hz clock and contact policies:

| Classification | Cases | Meaning |
| --- | ---: | --- |
| Accepted contact | 5 | Four pinned bracket/arch surface controls at5 m/s and one slow thin-plate control contact and reverse |
| Accepted clear | 2 | Arch passage at11.9 m/s and a low angular-speed free arch remain clear |
| Safe refusal | 12 | Native guard/envelope refusal with complete rejected-tick rollback |
| Accepted missed crossing | 3 | Diagonal thin plates pass through one another between sampled endpoints |

All 140 retained snapshots and all 22 owner results match across normal repeat and
ASan/UBSan. All 12 refused cases stop at tick0, preserving world, bodies, cache
and clock; a faulted owner cannot advance again. Static body bytes remain stable.
These deterministic results describe the defect; repeatability does not make
the missed crossings physically correct.

The four pinned face controls at11.9 m/s refuse with
`contact_partition_envelope`, despite being below their scene's nominal12 m/s
translation threshold. Their5 m/s controls succeed. At12.1 m/s those same scenes
refuse with `discrete_displacement_budget`. An open arch passage succeeds at
11.9 m/s and refuses at12.1 m/s. This separates collision/envelope refusal from
free-passage behavior. Free-arch angular controls at6.50835 rad/s succeed;
23.4301 and 26.2937 rad/s refuse at the unchanged displacement guard. The
initial surface-speed estimate alone is not an admission guarantee for rotation.

## Constructive missed-crossing witness

The analytic convex control consists of two 2m by2m by4 mm plates, parallel and
rotated diagonally in source coordinates. Their normal is(1,1,1)/sqrt(3).
The minimum source AABB extent is1.63530256m, so the current source-AABB/4 guard
allows0.40882564m per output tick, much larger than the4 mm physical thickness.
One plate is static; the other starts with its center14 mm before the static
center along that normal, leaving a10mm surface gap. Both have zero angular
velocity; gravity and friction are zero, restitution is1 and cutoff is zero.

| Speed | Analytic overlap interval within first tick | Native result |
| --- | --- | --- |
| 5 m/s | 0.002–0.0036s | Accepted, no contact impulse |
| 50 m/s | 0.0002–0.00036s | Accepted, no contact impulse |
| 98.0200 m/s | 0.000102020–0.000183636s | Accepted, no contact impulse |

The output tick lasts1/240s, about0.00416667s. At5 m/s, signed center distance
changes from-14 mm to+6.83333mm across ticks 0–1. Both endpoints are separated,
but at an intermediate time all eight collider corners coincide with the static
plate's corners, giving positive overlap volume0.016m³. This is an analytic
constant-orientation witness between accepted poses, not an extra simulated frame.
At1m/s the control contacts and reverses; just above the existing plate travel
budget the owner safely refuses. No threshold was changed to obtain this result.

Independent checks map the collision hull back to the original source corners
within3.19e-16m, retain visible source coordinates exactly and match analytic
unit-mass inertia within3.89e-16m². The geometric thickness is therefore present
in both source and collider. The runtime's full-segment endpoint-depth branch can
accept separated endpoints without testing the intervening crossing. The
source-AABB budget is not a local feature-width or continuous-collision guarantee.

## Persistence, playback and reproduction

Six selected accepted trajectories—including all three physically missed
crossings—pass sealed producer/store verification and portable optiC playback:
62 ticks and 6,480 original source vertices, maximum error6.67e-16m. This proves
faithful consumption of the supplied wrong trajectory, not collision accuracy.
The preceding spinning bracket/arch pair still reproduces 481 frames exactly.
The new test-only make target changes the sealed engine identity because the
makefile is hashed; ordinary preparation reconstructs/rebinds collision identities.
No digest guard is bypassed and numerical source/limits remain unchanged.

From the Ball checkout, use fresh contained build/output roots:

```sh
make BUILD_DIR=<normal-build> rigid-scene-native-build \
  rigid-scene-cache-io-build rigid-fast-motion-observer-build
make BUILD_DIR=<san-build> \
  DEBUG='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  rigid-scene-native-build rigid-fast-motion-observer-build
PYTHONDONTWRITEBYTECODE=1 python3 tests/diagnose_rigid_fast_thin.py \
  --backend <normal-build>/contracts/rigid_scene_native \
  --observer <normal-build>/contracts/rigid_fast_motion_observer \
  --sanitizer <san-build>/contracts/rigid_scene_native \
  --sanitizer-observer <san-build>/contracts/rigid_fast_motion_observer \
  --out <new-diagnostic>
```

`diagnostic.json` distinguishes completion of the diagnostic from failed physical
CCD qualification. `rigid_fast_thin_oracle.py` owns the independent collider and
sub-tick witness checks. `rigid_fast_motion_observer.c` observes natural refusals;
it adds no runtime feature or mutation beyond ordinary stepping in its own test
owner. This does not certify arbitrary shapes, rotations, grazing paths,
simultaneous many-body collisions, visual appearance or long-term behavior.
Program versions, shared APIs, canonical source, installed apps and releases stay
unchanged. No additional release version change is recommended for this slice.

## Next boundary

Qualify the existing Ball `ball_rigid3d_compound_cast_3d` donor against these exact
misses and clear/contact/refusal controls, before adopting a swept-contact path
in the staged adapter. Its presence in source is not proof of current adapter
use or readiness. Preserve current travel guards and atomic failure behavior;
keep general rotating/many-body CCD and the separate near-budget envelope
refusals outside that first integration decision.
