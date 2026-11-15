# Bounded V3 fog and glass estimators

The ordinary saved `surface_path_v1` profile accepts two additional experimental
transport choices. Defaults remain unchanged pending independent variance and
cost qualification. This is local source/headless qualification, not installed
application or release adoption.

```json
{
  "medium_sampling": "split_first",
  "caustics": {"camera_fresnel": "split_bounded"}
}
```

`medium_sampling` accepts `analog` (default) or `split_first`.
`caustics.camera_fresnel` accepts `roulette` (default), `split_first` or
`split_bounded` or `split_camera_exit`. These controls are independent of `camera_sampler`, photon
launch/population and reconstruction. Unknown values and malformed types fail.

## Transport ownership and weights

For the first eligible homogeneous exterior scattering segment, extinction
sigma_t, scattering sigma_s, length L and T=exp(-sigma_t L), the medium split
traces a ballistic boundary continuation with weight T and a collision
continuation with weight (sigma_s/sigma_t)(1-T). Collision distance has conditional
PDF sigma_t exp(-sigma_t t)/(1-T). Their product recovers the physical scattering
integrand. Zero/absorption-only handling stays analytic. `expm1` and `log1p`
preserve small-optical-depth precision; vanishing collision mass falls back to
the ordinary path. This is one bounded split, not exact multiple-scattering
integration.

Mapped volume is gathered once before splitting. The ballistic child skips that
same gather and segment collision sampling; it retains all prior scattering,
depth, PDF and inside/outside state. Actual collisions advance scattering order.
The ordinary linear components retain exact recombination. No new component
should be added on top of total volume or scattering-order subsets.

Bounded glass mode retains the first-interface split and adds at most three
further reflection/transmission splits. It skips roulette on split specular
chains, preserving their actual weights. Fresnel probabilities, radiance eta
factors, absorption, total internal reflection, finite event/depth caps and
map-owned source-hit exclusions remain in force. It does not solve arbitrary
specular light connections.

The two budgets are shared across all children of one primary path: one medium
split and three extra glass splits, plus the existing first-interface split.
Maximum branch count is six when both controls are enabled. The work guard is
`width * height * samples * max_depth * maximum_camera_branches <= 1200000000`.
It refuses excess work before rendering. Branch counters report medium splits,
additional internal interface splits and skipped specular roulette. Primary
paths subtract all split continuations from total paths.

`runtime_spectral_surface_path.c` owns setup/admission/film accumulation;
`runtime_spectral_surface_path_trace.c` owns continuation and branch state;
`runtime_spectral_surface_path_estimator.c` owns pure split math and signal
addition. Diagnostics and strict saved-profile parsing remain separate owners.

## Qualification and limits

The fixed-camera measured tungsten/LED glass/full-fog fixture holds maps,
reconstruction, exposure and thresholds fixed. Combined six-seed RMS reductions
are 53%/43% on floor focus and 68%/70% on the glint. All combined mean gates pass.
The held-out tungsten beam variance is 1.429 times baseline and fails its 1.0
limit despite better pooled results. Defaults are therefore not promoted.
A simultaneous 384-sample local pair costs 2.42 times baseline CPU time; this is
not a whole-frame benchmark. The unchanged guard rejects 1024 combined samples
at the 256x192 sensor/depth10; its maximum is 406. No limit was relaxed.

Independent split/PDF integration, Beer visibility and a closed IOR1.5 slab
finite-depth reflection series pass. All nine baseline mixed-physics groups
pass in 157 cases. The original four-seed 1cm fog precision case still fails
3% SE at 3.023%. A separate fixed-camera/fixed-1cm study isolates photon work:
65,536 photons/eight maps gives 4.72% ensemble SE; 262,144 photons/16 maps gives
1.81%, with about 7.23% map-to-map SD. The ensemble passes; single-map or
whole-scene convergence is not established. Fixed bandwidth bias remains.

Use independent camera/map axes, original and held-out seeds, mean stability,
linear XYZ/component covariance and equal-time work. Do not hide a failed
regional gate with pooled averages, denoising or a wider reconstruction kernel.

## Reproduction

```sh
make BUILD_DIR_BASE=<fresh-build-root> test-spectral-estimator
python3 tests/integration/test_spectral_estimator_transport.py --help
python3 tests/integration/test_spectral_estimator_controls.py --help
python3 tools/spectral_estimator_qualification.py --help
python3 tools/spectral_estimator_analysis.py --help
```

The qualification tool separates `regions` and `photons` phases. Region inputs
reuse the measured-source fixture and original-sensor region harness described
in [error diagnostics](spectral_error_diagnostics.md). Retain exact scene/assets,
request, build identity and controls. Region output is a diagnostic patch, not a
full-frame render. Strict controls test branch-aware preflight refusal; analytic
transport controls independently check direct visibility and slab transmission.
Focused ASan/UBSan covers new trace/estimator modules, with unchanged dependencies
built normally. Ordinary glass/fog, opaque surface, headless preflight and image
export regressions pass. Whole-product/native-UI qualification remains separate.

## November 13: camera-exit allocation and equal-time qualification

`caustics.camera_fresnel: "split_camera_exit"` retains the initial Fresnel split
and reserves one additional split for an eligible glass exit before any diffuse
or volume interaction. Other interfaces use ordinary Fresnel sampling. It keeps
specular-chain roulette suppression and the existing optical weights and source
ownership. Together with `medium_sampling: "split_first"`, its maximum is four
branches per primary sample, compared with six for `split_bounded`. A shared
helper drives work admission and diagnostics. Defaults remain unchanged.

Eight new discovery seeds per measured source pass all regional gates at equal
256 samples. Camera-exit costs 1.75–1.77 times baseline locally, compared with
2.32–2.33 for the broader split. The held-out test freezes baseline 256 samples,
bounded 96, and camera-exit 128. Camera-exit CPU ratios are 0.997/1.004, making
this a close local equal-time comparison. It improves floor RMS 26%/27% and glint
42%/43%, but tungsten beam variance 2.298 and LED floor-edge 1.298 fail unchanged
1.0/1.1 limits. All paired mean gates pass. The prior two-seed tungsten beam
failure also remains (1.400 at equal samples). Neither split policy is promoted.

Scattering-order covariance attributes about 81% of candidate tungsten beam
variance to two-or-more ordinary fog collisions; mapped-volume attribution is
about 0.6%. Exact sample-tail replay identifies a five-collision source-hit
branch after glass and diffuse reflection, including roulette compensation from
0.289 to 1. This motivates continuation-survival and repeated-scatter proposal
work, without changing photon reconstruction. A rare high weight alone does
not establish estimator bias.

158 regional cases include the original regressions and two exact-image tail
audit/replays. Sixteen analytic controls, 30 mixed glass reference/scaling cases,
strict parser/work admission, focused trace/estimator sanitizers, ordinary glass,
component writer and headless preflight/export pass. No full-frame timing,
whole-product/native-UI or installed-app qualification is inferred.

```sh
python3 tools/spectral_beam_qualification.py --help
```

Run phases in order: `discovery`, `equal-combined`, `candidate`, `heldout`,
`original`, with explicit `--cli`, `--inputs` and a fresh `--output` root.
The first equal-time calibration ran approximately 15% over baseline CPU;
the held-out sample counts were frozen from timing before its image review.
Keep actual CPU ratios alongside variance. The runner verifies scene/map/film
invariants and preserves existing identities, with 180 regional launches,
300 seconds per case and 1800 seconds per phase as ceilings. Small diagnostic
patches and eight held-out seeds do not prove full-scene convergence.

## November 13: continuation roulette after fog scattering

`surface_path_v1.continuation_roulette` accepts `throughput` (unchanged default)
or experimental `fog_survival`. At eligible roulette decisions after at least
one real volume collision, `fog_survival` uses survival 0.95 and compensates
survivors by dividing throughput by 0.95. Before a collision, the original
throughput policy applies. Existing specular-chain roulette suppression, physical
absorption, Fresnel/eta weights, PDF/MIS terms, photon ownership and finite caps
are unchanged. Maximum branches remain four with camera-exit plus medium split.
Diagnostics expose the effective policy, fog decisions and survivors.

The identity p * (beta/p) = beta preserves conditional expected contribution;
more low-weight paths survive with smaller individual weight increases. One
candidate was frozen before independent tests. Timing-only calibration chooses
112 candidate samples versus 256 baseline and 128 camera-exit samples. Eight
untouched seeds per measured source yield CPU ratios 1.032/1.024 to baseline.
All candidate-versus-camera-exit regional gates pass. Fresh beam variance ratios
0.731/0.669 pass, but colored-patch ratios 1.218/1.441 fail the unchanged 1.1 gate.
On eight prior seeds, tungsten beam 1.178 still fails 1.0 despite improving from
2.298. All paired mean checks pass. Defaults are not promoted.

Exact replay reduces the identified sample's total contribution by about 69%,
changing its share of one pixel from 76% to 48%. Its first compensated roulette
jump changes from 0.289→1 to 0.289→0.304; the trace remains complete. Residual
variance is predominantly ordinary orders 2–4, including the failing colored
patch. This calls for repeated-scatter continuation/direction/light diagnostics,
not a wider photon kernel. No reconstruction or denoising changed.

A uniform-radiance conservative-fog furnace provides an independent energy
reference of 1. Eight seeds per policy exercise order 5+; mean radiance is
0.999271 for throughput and 0.999226 for fog-survival. Normal and focused
ASan/UBSan runs pass. Sixteen analytic transport cases, 30 mixed glass/scaling
controls, strict parsing, exact legacy and audit replay, ordinary glass and
headless smoke checks also pass. This is local source/headless qualification.

```sh
python3 tools/spectral_fog_roulette_qualification.py --help
python3 tests/integration/test_spectral_fog_roulette.py --help
```

Use `calibrate`, then `heldout`, then `previous`, with explicit `--cli`,
`--inputs` and a fresh `--output`. The frozen budget file must be preserved.
The runner permits 130 regional launches, 300 seconds per job and 1200 seconds
per phase. Current evidence contains 98 regional cases including six legacy/
tail replays; physical controls have their own bounded suites. Timing refers
to local original-sensor patches, not a full-frame performance guarantee.
The existing mixed-reference runner accepts `--continuation-roulette` and
records it in completion metadata.

## November 13: repeated-scatter angular light and direction MIS

`surface_path_v1.volume_direction_sampling` accepts `phase` (unchanged default)
or experimental `light_mis`. On the emitting side of an explicit area rectangle,
from the second fog collision onward, a spherical cap encloses the source; a
hemisphere is used near its plane. The light proposal is uniform solid angle
inside this bound. Missed-rectangle directions are null samples, never resampled.
Continuation uses q = 0.75 pHG + 0.25 pLight with throughput pHG/q. Direct-light
MIS competes against q, and emitter-hit MIS uses the matching preceding-volume
light proposal. Surface and delta/map ownership remain unchanged. First-collision,
sky, native-source and back-side cases preserve the prior sampling path.

The dedicated `runtime_spectral_surface_path_volume_sampling.c` owns proposal
geometry, sampling and evaluation. Diagnostics report the effective policy and
trace the phase value, proposal PDF and continuation multiplier. Eligibility is
geometric, independent of sampled visibility, spectrum and audit pixel. The
physical phase, wavelength, fog, camera, maps, reconstruction and thresholds are
unchanged. See [PBRT's phase/PDF and volume MIS treatment](https://pbr-book.org/4ed/Light_Transport_II_Volume_Rendering/Volume_Scattering_Integrators)
for the estimator convention; this bounded-cap mixture has its own qualification.

Tail replay found source hits after glass and repeated scattering very near the
area-light plane. The selected sample shares fall from 68%/69% to 37%/56% at equal
112 samples. On eight new seeds per source, colored-patch variance versus the
prior fog-survival policy is 0.781/0.775. However, tungsten color variance versus
original baseline is 1.561, LED floor-edge variance 1.140, and LED beam variance
versus fog-survival 1.109: these fail their unchanged gates. All paired means pass.

Initial tungsten CPU ratio 1.111 fails the 10% timing gate. A timing-only correction
freezes 112 samples on eight further untouched seeds: CPU ratio 0.989, color
variance 0.784 versus baseline and 0.629 versus fog-survival. That group passes
all baseline gates, but floor edge 1.162 versus fog-survival fails. Older seed
regressions retain tungsten color 1.146 and beam 1.072 baseline failures.
Defaults remain unchanged; no group is hidden by pooling.

140 valid regional cases and six rejected timing-runner attempts are retained.
The rejected attempts used the wrong sample count and were replaced, not counted
as equal-time evidence. The portable analyzer rejects mixed sample budgets.
Independent cap support/moments, phase importance energy and analytic rectangle
solid-angle MIS integrals pass. Normal and focused ASan/UBSan area/fog per-order
means and source-scaling controls pass (34 cases each), as do furnace, Beer,
strong-glass, 30 mixed visible-wavelength controls, strict parsing, exact default
replay, ordinary glass and headless preflight/export. Instrumentation covers
trace, estimator and volume sampling, with unchanged dependencies built normally.
This is bounded local headless qualification, not installed-app or whole-frame
convergence. Photon bandwidth and finite-depth errors remain separate.

```sh
make BUILD_DIR_BASE=<fresh-root> test-spectral-volume-sampling
python3 tests/integration/test_spectral_volume_sampling.py --help
python3 tools/spectral_volume_sampling_qualification.py --help
```

The regional tool uses `calibrate`, `heldout`, `previous`, explicit CLI/input/output
paths and the original-sensor harness. Limits are 150 launches, 300 seconds per
case and 1200 seconds per phase. Preserve frozen budgets, raw components and
failed groups. Next: separate angular light sampling from the continuation
mixture in an ablation, then qualify a tighter rectangle-angle proposal and
whole-frame/temporal behavior before default adoption.

## November 14: angular light-only versus coupled continuation

`volume_direction_sampling: "light_only"` is a saved diagnostic ablation. It
uses the same angular light proposal and emitter-hit light PDF as `light_mis`,
but continues with HG sampling, continuation PDF pHG and unit phase/PDF weight.
Direct-light MIS competes against pHG. `phase` remains the ordinary default;
`light_mis` retains its 75/25 mixture. All use the same eligibility and null-cap
sample handling. This adds no branch, reconstruction or physical-medium change.

At equal samples, eight source/seed pairs preserve the phase reference's direct,
surface-indirect, surface-caustic, caustic-bounce, mapped-volume and first-scatter
components bit-for-bit, together with path termination ledgers. Four complete
phase/coupled replays match prior raw output exactly. The new control isolates
repeated-scatter lighting rather than changing path trajectories.

Timing-only calibration freezes phase 112 samples, tungsten coupled/light-only
116/116, and LED 116/108. Two untouched eight-seed groups per source use those
same counts. All pairwise CPU ratios are within 0.944–1.024 and every paired mean
gate passes. Camera, full fog, glass, maps, reconstruction, spectra, exposure and
thresholds are fixed. These are exact-sensor region tests, not whole-frame timings.

Light-only/coupled colored variance ratios are 1.002/1.040 for tungsten/LED in
heldout and 1.191/1.121 in confirmation. Removing the mixture does not resolve
the color failure. It helps the confirmation tungsten beam (0.904), but retains
floor/haze/glint tradeoffs. In confirmation, colored variance against phase is
1.121/1.331 for coupled and 1.335/1.491 for light-only. Both fail the unchanged
1.1 color limit; the 1.0 beam, 0.85 median and 10% timing gates also remain intact.
No method passes every regional and median gate across both groups; no default
is promoted. The comparison supports retaining coupled as the experimental
reference, not claiming universal convergence.

135 bounded regional runs, independent analytic PDF/MIS tests, 52 normal and
52 focused-sanitizer area/fog/order/scaling controls, furnace, Beer/slab, 30
visible mixed glass references, strict parsing/admission, exact replay/isolation,
ordinary glass and headless preflight/export pass their stated checks. Variance
failures remain failures. One inherited signedness warning in unchanged sequence
import code is outside this slice. Sanitizers cover trace, estimator and volume
sampling with normal dependencies; native UI/install/release remain separate.

```sh
python3 tools/spectral_light_ablation.py --help
```

Run `calibrate`, `heldout`, then `confirmation`, retaining the frozen budget file.
The runner verifies exact scene/map/film identity, uniform per-arm samples and
original acceptance gates, with 150 regional launches, 300 seconds per case and
1200 seconds per phase as ceilings. Each seed is paired across techniques;
replicas and calibration/validation groups are independent.

Next, measure cap misses, occlusion and contribution tails by source distance and
scattering order, then test tighter rectangle-angle light sampling against this
reference. The ablation rules out blaming the continuation mixture alone; it
does not prove the shared cap is the sole remaining cause. Map precision and
reconstruction remain separate error sources.

## November 14: rectangle solid-angle proposal and light diagnostics

The saved experimental policy `volume_direction_sampling: "rectangle_mis"`
samples the explicit area rectangle uniformly in solid angle at scattering
orders two and higher. The continuation mixture remains 75% HG / 25% light;
the phase/PDF throughput correction and both direct-light and emitter-hit MIS
use the same proposal PDF. `phase` remains the default. Existing cap `light_mis`
and HG-continuation `light_only` remain reproducible controls.

The implementation follows the spherical-rectangle parametrization of Urena,
Fajardo and King (EGSR 2013), also described in
[PBRT's rectangle sampling discussion](https://www.pbr-book.org/4ed/Shapes/Bilinear_Patches#Sampling).
A dedicated rectangle helper owns geometry, inversion and bounded fallback;
the volume proposal module owns the mixture and PDF evaluation. For solid
angles below 0.0001 sr or plane distance at most 0.00001 times the rectangle
half-diagonal, it uses the existing normalized cap instead. Fallback depends
only on geometry and is identical in both MIS techniques. There is no rejection
resampling and no fallback to uniform area while reporting a solid-angle PDF.
Sky/native sources, first scattering, maps and reconstruction retain their
existing paths. Floating-point edge tolerances and endpoint clamps are covered
by the independent CDF/support tests; these are numerical safeguards, not a new
physical blur or tail clamp.

`volume_light_diagnostics` adds a fixed-size, transport-neutral ledger to the
ordinary summary. Its 16 buckets cross scattering orders 1, 2, 3-4 and 5+ with
absolute emitter-plane distance divided by half-diagonal: below .01, .01-.1,
.1-1 and at least 1. Each bucket records attempts, angular/rectangle proposal
counts, angular misses, all invalid samples, occlusion, visibility and non-delta
volume emitter hits. NEE and emitter-hit sums, squared sums and maxima are
**unit-source scalar path contributions before spectral XYZ weighting**. They
are not image variance or a convergence score. The ledger skips sky/native
sources; emitter-hit entries exclude delta chains and photon-owned paths.
`angular - rectangle` is the eligible cap/fallback count. Back-side invalid
samples are included in attempts but not angular proposal counts, so use
`angular_missed / angular` when measuring proposal waste.

Use `tools/spectral_rectangle_qualification.py` with `calibrate`, `heldout`, then
`confirmation` to reproduce the exact-sensor six-region comparison. Timing-only
calibration freezes per-source sample budgets before two independent eight-seed
groups. The runner preserves camera, full fog, maps, reconstruction, source
spectra, exposure and the existing mean/variance/CPU thresholds. This is bounded
regional qualification, not whole-frame convergence or installation proof.

The bounded qualification completed with 138 regional runs and two independent
eight-seed validation groups per measured spectrum. Angular proposal misses in
heldout drop from about56.6% for the cap to0.0070% tungsten /0.0080% LED for the
rectangle policy. This resolves a measured proposal-waste problem in this fixture,
not all camera or photon-map noise. Ratios below compare rectangle with cap:

| Group | Source | CPU ratio | Beam variance | Color variance | Glint variance | Median variance |
|---|---|---:|---:|---:|---:|---:|
| heldout | tungsten | 0.892 | 0.856 | 0.717 | 1.140 | 0.951 |
| heldout | led | 0.963 | 0.839 | 0.786 | 1.079 | 0.858 |
| confirmation | tungsten | 1.008 | 0.939 | 0.727 | 0.998 | 0.900 |
| confirmation | led | 0.960 | 0.826 | 0.681 | 0.913 | 0.825 |

All original gates remain unchanged; no default is promoted. The heldout tungsten
rectangle/cap CPU ratio0.892 lies below the allowed0.9, so that comparison is not
called an equal-time pass. Mean and variance outcomes, timing failures and each
seed group remain separate in the retained analysis. Quiet timing replication
is needed before treating the full matrix as equal-time qualification.

The heldout glint's signed variance attribution is85–86% surface-indirect. That
label is depth-based and includes glass emitter paths with no fog scattering;
it is not proof that diffuse materials cause the glint noise. At equal112-sample
budgets, all eight source/seed pairs preserve direct and surface-indirect films
bit-for-bit between cap and rectangle. This confirms the repeated-fog sampler
does not directly improve that component. Different equal-time sample budgets
can still change its noise. First-scatter films also remain exact in these pairs.

Next isolate the glint's non-scattered glass/surface paths, reduce proposal setup
cost where possible, and qualify camera/branch allocation with independent
controlled timing. Keep repeated-scatter distance/order variance separate from
that glint work. Rectangle sampling remains an explicit experimental candidate;
map support, volume photon precision and reconstruction retain their separate
qualification boundaries. Do not conceal their errors with wider reconstruction.

Eight raw prior-policy replays, independent CDF/support/PDF/MIS energy checks,
70 normal and70 focused-sanitizer area/fog/scaling cases, furnace and Beer/slab,
30 mixed visible reference/scaling cases, strict controls, ordinary glass/fog,
and headless preflight/export pass. Native UI, install and release are outside
this bounded test. One inherited sequence-import signedness warning is unchanged.

## November 14: surface/volume MIS boundary correction

The glint diagnostic audit found that experimental angular policies could use
a volume light proposal at a surface reached after multiple fog collisions,
while the later surface-BSDF emitter hit still evaluated the area light PDF.
That violated the proposal pairing assumed by MIS. Surface vertices now always
use their existing area/sky/native source sampler, matching the competing
emitter-hit PDF. Volume vertices retain their selected volume proposal and
continuation mixture. This follows the previous-vertex PDF requirement described
in [PBRT's path-tracing treatment](https://www.pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer).

The ordinary phase route was unaffected. Replays show its complete film remains
bit-for-bit identical; corrected angular runs preserve direct, surface-indirect,
photon-map and first-scatter components, together with termination ledgers.
Only affected repeated-scattering lighting changes. Earlier angular-policy
variance studies remain historical evidence and require this correction before
reuse as current qualification; previous aggregate mean checks did not catch the
PDF mismatch. The new mixed surface/fog regression and diagnostic counter exercise
that boundary explicitly. Convergence statistics complement physical invariants;
they cannot replace them.

See [path classes and crossed budget diagnostics](spectral_error_diagnostics.md#non-scattered-indirect-classes-and-sampling-balance)
for the glint classification and broader camera/photon study. In the current
ordinary route, photon passes scale the launch budget as
`passes * photons_per_pass` for one seeded map; they are not independent-map
averaging. Camera wavelength strata depend on the requested sample count, and
ordinary jobs remain non-progressive. A future progressive or adaptive allocator
must define accumulation and unbiased stopping explicitly rather than treating
separately rendered sample-count settings as appendable prefixes.

## November 14: glint attribution and joint sampling-budget study

Ordinary summaries now expose disjoint non-scattered indirect classes and a
surface-light PDF consistency counter. The new audit reproduced and corrected
an angular-policy routing defect at surfaces after multiple fog collisions.
The phase default is bit-exact; corrected angular runs preserve non-scattered,
photon-map and first-scatter terms. Prior angular-policy studies require the
corrected route for current qualification. Aggregate mean agreement alone did
not detect the old mismatch.

A corrected 100-case exact-sensor regional matrix, plus eight before/after
replays, compares two measured spectra with independent camera and map seeds.
The four-by-four crossed estimates put focused-floor variance at roughly
40–51% camera and 46–56% map, with a small interaction term. The floor edge is
roughly 82–84% camera and 15–16% map. Glint, beam, haze and colored-patch variance
are predominantly camera-driven at the tested budgets. Within the non-scattered
indirect glint subset, glass paths containing reflection and transmission account
for 99.66%/99.75% of signed variance attribution under tungsten/LED.

The baseline is 128 camera samples and 65,536 photons. Fourfold camera samples
reduce camera-axis glint variance to 0.233/0.204 at about 3× regional CPU. Wider
existing glass splitting reduces it to 0.628/0.461 but costs about 2.4× CPU, so it
does not demonstrate a better efficiency choice here. Fourfold photons reduce
map-axis floor-focus variance to 0.180/0.287; that ratio does not describe camera
noise or total noise. In 32 controlled comparisons, direct, non-scattered indirect
and first-scatter films remain exact across map seeds/counts.

Existing full-path QMC helps several regions but regresses colored-patch variance
by 33.2% tungsten and 13.5% LED; LED CPU ratio 0.855 also misses the 0.9–1.1 timing
gate. Corrected rectangle-versus-phase comparisons pass regional and mean gates
in one eight-seed group per source, but tungsten median 0.882 misses the unchanged
0.85 target (LED median 0.832 passes this group). Defaults remain unchanged.

All paired mean gates pass. Independent classification/energy/sampling units,
70 normal and 70 focused-sanitizer volume cases, 32 normal and 32 sanitized mixed
surface/repeated-fog cases, furnace, Beer/slab, 30 mixed visible references,
strict controls, ordinary glass/fog and headless smoke pass. The new regression
explicitly exercises surfaces after multiple collisions and requires zero PDF
mismatches. One inherited sequence-import signedness warning remains unrelated.

The next direction is component-aware budget planning from an independent pilot,
followed by combined-budget full-frame and held-out-scene validation. Keep camera,
map and interaction uncertainty separate; account for map setup versus tracing
cost. Current passes build one larger seeded map, and camera wavelength strata
depend on requested sample count, so progressive accumulation and unbiased
adaptive stopping need explicit contracts. Reconstruction bias and fine-volume
precision remain independent reference gates. No automatic allocator, new global
sampling default, native UI/install or release claim is made by this study.


## November 14: independent-pilot budget planner and full-frame qualification

Added `spectral_budget_planner.py` and `spectral_budget_qualification.py` for
fixed-count ordinary V3 diagnostics. A complete independent crossed pilot
separates camera/map/interaction variance and constrains all measured XYZ
channels. Plans freeze before final rendering and reject pilot-seed reuse,
camera/policy/reconstruction drift, incomplete captures and insufficient budgets.
See [the planner contract and commands](spectral_error_diagnostics.md#independent-pilot-budget-planning).

Both measured spectra on the original and equal-area altered-emitter fixture
select 512 camera samples and 262,144 photons from a 128/65,536 baseline. Eight
independent regional seed pairs per scene/spectrum and eight ordinary full-frame
images qualify the combined counts. Observed regional luminance variance ratios
range 0.176–0.297; this is variance reduction, not an absolute
clean-still or convergence claim. Full-frame images use the exact 256 × 192 camera,
full fog, fixed reconstruction/exposure and no denoising. One full image per
condition is visual coverage, not whole-frame variance estimation.

The 0.30 planning target is separate from unchanged mean/variance gates; each
XYZ target miss remains explicit in the evidence. The planner's regional CPU
fit is approximate and does not certify full-frame wall time or an optimal time
allocation. Joint camera/map cost calibration, absolute error targets and
uncertainty estimation remain next steps. The held-out change is emitter aspect
at equal area; arbitrary glass geometry and media remain unqualified by this
study. Normal build, thirteen planner/refusal tests, headless smoke, exact linear
recombination and two full-frame/harness component replays pass. Renderer C,
defaults, progressive semantics, versions and installed apps are unchanged.
