# Photon quality settings — bounded Main Edit qualification

This is local source qualification of the charcoal-lens fixture, not an installed
preset change or release. The accepted actual-hit receiver and exact support-search
repairs are retained.

## Measured starting settings

| Use | Emitted photons | Status |
|---|---|---|
| Fast preview | 8,192 | Low cost; more spatial variation |
| Cleaner preview | 32,768 | Intermediate measured tradeoff |
| Balanced work | 65,536 | Preferred measured cost/quality balance here |
| Animation starting point | 65,536 | Provisional; motion acceptance still required |
| Critical still/reference | 262,144 | Initial reference setting; larger surface maps now supported |

Use explicit `direct_map`, `k_nearest`, `surface_query_radius=0.045`,
`surface_gather_max_radius=0.1`, radiance scale 1 and fixed emission seed for
comparisons. These radii are world-space lengths and depend on scene scale.
The fixture is `tests/fixtures/ppm_charcoal_lens_acceptance/scene_runtime.json`.
No universal radius or photon budget is established for other scenes.

The qualification holds 256×192, 16 passes, effective four diffuse samples,
two transmission samples, photon depth request 10 and denoise off fixed.
Requests for one diffuse sample are normalized to four. This is a photon-quality
comparison, not new qualification of a complete high-resolution glass preset.

## What the comparison supports

Seed-A focus NRMSE against independent seed B at 262K is 16.69%, 13.41%, 11.79%,
11.69% and 9.80% at 8K/32K/65K/131K/262K. The two ceiling seeds disagree by
9.85% normalized RMS. Sixty-four native linear-radiance sample pixels and all
16 subpasses are matched to contribution-off; signal sums are not integrated
surface energy. Neither ceiling realization is converged ground truth, and
single-run shared-host timing does not establish universal throughput.

There is little measured reason for a separate 131K preset here. Tightening
support to 0.01 raises error to 13.08%. Widening support to 0.09 or changing the
cap to 0.045/0.2 does not improve the selected focus, though some faint pixels
elsewhere change. Explicit neighbor-gather 16/64 yields 32.16%/18.61% error and
is not preferred for this fixture. Same-budget seed-A emitted/stored flux is
unchanged across those support/gather variants.

## Effective control semantics

The original qualification above used the former262,144 ceiling. Surface-only
maps now support up to1,048,576 emitted photons and records; the owned volume/beam
route remains capped at262,144. A524,288 surface request now emits524,288, and
requests above1,048,576 are explicitly capped. Progress exposes requested/effective/
limit/limited; post-frame photon runtime adds emitted count, storage capacities,
capacity rejections and prepared support-neighbor limit. Record storage bytes are
not whole-process memory. The original finite-reference metrics remain historical;
larger counts are not automatically converged or recommended as defaults.

For `k_nearest`, sample-centered supports use population-derived neighbor counts.
`caustic_photon_surface_gather_neighbors` tunes the separate gather estimators;
changing it from 16 to 64 in `k_nearest` is pixel-exact in this test. Gather max
radius is normalized to be no smaller than surface query radius.

All sixteen native cases complete, and accepted 8K/65K/262K images replay exactly.
Animation motion, other spatial distributions, higher resolution, combined high
camera/transmission budgets and measured-material calibration remain separate.

The higher-budget source qualification preserves legacy65K/262K pixels, linear
probes and energy/query readback exactly. Support preparation still caps its
neighbor target at256 for the larger maps; reference/filter, motion and volume
qualification remain separate. Reliable adaptive stopping is the next documented
work area, with active photon surfaces kept conservative until separately proven.
