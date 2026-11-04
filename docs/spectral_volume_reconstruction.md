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

Each node initially caches its weighted source toward a reference camera
position. For nonzero `g`, a bound valid for all incoming directions is

```
abs(d log(p_g) / d theta) <= 3*abs(g)/(1-g*g)
allowed_angle = log(1+tolerance)*(1-g*g)/(3*abs(g)).
```

The cache is used only if the actual outgoing direction satisfies this bound,
with a conservative floating-point margin. Otherwise the node evaluates the
original collision directions and precomputed normalized flux weights. Positive
weighted sums inherit the phase bound. Isotropic phase is direction independent.
This bounds angular approximation only, not spatial smoothing, spectral binning,
Monte Carlo noise or camera integration error.

A small per-ray cache reuses exact node evaluations. A changed ray origin or
direction invalidates it. State belongs to a serial render context; simultaneous
workers must not share it. Camera segments split at reconstruction, density and
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
links, connected regions, cached-node queries and exact-node queries. Counts
represent cache evaluations, not unique photons or physical scattering events.

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
A separate camera-ray integral covers optical depths 0.01, 1 and 100. Refusal
checks include rotated finite planes whose AABBs misleadingly cover the field.
The public request checks compare physical ledgers and approximate/exact XYZ,
exercise validation and preserve default box behavior. The native editor test
uses actual Render frame controls and the ordinary saved-scene preparation path.

Continuous/adaptive reconstruction remains opt-in pending broader performance
and geometry qualification. Release, installed desktop and default adoption are
separate from this source implementation.
