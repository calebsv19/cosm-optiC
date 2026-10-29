# Separately qualified angular support continuation

October 25, 2026. Local Main Edit source and headless profiles; the numerical
producer is Ball Bounce and optiC consumes immutable poses.

## Admission and ownership

Two new, independently qualified reduced-coordinate profiles supplement the
existing captured-impact and Coulomb sliding owners. They start from an authored
supported pose, with exactly zero COM velocity. They do not transfer a captured
sliding contact into an edge constraint. Only one dynamic rectangular solid and
one stationary rectangular platform are admitted. The adapter verifies all eight
distinct corners of each actual single-child convex asset in a common frame,
source COM alignment, contact height, principal-axis inertia and finite-platform
clearance. Child material overrides are excluded. Unsupported geometry follows
its existing owner; a failure after entering either new profile refuses its tick.

The app-local module is `src/interop/rigid_scene_angular_support.c` in Ball.
`core_scene`, `core_sim` and `core_pack` retain their existing metadata, tick and
storage roles. No shared API/minimum/version changes or renderer-side solver.

## Pinned support-edge tipping with rolling resistance

The box starts flat and stationary with its COM beyond one platform edge. A
fixed material contact line on its bottom face is a valid no-slip constraint only
while the normal reaction is positive and the entire reaction remains within the
rotating face's Coulomb cone. The edge axis must be a principal inertia axis;
the box's transverse span must fit the platform. This is an authored overhang,
not the retained sliding-off-edge case.

With outboard COM offset `d`, normal height `h`, and angle `theta`, the COM relative
to the edge is `x = d cos(theta) + h sin(theta)` and
`z = h cos(theta) - d sin(theta)`. The parallel-axis inertia is
`Ip = Iaxis + m (d*d + h*h)`. Rolling length `ell` is the maximum of the two
material lengths, consistent with the existing angular-resistance law. The
positive tipping branch solves

```
alpha = m [g*x - ell*(g*cos(theta) - omega*omega*h)] / (Ip - ell*m*d)
Nface = m [g*cos(theta) - alpha*d - omega*omega*h]
resisting torque = -ell*Nface
```

Sixteen RK4 substeps integrate one supported segment, its rolling work, reaction
impulse and external torque. This does not add contact-solver sweeps or recontact
events. Conservative whole-tick bounds on angle, angular speed, acceleration,
normal force and the rotated friction cone must pass before integration. The
qualified branch remains within 0.2 rad and 1 rad/s; its conservative bounds can
refuse earlier. For this rectangular geometry, every bottom-face point inboard
of the pivot rises above the platform, and below-plane points lie outboard.
This provides an interval geometry certificate rather than an endpoint-only
penetration test. The COM and orientation come from the physical constrained
coordinates; no unconstrained pose is projected onto an edge.

## Supported torsion and physical stopping

A box can start with pure spin about the support normal, zero COM velocity and
zero Coulomb friction. A circumscribed disk of its bottom face must fit strictly
inside the platform throughout rotation. The normal is a principal inertia axis.
Torsional length uses the same maximum-material rule. Constant opposing torque
`ell_torsion*m*g` decelerates the spin. The interval splits at the exact physical
stop `Iaxis*abs(omega)/(ell_torsion*m*g)` and integrates the remaining supported
rest. This also handles a stop inside a tick without an endpoint clamp or an
artificial minimum time. Simultaneous distributed sliding friction and spin is
not admitted by this profile.

## Gates, results and rollback

The original 240 Hz admission, gravity limit, displacement/partition budgets,
1e-12 m coordinate allowance, 0.5 micrometre analytic position, 1e-6 m/s velocity,
0.1% cumulative work/energy, 1e-8 impulse/angular-impulse and 2 mm source-overlap
gates remain fixed. Static support has an explicit external reaction/couple.
The edge integrator checks independent force and torque integrals against the
body's momentum changes before committing. Energy references persist across
ticks. Normal and tangent loads use existing result fields; result schemas and
immutable storage are unchanged.

The angular-support identity, reference orientation, angle, speed and cumulative
loss stage with the world, bodies, caches and clock. A refused initial or later
tick leaves these bytes unchanged and publishes no result/head. Existing sliding
finite-patch exit, general rotating contact, detachment, sliding/tipping transfer,
new-face landing, multiple dynamic bodies and concave multi-child tipping remain
outside this profile. This is not a general rigid-body settling solver.

## Qualification and rendering

`tests/qualify_rigid_angular_support.py` prepares actual box STL assets, checks
independent finer-step pendulum and analytic torsion references, deterministic
repeat, immutable record/replay, forced post-support rollback and refusal
publication. Normal and ASan/UBSan builds cover six tipping cases (zero/positive
rolling length, direction, mass, IDs and object order), three two-second torsion
cases (both signs and a stop inside a tick), and three safe refusals (low friction,
later interval exit and excessive travel). Each accepted tipping case spans
12 ticks, or 50 ms; this is short bounded onset/continuation, not a completed fall.
The longer interval-exit control proves later refusal. Existing angular-resistance
kernel controls and retained flat-face friction controls supply adjacent proof.

Complex meshes can already render from accepted immutable results. New local
640x480 direct-light bracket and arch images use exact revision/tick evaluation;
the arch also renders tick 240 with a different physical pose and image. These
are technical render proofs, not finished materials, lighting or visual approval.
Arbitrary concave drop/tumble/land/settle behavior remains separately unqualified.
No canonical adoption, installed-app refresh, package, remote job or release.


October25 successor: [sliding-edge transfer](rigid_sliding_edge.md) now carries the retained sliding trajectory through a physical face-to-edge switch. It does not pin the sliding body; later branch limits still refuse atomically.
