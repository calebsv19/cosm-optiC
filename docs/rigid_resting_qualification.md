# Resting dynamic blocks and finite-mass impact qualification

The local development rigid-scene lane now qualifies supported dynamic blocks at
rest and reacting to another dynamic body. This is a bounded physical, result-geometry and contact-presentation qualification.
General pile solving and operator visual acceptance remain separate.

The fixture uses a static floor, 2 kg blocks (0.8 × 0.6 × 0.5 m) and an 8 kg convex
cube projectile (0.4 m sides, 3 m/s launch). Blocks begin with zero velocity and
remain simulation-owned throughout. They are never converted to static bodies,
put to sleep, frozen or activated by a scripted collision event. The projectile is
a cube in this qualification; a spherical projectile remains a separate case.

Six cases run for five seconds at 240 Hz: one block resting, two blocks resting,
offset projectile impacts against each, and an offset two-block stack with and
without impact. The stack offset is (0.12, -0.08) m. Contact coefficients are
restitution 0.05, block/floor friction 0.6 and projectile friction 0.15.

## Repair and fixed checks

Imported convex assets retain source triangles. Clipping against just one triangle
of a flat physical face produced an off-center support patch, artificial torque
and an unstable stack. Contact generation now reconstructs the coplanar convex
face boundary before clipping, within its existing eight-vertex face capacity.
The existing triangle path remains for larger faces; those are not qualified here.
Allocation or degenerate reconstruction failure refuses the query. Source geometry,
mass properties, impulse law, solver iteration budgets and overlap recovery stay
unchanged. No shared library or schema changed.

Before repair, the impact accepted 3.492 mm overlap and the undisturbed stack failed
after tick 346. The unchanged full-mesh overlap gate is 2 mm. After repair, the
maximum across all six cases is 0.912 mm. The centered resting stack settles by
about 1.326 mm total; its final two-second drift is below 3e-13 m. Resting-case
limits also bound velocity, angular velocity, orientation change and kinetic energy.
Limits are recorded in `tests/rigid_resting_limits.json` before their respective
measurements. The independent SAT checks use transformed visible source meshes,
not the contact diagnostics alone.

An isolated zero-gravity, frictionless 8 kg/2 kg head-on impact checks momentum,
restitution and energy loss independently. Outgoing speeds are 2.37 and 2.52 m/s;
vector momentum error is below 4e-15 kg m/s and kinetic energy agrees with
28.818 J to about 1e-13 J. A centered impact produces no material artificial spin.

Normal and ASan/UBSan frames, contacts and poses agree exactly for 7,327 scene
snapshots across the six cases and isolated oracle. optiC consumes all 4,804 source
ticks across the four base cases with exact poses/quaternions and independently
checked visible-mesh vertices. Existing runtime, immutable-result, rollback,
contact/manifold/friction/world and BR6 qualification regressions pass.

## Repeat locally

Build the Ball native tool into a fresh task-owned directory as described in
[the rigid workflow](rigid_scene_workflow.md), then run:

```sh
python3 ray_tracing/tests/qualify_rigid_resting_scene.py   --backend <build>/rigid_scene_native   --parent <retained-pillar-scene>/scene.json   --out <fresh-proof>
```

The parent supplies scene envelope/camera/light presentation only. All collision
assets are generated and independently prepared by the selected native build.
Use `--cases stack-rest stack-impact --stack-offset 0.12 -0.08` for the offset
variant. Repeating with an instrumented native build provides sanitizer parity;
compare frame arrays, since engine-bound asset identities legitimately differ.

## Remaining boundary

The apparent separation reported during initial image review was a review error,
not a renderer or physics defect. A subsequent source-to-pixel proof verifies
continuous blue/orange contact; six fresh renders are byte-identical to the retained
originals. See [contact presentation proof](rigid_contact_presentation.md) for
paired acceleration checks, independent source-ray/RGB checks and injected-band
negative controls. Physical poses and renderer runtime remain unchanged.

The current eight-body, discrete, one-convex-child profile is unchanged. A six-brick
structure, long-term stacking, sleeping/waking, CCD, fracture, native physical
property authoring, installed packages, workers and releases are not qualified by
these tests.

The later [six-brick qualification](rigid_six_brick_qualification.md) separately
qualifies two crossed layers and repairs coupled recovery, pair-cache lookup and
an unrelated shared-edge trace remapping defect. It does not change the earlier
contact-presentation correction or imply general tower behavior.
