# Independent V3 still batches

`tools/spectral_batch_campaign.py` runs a finite list of independent ordinary
V3 jobs through the existing budget qualification runner, then combines their
float64 linear component films. `tools/spectral_batch_accumulate.py` can combine
already completed jobs without rendering again. NumPy and Pillow are required.
These are offline tools; native editor defaults and installed packages are not
changed. They do not denoise or modify photon reconstruction.

## Run a bounded campaign

Use a frozen executable budget plan from the existing budget-planning workflow,
an ordinary fixture directory containing `request.json`, `assets`, and `config`,
and a compatible headless CLI. An insufficient absolute-target plan is refused.
An explicitly labeled diagnostic cap plan is executable, but completion does
not assert that its absolute noise target passed.

```bash
python3 tools/spectral_batch_campaign.py \
  --cli <headless-cli> --plan <frozen-plan.json> --base <ordinary-fixture> \
  --output <new-campaign-directory> \
  --camera-seeds 51001 51128 51255 51382 \
  --map-seeds 55001 55132 55263 55394 \
  --timeout-seconds 7200 --job-timeout-seconds 1800
```

The campaign runs sequentially, with 2–64 explicitly supplied seed pairs and
at most 24 hours per invocation. Each job remains subject to the renderer's
ordinary admission guards. Both camera and map seed lists must be individually
unique. The frozen plan excludes its pilot seeds through the existing runner.
The wall budget is checked between jobs and limits each child timeout; final
verification and accumulation are additional bounded work. This is not a hard
CPU-time reservation. `--resume` requires exactly the same campaign identity and
budgets. Completed jobs are verified and reused. An unfinished job is refused;
inspect its process, summary, and failure before recovery. Never remove a lock
or resubmit an uncertain render merely because its output appears quiet.

`active.lock` provides one writer per campaign. A hard process crash can leave
that lock; its PID is diagnostic information, not proof that the process still
owns it. Inspect process identity and existing children before recovery.

## Combine completed jobs

```bash
python3 tools/spectral_batch_accumulate.py \
  --records <batch-a/result.json> <batch-b/result.json> \
  --output <new-accumulation-directory>
```

The accepted record is the completed ordinary full-frame output of
`spectral_budget_qualification.py`. Packed regional captures, duplicate records,
reused camera or map seeds, changed executable, scene, camera, reconstruction,
map budget, resource content or display policy are refused. Camera counts may
differ. Images are limited to 1,048,576 pixels and external asset/config hashing
to 1 GiB per job. These explicit offline limits do not alter renderer limits.
Explicit scene resource paths must resolve through the hashed `assets`/`config`
entries. External paths, parent traversal, and nested directory symlinks refuse;
the top-level fixture resource symlinks remain supported.
Resources are bound at accumulation time; a standalone legacy result does not
retroactively prove their bytes at original render time. A new campaign pins
its resources before and after its render jobs.

Every input film is hash-checked and verified for finite values, exact native
component recombination, fog-subset accounting and native display reproduction.
The tool processes eight rows at a time with memory-mapped films. Raw films and
receipts remain unchanged. The output directory is exclusive. `manifest.json`
is written last and is the completion marker; a directory without it is an
incomplete attempt, never an accepted accumulation. Output hashes are recorded.

Outputs:

- `components.npy`: unexposed linear XYZ means, same 11-layer layout as V3.
- `variance_of_mean.npy`: empirical per-component variance of the batch mean,
  only when camera counts are equal; the total layer includes cross-component
  covariance through the measured total, rather than summing layer variances.
- `preview.png`: exposure and display conversion applied once to the final total.
- `manifest.json`: inputs, seed identities, weights, timings, exact output hashes,
  accounting residuals, and bright-batch dominance diagnostics.

## Weight and ownership rules

For batch `i` with `n_i` primary camera samples, every component receives the
same deterministic weight `n_i / sum(n_i)`. Photon maps are already normalized
inside the renderer. Do not multiply their contribution by emitted photon count
again, or choose weights from observed brightness. Per-map budgets are fixed to
avoid mixing different adaptive-map estimators. Averaging independent maps is
not equivalent to constructing one larger photon map.

Accumulate the five top-level contribution families, then use the existing
canonical addition order for the total:

```python
total = ((direct + surface_indirect) + volume) + (caustic + caustic_bounce)
```

The volume-caustic and scattering-order layers are subsets, not additional
energy. They receive the same batch weights, but are not added to the total a
second time. The manifest records the roundoff difference from averaging the
original total directly; exact canonical recombination takes precedence over
bit-identical alternate summation order. No filtering, outlier rejection,
clamping, exposure or display transform touches the linear component means.

## Noise and rare-path interpretation

Whole independent batches are the replication units. Equal-budget batch means
produce an empirical variance-of-the-mean estimate. Unequal budgets still have
valid predetermined mean weights, but this implementation deliberately omits a
variance estimate because map and camera variances do not scale identically.
The tool does not provide confidence bounds or a convergence certificate.

Per-component maximum brightness, the largest batch's share of positive XYZ,
and the fraction of pixels dominated by one batch expose observed sparse
contributions. They are diagnostics, never a reason to remove a bright sample.
Unseen rare events and fixed reconstruction bias remain unbounded. Zero observed
variance does not imply zero true variance. Raw and variance films support a
subsequent denoise experiment without changing this estimator.

The current export is XYZ by transport component, **not** a wavelength-resolved
image cube. A later spectral-aware denoiser should preserve these components and
their color relationships, and should not claim frequency-domain filtering of
information that was not exported.

Run the focused contract tests (the existing precision suite also needs SciPy):

```bash
python3 tests/test_spectral_batch_accumulate.py
python3 tests/test_spectral_budget_planner.py
python3 tests/test_spectral_budget_precision.py
```

For immutable component-aware and whole-HDR postprocessing comparisons, see
[offline V3 denoise qualification](spectral_denoise.md). Preserve disjoint input
and reference batches when measuring filtered results.
