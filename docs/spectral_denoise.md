# Offline V3 component-aware denoising

`tools/spectral_denoise.py` compares a whole-beauty HDR OIDN baseline with a
conservative variance-guided component filter. It consumes completed independent
batch accumulations, preserves the raw files, and writes a separate candidate.
It does not change photon reconstruction, sampling, renderer defaults or the
native editor's denoise policy. V3 exports XYZ transport layers, not individual
wavelength images.

## Usage and identity

Install NumPy and Pillow in a disposable Python environment. The qualification
metrics additionally require SciPy. First produce a completed accumulation using
the [independent batch workflow](spectral_batch_accumulation.md). The adapter
requires verified equal-budget batches and an empirical variance film: at least
two for beauty-only OIDN modes, and at least four for component/variance-guided
filters. OIDN does not consume or certify that variance estimate.

```bash
python3 tools/spectral_denoise.py \
  --manifest <accumulation>/manifest.json \
  --output <new-candidate-directory> --mode component \
  --settings <settings.json>

python3 tools/spectral_denoise.py \
  --manifest <accumulation>/manifest.json \
  --output <new-baseline-directory> --mode oidn \
  --oidn-library <oidn-runtime>/lib/libOpenImageDenoise.dylib
```

The tested component settings are:

```json
{
  "name": "conservative",
  "radius": 2,
  "sigma": 1.25,
  "range_sigma": 1.0,
  "volume_blend": 0.65,
  "caustic_blend": 0.25
}
```

This is a qualified setting for two small fixtures, not a universal production
preset. Inputs are bounded to 1,048,576 pixels. Each run checks completed native
jobs, raw/resource hashes, compatible scene/camera/reconstruction, unique camera
and map seeds, native display reproduction, canonical accounting, and independent
strip-wise recomputation of the accumulation mean and variance. Existing output
directories refuse. A completion manifest is written last; an incomplete folder
is diagnostic evidence, not a resumable render checkpoint.

Outputs include `beauty_xyz.npy`, signed `difference_xyz.npy`, a matched
`preview.png`, and a manifest binding raw identity, backend/settings, source
hashes, output hashes, elapsed time and process peak resident memory. Component
mode also writes all eleven candidate layers to `components.npy`. Whole-beauty
OIDN has no component decomposition; its result must never replace the raw
component ledger. Raw variance remains raw; neither mode exports a denoised
variance estimate or convergence certificate.

The OIDN receipt records the observed CPU device and four-thread limit and
hashes the supplied library and its adjacent versioned dependencies. On Linux,
it also records the file-backed shared objects observed during filtering,
including the dynamically loaded CPU module and threading runtime. Use the
Linux `libOpenImageDenoise.so` entry point from the supplied distribution.
This fingerprint records which bytes were used on the executing host.

## Filter and color contracts

The component filter preserves `direct` and `surface_indirect` exactly. The latter
includes dielectric continuation and cannot be treated as a diffuse-only layer.
It filters volume and the two surface-caustic families with normalized spatial
weights informed by both family and beauty XYZ differences and their measured
batch variances. Every family uses the same weights for all three channels.
Volume children share one set of weights and one uncertainty blend; volume is
reconstructed from those children, then total uses the existing canonical order:

```text
total = ((direct + surface_indirect) + volume) + (caustic + caustic_bounce)
```

Sparse high-order children are never independently denoised. No radiance clipping,
firefly removal or brightness-selected batch weighting is performed. The image
filter is intentionally biased; preserving the input estimator does not make the
filtered result unbiased. Four-batch variance is a noisy guide, not a confidence
bound or evidence that unseen rare paths are covered.

The tested baseline is OIDN 2.5.1 `RT`, high quality, CPU with four threads, HDR,
explicit `inputScale` equal to the scene exposure, no auxiliary guides and
`cleanAux=false`. The adapter converts unexposed XYZ to linear sRGB with the
renderer matrix. Because OIDN expects nonnegative RGB, it filters the positive
part and restores the exact signed residual before converting back with the
matrix inverse. Input float32 roundoff and negative-component incidence are
recorded. Raw XYZ is never overwritten. Exposure and the native display transform
are applied once for previews. The signed residual policy retains unsupported
gamut energy but can retain its noise; highly saturated scenes need separate
qualification. See the [OIDN input and guide requirements](https://www.openimagedenoise.org/documentation.html).

## November 14 bounded qualification

The exact 256×192 glass/full-fog fixtures retained reconstruction, camera and
exposure 0.019. Batches 0–3 supplied 512 camera samples and four independent photon
maps; batches 4–7 supplied a disjoint full-image reference. Six 8×8 regions also
used sixteen independent 512-sample/16-pass reference captures. No reference
contained input samples. The references remain finite and noisy.

Preservation tolerances and three settings were frozen before tungsten selection.
The selected conservative setting was then tested once on LED without retuning.

| Fixture / method | Regional XYZ RMSE reduction | Full-image XYZ RMSE reduction | All frozen gates |
| --- | ---: | ---: | --- |
| Tungsten / whole HDR | 29.2% | 26.6% | Fail |
| Tungsten / component | 11.7% | 11.5% | Pass |
| LED / whole HDR | 24.2% | 26.3% | Fail |
| LED / component | 10.5% | 11.7% | Pass |

The component setting preserves glint peak within 3.0%, glint contrast within
3.8%, the floor-focus centroid within 0.014 pixels, focus width within 1.1%, and
regional XYZ means within 0.5% of raw across these fixtures. It still visibly
retains grain. OIDN is much smoother and reduces more reference error, but exceeds
the frozen beam-window/edge and regional brightness drift limits; LED glint
peak/contrast changes also exceed 5%. The brighter raw LED peak differs from the
independent reference, so this is a conservative preservation failure, not proof
that the OIDN peak is less accurate. Beam-window moments include glass-rim and
background contrast; they are image-space screens, not isolated physical beam
width. Reference-only paired-error bootstrap intervals favor both methods, but
do not cover new geometry, input-seed variation or rare unseen paths.

Per-candidate process measurements on the local Apple Silicon CPU were
0.11–0.15 seconds for the OIDN adapter, 1.15–1.47 seconds for component filtering
including component serialization, and 3.3–5.2 seconds end to end including
input verification and preview output. Peak process RSS was about 145–146 MiB
for OIDN and 175–189 MiB for component mode. These are single-run small-image
measurements, not portable performance claims.

The integration recommendation is to retain this as an explicit offline still
option. Before ordinary-job/default integration, qualify a higher-resolution
fixture and new geometry with independent references, include saturated colors,
and establish whether stronger cleanup needs footprint-matched reflected/
refracted glass and fog guides. Existing V2 opaque first-hit guides do not qualify
that extension. Animation, UV/fluorescence, default promotion, packages and
installed apps are outside this milestone.

```bash
python3 tests/test_spectral_denoise.py
python3 tests/test_spectral_batch_accumulate.py
python3 tests/test_spectral_budget_planner.py
python3 tests/test_spectral_budget_precision.py
```

`tools/spectral_denoise_qualification.py` accepts explicit retained batch and
regional-reference roots, a frozen contract, and separate `development` and
`holdout` stages. It refuses overlapping seeds and existing reference outputs;
LED requires the sealed tungsten selection and contract hash. Use fresh output
roots for another experiment; never silently replace this evidence.

## Explicit postprocess for an ordinary completed still

`tools/spectral_denoise_still.py` accepts a completed single-frame ordinary V3
headless summary and its native image. It verifies the native XYZ/component/display
contract before producing a separate HDR result. Four independent batches are not
required for this workflow; a single film supplies no variance estimate.

```bash
python3 tools/spectral_denoise_still.py \
  --summary <job>/summary.json \
  --image <job>/render/frames/frame_0000.bmp \
  --output <new-oidn-result> \
  --oidn-library <oidn-runtime>/lib/libOpenImageDenoise.dylib
```

Raw native files are never replaced. The output contains float64 beauty and signed
difference arrays, a native-display PNG, and a completion manifest binding inputs,
backend, source files and output hashes. Incomplete/multi-frame/non-V3 summaries,
unqualified transport diagnostics, input drift, invalid output and existing
output folders refuse. A backend failure leaves no completion manifest. The
bounded limit remains 1,048,576 pixels. This command is an explicit postprocess;
it does not change native renderer defaults or install an OIDN runtime.

### Signed RGB qualification and optional alternative

The original positive-RGB-plus-raw-signed-residual policy can retain isolated
colored flecks, even when the fraction of negative components is small. An
analytic highly saturated control with known positive XYZ and roughly 45%
negative linear-sRGB components failed: error changed between a 3.4% improvement
and a 3.8% regression, with up to 6.1% patch-mean drift. This is a synthetic color
stress test, not a rendered-scene quality claim.

The ordinary-still command refuses the original policy above 1% negative RGB
components. This is a coarse admission screen, **not** a guarantee of quality
below that fraction. Retained scene qualification and visual inspection remain
necessary. The lower-level comparison adapter retains the original policy for
explicit experiments and reproducing earlier evidence.

An explicit experimental alternative filters the positive RGB part and the
magnitude of the negative RGB part separately with the same OIDN settings, then
subtracts the two results in XYZ. It clips neither final XYZ nor raw values. These
are mathematical color parts, not physical transport components; independent
nonlinear filtering can introduce bias. Four frozen analytic noisy-color seeds
passed the original 5% mean tolerance and improved XYZ RMSE by 9.7–16.5%. Native
rendered holdout acceptance is recorded separately from these analytic controls.

```bash
# Explicit single-film experimental signed-color policy:
python3 tools/spectral_denoise_still.py \
  --summary <job>/summary.json \
  --image <job>/render/frames/frame_0000.bmp \
  --output <new-signed-oidn-result> \
  --oidn-library <oidn-runtime>/lib/libOpenImageDenoise.dylib \
  --signed-policy filter

# The same optional policy on a verified batch accumulation:
python3 tools/spectral_denoise.py \
  --manifest <accumulation>/manifest.json \
  --output <new-signed-oidn-result> --mode oidn_signed \
  --oidn-library <oidn-runtime>/lib/libOpenImageDenoise.dylib

python3 tests/test_spectral_denoise_still.py
```

The original `oidn` mode and `--signed-policy preserve` remain available. No
animation, temporal consistency, arbitrary-spectrum or production-wide color
accuracy claim follows from the bounded still tests.

### Larger rendered follow-up result (November 14)

The completed 384×288 unchanged-tungsten and changed-geometry/saturated-LED
campaign uses four independent 64-sample input batches and four disjoint
128-sample reference batches per scene. Every batch retains the same 16-pass
photon budget. References remain finite and noisy.

Original and signed HDR both pass the frozen full-image, 4×3 grid, inherited
5% maximum regional regression, mean and color gates. Signed HDR reduces full
XYZ RMSE by 39.42% for tungsten and 39.67% for LED versus the independent
references. No negative XYZ components occur in either filtered fixture.

**Feature preservation does not pass.** Glint peak/contrast and glass-edge
screens fail for both scenes; the mixed glass/fog width screen also fails for
tungsten. These fixed image windows and finite-reference bootstrap measurements
do not isolate physical beam width or establish unbiased feature energy. They
remain failed screens, not criteria to relax after seeing the images. The
accepted smooth appearance supports an explicit experimental visual derivative;
it does not justify a feature-preserving or automatic native default.

The earlier 1,800 s reference timeout was reproduced under a low-priority
launcher. The exact same renderer/request completed in 505 s under normal
scheduling, with byte-identical output over the compared 3.9 MB prefix. Remaining
references completed in 609–706 s. Sample budgets and time limits stayed fixed.
This is a host execution diagnosis, not a universal throughput benchmark.

### One-command experimental local still workflow

`tools/spectral_render_denoise.py` composes the existing local native CLI and
verified postprocess. Run it from the same working directory as the normal
native CLI so startup configuration resolution stays unchanged. Its v2 request
must select one serial V3 frame with explicit dimensions and a **fresh**
`output.root`; overwrite, video/external progress outputs and unsupported native
routes are refused. Native preflight must expose a prepared ordinary total film
with linear component export. The supplied external OIDN 2 runtime is checked
before native rendering starts.

```bash
python3 tools/spectral_render_denoise.py \
  --request <ordinary-v3-request.json> \
  --cli <headless-render-cli> \
  --oidn-library <oidn-runtime>/lib/libOpenImageDenoise.dylib \
  --signed-policy filter \
  --timeout-seconds 1800
```

The request is not rewritten. Under its output root, native raw output stays in
`frames/`; the verified derivative is in `denoised/`. `denoise_job_state.json`
records completion only after both stages pass. Request, scene and renderer
hashes are checked for drift; the native summary remains the native render's
record, and the derivative has its own hashes/backend identity. Referenced
assets are not copied into a new scene snapshot by this command.

A failed render never starts filtering. A filter failure preserves completed
raw output; recover by inspecting it and using `spectral_denoise_still.py` with
a fresh derivative destination. Existing job roots refuse rather than silently
resuming or overwriting. Native subprocess timeouts terminate and reap only the
owned process group. This POSIX local command does not submit a detached or
remote V3 job, install dependencies, alter native denoise flags, or promote a
default. `--signed-policy preserve` retains the original color policy; `filter`
remains explicit and experimental.

Verification comprises 90 passing tests: 17 denoise, 8 ordinary-still/signed
policy, 9 combined-workflow, 29 batch, 13 planner and 14 precision. An actual
96×72 native render/denoise command, raw-hash verification and existing-output
refusal pass. That small smoke proves workflow behavior, not image quality;
the larger independent-reference campaign supplies the quality evidence and
its feature-preservation limits. Next numerical work should test a frozen
feature-preserving/guided alternative against new independent evidence before
native/default integration.

## November 14 — variance-guided HDR detail comparison

`tools/spectral_denoise_guided.py` is a separate experimental postprocess for
completed independent accumulations. It uses four or more equal-budget raw
batches to guide detail restoration after the existing signed HDR OIDN path.
These are image-statistical guides, **not normal or albedo buffers supplied to
OIDN**. NumPy, Pillow, SciPy and the explicit external OIDN 2 runtime are required.

```bash
python3 tools/spectral_denoise_guided.py \
  --manifest <accumulation>/manifest.json \
  --output <new-guided-directory> \
  --oidn-library <oidn-runtime>/lib/libOpenImageDenoise.dylib
```

The fixed v1 policy smooths each raw batch at sigma 0.8 pixels before estimating
guide variance, retaining observed spatial correlations. A shared XYZ weight
uses raw/OIDN guide discrepancy, four times that empirical variance and a 0.5%
relative floor, restoring at most 90% of the raw-minus-OIDN residual. It does not
consume references or generate component layers. This heuristic can restore
noise as well as detail; it does not estimate OIDN covariance or certify unbiased
radiance, filtered variance, rare-event coverage or feature preservation.

Outputs are `beauty_xyz.npy`, `baseline_xyz.npy`, `restore_weight.npy`, signed
`difference_xyz.npy`, a matched preview and a completion manifest written last.
The adapter validates the input accumulation, independent seeds, resources and
raw hashes before and after processing. Existing outputs refuse. This requires
an independent-batch accumulation; it is not added to the single ordinary-still
command or the native editor/default policy.

The candidate and thresholds were frozen before eight new 384×288 reference
renders: four independent 128-sample batches per scene, with camera and photon
seeds disjoint from the previous campaign. Both methods use the same existing
256-sample input. The HDR baseline is bit-identical to the previous signed HDR
result. New seeds on these known scenes are not unseen-scene generalization.

| Scene | Current HDR XYZ RMSE reduction vs raw | Guided reduction vs raw | Guided error vs current HDR |
|---|---:|---:|---:|
| Tungsten | 39.30% | 36.48% | 4.65% higher |
| Changed geometry / saturated LED | 39.90% | 36.97% | 4.87% higher |

All frozen aggregate, grid, mean and color gates pass for both methods. Guided
restoration reduces glass-edge feature error by 21.3% (tungsten) and 18.1% (LED),
but **both still fail the glass-edge screen**. It passes the tungsten mixed
glass/fog width screen that current HDR fails. Glint recovery is small; both
methods pass the glint screens under this new finite reference realization,
which does not erase the failures against earlier references. Caustic screens
pass. The guide restores about 18% of the raw residual on average and introduces
visible grain. Edge-mask pixel RMSE is essentially unchanged, so improved edge
feature measurements should not be described as general edge-accuracy proof.

Retain current HDR as the smoother presentation option. The guided result is an
experimental detail/noise tradeoff, not a replacement or default promotion.
Ninety-nine focused/inherited tests pass. Both original scene input replays
match full component films and BMPs exactly after a temporary-runtime recovery;
all eight fresh references and source/seed/raw custody checks pass. The rebuilt
renderer has a different executable identity, so cross-build replay parity is
bounded to these fixtures. Native C, reconstruction, installed apps, versions,
packages and release state are unchanged.

## November 14 — accepted HDR choice and two-batch budget

The current signed HDR OIDN is the selected presentation option after the guided
comparison. The guided result remains experimental and unselected. The HDR filter,
color adapter, exposure and display transform are unchanged.

A retrospective 384×288 budget study reuses the verified independent raw inputs
and existing 512-sample references on the two known scenes. Four single batches,
two disjoint pairs and one four-batch accumulation compare 64, 128 and 256 total
camera samples respectively. Each batch also owns an independent photon map at
the unchanged per-map budget, so this is a joint camera/map-work comparison.

| Budget | Full XYZ RMSE increase versus accepted 256 HDR | Worst grid increase | Budget-screen result |
|---|---:|---:|---|
| 64, one batch | 3.05–3.79% | 11.44% | 4 of 8 replicates fail the 10% local cap |
| 128, two 64-sample batches | 1.23–1.58% | 5.66% | All 4 replicates pass |
| 256, four 64-sample batches | reference setting | reference setting | Retained accepted setting |

The 128 recipe is a balanced candidate for routine work on these scenes. It
halves camera samples and total independent-map work relative to four batches.
Recorded CPU sums halve on average; fresh wall-clock speedup was not measured.
64 is suitable for quick previews; 256 remains available for detail-sensitive
review. A single 128-sample render with one photon map was not tested and should
not be substituted for the two-batch recipe when citing these results.

Use the existing `spectral_batch_campaign.py` with a frozen 64-sample-per-batch
plan and two distinct camera/map seed pairs, then:

```bash
python3 tools/spectral_denoise.py \
  --manifest <two-batch-campaign>/accumulated/manifest.json \
  --output <new-hdr-directory> --mode oidn_signed \
  --oidn-library <oidn-runtime>/lib/libOpenImageDenoise.dylib
```

Beauty-only `oidn` and `oidn_signed` now admit two or more verified equal-budget
batches. The four-batch default remains for component and variance-guided callers.
All seed/resource/hash, compatible-input, mean/variance recomputation and
no-overwrite checks remain. No filtered variance or two-batch confidence claim
is added. Four actual two-batch CLI results exactly match the study and preserve
raw input; overwrite refusal passes. The accepted four-batch HDR is also checked
for exact pixel parity. All 103 focused/inherited tests pass.

Budget acceptance means at most 10% additional full/per-grid error, fixed mean
allowances, u′v′ drift no greater than 0.005, and no material additional feature
error relative to the accepted HDR result. It is not absolute convergence or
feature-preservation acceptance: the earlier glass-edge failures remain. These
are known scenes and previously analyzed, finite noisy references, not a fresh
held-out generalization study. No native/default, installed-app, VERSION or
release change is made.
