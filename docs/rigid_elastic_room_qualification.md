# Analytical sphere and elastic mixed-room qualification

The October 21 Main Edit implementation adds a verified analytical sphere to the
existing Ball producer → immutable result → optiC playback workflow. Ball owns
collision, mass properties and accepted poses; optiC owns visible geometry and
rendering. Rendering never advances the physics solver.

## Contract and authoring

`prepare-sphere source.stl radius_m asset_id new_directory` creates the same
three-file asset layout as ordinary `prepare`. The collision file has the typed
`BALL_RIGID_ANALYTIC_SPHERE_V1` header, exact radius, source/engine identities and
isotropic inertia per unit mass `2*r*r/5`. The native loader independently
recompiles the source, checks the visible file, reconstructs the entire collision
record and requires byte equality. Invalid radii, wrong mass properties, altered
source/visible/collision files and stale engine identities refuse admission.

The source STL must be closed and convex, centered at the sphere origin, with
vertices on the declared radius and triangle planes within 3% of that radius.
Its visible surface is an inscribed approximation. Contact support and inertia
use the analytical sphere, not those triangles. The current explicit radius
range is 1 micrometre–100 metres; these are bounded development assets.

Shared `core_scene` development source 1.6.0 adds `analytic_sphere` metadata and
an explicit contact-policy enum. Recompile consumers after the public struct
change. `min_restitution_geometric_friction_v1` preserves the existing 0.5 m/s
restitution cutoff. Opt-in `min_restitution_geometric_friction_zero_cutoff_v1`
sets that cutoff to zero and avoids the legacy static support-normal heuristic.
Both policies still combine restitution by minimum and friction geometrically.
The policy is part of the physical projection: changing it invalidates a saved
result. Old results and default-policy behavior remain supported.

## Measured source/tool qualification

Eleven controls cover normal/low-speed/oblique wall reflection, equal/unequal
masses, off-center rotational transfer, a static sphere and three 13-body rooms.
The rooms contain six boundaries, two fixed obstacles, four initially resting
2 kg dynamic cubes and an 8 kg sphere. One variant substitutes a convex wedge.
All run at 240 Hz with no prescribed object motion. Elastic cases use zero
gravity, restitution one on every contact and zero friction.

All eleven primary cases pass, repeat exactly and match between normal and
ASan/UBSan builds: 8,171 scene snapshots. Isolated kinetic-energy errors are at
floating-point roundoff, and isolated dynamic-pair linear momentum is exact.
All four props move in each elastic room. Maximum relative kinetic-energy drift
is 0.000179585 (about 0.018%) in the box-obstacle room and below 2e-15 in the wedge
variant, against the predeclared 2% mixed-room limit. Independent source-geometry
checks keep maximum overlap below 0.501 mm against the existing 2 mm gate.

The gravity/friction companion is deliberately **not qualified as fully settled
throughout 4–6 seconds**. All four props pass the original quiet-window gates;
the sphere briefly reaches 0.01496 m/s and 0.10583 rad/s near the window start,
above the 0.01 m/s and 0.02 rad/s limits. It is stationary by six seconds. Drift,
orientation, energy and overlap gates pass. No threshold/window change, added
sleeping or artificial damping turns that failure into a pass.

Ordinary native optiC playback checks all 4,323 room ticks and 7,452,852 source
vertices, with maximum error below 2.5e-15 m. Selected new-room playback also
passes consumed-module ASan/UBSan checks. Sphere-body controls inspect the actual
GJK shape, curved support and inertia, rather than accepting metadata alone.
Malformed-asset and policy-change refusal tests preserve the result boundary.

## Reproduction and boundaries

Ball tests: `tests/test_rigid_elastic_scene.py`,
`tests/test_rigid_sphere_refusals.py`, `tests/audit_rigid_elastic_scene.py` and
`make rigid-scene-sphere-contract-build`. The geometry audit intentionally exits
nonzero for the retained sphere quiet-window failure. optiC's generic
`tests/test_rigid_elastic_playback.py` compares the source result at every tick.
Use `tools/rigid_scene_workflow.py run` to prepare a portable verified scene.
The local proof includes a 96-frame, 16 FPS cutaway preview; hidden presentation
walls remain active physical colliders.

This is bounded Main Edit source/tool qualification. Capacity remains 16 bodies,
120 pairs/constraints and 8 distinct assets. Discrete stepping and the existing
displacement guard remain; no continuous collision detection, general concave or
compound asset import, gyroscopic model, new installed app, worker or release
is implied. Ball rolling/contact quiet-window qualification is the next focused
physics issue before calling the dissipative mixed room settled.

## Follow-up diagnostic — original motion explained

Seven isolated floor controls now separate expected rolling from a numerical
settling defect. The original room replays all1441frames exactly in normal and
sanitized builds. Both speed/spin gates remain satisfied from tick965(4.020833s),
just after the original4s window begins; the historical4–6s check remains failed.
Rest/drop controls settle, pure rolling persists, sliding approaches5v0/7 and
normal-axis spin persists. A fresh floor-only world initialized from the original
ball's3.75s state retains its motion/energy over2.25s; clock/contact cache reset
is explicit. The original room decelerates during later prop contacts.

No solver defect is established by this effect. The model has Coulomb contact
friction but no rolling/torsional resistance. Realistic gradual stopping would
require an explicit qualified model extension. No damping, sleep, threshold,
window, saved-result or numerical source changes were made. The maintained
companion diagnostic is Ball Bounce tests/diagnose_rigid_sphere_settling.py;
its source reference is docs/rigid_sphere_settling_diagnostic.md in that repo.
