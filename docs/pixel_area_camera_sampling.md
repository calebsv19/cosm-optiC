# Pixel-area camera sampling

Native multipass 3D rendering takes one camera sample per pixel per outer
subpass. Each sample covers a position inside the pixel, then the existing
lighting/transmission estimators evaluate that camera ray. Contributions are
averaged in linear radiance before tone mapping, using a pixel-local box filter.
Transmission samples refine transport from that position; they do not add
independent spatial positions.

The default is a deterministically nested-scrambled two-dimensional Sobol
sequence. The first 16 samples cover all 4x4 dyadic cells. Among multipass budgets, its spatial prefix is
independent of the final pass budget (single-pass center compatibility is the exception). Pixel coordinates, a captured frame seed
and subpass index determine the sample, without consuming the transport RNG.
The seed is captured before transport increments or its zero-sequence repair.
Tile origin, worker scheduling and traversal order do not enter the spatial key.

The projector retains its existing coordinate convention: integer pixel
coordinates mean center, and fractional coordinates specify offsets. Shading
loops pass the same sampled coordinates to geometry, finite-emitter visibility,
background, extinction and primary volume-scatter queries. Camera ray
differentials retain a one-pixel displacement about the sampled position.
Picking and center-based feature/denoise guides retain their original behavior.
Single-pass renders also retain center sampling for compatibility.

## Development comparisons

`RAY_TRACING_NATIVE3D_CAMERA_SAMPLER` selects `center`, `stratified` or
`progressive`. Missing or unrecognized values resolve to `progressive`.
Selection is captured at frame preparation, not read per pixel. The C sampling
context can specify a mode explicitly. Headless summaries report the effective
camera sampler. This adds no UI control, MCP command or saved-scene field.

The stratified comparison uses a complete rectangular grid with exactly the
requested number of cells, a deterministic cell permutation and random jitter.
For prime counts that grid is one-dimensional; the progressive default avoids
such budget-specific layout changes. Center mode supports legacy reference
comparisons. No cross-pixel reconstruction filter, lens sampling, shutter-time
sampling, denoiser change or new material response is included.

## Convergence and recovery boundaries

Existing center-hit convergence guides cannot certify subpixel coverage, so
area-sampled render units do not prune temporal samples. Full spatial budgets
are retained until area-aware adaptive stopping is separately qualified.
Diagnostic state may still be measured. Center mode retains the prior policy.

Durable checkpoint sampling identity includes the camera sampler and algorithm
revision. Changing modes refuses incompatible accumulation. Same-mode resumed
subpasses reuse their spatial samples. A stable progressive prefix does not
relax existing checkpoint request/budget compatibility rules or add an operation
to extend a completed render's budget.

Qualification includes sample coverage, seed and budget-prefix independence,
ray differentials, analytic native linear-radiance area accounting, raw region
partition parity, matched worker/tile images, edge and mirror/water-glint
comparisons and a short controlled camera translation. These isolated tests do
not establish full Aquarium, animated-water, denoised or production-package
appearance acceptance. A finite set of samples can still miss a very thin glint;
more camera samples and convergence measurements remain necessary.
