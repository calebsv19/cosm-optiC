# Landing from retained detached motion

The native Ball adapter supports a bounded landing from the retained finite-edge
departure onto a second, lower static rectangular receiver. One rectangular body
moves under normal gravity. The donor and receiver are authored static boxes,
with the receiver below and outboard of the donor and wide enough for the full
moving edge. Both landing restitution coefficients must be zero; contact friction
uses the existing geometric combination. Rolling/torsional resistance and other
shapes or moving receivers remain outside this profile.

Before departure, the receiver must remain separated by more than the complete
existing point-travel budget. The unchanged two-body path then produces the
impact, flat sliding, edge transfer and departure. Once detached, the adapter
retains that branch's pose, velocity, spin and original work identity. It locates
the unique first crossing of the leading source edge with the receiver's top
plane, using the actual rotating ballistic trajectory.

At landing, a planar Coulomb line-resultant impulse closes the normal velocity
while preserving positive tangential slip. The solved pose is retained. The
remainder of the tick and later ticks integrate sliding leading-edge support,
including its nonzero angular velocity. This is a moving contact on the receiver,
not a pinned body or an assumed rest state. A positive normal reaction and slip,
finite receiver footprint, original-donor separation, and angle strictly between
zero and 0.2 rad must hold over each complete subinterval.

The 240 Hz clock, source/coordinate tolerances, travel and whole-body envelope,
0.1 percent energy/work gate and 1e-8 linear/external-angular impulse gates remain
unchanged. The cumulative work ledger includes initial support, flight, landing
impact loss and subsequent sliding loss without resetting its reference. A failed
landing or continuation commits no body, world/cache, clock or private branch
state and publishes no result. Unrelated triples keep their previous routing.

The diagonal plate trajectory lands at approximately 0.473993842 s in tick 114.
A 120-tick prefix continues in edge support; the longer case accepts through tick
128 and safely refuses before flattening into the next face contact. A second
case lands one microsecond after a tick boundary. The preceding 101 body-pose
frames match the retained departure case exactly with the separated receiver
added. `tests/qualify_rigid_retained_landing.py` provides independent Newton/Euler
and impulse matrix references, repeat/SAN parity, transition rollback, immutable
record/replay and retained controls. optiC consumes the ordinary recorded poses.

See [edge departure](rigid_edge_departure.md) for the preceding profile. The next
boundary is opposite-edge/flat-face contact on the receiver. Zero-reaction release,
slip reversal, arbitrary concave landing and general settling remain separate.

October 27 successor: [receiver face qualification](rigid_receiver_face.md) and [complex STL collision status and priority](rigid_asset_collision_status.md).
