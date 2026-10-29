# Bounded resting continuation after a swept impact

October 23, 2026 — Main Edit source/headless qualification. The retained centered
zero-restitution plate impact now completes its remaining tick. The actual
post-impulse velocity and orientation advance unchanged; this certificate adds
no damping, pose projection, frozen motion or solver iterations.

The previous separating-speed certificate remains the first path. If it fails,
Ball's `rigid_scene_resting_certificate.c` bounds the entire remaining interval:

```text
closing_bound = max(0, rotational_surface_bound - normal_recession)
minimum_gap_bound = measured_impact_gap - closing_bound * remaining_time
```

Each arithmetic operation rounds outward. Admission requires finite inputs,
nonnegative time/surface speed, the existing impact gap check, and a minimum gap
bound of at least -1e-12 m. This reuses the impact/final-plane coordinate allowance;
it does not increase contact slop or the 2 mm independent source-overlap gate.
The initial measured gap consumes the allowance. A short interval can certify a
minute residual that must refuse over a longer interval. Final plane, envelope,
displacement, finite-state, energy and momentum checks remain in force.

The retained refusal had normal recession -2.57078e-12 m/s and rotational bound
6.81535e-12 m/s over 0.00216667 s: possible closure about 2.034e-14 m. The accepted
result preserves the original nonzero post-impulse velocity exactly and advances
its position by that velocity through the real remaining time.

## Qualification and limits

Nine accepted resting cases cover the retained input, 50 and 98.0200356 m/s,
equal/unequal finite masses, common/tangential translation, reversed pair order
and two seconds of resting continuation. Two offset cases with larger closing
motion still refuse atomically and publish no result. All eleven cases agree
exactly normally, on repeat and under ASan/UBSan. Every accepted case also passes
a later-tick injected failure with byte-exact world/body/cache/clock rollback.
Fourteen certificate admission cases include independent thin-box support
oracles, longer-window refusal, consumed allowance and invalid-input controls.

The updated swept-response matrix passes 36 controls; its original ten accepted
histories remain exact. Existing 38 runtime and 49 immutable-result checks pass.
The physical gates stay 0.1% elastic/expected inelastic energy, 1e-8 closed linear
and angular momentum, 2 mm source overlap, 0.5 micrometre analytic position and
1e-6 m/s analytic velocity. Static supports can exchange momentum with the scene;
closed-system conservation applies to the two-dynamic-body cases.

This extends the isolated two-body zero-gravity certified face-impact profile.
Gravity/work-energy, larger coupled groups and general rotating/concave CCD
remain separate. Shared contracts, versions, canonical/installed applications,
packages and releases are unchanged. Sanitizer runs disable leak detection.
See also [the preceding swept-response profile](rigid_swept_response.md).

All 15 original bracket/arch histories also reproduce 6,735 full frames exactly
in fresh normal/sanitized producers. The 35 current/legacy optiC scene cases pass
7,311 ticks and 904,488 source-vertex comparisons per build; maximum error is
1.555e-15 m against the unchanged 2e-12 m gate. Twenty portable scene bundles
(19 unique result revisions) verify and replay after relocation.

Later October23: [isolated gravity-driven swept impact](rigid_gravity_sweep.md)
adds a separate ballistic line profile. Immediate gravity-driven rest/recontact
still refuses; the zero-gravity qualification above remains historical evidence.
