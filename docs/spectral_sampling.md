# V3 sampling and convergence

The [ordinary surface path workflow](spectral_surface_path.md) has its own pinned quality defaults and fixed sample/depth budget, shared by native/editor and local headless rendering. The panel's ordinary **Samples (0=scene)** override controls camera paths; photon **Max passes** still counts independent maps. The [backend contract](spectral_transport_backend.md) separates these owners. The photon-map convergence controller described here does not certify fixed-budget room renders.

V3 photon deposition can render each camera frame until measured convergence.
The ordinary native render scheduler, editor Render frame panel and local
headless spectral renderer use the same controller. Existing scenes with no
sampling configuration retain their single-pass behavior.

## Editor controls

Choose **Disney V3**, then **Surface** or **Surface + Volume** deposition.
The **Sampling** button cycles **Scene**, **Converge**, and **Single**:

- Scene uses the saved optical profile, including its convergence thresholds.
- Converge selects the standard thresholds below and no pass limit.
- Single explicitly selects the legacy single-pass path.

**Max passes (0=auto)** optionally caps convergence at a multiple of eight,
starting at 16. In Scene mode, zero preserves the saved maximum. A nonzero
maximum must be at least the saved minimum. A cap produces a usable image with
`pass_limit` status if convergence has not been reached. The panel and job
receipt distinguish that result from `converged`. Cancel stops the job without
publishing a final image. Render-panel overrides do not edit the saved scene.

The editor command catalog exposes `render_start_spectral_convergence` with
`sampling_override` (0=Scene, 1=Converge, 2=Single) and `max_passes_override`.
These supplement the existing deposition, gather, photon-count and exposure
arguments. Other render commands retain their previous arguments.

## Saved scene and headless settings

Add the following inside `extensions.ray_tracing.spectral_transport_v1` in a
saved scene, or inside `spectral` in a headless photon-map request:

```json
"sampling": {
  "mode": "converge",
  "min_passes": 16,
  "max_passes": 0,
  "camera_coverage": "adaptive",
  "camera_sequence": "balanced",
  "visible_error": 2.5,
  "edge_error": 3.5,
  "local_error": 2.5,
  "energy_error": 0.01
}
```

Only `mode` is required; the other values shown are defaults. Minimum and
nonzero maximum counts are multiples of eight, at most 1048576. Minimum is at
least 16; zero maximum means unlimited. `camera_coverage` accepts `adaptive`
(the default) or `full` (every pixel, every pass). Visible, edge and local thresholds are RMS
errors in 8-bit display code values, in `(0,255]`. Energy error is a relative
integrated linear-luminance difference in `(0,1]`. Explicit `null`, unknown
fields and malformed values are rejected. `{"mode":"single"}` admits no
additional fields. Sampling requires photon deposition, not the line-pencil
calibration or ordinary V3 direct-light preview.

## Sampling and stopping

An independent full-frame pilot selects visible color/luminance edges and
bright pixels. Its edge mask is dilated by three pixels and its contributions
are excluded from the final image. Each subsequent pass traces a fresh photon
map with a deterministic seed and a subpixel camera offset.

`camera_sequence` selects `balanced` (the compatible default, one shared
four-quadrant offset per pass) or `progressive` (the existing native per-pixel
Owen-scrambled Sobol sequence). Progressive sampling gives each pixel and each
of the two comparison groups a distinct scramble. Each group advances its own
sequence; full coverage at 8, 32 and 128 total passes gives each group a complete
2×2, 4×4 and 8×8 subdivision respectively. Sparse masks can skip sequence points,
so this coverage property is for full-camera sampling. The native 16-bit sample
index starts a newly scrambled epoch after 65536 samples per group, avoiding
repeated camera rays in unlimited runs.

Optional `camera_seed` is an unsigned integer from 0 through 4294967295. Omission
uses the original photon-map seed, preserving legacy replay. An explicit seed
controls camera quadrature independently of photon transport. Receipts record
the effective camera seed and sequence. For a noise audit, hold camera seed
fixed while changing photon seeds, then hold photon seed fixed while changing
camera seeds; finally compare runs with both seeds changed. These conditional
comparisons expose different noise sources, not separate physical light terms.

Optional `camera_samples_per_map` accepts 1 (default), 2, 4 or 8. Values above
one require `camera_sequence: "progressive"`. Each pass builds one photon map,
evaluates the requested camera rays against that same map, and contributes their
arithmetic mean with the same weight as every other map. Each comparison group
advances its own native camera sequence by the bundle size. Changing bundle size
changes the camera-to-map pairing; it does not change photon seeds or transport.
The camera subdivision counts above apply to one ray per map; with bundles,
each group advances by `maps_in_group * camera_samples_per_map` samples.

Pass limits, minimum passes, eight-map checkpoints and consecutive acceptance
still count independent photon maps. Repeated rays on a map do not create extra
independent maps. Receipts expose `camera_samples_per_map`, `min_pixel_maps` and
`max_pixel_maps`; `min_pixel_samples`, `max_pixel_samples` and `camera_samples`
count actual included camera rays. The pilot is excluded from both counts.
An interrupted or failed bundle is not committed as a completed map or published
as a final image. The default one-ray path preserves existing output.

Bundles are opt-in sampling controls, not a convergence guarantee or recommended
production default. Compare measured cost and error on complete regions with
multiple photon seeds. Display clipping can hide local linear disagreement.

Eight passes form a checkpoint. They are split into two disjoint groups of four;
each group samples every pixel quadrant once. Both groups use exactly the same
pixel mask. Edge pixels are sampled every pass. Elsewhere a six-pixel sparse
lattice changes phase between checkpoints. The first checkpoint and every
fourth checkpoint after it are full-frame refreshes. A passing check requests
full-frame validation immediately; a passing full-frame check keeps the next
checkpoint full as well. All included samples are
actual native camera rays; there is no interpolation of sparse corrections.

Signed linear RGB is accumulated separately for the two groups. At each
checkpoint, the controller averages their means for output, then compares
half their difference after fixed exposure/display conversion. It measures
visible-pixel RMS, pilot-edge RMS, worst visible-pixel RMS within a 16×16 block,
and integrated linear luminance difference. The local check prevents a noisy
patch from being diluted by quieter regions; it does not blur or reconstruct
pixels. Newly illuminated pixels join the visible metric. Two consecutive
passing **full-camera** checkpoints, with the minimum pass count satisfied,
produce `converged`. A sparse checkpoint may request validation but cannot count
as fresh full-camera acceptance. This corrects the earlier stop rule that could
count reused interior evidence twice. Existing converge requests may therefore
take more work or reach their cap where the old controller reported convergence.

These are empirical agreement checks, not confidence intervals or a proof of
physical accuracy. Display clipping can conceal errors; the linear energy check
is global and cannot detect every local bias. Fixed wavelength bands, photon
kernel support, pilot edge selection and sparsely updated interiors can still
limit accuracy. Full-frame reference renders and multiple seeds remain useful
qualification tools. The controller does not silently broaden photon kernels
or force mixed light to white.

## Lifecycle and cost

Every evaluated camera frame starts fresh: pilot, photon maps, masks, counters
and image sums are frame-local. Moving the camera and returning to the original
frame reproduces the original image for unchanged inputs and seed. This avoids
stale accumulation across a future camera sequence. Long moving-camera sequences
and temporal flicker have not yet been qualified.

The current controller evaluates camera samples serially. Edges can cover most
of a close-up, reducing sparse-sampling savings. It allocates at most 512 MiB for
its accumulation film, in addition to the prepared scene and photon/volume maps.
The ordinary prepared map also remains alive during the render. Cancel is
checked between eight-row camera strips and maps; map construction itself is
not interruptible. Progress updates within camera passes; preview images publish
at eight-pass checkpoints. Unlimited convergence can run indefinitely when a
scene fails the thresholds; use Cancel or an explicit maximum when appropriate.

Receipts include status, committed passes, full passes, checkpoints, consecutive
passing full-camera checkpoints, full-checkpoint count, camera samples, minimum
and maximum accumulated samples per pixel, pilot edge pixels and measured errors.
`validation_pending` identifies a passing sparse check awaiting full-camera
validation. `local_error` is the worst-block display error. `camera_coverage`
records the requested schedule. The pilot is excluded from pixel counts.
Headless center-pixel XYZ/sensor values average samples of that pixel; physical
map ledgers describe the last map, not the sum of all maps. No tile checkpoint
resume or remote-worker support is added by this controller.

## Qualification

`make test-spectral-sampling` checks legacy quadrant balance, deterministic
per-pixel replay, independent groups, progressive subdivision and epoch renewal.
`make test-spectral-convergence` links the actual controller to a deterministic
film producer and checks late interior changes, fresh full-camera acceptance,
localized-noise refusal and full reference coverage.
`test-headless-spectral-sampling` checks single-mode byte parity, deterministic
convergence, a capped nonconverged result, camera reset and invalid settings.
The native editor fixture can be run with:

```sh
python3 tests/integration/test_editor_spectral_sampling.py \
  --binary <build>/tests/scene_editor_workspace_visual_test \
  --output-root <fresh-output-directory>
```

It exercises the actual controls, saved-profile preparation, ordinary scheduler,
same-process camera movement/replay and cancellation. The small fixtures qualify
integration and lifecycle; they are not high-resolution quality benchmarks.

For bounded source comparisons with fixed camera/medium/reconstruction, use the
[spectral source testbench](spectral_source_testbench.md). Its default full-camera
schedule is a reference for the adaptive path; neither schedule changes photon
support, wavelength bins or exposure. A small image with `converged` status is
not proof of high-resolution or moving-camera quality.


## Live headless convergence progress

For `sampling.mode = "converge"`, the ordinary request
`progress.progress_path` receives updates during rendering. An atomic companion
`<progress_path>.spectral.json` uses schema `optic_spectral_progress_v1` and
contains run/frame identity, frame-local elapsed seconds, stage, started passes,
completed/total camera tiles, maximum passes, and the latest sampling statistics.
Stages are `preparing_map`, `sampling`, and `checkpoint`. Map preparation and
checkpoints publish immediately; camera updates are throttled to about once per
second. Flushed `SPECTRAL_PROGRESS <JSON>` lines on stderr retain update history.

Sampling statistics describe committed passes and the most recent convergence
check, not a prediction of remaining work. Zero maximum passes still means
unlimited convergence; no completion percentage or ETA is inferred from it.
The final render summary is authoritative. Checkpoints are observable progress,
not resumable renderer state. A configured progress publication failure fails
the render instead of silently losing observability. Existing editor progress,
preview and cancellation continue through the same convergence callbacks.

Run `tests/integration/test_headless_spectral_camera_cache.py --cli <cli>
--output-root <fresh-output>` to check live readback before completion, exact
checkpoint/final-summary agreement, deterministic replay and publication failure.


### Ordinary V3 linear films and scattering diagnostics

Ordinary V3 final jobs export unexposed float64 XYZ component films by default,
with saved `surface_path_v1.linear_components: false` opt-out. The native editor
and headless path share the export and scattering-order report. See
[the format, exact recombination rule, storage cost, and qualification](spectral_linear_components.md).
