# Twelve-brick offset query repair and qualification

October 21, 2026. Local Main Edit numerical and presentation qualification.
The unchanged +0.02 m offset strike now completes all 1,440 ticks and all 84
physical checks pass normally and under ASan/UBSan. Every brick settles within
the existing four-to-six-second window. No capacity, material, time window,
tolerance, sleeping, damping, pinning or impulse-law adjustment was made.

## Cause and bounded repair

At tick 150, recovery queried brick4/body162 and brick8/body278. Their accepted
staged geometry was separated: an independent full box SAT over actual principal
vertices gives a 0.002227365923079483 m gap. GJK cycles at both 32 and 128
iterations. Increasing the iteration allowance therefore does not repair the
query. The original penetration adapter returned ITERATION_LIMIT before using
its existing face/edge support-projection checks.

The adapter now runs those existing checks after an inconclusive GJK search.
A found separating axis certifies separation. Without such a certificate it
still returns ITERATION_LIMIT: bounded/truncated directions never imply overlap
or touching. Diagnostics preserve the actual limited GJK kind and iteration
count. GJK/simplex code, sphere recovery, solver response and scene integration
are unchanged. The repair is an in-place change in Ball's existing penetration
query; it introduces no shared or renderer API.

The frozen source geometry, adjacency, staged poses and independent vertex SAT
oracle live in the existing penetration contract test. It reproduces the old
failure, verifies that insufficient projections still refuse, checks a real
nearby overlap, and exercises 68 small-position, swapped-body and common-origin
translation controls. The test fails against the retained original query and
passes normally and sanitized against the repair. Shared core_collision3d's
existing point/plane contract does not own this polytope query; no shared module,
version or adoption change was needed.

## Physical evidence

Normal and sanitized runs agree on all 1,441 snapshots / 20,174 body poses and
contacts. Each has 84 passing physical checks and exact native repeat. The first
150 frame values (ticks 0–149) exactly match the original runtime before its
first refused tick. All three upper bricks simultaneously overturn at least
60 degrees and fall at least 0.35 m, at ticks 277, 282 and 287. Maximum independent
source-mesh overlap is 0.000500999874607 m against the unchanged 0.002 m gate.

Every brick passes the same quiet-state gates. Worst four-to-six-second position
drift is 2.551e-13 m, linear speed 6.178e-12 m/s, angular speed 2.503e-11 rad/s,
and kinetic energy 5.404e-23 J. These are deterministic numerical results, not
laboratory validation or a promise for arbitrary larger structures.
The independent quaternion oracle checks 18,720 dynamic steps with zero component
error. Upper bricks have 24/13/38 consecutive contact-free ballistic ticks.
Signed momentum balance includes gravity and static-floor reaction and has a
maximum step-component residual of 1.642e-10 kg*m/s.

The two prior twelve-brick rest/centered cases and all three prior top-heavy
six-brick cases remain exactly unchanged: 7,205 snapshots / 74,932 body poses.
The original frozen failure remains retained in its prior checkpoint.

## Verification and presentation

Fresh native builds pass; the frozen penetration regression passes normally and
with ASan/UBSan, and the retained old query fails that regression as expected.
Nine surrounding GJK/penetration/contact/world groups pass. Native runtime
38-case suites pass normally and sanitized; the 49-case result-store suite and
late-failure owner rollback checks also pass. Broad desktop/application/package
and installed-worker gates are outside this native collision repair; no new
Desktop or release acceptance is claimed. LeakSanitizer is not claimed.

Native producer/store replay and portable optiC admission pass. Every accepted
source tick and vertex matches renderer playback: 1,441 scene samples,
726,264 triangle-vertex comparisons, maximum error
1.00169872397e-13 m. Eight selected views include ticks 149/150,
where execution previously stopped, and the final quiet state. They pass
884,736 paired native rays and
82,944 independent source/material pixels.

A new 512x384 preview decodes as exactly 32 frames at 16 FPS, two seconds.
Frames 0–31 map exactly to native ticks 0–465 in steps of 15, with no interpolation
or time stretching. It shows the opening collision/fall; six-second results and
selected final views establish settling. Full decode succeeds, source bundle
hashes remain unchanged, and raw frames are deleted after verification.

## Remaining boundary

The selected rest/centered/offset twelve-brick qualification is complete.
The native envelope remains 16 bodies / 120 pairs / 120 constraints with eight
distinct assets and one convex child per body. General unresolved GJK searches
still refuse; arbitrary larger/concave/CCD/compound configurations are not newly
qualified. Larger structures require a separate fixture/design and qualification
step. Canonical, installed app, worker, release and protected P5b lanes are
unchanged. Application VERSION files are unchanged.
