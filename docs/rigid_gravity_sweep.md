# Isolated gravity-driven swept impact

October 23, 2026 — bounded Main Edit source/headless qualification.
Ball's native adapter now supports a ballistic swept impact tick for one dynamic
body against one static body, with gravity parallel to the incoming motion.
The actual contact time, impulse and remaining free segment include gravity.
No shared scene/result schema or optiC runtime change is needed.

## Supported path

A separate `rigid_scene_gravity_sweep.c` owner first tests the bounded line
profile. Gravity and incoming velocity must be parallel within 1e-12 relative
direction roundoff; velocity must not reverse over the candidate tick.
The scene retains its existing 50 m/s² gravity ceiling and 240 Hz clock. At that
ceiling the corresponding curvature roundoff is below 5e-16 m per tick. Initial
rotation, two dynamic bodies, nonparallel motion and larger groups retain the
previous owner/refusal policy. Geometry still needs a certified positive-area
face patch and a whole-compound supporting plane.

The existing full-kick displacement admission runs before the new query.
The donor queries the exact ballistic endpoint `p + v dt + g dt²/2`. Its
line-distance witness is mapped to time by the stable positive quadratic root.
Ball advances by `v t + g t²/2` and updates velocity by `g t` to actual impact.
The existing manifold solver consumes that retained pose without COM/attitude
projection, added solver iterations or warm-start history.

After response, let `r` be normal recession, `S` the existing energy-bounded
rotational surface speed and `a` relative normal gravity. The whole-compound
support gap is bounded below by

```text
gap(t) >= measured_gap + (r - S) t + a t²/2
```

Outward-rounded endpoint and conservative stationary-minimum checks certify the
whole remaining interval against the same -1e-12 m coordinate allowance.
Concave closing-gravity motion needs both endpoints; convex motion also needs
its possible interior minimum. A second contact inside the tick refuses.
Position and velocity then advance ballistically through the actual remaining
time; orientation uses the existing free-rotation integrator. Final gap,
cumulative travel, envelope and finite-state checks still apply.

## Explicit work accounting

Each free segment accounts for gravitational work `W = sum(m g dot delta_p)`
over the actual displacement. Before impact, the kinetic-energy change must
match that work. After impact, the existing energy gate consumes kinetic energy
minus remaining-segment work, preserving the 0.1% tolerance and restitution
policy. Gravity is never treated as unexplained kinetic drift. Static reaction
can exchange momentum; independent qualification balances the known gravity and
collision impulses instead of claiming closed-system conservation.

## Qualification and boundary

Fifteen normal/repeat/sanitizer controls pass: ten accepted and five atomic
refusals. Accepted cases include gravity toward/away from the obstacle, half/zero
restitution where a free segment is safe, 50 m/s, a 3 kg dynamic mass, reversed
IDs/object order, small gravity and a 10 micrometre plate impact from rest.
The independent oracle checks impact time, final position/velocity, expected
restitution loss, gravitational work, impulse and momentum budget. Sixteen
scalar interval cases include independent parabola checks, an interior minimum,
consumed allowance, closing overflow and invalid inputs.

Fixed gates remain 0.5 micrometre position, 1e-6 m/s velocity, 0.1% work-adjusted
energy, 1e-8 kg m/s momentum-ledger error, 2 mm full-source overlap and the original
motion/capacity/envelope/rollback limits. Each accepted case also proves a later
injected failure rolls back world/body/cache/clock exactly. Every natural refusal
publishes no result, head or staging. Sanitizer runs disable leak detection.

This qualifies one impact tick and its remaining free segment. Later non-impact
ticks retain the previous gravity integrator; they have not acquired general
ballistic free-flight semantics. The existing endpoint/support policy is retained.
Immediate gravity-driven resting contact, recontact, arbitrary curved/rotating
CCD and coupled groups remain separate. The original off-axis gravity refusal
remains refused. No canonical/installed/package/release or rendered-visual claim.
See [resting continuation](rigid_resting_continuation.md) for the prior profile.

All 20 earlier response/resting histories and all 15 original bracket/arch
histories reproduce exactly with both producers. The bracket/arch replay covers
6,735 full frames. Each producer passes 38 runtime and 49 result checks.
The 45 current/retained playback scene cases pass 7,331 ticks and 905,928
source-vertex comparisons per build, with maximum error 1.555e-15 m. All 21
current scene bundles verify and replay after relocation. These are scene-case
counts across overlapping test matrices, not counts of unique physical inputs.

Later October23: [gravity-driven rest/recontact continuation](rigid_gravity_contact.md)
repairs the two retained refusals and qualifies captured subsequent ticks. The
15-case checkpoint above remains historical;13 unchanged controls remain here
and the two repaired inputs move to the new contact matrix.
