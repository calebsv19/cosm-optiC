# Sliding-to-edge transfer and finite contact switching

October 25, 2026 — bounded Main Edit source/headless implementation.

A captured flat-face Coulomb sliding pair can now reach the finite platform edge,
switch at the physical loss of a realizable interior pressure resultant, and
continue through the remaining tick and later ticks on a sliding edge contact.
The material point in contact moves along the dynamic body's bottom face. It is
not pinned. This supplements the separately qualified stationary overhang and
supported-torsion profiles in [angular support](rigid_angular_support.md).

## Admission and physical event

The profile admits one unrotating dynamic rectangular solid sliding along a
principal in-plane direction on one stationary rectangular platform, under normal
gravity. Both collision shapes must have one convex child whose eight distinct
corners form the verified rectangular geometry. Transverse support clearance,
actual contact height, principal inertia and original travel limits must pass.
Rolling/torsional resistance, initial spin and oblique gravity retain their
existing owners/refusals. Captured-contact identity is required; arbitrary
initial-support scenes are not silently moved into this branch.

For height `h`, geometric material friction `mu`, positive tangential speed `v`,
and COM distance `x` from the platform edge, the flat pressure resultant lies at
`x + mu*h`. Its arrival at zero is the event. The existing quadratic sliding
trajectory supplies the stable physical root; there is no synthetic progress
step or invented minimum time. A finite rectangular certificate also carries the
short pre-edge interval where the previous conservative excursion bound can
refuse before the actual event. The accepted earlier history is preserved.

## Moving contact on the edge

In the plane of sliding, let `theta` be positive tipping angle and `omega` its
rate. The contact normal rotates with the body's bottom face. The COM height and
the material coordinate in contact follow the physical constraint:

```
z  = (h - x*sin(theta)) / cos(theta)
xi = (-x + h*sin(theta)) / cos(theta)
vz = (xi*omega - vx*sin(theta)) / cos(theta)
slip = vx*cos(theta) - vz*sin(theta) - h*omega
N = [g*cos(theta) - omega*(2*slip + h*omega)]
    / [1/m + xi*(xi - mu*h)/I]
alpha = N*(-xi + mu*h)/I
ax = N*(sin(theta) - mu*cos(theta))/m
```

The branch requires strictly positive slip and reaction. Its contact force lies
on the same Coulomb cone with the existing geometric material coefficient.
`xi` must remain within the finite bottom face. Nonnegative angle and angular
speed follow from `xi' = -slip` and the initial `xi = mu*h`. The box's bottom
plane separates the entire static platform while `0 <= theta < pi/2`; the
certified implementation stops within the narrower 0.2-rad branch.

Each supported tick uses sixteen RK4 substeps. Outward-rounded interval
arithmetic encloses the complete physical interval of each substep and certifies
positive slip/reaction, finite material-contact coordinate, rotation and travel.
It does not merely sample the RK4 endpoints. These are integration substeps,
not additional point-solver sweeps or recontact events.

## Gates, work and transaction

The original 240 Hz, gravity, travel/partition, 1e-12 m contact-coordinate,
0.5 micrometre reference-position, 1e-6 m/s velocity, 0.1% work/energy,
1e-8 momentum/angular-impulse and 2 mm source-overlap gates remain fixed.
The cumulative energy reference passes unchanged through the contact switch.
Gravity work and dissipative work `integral(mu*N*slip)` remain separate.
Native force and external-torque integrals check both linear and angular momentum
before commit. The flat segment includes the moment of its interior pressure
resultant; after switching, reaction acts at the static edge.

Identity, event time, basis, orientation, material coordinate state and all work
state stage with bodies, world, caches and clock. Initial/later interval refusal
or a forced fault after switching leaves all pre-tick bytes unchanged and
publishes no result/head. Existing result schema and immutable storage remain
unchanged. optiC continues to consume exact poses without a physics solver.

## Scope and qualification

`tests/qualify_rigid_sliding_edge.py` compares the motion with an independent
Newton/Euler/constraint matrix solve and a finer-step reference. It covers the
retained trajectory, the event tick and following tick, opposite/second tangent
directions, mass, geometric material combination, reversed IDs/object order,
stopping before the edge, later refusal, deterministic repeat and post-switch
rollback. Normal and sanitizer poses, immutable results and native optiC playback
are qualified separately in the retained internal evidence.

This is a flat-face to sliding-edge contact switch. Edge departure, slip reversal,
detachment, new-face landing, coupled groups and concave compound contact switching
remain separate. A longer refused trajectory is not advertised as a complete
fall or settling sequence. The implementation is app-local in Ball; existing
core_scene/core_sim/core_pack semantics and versions are reused.
No canonical adoption, installed-app refresh, package, remote job or release.


## October 26 successor — finite-edge departure

A separately certified trailing-corner event now transfers moving edge contact into gravity-driven free flight within the remaining tick and subsequent ticks. Positive outward separation and the existing 0.2 rad range remain required. The retained slower sliding case keeps its prior refusal; new-face landing and zero-reaction release remain separate. See [departure profile](rigid_edge_departure.md).
