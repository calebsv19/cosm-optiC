# V3 continuous volume reconstruction

The shared V3 spectral renderer supports opt-in continuous reconstruction of
stored volume collisions. The public headless request and the editor's saved
`extensions.ray_tracing.spectral_transport_v1` profile use the same fields inside
`participating_volume_v1`:

```json
{
  "reconstruction": "adaptive",
  "cell_m": 0.0005,
  "phase_tolerance": 0.01
}
```

These fields supplement the existing volume kind, density source, optical curves
and anisotropy. They do not enable a volume by themselves. The existing editor
V3 `surface_volume` deposition choice attaches the profile's volume; `surface`
omits it. This change adds no reconstruction picker to the editor UI.

- Omitted `reconstruction`, or `"box"`: unchanged legacy cell estimator.
- `"continuous"`: fixed-support, piecewise trilinear nodal reconstruction.
- `"adaptive"`: each collision uses the base cell spacing if its cell and
  wavelength band contain at least eight collisions, otherwise twice that spacing.
  A collision contributes to one level; evaluation sums both levels.
- `phase_tolerance`: relative phase-cache tolerance, default `0.01`, admitted
  range `[0,0.05]`. Zero requests exact stored-direction evaluation. Supplying
  this field for `box` is an error.

The same scene geometry, wavelength samples, random stream and physical forward
transport are reused. Reconstruction changes the camera estimator, not the
surface photon map, collision count, absorption ledger or emission power.

## Scattered deposits on surfaces

In V3, headless requests and saved editor spectral profiles accept
`surface_map.gather` as `fixed` (default) or `adaptive_scattered`. Both use the
same forward photon paths. The adaptive mode assigns a radius to each deposit
that reached a receiver after scattering in the medium. It counts deposits on
the same primitive, side and wavelength band within the base radius, including
the deposit itself. With count n, its normalized planar kernel uses
`receiver_radius_m × clamp(sqrt(16/n), 1, 2)`. Direct light and unscattered
caustic deposits use the base radius.

The renderer keeps the base radius near a finite receiver edge, another plane
or another primitive's bounds. Missing object bounds also select the base
radius. These conservative tests restrict enlargement in ambiguous geometry.
The headless summary reports the selected gather and the number of expanded
deposits. In the editor’s Render frame panel, enable surface deposition and cycle
Gather between Scene, Adaptive and Fixed. Scene uses the saved profile. The job
receipt records the override. Map identity includes the mode. The same gather works with the box
and continuous volume estimators.

## Boundary normalization

For collision `i` in occupied connected component `C`, let `w_ij` be its
trilinear deposition weight onto node `j`, and `phi_j(x)` the nodal tent. Define

```
Z_j,C = integral over C of phi_j(x) dV
J_b(x, omega) = sum_i sum_j W_i w_ij phi_j(x) p_g(omega_i dot omega) / Z_j,C
```

Only nodes carrying the query's component identifier and wavelength band are
considered. The deposition weights sum to one. Consequently spatial integration
preserves each collision's phase-weighted source contribution even when its
support is clipped by a boundary. This is reconstruction conservation; it is not
a claim that a fixed-bandwidth photon density estimator is unbiased.

For nonnegative VF3D trilinear density, an interpolation cell's interior is
occupied exactly when its midpoint density is positive. The native density
sampler also applies masks. Connected-component labeling uses those occupied
cells and admitted plane barriers. The integral is a product of analytic clipped
one-dimensional tent integrals for a rectangular component; otherwise it sums
those products over the component's intersecting interpolation cells. No source
is reconstructed in empty cells or a disconnected component across a gap.

Admission is intentionally bounded:

- Untransformed homogeneous boxes and VF3D density fields are supported.
- Axis-aligned interior planes must span the entire field. Their exact coordinates
  split both component labeling and camera quadrature, including off-grid planes.
- Intersecting finite or oblique planes, curved/mesh boundaries, water volumes and
  transformed fields are refused explicitly. Geometry outside the field is reused.
- Masks follow VF3D's interpolated density semantics. They are not an independent
  opaque surface model. Within a single connected component, support can still
  smooth around a narrow cavity; this is not general visibility-aware gathering.

## Angular cache and exact fallback

The Henyey–Greenstein phase function is

```
p_g(mu) = (1-g*g) / (4*pi*(1+g*g-2*g*mu)^(3/2)).
```

Outgoing directions use fixed deterministic direction cells. For nonzero `g`,
a bound valid for all incoming directions is

```
abs(d log(p_g) / d theta) <= 3*abs(g)/(1-g*g)
allowed_angle = log(1+tolerance)*(1-g*g)/(3*abs(g)).
```

The Cartesian direction-cell spacing is `2*sin(min(pi,allowed_angle)/2)/sqrt(3)`.
The rounded direction is normalized and checked against the angular bound with
a conservative floating-point margin before use. On a cache miss, the node sums
the original collision directions and normalized weights at that fixed target.
A matching target key reuses the result. A failed bound check, zero rounded vector,
or spacing at most `1e-6` uses the actual outgoing direction instead. Zero tolerance
also uses the actual direction; isotropic phase is exactly direction independent.
Positive weighted sums inherit the bound. Fixed targets ensure that cache history
and query order cannot change the result. This bounds angular approximation only,
not spatial smoothing, spectral binning, Monte Carlo noise or camera integration.

The 63-bit direction key replaces build-only normalization storage after freeze;
nodes remain 40 bytes. A separate 8192-entry (320 KiB) spatial cache stores the eight
node indices for a cell, wavelength band, level and connected component. It does
not approximate positions or interpolation weights. Up to four entirely rectangular
components use exact half-open bounds tests for domain lookup; all other domains
retain the interpolation-cell label lookup, including nonrectangular masks.

A small per-ray cache reuses exact node evaluations. A changed ray origin or
direction invalidates it. The editor serializes continuous-volume queries within each prepared frame so
concurrent tiles cannot overwrite its per-ray cache. Independent frames can render
in parallel. Camera segments split at reconstruction, density and
admitted geometry knots. Two-point Gauss integration is used below segment
optical depth 0.025, four-point otherwise, with subdivision above 0.25.

## Storage and failure behavior

Legacy collision records retain their existing layout. Continuous modes retain
positions during emission, build sparse node/link arrays and then free the
positions. Links store a collision index and precomputed normalized flux; incoming
directions remain in the collision array. The built map rejects further deposits.

Owned reconstruction plus collision/index storage has a 256 MiB allocation
budget, including transient table growth. Additional caps are 4095 reconstruction
cells per axis, one million density interpolation cells, 65534 components,
64 admitted planes per axis and 64 million normalization-cell visits. Exceeding
a limit or encountering unsupported geometry fails explicitly. These are estimator
allocations, not a whole-process RSS bound.

Headless summaries expose `spectral_reconstruction`: owned/peak bytes, nodes,
links, connected regions, cached-node queries, exact-node queries, and spatial
cache hits/misses. Exact-node queries count original-direction HG sums at either
a bounded target or the actual query direction; they are not exclusively angular
fallbacks. Cached-node queries count reused sums. Spatial misses populate eight
node indices. Counts are estimator work, not physical scattering events.

## Repeatable checks

```sh
make ray-tracing-render-headless test-spectral-reconstruction
make test-headless-spectral-reconstruction
python3 tests/integration/test_headless_spectral_prism.py --cli <headless-cli>
python3 tests/integration/test_headless_spectral_volume.py --cli <headless-cli>
make scene-editor-workspace-visual-test
python3 tests/integration/test_editor_spectral_render.py \
  --binary <build>/tests/scene_editor_workspace_visual_test \
  --output-root <fresh-output> --reconstruction adaptive
```

Native checks integrate fixed/adaptive kernels over off-grid planar partitions,
masked gaps and a nonrectangular occupied domain. Independent analytic tent/HG
oracles test near cameras, positive/negative high anisotropy and exact mode.
Coherent-ray tests require spatial/angular reuse and bit-exact forward/reverse
query-order results, including tiny-tolerance fallback. Rectangular edge probes
compare the shortcut with the original label lookup. A separate camera-ray integral covers optical depths 0.01, 1 and 100. Refusal
checks include rotated finite planes whose AABBs misleadingly cover the field.
The public request checks compare physical ledgers and approximate/exact XYZ,
exercise validation and preserve default box behavior. The native editor test
uses actual Render frame controls and the ordinary saved-scene preparation path.

Continuous/adaptive reconstruction remains opt-in pending broader performance
and geometry qualification. Release, installed desktop and default adoption are
separate from this source implementation.

See [V3 balanced edge-aware convergence](spectral_sampling.md) for ordinary render, saved-scene and headless sampling controls.
