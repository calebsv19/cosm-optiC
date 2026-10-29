# Rigid pillar scene qualification

BR6 qualifies a bounded local scene with a floor, two static pillars and one
simulated convex solid. The original asymmetric tetrahedron and a separately
prepared six-vertex triangular prism each pass three cases: gravity drop,
zero-gravity isolated pillar impact, and a gravity-driven throw that contacts
both a pillar and the floor.

This is a two-second, four-body qualification at 240 Hz. It does not establish
general stacking, long-term resting stability, concave collision, CCD or
physically correct torque-free gyroscopic precession. The supported solver
profile still has one convex child per body and discrete collision stepping.

## Physical checks

The checked-in limits are in `tests/rigid_scene_qualification_limits.json`.
They are declared before the run and are not loosened to accept a failure.

| Boundary | Limit or requirement | Qualified result |
| --- | --- | --- |
| Accepted source-mesh overlap | At most 2 mm, independently checked with convex SAT | At most 0.501 mm across all six cases |
| Incoming contact-point depth diagnostic | At most 35 mm | At most 22.006 mm |
| Static bodies | Exactly unchanged serialized state; native adapter also checks body bytes | Pass at every accepted tick |
| Quaternion squared-norm error | At most 1e-10 | Below 7e-16 |
| Gravity velocity oracle | At most 1e-10 m/s | About 2.3e-15 m/s |
| Semi-implicit position oracle | At most 1e-10 m | About 2.9e-14 m |
| Isolated restitution | Outgoing speed within 0.025 m/s of the lower body's restitution times incoming speed | 0.35000000000000225 m/s for expected 0.35 m/s |
| Source mesh / Ray world vertex mapping | At most 2e-12 m | Pass at all 2,886 scene snapshots (11,544 body poses) |
| Replay and sanitizer parity | Exact serialized state | Pass |

The prism has a 0.8 by 0.5 m right-triangle base extruded 0.6 m, offset from its
source origin. Its independently calculated COM and principal inertia per unit
mass agree with native preparation to better than 1.2e-16. It is not a renamed
copy of the tetrahedron or a bounding-box collision substitute.

The full-convex overlap check is deliberately separate from
`maximum_penetration_m` in the result's contact diagnostics. The latter measures
contact witnesses before positional recovery and can understate full-solid
overlap for tilted geometry. It is not an accepted visible-mesh clearance test.

## Repairs found during qualification

The first run exposed accepted overlaps of about 28 mm against the floor and
61 mm against a pillar. Contact impulse solving alone left static penetration
largely unresolved. Repeating recovery from clipped manifold point depths also
missed overlap in the combined gravity throws.

Ball's native scene adapter now uses its existing full convex-pair penetration
query for bounded positional recovery against static bodies. It revisits static
pairs for the configured recovery budget, changes only dynamic position, and
refuses an unconverged tick. Static state and the solver's impulse, orientation
and velocity response are preserved. The whole operation remains inside the
existing staged/atomic tick boundary. Other simulation families' room policies
are unchanged.

Ray's immutable-result reader also now accepts a verified asset library with
unused solids. It authenticates every artifact while deriving render/physical
metadata only for referenced assets. Corrupt unused bytes and duplicate asset
IDs or roots still refuse admission and retain prior playback.

## Visual checks

Four gravity-driven cases have 61-frame, 256 by 192 diagnostic sequences at
30 fps. Each presentation frame maps exactly to source tick `frame * 8`; there
is no motion interpolation. The fixed camera, light and static geometry remain
consistent. Selected frames before, at and after impact are audited for visible
projectile pixels and exact result/tick identity.

Full-sequence review sheets show both solids falling/tumbling, striking a pillar
in the throw cases, reaching the floor and responding without visible mesh-frame
misalignment. The comparison video plays at half speed and labels simulation
time. These are direct-lighting geometry/motion proofs, with visible shadow
aliasing; they are not optical-quality or material-look acceptance. The retained
editable scenes and immutable results are the authoritative pose/geometry proof.
A human acceptance session and installed-app delivery are separate.

## Repeat locally

Build native tools in fresh task-owned roots. From the parent checkout folder:

```sh
make -C ray_tracing BUILD_DIR_BASE=<task>/ray-build \
  ray-tracing-render-headless rigid-result-consumer-probe
make -C ball_bounce_sim CONTRACT_BUILD_DIR=<task>/ball-build \
  rigid-scene-native-build rigid-scene-cache-io-build
```

Run `tests/qualify_rigid_pillar_scene.py --help` for the explicit parent fixture,
backend, pack reader, Ball tool, renderer, probe and fresh output arguments. The
parent is the original BR5 three-asset floor/pillar/tetrahedron baseline, a portable prepared scene directory containing `scene.json` and
`result/`, as produced by [the local workflow](rigid_scene_workflow.md).
The harness prepares its own source assets, authors named test fixtures and runs
native replay, analytic controls, source-mesh SAT and all-tick Ray geometry checks.
It does not modify the parent or an existing result store.

`tests/render_rigid_pillar_proof.py --help` renders a qualified named case with a
presentation-only 30 fps scene. `tests/test_rigid_unused_assets.py --help` checks
the unused-library integrity and alias boundaries. Native runtime/result,
consumer sanitizer, rollback and surrounding scene regression suites remain
separate verification layers. No package, release or remote worker is required.
