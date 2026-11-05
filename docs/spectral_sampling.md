# V3 balanced edge-aware convergence

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
  "visible_error": 2.5,
  "edge_error": 3.5,
  "energy_error": 0.01
}
```

Only `mode` is required; the other values shown are defaults. Minimum and
nonzero maximum counts are multiples of eight, at most 1048576. Minimum is at
least 16; zero maximum means unlimited. Visible and edge thresholds are RMS
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

Eight passes form a checkpoint. They are split into two disjoint groups of four;
each group samples every pixel quadrant once. Both groups use exactly the same
pixel mask. Edge pixels are sampled every pass. Elsewhere a six-pixel sparse
lattice changes phase between checkpoints. The first checkpoint and every
fourth checkpoint after it are full-frame refreshes. All included samples are
actual native camera rays; there is no interpolation of sparse corrections.

Signed linear RGB is accumulated separately for the two groups. At each
checkpoint, the controller averages their means for output, then compares
half their difference after fixed exposure/display conversion. It measures
visible-pixel RMS, pilot-edge RMS and integrated linear luminance difference.
Newly illuminated pixels join the visible metric. Two consecutive passing
checkpoints, with the minimum pass count satisfied, produce `converged`.

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
passing checkpoints, camera samples, pilot edge pixels and measured errors.
Headless center-pixel XYZ/sensor values average samples of that pixel; physical
map ledgers describe the last map, not the sum of all maps. No tile checkpoint
resume or remote-worker support is added by this controller.

## Qualification

`make test-spectral-sampling` checks quadrant balance and deterministic jitter.
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
