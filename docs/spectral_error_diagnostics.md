# Bounded V3 error-budget and mixed-path qualification

These developer tools separate repeatability at fixed reconstruction from error
against an independent transport reference. They do not enable a new renderer,
change quality defaults, certify arbitrary scenes, or replace native convergence
checks. The [linear component contract](spectral_linear_components.md) defines
the exported layers and prevents double counting.

## Crossed seed experiment

`tools/spectral_error_budget.py` accepts two retained matched fixture directories,
`tungsten-64-s19/` and `led-64-s19/`, each containing `scene.json`, `request.json`,
`assets/`, `config/`, and the corresponding full-frame component film under
`render/frames/frame_0000.bmp.components.npy`. These baseline films use 64 camera
samples, 16 photon passes, and seed 19. Their exact 256x192 camera and optics are
part of the input contract; this tool is deliberately fixture-specific.

The crossed experiment uses four independent seeds on each axis, 64 camera
samples and 4 x 16,384 photon paths. Its extensions use 256 camera samples with
map seed 19, or 16 photon passes with camera seed 19. There are 48 distinct cases.
Every ordinary case uses the same reconstruction, fog, glass, exposure and
camera. Partial identities stop a rerun for inspection; they are not overwritten.

For bounded work, `tests/spectral_region_audit.c` relinks a **test-only** wrapper
around the existing production region API. It keeps the original full-sensor
projector, pixel coordinates and RNG. Six central 8x8 patches are packed into an
8x48 component file. The 256x192 BMP is only a sparse diagnostic canvas; it is
**not a completed full-frame render**. The wrapper is not used by ordinary
application jobs. Exact component-by-component parity against both retained
full films is a prerequisite for interpreting these crops.

Build the ordinary headless CLI into a fresh task-owned build directory, retain
its trusted build log and objects, then build the wrapper:

```sh
python3 tools/build_spectral_region_audit.py \
  --build-log <task-build.log> --output <new-audit-build-dir>
python3 tools/spectral_error_budget.py \
  --cli <new-audit-build-dir>/ray_tracing_region_audit \
  --inputs <matched-fixture-root> --output <new-results-dir> --roi-audit
python3 tools/spectral_error_analysis.py \
  --matrix <new-results-dir> --output <analysis.json>
```

Analysis needs NumPy. No denoising is applied. It reports raw XYZ and luminance
variance, the full covariance matrix of disjoint components, signed covariance
attributions, camera/map interaction, and paired leave-one-seed-out ratio
sensitivity. Negative method-of-moments estimates remain visible. Four seeds do
not establish reliable asymptotic confidence bounds. Region-average stability
can coexist with noisy pixels. The largest 1% squared-deviation share measures
**rendered-pixel replicate deviations**, not individual path-weight tails.

The matrix permits two concurrent independent cases, a 300-second per-case cap
and no new pair after 3,300 seconds. Wall timings on a busy host are not renderer
performance benchmarks. A stopped budget is incomplete evidence, never a pass.

## Independent mixed-path controls

`tools/spectral_mixed_reference.py` implements independent area/line quadrature
using NumPy, plus ordinary V3 fixtures. It does not call renderer transport or
map reconstruction. The shared CIE observer table is used only to project or
compare spectral results. The fixtures cover:

- colored diffuse surface -> homogeneous fog -> camera: paired colored/black
  subtraction isolates one scattering order; 16 independent seeds per wavelength;
- smooth glass -> diffuse surface -> receiver: four independent seeds at each
  wavelength, isolating the mapped surface-bounce layer;
- smooth glass -> homogeneous fog -> camera: four independent seeds per
  wavelength, isolating the mapped volume layer.

Controls use 450, 550 and 650 nm, wavelength-dependent reflectance and absorption,
unchanged wavelength in recorded elastic paths, and doubled source power. The
references use Lambertian factors, geometric cosines, inverse-square area
conversion, and Beer transmittance. These follow the surface/volume transport
integrals; see [PBRT's equation of transfer](https://pbr-book.org/4ed/Light_Transport_II_Volume_Rendering/The_Equation_of_Transfer).

The glass controls use a closed slab with index 1.0001 to approach transparent
transport while still exercising the glass path classification. They are not
independent validation of a strongly dispersive orb or prism. The volume
reference is first scattering in an optically thin medium; the exported mapped
volume can include further camera scattering. Reference/model discrepancy must
therefore be reported separately from statistical uncertainty.

A separate sensitivity experiment halves/doubles surface radius, refines volume
cell size, and changes continuum bands 8/32/128 under a measured LED spectrum.
These changes belong only to the small reference fixtures. They are never mixed
into the fixed-reconstruction crossed experiment. Fixed-radius repeatability
alone cannot establish physical convergence: photon reconstruction introduces
bias, as described in [PBRT's photon-mapping derivation](https://pbr-book.org/3ed-2018/Light_Transport_III_Bidirectional_Methods/Stochastic_Progressive_Photon_Mapping).

```sh
python3 tests/test_spectral_error_diagnostics.py
python3 tools/spectral_mixed_qualification.py \
  --cli <ordinary-headless-cli> --output <new-physical-results-dir> \
  --sources <matched-light-manifest.json> --wall-budget 1800
```

The source manifest contains `sources.led.continuum`, the same measured curve
used for the matched scene. The declared gates are 10% relative mean error,
3% relative standard error, 1% quadrature refinement, and 1e-9 source-scaling
relative tolerance. Mean +/- 3 standard errors is descriptive; it is not a
formal confidence interval with four seeds. Failed mean/precision gates are
recorded rather than weakened. Reuse checks saved inputs and binary identity.
The tests retain their own native scenes, meshes, summaries and linear films.


## Camera sampling and sample attribution

The ordinary saved `surface_path_v1` profile accepts an observational audit:

```json
{
  "camera_sampler": "hashed_v1",
  "sample_audit": {"x": 127, "y": 125, "limit": 8},
  "path_trace": {"x": 127, "y": 125, "sample": 50}
}
```

Omit `sample_audit` to disable it. Coordinates must be inside the effective
sensor; `limit` is an integer from 1 to 16, default 8. The summary retains the
largest total-Y samples in descending order, preserving earlier samples on ties.
It reports unexposed XYZ **before dividing by the camera sample count**, spectral
weight and wavelength, subpixel location, component Y, source-hit and volume-event
counts. Its mean includes every observed sample, not only the retained tail.
Mapped volume and scattering orders are subsets: do not sum them on top of
`volume`. The audit consumes no random draws. Reuse a recorded sample index in
`path_trace` to inspect its events; check the existing trace truncation flag.
The two controls are independent and may select different pixels.

The default `camera_sampler: "hashed_v1"` preserves existing images exactly.
`"scrambled_path_v1"` is an **explicit experimental control, not a qualified
quality preset**. It uses the existing native Owen-scrambled 2D primary sampler,
distinct prime-base prefix-scrambled transport dimensions for the first four
events (16 draws each), and independent hashed fallback beyond that allocation.
It does not change wavelength proposals, transport PDFs/weights, photon maps,
reconstruction or exposure. The rejected earlier repeated-pair sampler name
`"sobol_pairs_v1"` is not accepted. Unknown strings, booleans and malformed audit
fields fail strict parsing. Summary quality identifies both pixel and camera
samplers. Per-pixel IID standard error remains a heuristic; use independent
seed replicas for uncertainty with these stratified samples.

The bounded qualification found lower floor-focus variance but failed glint,
held-out beam and fine-volume precision gates. Therefore it did **not** change
ordinary V3 defaults. Sampling coverage alone does not establish lower variance,
accurate means, correct physical transport, or color convergence.

```sh
make BUILD_DIR_BASE=<task-build-root> test-spectral-path-sampling
python3 tests/integration/test_spectral_camera_controls.py   --cli <ordinary-headless-cli> --output <new-controls-dir>
python3 tools/spectral_camera_qualification.py   --cli <exact-sensor-region-cli> --inputs <matched-fixture-root>   --output <new-camera-matrix> --phase candidate
python3 tools/spectral_camera_qualification.py   --cli <exact-sensor-region-cli> --inputs <matched-fixture-root>   --output <same-camera-matrix> --phase heldout
python3 tools/spectral_mixed_qualification.py   --cli <ordinary-headless-cli> --output <new-physical-results-dir>   --sources <matched-light-manifest.json> --camera-sampler scrambled_path_v1   --wall-budget 1200
python3 tools/spectral_camera_analysis.py   --baseline <prior-crossed-matrix> --matrix <same-camera-matrix>   --physical <new-physical-results-dir> --output <camera-analysis.json>
```

The corrected candidate phase has 10 cases: both sources at 64 samples/seed19,
then 256 samples/four original seeds. Held-out comparison has eight cases:
both modes/sources at 256 samples/two additional seeds. Audit phase adds three
legacy tail captures when requested. Each phase permits two concurrent jobs;
per-case cap is 300 seconds. Use a task-wide launch/time budget across phases
and count terminated attempts. A `STOP_BEFORE_NEXT_PHASE` file refuses a new
phase. Partial identities require inspection; they are never overwritten.
CPU time and path work are reported separately from contended wall time.

Acceptance preserves a median variance ratio <= 0.85, beam ratio <= 1.0,
other-region ratios <= 1.1, paired mean shift <= max(5% baseline mean,
3 paired standard errors), and the independent physical gates. Original and
held-out failures remain visible; combining seeds cannot erase them. The 1 cm
mapped-volume precision control is mandatory and still fails its 3% limit.
`qualification-complete.json` means scheduled work finished, not that its
physical acceptance gates passed. Raw XYZ and leave-one-out sensitivity remain
available beside luminance results; small seed counts are not confidence bounds.


## Bounded transport estimators and independent photon precision

The [fog/glass estimator contract](spectral_estimator.md) separates ballistic
visibility and scattering, and reduces internal-glass source-hit variance.
`spectral_estimator_qualification.py` holds maps/reconstruction fixed for its
camera matrix, then isolates photon seed/population at fixed camera samples and
1cm volume support. `spectral_estimator_analysis.py` retains original/held-out
variance and paired-mean gates. Combined floor/glint improvement does not erase
the held-out tungsten beam failure. The earlier four-seed fine-volume failure
is retained; the separate 16-map high-population ensemble passes mean precision,
with substantial single-map variation still present. Neither estimator nor the
experimental camera sampler is promoted to the default quality policy.


The subsequent [camera-exit allocation qualification](spectral_estimator.md#november-13-camera-exit-allocation-and-equal-time-qualification)
uses eight discovery and eight untouched camera seeds for each measured source,
with sequential alternating equal-time pairs. `tools/spectral_beam_qualification.py`
retains the fixed scene/map invariants and the original regional mean/variance
gates. Strong floor/glint improvements coexist with tungsten beam and LED
floor-edge failures. Do not interpret a lower branch count or pooled mean as
permission to promote defaults. Repeated ordinary fog scattering, including
roulette-amplified source-hit paths, remains the next estimator target.


The [fog-survival roulette qualification](spectral_estimator.md#november-13-continuation-roulette-after-fog-scattering)
freezes camera budgets using timing only before new held-out images are reviewed.
It passes the fresh beam comparison but retains colored-patch and prior tungsten
beam failures against baseline. Signed component covariance and exact replay
identify ordinary orders 2–4 as the next target. A uniform-radiance conservative
fog control independently checks repeated-scattering energy. Default-off image
parity and audit-on parity remain mandatory; lower variance alone is not proof
of physically correct transport or universal default readiness.

## Repeated-scatter light/direction proposal qualification

Use `tools/spectral_volume_sampling_qualification.py` with the existing exact-sensor
region harness and measured-source inputs. The `phase` default and experimental
`light_mis` saved policy are described in [bounded estimators](spectral_estimator.md#november-13-repeated-scatter-angular-light-and-direction-mis).
The analyzer requires a uniform sample count within each seed ensemble and keeps
beam 1.0, other-region 1.1, median 0.85 and 10% CPU gates. All paired mean checks
use the existing 5%-or-3SE criterion. Scene/map/film identities are verified.
Near-area-source tails require the actual light and continuation PDFs in solid
angle; area-density values must not be compared directly with phase densities.
Cap directions missing the rectangle are null samples, not resampling requests.
Independent analytic support/energy tests and ordinary per-order/source-scaling
controls complement seed convergence. They do not make remaining variance failures
or finite-depth/reconstruction bias disappear. The current policy is not promoted.

## Angular light-only ablation

`tools/spectral_light_ablation.py` compares phase, coupled and light-only with
serial rotated arm order. `calibrate` freezes sample counts using only timing;
`heldout` and `confirmation` use independent predeclared seeds and identical
budgets. Each seed is paired across techniques. Exact camera/fog/map/film state
and the existing mean/variance/CPU thresholds are checked. Do not pool away a
failed group or treat better selected pixels as general convergence. See
[the implementation and result](spectral_estimator.md#november-14-angular-light-only-versus-coupled-continuation).
The equal-sample isolation check separates unchanged photon/first-scatter films
from changed repeated-scatter lighting estimates. It is not a new reconstruction.

## Rectangle proposal diagnostics

The ordinary `volume_light_diagnostics` summary separates angular proposal
misses from occlusion and non-emitting-side invalid samples, stratified by
scattering order and normalized plane distance. Contributions are pre-spectral
scalar values; use raw linear XYZ replica statistics for image variance. See
[the ledger and sampler contract](spectral_estimator.md#november-14-rectangle-solid-angle-proposal-and-light-diagnostics).
`tools/spectral_rectangle_qualification.py` holds the 75/25 continuation mixture
fixed while comparing rectangle, cap and phase policies at equal CPU time.
Independent seed groups remain separate; do not pool away a failing gate.

## Non-scattered indirect classes and sampling balance

The ordinary `nonscattered_indirect` summary partitions `surface_indirect` into
five disjoint linear-XYZ classes: `opaque_no_glass`, `glass_reflection_only`,
`glass_transmission_only`, `glass_reflection_and_transmission`, and
`opaque_and_glass`. It reports film sums and configured pixel probes with
recombination residuals. These are subsets, not additional beauty-film energy.
They contain zero actual fog-scattering events, but fog may attenuate their rays.
Opaque means any non-glass material interaction, including an opaque specular
material. Reflection-and-transmission records either order; the classification
alone does not establish that a reflection happened inside the glass.

The classifier observes path history through the existing bounded branch tree;
it consumes no RNG dimensions and does not alter transport arithmetic. A
surface-light PDF consistency counter checks visible surface NEE after two or
more fog collisions. All surface vertices retain their surface light proposal;
volume proposals belong only to volume vertices. The corrected light sampler
and competing emitter-hit PDF must describe the same distribution.

`tools/spectral_sampling_balance.py` runs an explicit, bounded regional study:
independent camera replicas, crossed camera/map seeds, fourfold camera samples,
fourfold photon counts, and existing camera/glass sampling policies. The exact
camera, full fog, spectral responses and reconstruction stay fixed. Its test-only
harness records per-pixel class XYZ alongside the unchanged 11-component film;
ordinary production film schema remains unchanged. Class capture recombines to
the original surface-indirect component within floating-point tolerance.

The four-by-four crossed grid uses a two-factor random-effects variance estimate
for camera, map and interaction terms. Raw signed estimates are retained;
negative estimates signal limited statistical precision and are not silently
clipped into confident allocations. Photon-only comparisons vary map seeds at a
fixed camera seed; camera-only comparisons do the reverse. Their variance ratios
have different denominators and must not be ranked as interchangeable total-noise
reductions. CPU-times include the regional harness and map setup, so they do not
establish whole-frame rendering times. Defaults require independent scene and
equal-time qualification beyond this diagnostic study.


## Independent-pilot budget planning

`tools/spectral_budget_planner.py` is an offline diagnostic tool for ordinary V3
linear captures. It selects fixed global camera and photon counts before the
final render. It does not change renderer defaults, stop pixels adaptively,
accumulate pilot samples, or change photon reconstruction. NumPy is required.

Provide a JSON pilot manifest with `camera_seeds`, `map_seeds`,
`baseline_camera_samples`, `baseline_photon_passes`, `records` (paths to verified
regional `result.json` captures), `camera_multipliers`, `photon_multipliers`, and
`target_variance_ratio`. At least three unique seeds per axis and their complete
Cartesian product are required; four or more are preferred. Add camera-only
and photon-only increased-budget calibration records. All captures must use the
same scene, request camera, sampler/transport policies, reconstruction and binary.
Only sample counts and the two seeds may differ. Paths are relative to the pilot
manifest, or absolute. Existing packed exact-sensor captures default to the six
fixture patches. For ordinary full-frame result records (`regions: false`), an
optional manifest `regions` object maps arbitrary names to `[x,y,width,height]`.
Patches must have equal pixel counts and lie inside the film. The planner verifies
the full linear export hash before extracting those patches; it never interprets
a packed audit as a full image. Full-frame plans retain this region layout for
subsequent ordinary-job capture. Custom patches require full-frame execution;
the current small harness supports only its fixed fixture patches.

```sh
python3 tools/spectral_budget_planner.py --pilot pilot.json --output plan.json
python3 tools/spectral_budget_qualification.py \
  --cli <build>/tools/cli/ray_tracing_render_headless \
  --plan plan.json --base <saved-fixture> --output <fresh-result-directory> \
  --camera-seed 32003 --map-seed 32117
```

Run `python3 tests/test_spectral_budget_planner.py` for the analytic allocation,
manifest-integrity and final-seed contract checks.

The first command exclusively creates a plan; existing output is not overwritten.
The second uses the normal full-frame renderer. `--budget baseline` renders the
pilot counts with the same fresh final seeds. `--regions` is reserved for the
test-only exact-sensor harness and never represents a full-frame render.
`--timeout-seconds` sets the explicit per-job wall-time bound (default 1800).
Unfinished case directories refuse automatic retry. Completed reuse verifies
plan, binary, request, scene and ROI identities.

The planner estimates camera, photon-map and interaction variance separately for
all six named regions and all XYZ channels. It retains raw signed moment
estimates and clips negative estimates only in the allocation model. The model
predicts `camera/c + map/p + interaction/(c*p)` for count multipliers `c,p`; this
inverse-count assumption must be checked with independent final renders. A
nonnegative setup/camera/photon CPU fit ranks candidate budgets. Its residual
and regional scope are explicit: it is not a full-frame wall-time prediction.
The grid cannot exceed measured axis calibration bounds. Missing/duplicate
cells, artifact or camera drift, nonfinite data, unresolved zero variance and
pilot-seed reuse refuse. If no candidate satisfies every measured XYZ constraint,
`budget_cap_insufficient` is reported without an executable selection.

Freeze the plan before final rendering and use seeds absent from every pilot
and calibration capture. Do not include pilot pixels in the final estimate.
This avoids making the final sample count depend on the same samples used to
form that image; see [PBRT's adaptive-sampling discussion](https://www.pbr-book.org/4ed/Sampling_and_Reconstruction/Further_Reading).
The crossed variance decomposition follows the balanced random-effects layout
([NIST](https://www.itl.nist.gov/div898/handbook/ppc/section2/ppc232.htm)).
Spatial pixels are averaged within each region, not treated as independent
seed replicas. Small seed groups give uncertain variance estimates. A relative
variance target is neither an absolute visible-noise target nor convergence or
physical-accuracy certification; reconstruction bias, unmeasured regions and
perceptual color error require separate checks.


## Joint cost and absolute noise planning

`tools/spectral_budget_precision.py` adds a measured-candidate path alongside the
crossed-pilot ratio model. It requires NumPy and SciPy in the diagnostic Python
environment; neither is a renderer dependency. It consumes a reference plan for
the fixed scene, camera and region layout, plus a complete calibration manifest:

- `budgets`: distinct `[camera_samples, photon_passes]` pairs, at least six cells
  spanning independent camera, photon and combined increases;
- `seed_pairs`: at least 16 pairs, with a unique camera seed and unique photon-map
  seed on both axes, paired consistently across every candidate;
- `records`: paths to all verified `result.json` captures;
- `exposure`: exactly the frozen scene exposure;
- optional `confidence` (default 0.95) and `resamples` (default 8192).

```sh
python3 tools/spectral_budget_precision.py \
  --reference reference-plan.json --calibration calibration.json \
  --target-rms 0.05 --output absolute-plan.json
python3 tests/test_spectral_budget_precision.py
```

The target is RMS sampling noise in **exposure-scaled linear XYZ**, averaged over
pixels within each region, constrained separately in X, Y and Z. It is not a
percentage of local brightness, a display-code threshold, perceptual color error,
or error against a known physical solution. Exposure cannot be lowered to make a
plan pass. Choose and record the target before looking at validation results.

Noise uncertainty uses delete-one-render log-variance influence values and a
simultaneous Gaussian-multiplier band across all candidate/region/channel cells.
Pixels are never counted as independent render replicas. A conservative
Gaussian-reference envelope also forms Bonferroni-adjusted per-pixel chi-square
intervals and averages their variance limits; this handles spatial dependence
without assuming independent pixels. See the [NIST variance interval](https://www.itl.nist.gov/div898/handbook/eda/section3/eda358.htm)
and [SciPy chi-square quantiles](https://docs.scipy.org/doc/scipy/reference/generated/scipy.stats.chi2.html).
The final band contains both intervals. This intentionally conservative envelope
can require more independent seeds before a target is supportable. Its nominal
confidence is conditional on distributional assumptions: unseen rare bright
paths and non-Gaussian tails can invalidate coverage. The output includes the
largest observed seed contribution to each variance estimate. No finite pilot
establishes the absence of an unobserved tail.

Only measured candidates are eligible. A plan reports `measured_bound_target_met`
when every upper RMS bound meets the target; `uncertainty_limited` when some point
estimates pass but no complete bound does; otherwise `budget_cap_insufficient`.
The latter two have no executable selection and the ordinary-job runner refuses
them before creating an output directory. A separate diagnostic cap probe may
measure the best available candidate, but must retain its failing target/status
and must never be presented as a passing clean-still plan. Final seeds must be
absent from all calibration and reference-pilot records. Calibration pixels do
not enter final output.

The cost diagnostic compares nonnegative `setup + camera + photon` and
`setup + camera + photon + camera*photon` models. It holds out each entire budget
cell, chooses the model with smaller held-out relative RMSE, and reports all
prediction errors, measured CPU means and standard errors. The documented cost
check is a maximum 20% relative held-out error; failing it does not invalidate
noise measurements, but prevents an efficiency claim. Regional CPU timings must
not be scaled into full-frame predictions. Full-frame calibration and independent
full-frame timing checks are separate evidence, with their replicate counts
explicit. Selection within a measured grid is not global optimality.

The ordinary renderer's admission guards still apply. The per-source saved
profile limits total photons to 262,144. Camera admission includes film width,
height, depth and maximum branches, with a 1,200,000,000 work-product ceiling.
An inadmissible request is not a failed convergence observation; preserve the
guard and report it. This tool changes no reconstruction, transport estimator,
renderer defaults, denoising, or progressive accumulation semantics.


`tools/spectral_budget_cost.py` makes the separate full-frame timing check
repeatable. Provide at least four identifiable training cells and two holdout
records with camera and map seeds absent from training. All must be verified
ordinary full frames of the reference scene/camera/reconstruction and executable.
The tool checks image/linear-film/ROI hashes, seed/count identity, termination and
PDF diagnostics. It fits only training records and reports additive and joint
predictions for the held-out records. An insufficient grid, reused holdout seed,
regional capture or identity drift refuses. No timing confidence interval is
claimed from this small full-frame design.

```sh
python3 tools/spectral_budget_cost.py --reference reference-plan.json \
  --training corner-a/result.json corner-b/result.json \
             corner-c/result.json corner-d/result.json \
  --validation middle/result.json independent-repeat/result.json \
  --output full-frame-cost.json
```

This separate cost artifact does not silently replace the regional noise plan's
ranking model. Automatic full-frame time-budget selection remains a later
integration boundary; do not use a regional cost as a full-frame prediction.
