# Finite-edge departure and detached continuation

The native Ball adapter supports a bounded transition from captured flat sliding,
through moving contact on a finite rectangular donor edge, into detached motion.
Departure occurs when the moving body's trailing bottom corner reaches that
edge. The adapter locates that event within the tick, retains its pose, linear
velocity and principal-axis angular velocity, and integrates gravity through the
remaining fraction and subsequent ticks. No positional correction or artificial
minimum event time is applied.

Admission remains one dynamic rectangular convex source on one static rectangular
source, normal gravity, positive tangential slip and positive normal reaction
through contact, and zero rolling/torsional resistance. Whole-interval reaction,
slip and source-coordinate bounds certify contact. The departure root must agree
with the finite material boundary to coordinate roundoff. In detached motion,
`vx - height*omega > 0` and a total angle in `[0, 0.2]` certify that every source
corner remains outside the donor's side plane for the entire interval. Principal
axis rotation and exact constant-gravity translation carry the retained state.
Zero normal reaction before the trailing-corner event remains a refusal.

The existing displacement/envelope limits, 240 Hz clock, energy/work tolerance
(0.1 percent), momentum and external angular impulse tolerances (1e-8), and
coordinate tolerance (1e-12 m) remain in force. The original cumulative work
identity spans impact, supported sliding, edge contact and free flight. Contact
impulse ends at departure. Later detached ticks carry no contact constraint.
All private branch/event state participates in the existing staged tick, so a
failed event or continuation commits no body, world/cache, clock or branch state
and publishes no result.

A diagonal 2 m plate case with initial tangential speed 8 m/s departs in tick 63
at approximately 0.259848966 s. The retained 2 m/s sliding case still refuses
beyond its prior angle boundary; this profile does not widen that interval.
`tests/qualify_rigid_edge_departure.py` checks finer independent Newton/Euler
constraint solves, ballistic motion, physical ledgers, repeatability, sanitizer
parity, rollback within and after departure, and immutable record/replay. optiC
consumes ordinary recorded poses through its existing compound-motion bridge.

This is a finite rectangular departure profile. It does not qualify slip reversal,
zero-reaction release, landing on another face/body, concave compound settling,
or general multi-body contact switching. See [sliding edge support](rigid_sliding_edge.md)
for the preceding profile and [current truth](current_truth.md) for wider limits.


## October 26 successor — landing into receiver edge support

A separately admitted lower receiver now accepts landing from the retained detached state. The tilted impact and remaining-tick support retain pose/velocity, positive reaction/slip, the original work ledger, physical gates and full rollback. The representative case lands in tick114, continues through120 and safely refuses after128 before the next face switch. See [retained landing profile](rigid_retained_landing.md). General concave landing/settling remains separate.
