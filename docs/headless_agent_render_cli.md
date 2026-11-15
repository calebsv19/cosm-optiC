# RayTracing Headless Agent Render CLI


## Selecting a qualified renderer

For ordinary opaque spectral indirect lighting, see the [V3 surface path workflow](spectral_surface_path.md) and [backend contract](spectral_transport_backend.md). Native rendering, editor jobs and local headless v2 requests share the versioned quality default, scene policy and diagnostics. Explicit per-object spectra, native RGB adapters, mesh geometry and a homogeneous scattering medium are supported within the documented limits. The prism calibration and glass photon-map estimator remain separate routes.

Disney V3 Spectral is available on the local headless route using a v2 request
with a saved/default ordinary scene profile or explicit calibration spectra. Successful preflight returns
`spectral_execution` with the effective film schedule and capabilities.
The local film uses one serial pass for one static frame. The request may spell
out `temporal_frames: 1`, `frame_count: 1`, `denoise_enabled: false` and
`use_tiled_renderer: false`. Additional temporal passes, tiles, explicit worker
scheduling, denoise and checkpoint/resume require a different route. Detached
submission rejects spectral requests before creating a job. Worker protocol v1
reserves optional capability bit 7 for spectral transport.

The scene snapshot and worker export tools can prepare an ordinary V3 still
with a saved `surface_path_v1` profile and raw linear components. They
preserve the selected camera and use the same pinned dependency manifest as
RGB. V2 carries one serial frame and one temporal pass. Its output object
contains only `root` and `overwrite`, with the summary and progress paths
bound inside the worker attempt. An exported input bundle is source-side
preparation. Running it on a worker requires a qualified package and worker
materializer that both accept the V2 request.

For an explicit experimental render-then-HDR workflow, use
[`tools/spectral_render_denoise.py`](spectral_denoise.md#one-command-experimental-local-still-workflow).
It composes the ordinary local raw V3 render with a separate verified OIDN
postprocess, retaining raw output. Native `denoise_enabled` remains false;
this does not enable detached/remote V3 jobs or establish feature preservation.

The editor's New Scene command seeds Direct Light. To create new scenes with
the RGB Disney V2 preset, start the editor with
`OPTIC_NEW_SCENE_INTEGRATOR=disney_v2`. Set this to `direct_light` to roll
back. The editor saves the selected preset in
`extensions.ray_tracing.editor_render_integrator_v1`. Opening a saved scene
uses its own preset. Older documents use their historical rendering settings.
The panel still offers an explicit per-render selection. Existing readers use
their own historical settings when they ignore the additive editor preference.
The native editor also accepts `disney_v3_spectral` as a new-scene preset.

### Render a spectral frame in the editor

In the Render workspace, open **Render frame**, then cycle **Direct Light → Disney V2 → Disney V3 Spectral**.
The main 3D integrator menu also includes V3 immediately after V2. In the frame panel select one of the following.

| V3 control | What the frame uses |
| --- | --- |
| Scene transport | The saved/default qualified scene policy. Ordinary opaque scenes use spectral GI with explicit spectra or labeled RGB source/material adapters. A saved glass photon-map scene retains that estimator. |
| Surface photons | Spectral emitters, the named closed glass mesh, receiver planes and a wavelength-resolved surface photon map. The renderer omits the participating volume and can run with its file offline. |
| Surface + volume | The same surface instrument plus the selected spectral medium. A VF3D input supplies the density grid and frame identity. Its absorption and scattering spectra come from the scene profile. |

For photon modes, place the v2 request’s `spectral` object at
`extensions.ray_tracing.spectral_transport_v1` in a meter-scale scene. Use a surface map with
`enabled: true`. Use the profile’s sources for a photon scene and leave the ordinary light list empty. Volume paths resolve relative to the scene file. The supported glass, emitters,
sensors, receivers and volume coefficients follow the contracts below. Use this spectral volume
attachment with V3. RGB atmosphere attachments use the legacy integrators.
The normal 3D menu loads the saved profile when present and uses `surface_path_v1`
otherwise. The editor panel overrides selection for one native frame job.
Ordinary V3 uses its spectral display transform through both native and headless
paths, without an additional desktop tone transform. Photon and calibration
instruments keep their declared film/display contract.

The scene profile currently supplies measured optics, emitter parameters, volume asset and photon budget.
The panel controls integrator, scene transport/deposition mode, ordinary camera
samples (**0=scene**), photon budget per source (scene, 16,384, 65,536 or 262,144)
and photon exposure relative to the scene (−1 through +2 EV). Use an explicit
optical profile for dispersive glass. `legacy_direct_v1` is the explicit saved
rollback to the historical direct spectral approximation.
The editor job receipt records the selected mode, surface deposits, volume scatter events, VF3D frame digest
and verified image SHA-256. The native UI regression is `tests/integration/test_editor_spectral_render.py`;
pass the workspace visual-test binary via `--binary` and a fresh directory via `--output-root`.

Run `make test-headless-spectral-compatibility` to exercise the renderer and
local worker contract. For the 512 × 288 default-qualification measurements,
run `python3 tests/integration/measure_spectral_default_budget.py --cli <renderer> --output <new-report.json>`
on macOS or Linux. The report includes full-process wall time and peak resident
memory, three warmed timing samples and two independent higher-budget
references for per-band receiver power and center-pixel radiance.

## Participating volumes in headless Disney V3

The [slab request](../tests/fixtures/spectral_volume/request.json) illuminates
a participating medium with two spectral lines and renders the light scattered
toward the camera. From the checkout, build `make ray-tracing-render-headless`
and pass `--request tests/fixtures/spectral_volume/request.json --render` to the CLI.
Use `make test-headless-spectral-volume` for radiometry and simulation input checks.

Add `participating_volume_v1` to the `spectral` block of a v2 surface-map request
with `surface_map.enabled = true`. The medium occupies the exterior portion of
its domain. One optional closed dielectric uses its own interior optics. Omit
`glass_id`, `glass`, `glass_roughness` and `constant_ior` together for a scene
with only matte planes. At least the named receiver plane is required.

| Field | Meaning |
| --- | --- |
| `kind` | `homogeneous`, `vf3d_density` or `water_heightfield` |
| `sigma_a_per_m`, `sigma_s_per_m` | Separate nonnegative piecewise-linear absorption and scattering coefficients in m⁻¹ at unit density. Each table has 2–32 increasing `[vacuum_nm, value]` pairs spanning exactly 380–780 nm. |
| `g` | Henyey–Greenstein mean cosine in [−0.99, 0.99]. Zero is isotropic, positive is forward scattering. |
| `mie_phase_path` | Full-angle phase table for a homogeneous volume with box reconstruction. Supply this field in place of `g`. Paths resolve relative to the request. |
| `cell_m` | Base photon bucket and averaging scale; also the evaluation spacing unless `reconstruction_cell_m` is set. Adaptive support may widen to twice this scale. Refine it together with the photon budget. |
| `reconstruction_cell_m` | Optional experimental continuous/adaptive evaluation spacing, from `cell_m / 2` through `cell_m`. Photon buckets, averaging widths and adaptive count selection retain `cell_m`. Omission preserves legacy behavior. Finer independent spacing currently requires rectangular occupied components, refuses intersecting mesh obstacles, normalizes each photon kernel over its component, and retains the 256 MiB reconstruction budget. |
| `bounds_min_m`, `bounds_max_m` | World-space XYZ corners, required only for the homogeneous box. Density inside is one. |
| `source_path`, `frame_index` | Required for simulation inputs. Paths resolve relative to the request. The decoded frame must match the requested nonnegative index. |

### Tabulated Mie scattering

The table is a regular text file with whitespace-separated values. Start with
`optic_mie_phase_v1 B N` where `B` is the number of wavelength bands (2–16)
and `N` is the number of angular nodes per band (3–32,769). For each band,
write its vacuum wavelength in nanometers followed by exactly `N` nonnegative
phase densities in sr⁻¹. Wavelengths increase from exactly 380 to 780 nm.
Angular nodes run from backscatter to forward scatter with cosine
`mu[i] = -cos(pi * i / (N - 1))`.

Each band's piecewise-linear integral over solid angle must be within 0.5%
of one. The renderer normalizes the table and uses the same distribution
for photon direction sampling and camera radiance. Between wavelength knots
it linearly mixes their normalized phase functions. Use enough wavelength
and angular nodes to resolve the droplet distribution of interest. The phase
table and the per-meter extinction coefficients are separate inputs.

The participating-volume digest includes the loaded normalized phase values.
The source qualification fixture uses the Particle Sim sphere oracle to
generate a five-band, full-angle table and checks its rainbow against the
independent droplet-angle reference.

### Experimental analytic reconstruction and local integration

`reconstruction: "kernel"` evaluates normalized, photon-centered tensor tents
without a global fine reconstruction lattice. It reuses admitted occupied
components and the convex-mesh face partition, including clipped normalization
and opposite-face restrictions. The original box, continuous and adaptive modes
retain their behavior. Kernel mode does not accept `reconstruction_cell_m`.

Optional kernel-only fields:

- `higher_scatter_support_m`: requested wider bandwidth for sparse higher-order events,
  between `cell_m` and `64 * cell_m`. First scattering retains its dense/sparse
  base support; dense higher-order events also retain the base support. This
  setting never narrows the existing dense/sparse bandwidths.
- `local_refinement_v1`: `bounds_min_m`, `bounds_max_m`, and positive `step_m`
  no greater than `cell_m`. This refines camera integration inside an explicit
  world-space box. It changes neither the VF3D extent nor the density or photon
  support. The analytic source field is shared across the box boundary, so no
  coarse/fine field blend or extra photon energy is introduced. The box does
  not automatically follow an animated camera.

For example, a 2 mm base scale can use `step_m: 0.00025` in a small focus region
while retaining the full VF3D asset. Reconstruction-owned storage stays under
256 MiB; the optional achromatic candidate cache falls back to the scalar BVH
when unavailable or full. This is an opt-in qualification mode, not a default.

The existing `surface_map.gather: "adaptive_scattered"` additionally accepts
`scattered_support_m` (between the receiver radius and 64 times that radius,
with a 1 m parser ceiling) and `scattered_neighbors` (4–64, default 16).
These choose sample-point k-nearest bandwidths for scattered receiver deposits
only. A separate coarse index covers those footprints; unscattered caustics
retain the original radius. Expanded supports are capped by finite receiver
edges and conservative clearance to other geometry. Each kernel retains unit
area normalization. These controls trade spatial bias for lower variance;
qualify beam detail and energy for the intended scene. They do not change
photon transport or remove the participating volume.

For example, use this medium with the water fixture from a request alongside the slab request.

```json
{
  "kind": "water_heightfield",
  "source_path": "../water_body_boundary/manifest_valid.json",
  "frame_index": 0,
  "sigma_a_per_m": [[380, 0.1], [550, 0.2], [780, 1.5]],
  "sigma_s_per_m": [[380, 0.4], [780, 0.1]],
  "g": 0.3,
  "cell_m": 0.05
}
```

For VF3D, set `kind` to `vf3d_density` and point to a raw frame, pack, or
VF3D manifest, for example `../scene_project_worker_snapshot/assets/vf3d/active/manifest.json`
at frame 0. Density multiplies both coefficients. The native importer supplies
the metric grid, frame time, trilinear density and masked samples. For PhysicsSim
water, use a manifest with a validated closed `water_body_boundary_v1`. Density
is one between the bottom and resolved triangular top surface. The native
closure supplies the inset, dry-column policy and simulator Y to scene Z mapping.
This input describes occupancy in the exterior optical medium. A separate
dielectric supplies refraction. Each request selects one static simulation frame.

Forward paths sample an optical depth, consume it along the ray and scatter or
absorb at the collision. The photon retains its vacuum wavelength. Camera and
shadow paths integrate Beer–Lambert extinction. Camera radiance includes the
normalized phase-weighted collision flux per reconstruction-cell volume, integrated
along the camera path. The estimator includes repeated scattering and caustic
light entering or leaving glass. The grid averages emission within each cell,
so a small cell and a sufficient photon count matter near sharp density boundaries.
The reference route bounds each spatial axis at 4096 cells, the retained map at
1,048,576 scattering events and a path at 1024 combined volume/surface steps.
Exceeding a work bound fails the request. Existing beam and brick implementations
apply to their separately qualified renderer routes.

The JSON summary adds `spectral_volume`. It reports the field digest, observed
source and selected-frame SHA-256 values, frame/time metadata and a per-band power
ledger. Terminal watts are classified as absorbed in the volume, received on a
plane, escaped, limited at a dielectric, or net optical loss at surfaces. First and
higher scattering orders count interaction power, which can exceed launched
power when a path scatters repeatedly. Rendering also reports center-pixel
single- and multiple-scattering radiance before exposure and sensor response.

## Disney V3 spectral emitters, sensors and materials

The [solar disk request](../tests/fixtures/spectral_optics/request.json) renders a
5770 K blackbody through the prism with a finite angular radius and two simultaneous
sensor readings. Build the headless CLI and pass
`--request tests/fixtures/spectral_optics/request.json --render`. Run
`make test-headless-spectral-optics` for the metamer, emitter, sensor, rough glass,
procedural texture, optical thickness and incidence checks.

**Emitters.** Each `spectral.surface_map.sources[]` entry accepts the S3 line and
continuum tables below, or a `spectrum` object with one of these types. All generated
spectra normalize to `visible_watts` integrated over 380–780 vacuum nm.

| Type | Fields and interpretation |
| --- | --- |
| `blackbody` | `kelvin` from 500 to 100000. Uses Planck wavelength density on a 4 nm grid. |
| `gaussian_mixture` | `peaks` contains one to four triples of center nm, standard deviation nm (0.01–400), and nonnegative relative peak height. Each peak contributes nodes at half-sigma intervals. The tabulation tapers to zero beyond six sigma. Use the exact-line input for atomic power. |
| `rgb_approximation` | `linear_rgb` contains three values in [0,1]. The explicit compatibility model weights 610, 545 and 455 nm Gaussian primaries of 20 nm standard deviation. Measured SPDs can use the continuum table directly. |

Set `angular_radius_deg` on any source to give its planar aperture a uniform-radiance
angular disk. Zero gives a collimated source. A solar radius of 0.265° gives a 0.53°
diameter. Forward paths sample a uniform projected angular disk (cosine-weighted solid
angle). Direct illumination integrates 64 equal projected-area samples with aperture
and visibility tests. Atmospheric absorption and sky radiance require separate inputs.

**Sensors.** The optional `spectral.sensors` array accepts one to four scalar
channels. Each has a `type` of `radiometric` or `quantum_efficiency` and a
piecewise-linear `response` table of two to 32 `[vacuum_nm, sensitivity]` pairs.
Use increasing wavelengths with endpoints 380 and 780 and sensitivity in [0,1].
All channels integrate the same band radiance in one render. The radiometric result
has units W m⁻² sr⁻¹. The QE result has units electrons s⁻¹ m⁻² sr⁻¹ and multiplies
energy by λ/(hc) once, with λ in meters. Results appear in request order in
`spectral_center_sensor_signals`, before exposure. Optional zero-based
`spectral.display_sensor` displays that channel as grayscale across the frame.
Choose exposure for the selected units. The default display and independent raw
XYZ measurement use the CIE observer. Band midpoints approximate continuum integrals.

**Materials.** These optional fields belong to `spectral` when using the surface map.

| Field | Behavior |
| --- | --- |
| `exterior_ior` | Constant index of the surrounding medium (1–3, default 1). Source and camera begin in that medium. Glass and exterior indices use the same reference. |
| `glass_roughness` | Zero selects smooth ballistic transmission and TIR. Positive values up to 1 use the native solid dielectric Smith GGX reflection/transmission sampler with α = roughness². The camera averages 16 seeded paths through rough interfaces per band. Forward samples carry flux and camera samples carry radiance. |
| `receiver_reflectance` | Bounded 380–780 nm table in the same format as sensor response. Multiplies direct and gathered Lambertian radiance on the named receiver. |
| `receiver_texture` | `scene_rgb_approximation` opts the named receiver into the native matte material and procedural texture at each camera hit. RGB channels from the native material are interpolated at 455/545/610 nm, held at the endpoints and clamped to [0,1]. This factor multiplies the spectral reflectance curve. |

Texture authoring can use a single named matte receiver entry in
`extensions.ray_tracing.authoring.object_materials` with its packed object color
and native procedural controls (none, rust or fog). Additional receiver planes
keep their neutral response. The transport contract has one closed dielectric
and one homogeneous exterior. Sensor, display and terminal receiver color can
change independently of the incident-flux map. Preflight lists the effective
source models and the explicit RGB receiver approximation.

## Local Disney V3 two-surface diffuse calibration

A v2 `disney_v3_spectral` request can opt into a single diffuse bounce between
two finite rectangular planes. This is a local headless calibration route.
Use a meter-scale runtime scene with a named receiver and panel, optionally
with opaque rectangular blockers (at most eight planes total). The two named
planes use explicit, one-sided Lambertian reflectances. Do not supply scene
lights, object material extensions, photon maps or participating media in this
profile.

Example `spectral` block:

```json
{
  "receiver_id": "receiver",
  "receiver_reflectance": [[380, 0.8], [780, 0.8]],
  "origin_m": [0.4, 1, 0],
  "lines": [[450, 1], [610, 1]],
  "exposure": 50,
  "selected_pixel": {"x": 4, "y": 4},
  "diffuse_bounce_v1": {
    "panel_id": "panel",
    "panel_reflectance": [[380, 0.02], [450, 0.02], [610, 0.8], [780, 0.8]],
    "grid": 32,
    "indirect": true
  }
}
```

The source is an isotropic point with total radiant power in watts for each
discrete vacuum wavelength (nm). Reflectance curves span 380–780 nm and are
bounded by 0 and 1. The panel uses deterministic midpoint area quadrature
with `grid` samples along each axis (4–64). The request caps
`width * height * grid * grid` at 4,194,304. Set `indirect: false` for the
same profile's direct-only control. Test with `make test-headless-spectral-diffuse`.

For environment illumination, replace both `origin_m` and `lines` with a
`sky` object. The sky emits constant radiance inside the specified angular
cap and zero outside it. The axis points from the scene toward the sky.
A 90-degree half angle describes a hemisphere.

```json
"sky": {
  "axis": [-0.6, 0.8, 0],
  "half_angle_deg": 10,
  "grid": 16,
  "radiance_lines": [[450, 1], [610, 1]]
}
```

Each line gives a vacuum wavelength in nm and radiance in W/m²/sr.
The axis is normalized. Half angles range from 1 to 90 degrees and the grid
ranges from 4 to 64 cells along each angular coordinate. Equal solid-angle
cells sample incoming directions, with opaque-plane visibility and cosine
weighting at the receiver and panel. The sky profile also caps
`(width * height + panel_grid²) * sky.grid²` at 4,194,304. The selected-pixel
report gives the source `sky_radiance` per line and the
`direct_geometry_per_sky_radiance` and `indirect_geometry_per_sky_radiance`
transfer factors. The scene background remains black in this calibration route.

The optional `selected_pixel` accepts one zero-based in-film pixel. It reports
unexposed XYZ and signed linear RGB, direct and bounced radiance per wavelength
line, the geometric transfer, and panel visibility counts. The probe also
works for single-map v2 requests (without progressive sampling). For maps it
reports direct surface, photon surface, first and higher volume scatter,
camera throughput and interface counts, gather candidates and the largest
surface deposit per band, including the source index and map record index
relative to the existing map key. Band values are integrated radiance in
W/m²/sr; `width_nm: 0` identifies an exact line. The reported map record is
not a complete photon path history. Adding a probe does not affect the
sampling, map key or displayed image.

## Spectral display and linear film controls

A local V3 single-frame request can set these optional fields in `spectral`.

| Field | Value | Effect |
| --- | --- | --- |
| `display_transform` | `"spectral_soft_v1"` | Compresses out-of-gamut colors toward neutral at constant luminance, then rolls off highlights with a common scale for the RGB channels before sRGB encoding. |
| `surface_observer` | `"deposit_wavelength"` | Uses the exact vacuum wavelength of each gathered surface deposit to form XYZ. Requires an enabled surface map. |
| `linear_strip` | `{"x":0,"y":31,"count":257}` | Returns an in-film horizontal row of unexposed XYZ values in the summary. A surface-map request can select up to 512 pixels. |
| `pixel_grid` | Integer 1–4 | Integrates a regular subpixel grid in linear light before display. A 2×2 grid uses four primary ray positions. A surface-map request is required. |

The observer option changes the color weighting of deposited surface light.
Camera attenuation, reflectance, direct and volume light, and custom sensor
responses keep their band evaluation. The option shares the same photon
map and power ledger. Use the linear strip to compare these estimates at
identical screen coordinates. The display transform operates after exposure
and leaves the strip and selected-pixel linear values intact.

These controls are qualified for local headless requests. Saved editor
profiles and progressive convergence requests use their existing controls.
Omitting the new fields preserves the previous display, observer and
one-ray behavior. Run `make test-headless-spectral-prism-film` for the
independent prism, pixel-area and display checks.

## Smooth Fresnel glass and mixed spectral lighting

For a V3 surface-map request with one closed smooth dielectric, set
`spectral.smooth_glass_transport` to `"fresnel"`. Each smooth interface
samples ordinary reflection or transmission using the wavelength-dependent
Fresnel probability. Total internal reflection continues deterministically.
`glass_camera_samples` is optional and accepts 1–64, with a default of 16.
The forward map key identifies the Fresnel mode independently of this camera
budget. Omission of the mode retains the ballistic comparison profile.

The `fresnel_transport` record in each map band lists launched, receiver,
escaped, glass-absorbed, medium-absorbed, depth-limited and other loss in watts.
It also counts ordinary reflection and total internal reflection events.
The terminal power categories sum to launched power. Reflection event counts
can exceed the number of reflected exits when a ray bounces inside the glass.

A local single-map request can combine Fresnel glass with the
`diffuse_bounce_v1` and `sky` objects above. Declare each sky line wavelength
as an exact line in a map source. A zero-watt line suffices to register a band.
Set `receiver_reflectance` and use distinct named receiver and panel planes.
The map lamp supplies caustics and fog scattering. The sky contributes direct
light and one diffuse panel bounce, including medium extinction along those
segments. A glass mesh blocks these sky illumination segments. Camera rays
can see the lit surfaces through reflection and refraction. The mixed mode
accepts up to eight scene objects and applies a bounded sky quadrature budget.

The selected-pixel report separates `sky_direct_radiance` and
`indirect_radiance` from the lamp's direct, photon surface and volume terms.
This is a headless mixed fixture profile for these enumerated paths.
Run `make test-headless-spectral-fresnel` for the independent slab controls.

## Disney V3 spectral surface photon map

The [broadband prism request](../tests/fixtures/spectral_surface_map/request.json) sends
1 W distributed uniformly over 380–780 vacuum nm through N-BK7 glass onto a matte
screen. Build with `make ray-tracing-render-headless`, then run the CLI with
`--request tests/fixtures/spectral_surface_map/request.json --render`. The BMP is
under `build/agent_runs/spectral_surface_map/frames/`. Run
`make test-headless-spectral-surface-map` for the independent optical and replay checks.

Select the v2 request schema and `disney_v3_spectral`. The `spectral` block uses
the glass, primary receiver, exposure and glass-index settings described below.
In place of the pencil source fields, supply `surface_map` with these settings.

| Field | Meaning |
| --- | --- |
| `enabled` | Include gathered surface caustics. False gives the direct-light control. |
| `photons_per_source`, `seed` | Deterministic launch count per source (up to 262144) and sampling seed. |
| `bins` | Number of equal-width continuum intervals across the visible domain (1–128). Each distinct line has its own channel. |
| `absorption_per_m` | Nonnegative uniform bulk coefficient in inverse meters or a piecewise-linear table of wavelength and coefficient covering the visible domain. |
| `sources` | One to four collimated disks. Give each an `origin_m`, `direction`, positive disk radius `aperture_m`, and its spectrum. |
| `continuum`, `lines` (per source) | Ordered pairs of wavelength and W/nm for a linear SPD (2–32 knots), and wavelength and integrated W for exact lines (up to 16). An unused component may be empty. |

The launch sampler draws uniformly over disk area and samples the combined radiant
power distribution. Each path starts with visible source watts divided by the
launch count. It keeps its sampled wavelength through refraction, Fresnel transmission,
total internal reflection and Beer–Lambert absorption. A path reaching a matte plane
after a glass interaction deposits its remaining watts once. A camera hit gathers
deposits on the same plane, side and spectral channel. For distance *d* within
`receiver_radius_m` = *r*, the unit-albedo Lambertian radiance contribution is
`watts × 2(1 − d²/r²)/(π²r²)`. Direct disk illumination is evaluated separately with
visibility and incidence cosine. Each camera channel traces its own dielectric path.
The film sums band-integrated radiance times camera throughput and the CIE observer
at the band midpoint or exact line. The continuum approximation can be refined by
increasing the band count together with the photon budget.

`--preflight` reports `spectral_surface_map` with its SHA-256 key, deposit counts,
watts and flux-weighted world centroids by band. Rendering adds center-pixel
band-integrated radiance and raw `spectral_center_pixel_xyz`. Record size, gather
candidates, build and film milliseconds are diagnostic fields. Within one CLI
process the preflight map can serve the render. Geometry, optical profile, source,
sampling and gather settings enter the key. Camera and exposure are independent.

The base S3 request supports a static scene with one closed smooth dielectric mesh and flat,
unit-albedo matte receivers. It follows the transmitted beam and total internal
reflections. Partial Fresnel reflections leave that beam. Volume deposition,
subsequent diffuse bounces and disk checkpoints require later work. Use the S4
material settings above for rough solid dielectric transport.

## Disney V3 Spectral pencil instrument

Select `render.integrator_3d = "disney_v3_spectral"` with
`schema_version = "ray_tracing_agent_render_request_v2"`. The [prism request](../tests/fixtures/spectral_prism/request.json)
is an executable example with a portable scene and mesh. From the checkout run
`make test-headless-spectral-prism` for the numerical gates. To keep the image, build
`make ray-tracing-render-headless` and run the resulting CLI with
`--request tests/fixtures/spectral_prism/request.json --render`. The fixture writes
`build/agent_runs/spectral_prism/frames/frame_0000.bmp`. Use a fresh output root for a replay.

This mode follows one collimated source through one outward-wound closed glass solid
onto one flat, unit-albedo matte detector. The request binds the optics by scene object
ID. Geometry, camera projection, ray intersection and dielectric interfaces are native
renderer operations. Existing saved RGB requests retain their selector and default.

* Author the ordinary geometry in meters at world scale 1, with no scene-level
  extensions or lights. Use a closed mesh asset for the glass.
  The detector must be a plane. The only object extension supported is the mesh
  path in `line_drawing.runtime_mesh_path`. The source and camera start in air.
* Set a finite `origin_m`, a nonzero `direction`, and 1–16 strictly increasing
  discrete `lines` from 380 through 780 vacuum nm. Each line has integrated
  `watts` and a nonnegative glass `absorption_per_m`. Exact line summation
  preserves each authored power without a sampling-PDF correction.
* Select `glass = "constant"` and `constant_ior`, or `"schott_n_bk7"` for the
  three-term Sellmeier fit at catalog conditions. The fit uses standard-air
  wavelength in micrometers and refractive index relative to air. The path keeps
  its vacuum wavelength across every boundary. [SCHOTT optical glass datasheet](https://media.schott.com/api/public/content/41e799d0bf874807a0bb8e702fbb75b5?v=54856406)
* Each transmitted branch receives the unpolarized Fresnel power fraction and
  Beer–Lambert attenuation over its actual internal path length. Camera
  transmission also carries the squared incident/transmitted index ratio.
  TIR continues inside the solid. Partial Fresnel reflections leave the measured
  transmitted beam. A 32-interface path limit is reported in each probe.
* `receiver_radius_m` is the radius of a normalized circular detector response.
  For line flux Φ its radiance inside the footprint is Φ/(π²r²) on the illuminated
  face. Each camera line traces its own path to that face. Camera throughput
  weights its raw CIE 1931 XYZ contribution. `exposure` scales summed XYZ before
  D65 linear sRGB conversion, per-channel clipping and sRGB display encoding.
* `--preflight` reports `spectral_pencil` with line wavelength, index, deviation,
  final direction, position, interface count, internal length, forward path watts
  and optical-axis camera throughput. Use `receiver_hit` to interpret a position
  as a screen measurement. On render, `spectral_center_pixel_xyz` gives the raw
  pre-exposure pixel at (width/2, height/2), with integer division.
* S2 selects a single static undenoised local frame, with a maximum of 4096
  pixels along either axis. General light spectra, rough or nested media, photon
  map gathering, video, checkpointing, worker jobs and desktop selection are
  outside this request contract. The v1 request remains the RGB contract.

## Photon runtime reports

When `inspection.caustic_photon_render_prep_population_enabled` is true, completed
frames write `photon_runtime_0000.json` (and corresponding frame indices) in the
output root. These are `ray_tracing_photon_runtime_v1` snapshots captured after
rendering and before synthetic diagnostic probes. `preparation_count`,
`rebuild_count` and `reuse_count` include preflight and runtime preparations in
this process; the latest preparation flags describe the actual most recent
frame preparation. An unchanged three-frame request can therefore report one
initial build and three reuses. Separate CLI processes do not share this map.

`cumulative_wall_ms` includes fingerprinting, map build, emission, path transport,
support preparation and preparation probes/deposition. Emission, path transport
and support preparation are nested parts of map build, not additive to it. Build
phases only accumulate on rebuild; `latest_map_build_wall_ms` is zero on reuse.
Frame-render wall time includes runtime preparation and shading. Camera-gather
time is explicitly unavailable; surface-query counts cover calls since the last
consumer bind/reset and are not unique pixels. Support-search counts describe the
last successful map build. Flux totals describe the stored photon population.

The existing final-summary `map_lifecycle` stays a compatible preflight snapshot,
now labeled `capture_stage: preflight_legacy_snapshot`. Its legacy `*_cpu_ms`
fields measure wall milliseconds; explicit wall aliases and inserted-record
counts clarify those units. Use the per-frame reports to establish runtime reuse.
This reporting adds no per-ray timer or locking and does not change rendering,
estimators, sampling budgets, UI, MCP or saved scenes.

## Opt-in BVH traversal diagnostics

Headless renders omit expensive per-node BVH counter updates by default. Set
`RAY_TRACING_BVH_TRACE_STATS=1` on the CLI process to enable them; an unset
variable or any other value keeps them off. The `bvh_summary.trace_stats_enabled`
field reports whether these counters were enabled. Disabled traversal totals
are reset to zero, which means unavailable measurement rather than zero tracing.
Overflow and fallback safety totals remain active in either mode, preserving
render failure detection. Scene preparation, TLAS/route statistics, render budgets
and optical behavior are unchanged. This process-local setting does not alter
the Desktop app or saved scenes/settings.

For editable scene revisions, sampled/consecutive previews and frozen local
render packages, see [Local scene workflow](scene_workflow.md).

For a repeatable fixed-camera object animation example, see
[Agent motion frame sets](agent_motion_frame_sets.md).

Status: Phase 4 volume handoff image export contract landed, with runtime-scene camera fallback for native `3D` renders, additive colored volume inspection tint support, first PhysicsSim water-surface sidecar ingestion, and a first detached RayTracing local job runner.

Environment-light inspection overrides now also support the shipped three-way
renderer lane:
- `off`
- `top_fill`
- `ambient`

Use `ambient_strength` for the ambient-mode surface-fill amount (`0.0..1.0`)
and `top_fill_strength` for the top-fill lane (`0.0..20.0`). Ambient mode now
resolves through an authored native `3D` environment model:
`environment_preset` selects `sky`, `warm_sky`, or `neutral`;
`background_brightness` controls miss-pixel/background radiance independently
from ambient surface fill; and `background_color` tints the preset gradient.
The older `environment_brightness` override remains available as a
compatibility knob for the underlying byte-domain environment brightness state.

The first RayTracing agent command is:

```bash
make -C ray_tracing ray-tracing-render-headless
ray_tracing/build/arm64/tools/cli/ray_tracing_render_headless \
  --request ray_tracing/tests/fixtures/agent_render_preflight_request.json \
  --preflight
```

To render BMP frame images:

```bash
make -C ray_tracing ray-tracing-render-headless
ray_tracing/build/arm64/tools/cli/ray_tracing_render_headless \
  --request ray_tracing/tests/fixtures/agent_render_image_export_request.json \
  --render
```

When a caller already declares a durable summary artifact, it may suppress
only the duplicate JSON summary written to stdout:

```bash
ray_tracing/build/arm64/tools/cli/ray_tracing_render_headless \
  --request ray_tracing/tests/fixtures/agent_render_preflight_request.json \
  --preflight \
  --summary /tmp/render_summary.json \
  --summary-file-only
```

`--summary-file-only` requires a resolved summary path, either from
`--summary` or the request. It preserves small legacy startup diagnostics on
stdout; it does not promise a globally silent process. The immutable
CPU-still guest invocation profile uses this mode so the durable JSON artifact
is not duplicated into the compute runtime's bounded stdout channel.

For the baseline unattended visual proof, use the make target:

```bash
make -C ray_tracing visual-artifact
```

The target renders a deterministic first frame, validates that the BMP is
parseable and nonblank, writes metrics to
`ray_tracing/visual_artifacts/source_first_frame/artifact_validation.json`, and
prints:

```text
ray_tracing visual artifact ready: <repo>/ray_tracing/visual_artifacts/source_first_frame/frames/frame_0000.bmp
```

`visual_artifacts/` is ignored and is separate from the `build/agent_runs/`
test-output roots.

For the canonical RT-R2 agent demo pack, including the richer mesh-asset spheres
scene and local queue-bundle dry run, see `docs/AGENT_DEMO_PACK.md`.

On non-Darwin builds the binary may be under:

```bash
ray_tracing/build/tools/cli/ray_tracing_render_headless
```

Detached submit/status/cancel now also exist:

```bash
make -C ray_tracing ray-tracing-job-runner
ray_tracing/build/arm64/tools/cli/ray_tracing_job_runner \
  submit --request ray_tracing/tests/fixtures/agent_render_job_runner_request.json
```

Optional submit policy flags:

- `--overwrite`: rerender even when target frames already exist
- `--resume`: continue from the first missing contiguous frame when earlier
  frames already exist

## Portable Worker Queue Fixture Export

Before packaging a project, create or update its RayTracing-owned render
intent without opening the menu:

```bash
make -C ray_tracing scene-project-render-request-tool
ray_tracing/build/toolchains/clang/arm64/tools/cli/scene_project_render_request_tool \
  --runtime-scene /path/to/project/scene_runtime.json \
  --start 0 \
  --count 3 \
  --stride 1
```

The tool uses the same resolver and JSON-preserving writer as the desktop
menu. It writes only a project-owned request, preserves unknown request fields,
and rejects unsafe or external write targets. When PhysicsSim has promoted a
valid project-owned active VF3D cache, the writer also enables the native
volume request and binds its contained active manifest. An absent cache keeps
the compatibility behavior; an invalid existing cache fails closed.

Validate the complete LineDrawing -> PhysicsSim -> RayTracing project contract
before export:

```bash
python3 ray_tracing/tools/validate_scene_project.py \
  --project-root /path/to/project \
  --report /path/to/project/ray_tracing/project_validation.json
```

The validator checks active pointers, project containment, scene identity,
LineDrawing mesh attachments, declared hashes, PhysicsSim retained-frame
availability, and RayTracing frame-window consistency. Its stable content hash
is independent of the project root and is copied into scene-plus-physics-cache
package manifests. The separate `--runtime-scene` mode retains loose explicit
path compatibility without claiming that those files form a portable project.

RayTracing can now create a local, queue-root-style fixture package for the
Desktop VPS worker queue without opening the UI or submitting remotely:

```bash
python3 ray_tracing/tools/export_worker_queue_fixture.py \
  --fixture \
  --output-root ray_tracing/visual_artifacts/worker_queue_exports \
  --item-name ray-tracing-portable-fixture-20260624a
python3 bin/vps_worker_job_queue.py \
  --queue-root ray_tracing/visual_artifacts/worker_queue_exports \
  validate \
  --item-name ray-tracing-portable-fixture-20260624a
```

The original mode is `scene-only`: it exports a runtime scene, referenced
`assets/mesh_assets/*.runtime.json` sidecars, `bundle/render_request.json`,
queue-compatible `bundle/request/job.json`, `bundle/request/payload/...`, and a
manifest. That package shape remains unchanged.

For a project-backed scene with an active PhysicsSim cache, use the explicit
R2-B mode:

```bash
python3 ray_tracing/tools/export_worker_queue_fixture.py \
  --mode scene-plus-physics-cache \
  --scene-project /path/to/portable_scene_project \
  --output-root ray_tracing/visual_artifacts/worker_queue_exports \
  --item-name ray-tracing-scene-project-cache-20260712a \
  --job-id ray-tracing--scene-project-cache--20260712T220000Z--rtbundle05 \
  --ray-version <active-worker-package-version>
```

This mode reads the project-owned `ray_tracing/render_request.json`, selects
PhysicsSim frames using `simulation_frames.start`, `count`, and `stride`, and
copies only those VF3D/pack and water-surface frames. It also includes the
project, authoring, runtime, optional object manifest, referenced mesh assets,
active cache/scene-bundle/water manifests, package hashes, and source lineage.
The package manifest also records `source_project_content_sha256` and its file
count, allowing a relocated copy to be matched to the validated source content
without comparing absolute lineage paths.

The portable bundle and live worker request keep canonical project attachment
paths under `assets/vf3d/...` and `assets/physics/...`. Portable-only project
metadata remains in the snapshot and lineage but is not duplicated into the
inline renderer projection. This keeps the current live preview request within
its 16-file limit. This is a snapshot of an existing cache: `start_stage` is
`ray_tracing`, and PhysicsSim is not run automatically.

When a compatible package exists on only one worker, `--force-worker-id <id>`
can hard-target that exported job. The option changes only the generated job's
submit arguments; it does not alter scheduler policy.

The first approved live proof reached Linux PC and rendered successfully but
exposed a completed-result schema mismatch. S5-B corrected the bounded inline
result contract; fresh job `r2bscene05` subsequently imported four RayTracing
outputs on the VPS and reached terminal `batch_ready` publication.

The same helper can export explicit runtime scene and render-request paths:

```bash
python3 ray_tracing/tools/export_worker_queue_fixture.py \
  --scene-runtime ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/scene_runtime_pressure_mrt8.json \
  --render-request ray_tracing/tests/fixtures/agent_render_mesh_asset_sphere_pressure_mrt8_request.json \
  --mesh-asset-root ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets \
  --output-root ray_tracing/visual_artifacts/worker_queue_exports \
  --item-name ray-tracing-explicit-scene-only-20260624a \
  --job-id ray-tracing--explicit-scene-only--20260624T000003Z--rtbundle04
```

Use `--scene-authoring <path>` when an authoring-state JSON should be copied
alongside the runtime scene as `bundle/scene_authoring.json`.

`bundle/render_request.json` is the portable render request for review and later
direct tooling. Current Desktop queue prepare still expects
`--ray-inspection-file` to be an inspection-settings JSON object, so the fixture
also writes `bundle/presets/inspection_settings.json` and points queue
`submit_args` at that file.

For a runtime scene beside `scene_project.json`, RayTracing also recognizes a
project-owned render request. It prefers the manifest's active render-request
pointer and otherwise uses `ray_tracing/render_request.json`. The request keeps
the existing `ray_tracing_agent_render_request_v1` schema and adds
project-relative `scene_project` metadata plus a `simulation_frames` object:

```json
{
  "simulation_frames": {
    "cache_manifest": "physics_sim/active_cache_manifest.json",
    "start": 0,
    "count": 1,
    "stride": 1
  }
}
```

The menu's existing scene-only export creates this sidecar when it is absent.
Existing request fields are preserved during writeback. Unsafe project pointer
traversal is rejected; explicit external request paths remain read-only
compatibility inputs. The R2-B CLI snapshot mode consumes this state without
changing the menu's scene-only behavior.

After terminal worker evidence has been fetched through an approved handoff,
retain its lineage in the originating project without downloading large frame
or video outputs implicitly:

```bash
python3 ray_tracing/tools/scene_project_worker_receipt.py import \
  --project-root /path/to/portable_scene_project \
  --export-manifest /path/to/export/manifest.json \
  --completion-evidence /path/to/completion_evidence.json \
  --render-run-id <render-run-id> \
  --render-summary /path/to/render_summary.json

python3 ray_tracing/tools/scene_project_worker_receipt.py validate \
  --project-root /path/to/portable_scene_project \
  --render-run-id <render-run-id>
```

Import requires terminal `completed` evidence whose job and source-project
identity match the worker export. It writes only beneath
`ray_tracing/runs/<render-run-id>/`, is idempotent for identical evidence, and
rejects conflicting or tampered retained files. It does not mutate the
LineDrawing authoring document or PhysicsSim cache manifests.

## Publication Lanes

There are two different post-render publication targets:

1. local repo docs review sets
   - helper: `ray_tracing/tools/publish_render_review_set.sh`
   - output root: `ray_tracing/docs/render_review_sets/<set_id>/`
   - purpose: local documentation and repo-side artifact inspection
2. live visualizer website runs
   - helper lane: `skills/codework-visualizer-drop/`
   - contract: `visualizer-run/v1`
   - local staging root:
     `_private_workspace_artifacts/codework_visualizer_runs/<drop_id>/`
   - purpose: actual website/VPS-facing visualizer publication
3. unified publish helper
   - helper: `ray_tracing/tools/publish_render_outputs.sh`
   - mode switch: `--mode local|visualizer|both`
   - defaults to the visualizer lane when `--mode` is omitted
   - can also include the full frame sequence in the staged visualizer drop
     with `--include-all-frames`
4. latest-run convenience helper
   - helper: `ray_tracing/tools/publish_latest_render_run.sh`
   - can infer the latest run root, latest frame, default set id, and title
   - useful immediately after a completed detached render when you do not want
     to spell out every path manually

Publish helpers require publish run roots to be absolute existing directories
and treat `set_id`, `drop_id`, `job_type`, and selected frame names as single
path segments. Use letters, numbers, dot, underscore, or dash; slashes,
traversal segments, and leading dash values are rejected before staging.

`publish_render_review_set.sh` is not a live website deploy step. Use it for
local docs only. Its public `request.json` and `summary.json` copies redact
local/private paths before writing into `docs/render_review_sets/<set_id>/`;
the full source artifacts remain in the private run root.

The current D2.18 denoise ablation review set is:

```text
ray_tracing/docs/render_review_sets/disney_v2_d218_denoise_on_off_visual_proof/
```

## Request Schema

The request root must declare:

```json
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "run_id": "example_run",
  "scene": {
    "runtime_scene_path": "scene_runtime.json"
  },
  "volume": {
    "enabled": false,
    "source_kind": "auto",
    "source_path": "scene_bundle.json",
    "visible": true,
    "affects_lighting": true,
    "debug_overlay": false
  },
  "render": {
    "start_frame": 0,
    "frame_count": 1,
    "width": 640,
    "height": 360,
    "normalized_t": 0.0,
    "temporal_frames": 1,
    "integrator_3d": "direct_light",
    "denoise_enabled": true
  },
  "resources": {
    "cpu_percent": 50,
    "max_workers": 2,
    "reserve_cpu_count": 1
  },
  "inspection": {
    "preset": "glass_preview",
    "camera_zoom": 0.95,
    "camera_position": { "x": -3.8, "y": -7.2, "z": 2.2 },
    "camera_look_at": { "x": -0.2, "y": 0.8, "z": 1.2 },
    "environment_light_mode": "ambient",
    "ambient_strength": 0.25,
    "environment_preset": "sky",
    "background_brightness": 0.35,
    "background_color": { "r": 0.90, "g": 0.95, "b": 1.00 },
    "light_intensity": 2.6,
    "light_radius": 0.10,
    "forward_decay": 220.0,
    "volume_scatter_gain": 3.0,
    "volume_step_scale": 1.0,
    "secondary_diffuse_samples_3d": 8,
    "transmission_samples_3d": 4,
    "object_audit_enabled": true,
    "object_audit_max_dimension": 160,
    "volume_tint": { "r": 0.35, "g": 0.65, "b": 1.80 }
  },
  "output": {
    "root": "ray_tracing",
    "overwrite": false
  },
  "progress": {
    "summary_path": "ray_tracing/render_summary.json",
    "progress_path": "ray_tracing/render_progress.json"
  }
}
```

The optional `resources` object controls native `3D` tile scheduler concurrency
for headless renders:

- `cpu_percent`: `1..100`, caps worker threads to an approximate percentage of
  detected host CPUs. `50` on a 2-vCPU VPS resolves to one render worker.
- `max_workers`: explicitly selects a render-worker ceiling, after CPU-percent,
  CPU-reservation and available-job limits, up to the scheduler maximum of 12.
  Omitted or nonpositive native limits retain the established four-worker default.
  A request for 12 on an eight-CPU host resolves to at most eight workers.
  Headless progress reporting does not reserve an interactive-preview CPU.
- `reserve_cpu_count`: subtracts host CPUs before resolving the worker cap, so
  colocated services keep explicit headroom.

Completed native tiled render summaries report
`temporal_tile_scheduler_requested_workers` (zero when no explicit native limit
or no pool was created),
`temporal_tile_scheduler_actual_workers`, `temporal_tile_scheduler_host_cpu_count`
and `temporal_tile_scheduler_wall_ms`. Actual workers means the successfully created pool size, not average busy CPUs.
Counts are maxima across rendered frames; wall milliseconds sum across frame scheduler invocations and include setup,
worker dispatch/wait and final resolve. These are not whole-command timings or
photon-preparation timings. Existing frame-dataflow `total_tile_ms` counts overlapping
worker intervals and is not CPU time; it must not be added to wall time.
Existing rendered/skipped pixel-sample counters describe delivered sampling.
Whole-process peak memory and CPU utilization require an external process
measurement. Untiled paths leave scheduler-only counters at zero; pool counts also remain zero
when there are no tile jobs and no pool is created.

When `resources` is omitted, direct local CLI runs keep legacy behavior. The
packaged Linux worker wrapper sets `CODEWORK_RAY_TRACING_DEFAULT_CPU_PERCENT=50`
unless the environment already provides another value, so older worker jobs get
a conservative VPS-safe default after the worker package is refreshed. Explicit
request JSON takes precedence over package environment defaults.

Supported `volume.source_kind` values:
- `auto`
- `manifest`
- `scene_bundle`

`volume.visible=false` keeps the configured volume source available for
sidecar-driven imports such as `scene_bundle.json.water_source`, but skips
attaching the visible VF3D volume body to the native `3D` render scene. This is
the preferred WTR water-surface review mode when the water heightfield should
render as a native mesh without the surrounding volumetric slab/box.
- `raw_vf3d`
- `vf3d`
- `pack`
- `none`

Supported `render.integrator_3d` values:
- `direct_light`
- `diffuse_bounce`
- `material`
- `emission_transparency`
- `disney`
- `disney_v2`

Optional `render.denoise_enabled` overrides the runtime animation setting for
that detached request. It is intended for apples-to-apples denoise ablations,
for example rendering the same `disney_v2` scene with `temporal_frames=12`
once with denoise off and once with denoise on.

Relative paths resolve from the request file directory.

## Visual Matrix Runner

Use the visual matrix runner when a proof needs multiple comparable headless
cells, copied summaries, PNG frames, contact sheets, and amplified difference
metrics:

```bash
make -C /Users/calebsv/Desktop/CodeWork/ray_tracing \
  build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless

/Users/calebsv/Desktop/CodeWork/ray_tracing/tests/integration/run_ray_tracing_visual_matrix.py \
  --manifest /Users/calebsv/Desktop/CodeWork/ray_tracing/tests/fixtures/disney_v2_visual_matrix/transparent_interior_stack/matrix_manifest.json \
  --group disney_v2_denoise_ablation
```

The first Disney v2 D2.18b canonical matrix fixtures live under:

```text
ray_tracing/tests/fixtures/disney_v2_visual_matrix/
  primitive_glass_corridor/
  transparent_interior_stack/
  mirror_glossy_preservation/
  high_noise_emitter/
```

Each matrix writes a private review package by default under:

```text
_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/<matrix_id>/matrix_review/
```

Generated packages include:

- `matrix_report.json`
- `index.md`
- `matrix_contact_sheet.png`
- `frames/<cell>.png`
- `requests/*.json`
- `summaries/summary_<cell>.json`
- `comparisons/<comparison_id>/diff_metrics.json`
- `comparisons/<comparison_id>/side_by_side_*.png`

The D2.18b fixture manifests use `render.denoise_enabled=false` and `true`
with the same `temporal_frames=12` budget so the resulting diff is the
Disney-v2 edge-safe final resolve only, not a temporal-sample-count change.

For caustic-readiness work, use the glass-sphere probe:

```bash
make -C ray_tracing test-ray-tracing-caustic-probe-matrix
```

The fixture lives under
`ray_tracing/tests/fixtures/caustic_probe_glass_sphere/`. Its Disney v2 request
uses the high-fidelity `asset_sphere_256x128` glass mesh and runs the Disney v2
caustic policy with `inspection.caustic_mode = "analytic"`. The summary
readback records the request mode, sidecar strength, sidecar sample count,
contributing sample count, and sidecar radiance totals. Direct-light and
emission-transparency requests remain baseline measurements and do not enable
the Disney v2 caustic path.

Optional `inspection` fields are headless-only tuning overrides. They do not
change the persisted runtime scene:

- `camera_zoom`
- `camera_position`
- `camera_look_at`
- `environment_light_mode`
- `ambient_strength`
- `environment_preset`
- `background_brightness`
- `background_color`
- `top_fill_strength`
- `environment_brightness`
- `light_intensity`
- `light_radius`
- `forward_decay`
- `volume_density_scale` (`density_scale` alias)
- `volume_density_gamma` (`density_gamma` alias)
- `volume_scatter_gain`
- `volume_absorption_gain` (`absorption_gain` alias)
- `volume_opacity_clamp` (`opacity_clamp` alias)
- `volume_step_scale`
- `preset`
- `secondary_diffuse_samples_3d`
- `transmission_samples_3d`
- `object_audit_enabled`
- `object_audit_max_dimension`
- `volume_tint`
- `volume_albedo` / `volume_albedo_tint`

Preferred environment-light override contract:
- `environment_light_mode = "off"` disables the extra environment fill lane
- `environment_light_mode = "top_fill"` uses `top_fill_strength`
- `environment_light_mode = "ambient"` uses `ambient_strength`
- `environment_preset = "sky" | "warm_sky" | "neutral"` selects the resolved
  background gradient before `background_color` tinting
- `background_brightness` (`0.0..4.0`) explicitly controls background/miss
  radiance. When omitted, it is derived from `ambient_strength` for
  compatibility with existing ambient requests.
- `background_color` accepts `{ "r": ..., "g": ..., "b": ... }` or
  `[r, g, b]` with channels clamped to `0.0..1.0`
- `environment_brightness` still maps directly to the persisted
  `0..255` environment brightness value and should be treated as a lower-level
  compatibility override rather than the first-choice authored request field

The headless summary writes a resolved `environment_lighting` object with:
- `mode`
- `preset`
- `ambient_strength`
- `background_brightness`
- `background_brightness_source`
- `background_color`
- `background_top_color`
- `background_bottom_color`
- `ambient_surface_fill_contributes`
- `background_miss_contributes`
- `top_fill_contributes`

## Current Behavior

`--preflight`:

1. parses the request schema
2. applies the runtime scene through the existing bridge
3. optionally validates and attaches the configured VF3D volume source and
   `scene_bundle.json.water_source` water-surface sidecar
4. resolves the native 3D route
5. prepares a native 3D frame in memory
6. writes `ray_tracing_headless_summary_v1`

`--render` runs the same readiness path and then writes BMP frames to:

```text
<output.root>/frames/frame_%04d.bmp
```

Frame filenames use the requested absolute render frame index. For example, a
single-frame request with `render.start_frame = 17` writes
`frames/frame_0017.bmp`.

When `progress.progress_path` is set, the render CLI now also writes a
`ray_tracing_render_progress_v1` JSON file with stage transitions such as
`loading_settings`, `applying_runtime_scene`, `runtime_scene_applied`,
`attaching_volume`, `resolving_native_route`, `preparing_native_frame`,
`bvh_ready`, `object_audit`, `object_audit_ready`, `preflight_ready`,
`rendering_frame`, `writing_frame`, and `completed`.

Headless object-audit summaries are enabled by default but capped by
`inspection.object_audit_max_dimension`, which defaults to `160` pixels on the
longest side. The audit still records per-object primary hit counts, but uses a
downsampled primary-ray pass and scales hit counts by the audit stride so large
worker renders do not perform a full-resolution diagnostic ray pass before the
first frame. Set `inspection.object_audit_enabled` to `false` to skip this
diagnostic pass entirely for production-only runs.

The frame index starts at `render.start_frame` and writes `render.frame_count`
frames. The summary reports:

- `rendered_frames`
- `frames_rendered`
- `outputs.frame_dir`
- `outputs.first_frame_path`
- `outputs.last_frame_path`
- `render.has_denoise_enabled_override`
- `render.denoise_enabled`
- `denoise.has_request_override`
- `denoise.enabled`
- `denoise.applied`
- `volume_visible`
- `volume_summary_built`
- `volume_summary.density_non_zero_cell_count`
- `water_surface_source_found`
- `water_surface_loaded`
- `water_surface_mesh_attached`
- `water_surface.frame_selection_dynamic`
- `water_surface.selected_first_frame_path`
- `water_surface.selected_last_frame_path`
- `water_surface.triangle_count`
- `water_surface.grid_w`
- `water_surface.grid_d`
- `water_surface.wet_columns`
- `water_surface.surface_min_y`
- `water_surface.surface_max_y`
- `water_surface.material.ior`
- `water_surface.material.absorption_distance_m`
- `water_surface.material.absorption_rgb`
- `water_surface.payload.applied`
- `water_surface.payload.ior`
- `water_surface.payload.absorption_distance_m`
- `water_surface.payload.transparency`
- `water_surface.payload.tint_rgb`
- `render_stats.hit_pixels`
- `render_stats.visible_pixels`
- `render_stats.nonzero_pixels`
- `render_stats.max_radiance`
- `render_stats.max_rgb`
- `render_stats.emissive_area_candidate_count`
- `render_stats.emissive_area_selected_candidate_count`
- `render_stats.emissive_area_visibility_ray_count`
- `render_stats.emissive_area_primary_sample_count`
- `render_stats.emissive_area_recursive_sample_count`
- `render_stats.emissive_area_full_scan_fallback_count`
- `render_stats.mirror_dominant_pixels`
- `render_stats.mirror_base_attenuated_pixels`
- `render_stats.mirror_reflection_hit_pixels`
- `render_stats.mirror_emitter_reflection_pixels`
- `render_stats.mirror_geometry_reflection_pixels`
- `render_stats.max_mirror_dominance`
- `render_stats.max_mirror_specular_reflection_radiance`
- `render_stats.max_mirror_base_radiance_before_attenuation`
- `render_stats.max_mirror_base_radiance_after_attenuation`
- `render_stats.total_mirror_specular_reflection_radiance`
- `render_stats.total_mirror_base_radiance_before_attenuation`
- `render_stats.total_mirror_base_radiance_after_attenuation`
- `render_stats.denoise_temporal_frame_count`
- `render_stats.denoise_raw_pixel_count`
- `render_stats.denoise_reconstructed_pixel_count`
- `render_stats.denoise_stable_interior_sample_count`
- `render_stats.denoise_rejected_edge_sample_count`
- `render_stats.denoise_preserved_transparent_pixel_count`
- `render_stats.denoise_preserved_mirror_glossy_pixel_count`
- `render_stats.denoise_skipped_unstable_temporal_pixel_count`
- `render_stats.denoise_skipped_invalid_surface_pixel_count`
- `render_stats.denoise_raw_radiance_luma_total`
- `render_stats.denoise_reconstructed_radiance_luma_total`
- `render_stats.denoise_radiance_luma_delta`
- `object_audit_summary.enabled`
- `object_audit_summary.width`
- `object_audit_summary.height`
- `object_audit_summary.sample_count`
- `object_audit_summary.full_resolution_pixel_count`
- `object_audit[*].object_id`
- `object_audit[*].material_id`
- `object_audit[*].alpha`
- `object_audit[*].reflectivity`
- `object_audit[*].roughness`
- `object_audit[*].emissive_strength`
- `object_audit[*].texture_id`
- `object_audit[*].texture_strength`
- `object_audit[*].texture_scale`
- `object_audit[*].texture_offset_u`
- `object_audit[*].texture_offset_v`
- `object_audit[*].texture_seed`
- `object_audit[*].texture_pattern_mode`
- `object_audit[*].texture_coverage`
- `object_audit[*].texture_grain`
- `object_audit[*].texture_edge_softness`
- `object_audit[*].texture_contrast`
- `object_audit[*].texture_flow`
- `object_audit[*].texture_color_depth`
- `object_audit[*].texture_surface_damage`
- `object_audit[*].packed_color`
- `object_audit[*].primitive_count`
- `object_audit[*].triangle_count`
- `object_audit[*].primary_hit_pixels`
- `object_audit[*].center_screen`
- `inspection.camera_zoom`
- `inspection.camera_position`
- `inspection.camera_look_at`
- `inspection.environment_brightness`
- `inspection.light_intensity`
- `inspection.light_radius`
- `inspection.forward_decay`
- `inspection.volume_scatter_gain`
- `inspection.volume_step_scale`
- `inspection.preset`
- `inspection.secondary_diffuse_samples_3d`
- `inspection.transmission_samples_3d`
- `inspection.caustic_mode` (`disney_v2` only; `analytic` default, `off`,
  `transport` for explicit caustic cache transport)
- `inspection.caustic_transport_engine` (`exploratory_lens_transport` default;
  `photon_map` selects the explicit photon population/query route)
- `inspection.caustic_product_mode` (`off`, reference modes, or explicit
  `production`/`photon_map`)
- `inspection.caustic_photon_consumer` (`direct_map` or `cache_bridge`)
  - `direct_map` evaluates the surface photon map at each actual opaque receiver
    hit using the already resolved material's diffuse BSDF. Primary, transmitted
    and reflected receivers share this response; arriving path throughput is
    applied by the enclosing transport owner. It neither deposits nor samples the
    surface bucket bridge, and the host does not add a selected transmitted
    receiver a second time.
  - `cache_bridge` retains the explicit bucket-centroid reconstruction path.
    It is an approximation distinct from actual-hit direct-map evaluation.
  - Receiver accounting sums count query calls; they are not integrated power
    or final camera-pixel radiance. Compare emitted/stored photon flux separately
    from receiver BSDF response and display-encoded image differences.
- `inspection.caustic_photon_render_prep_population_enabled`
- `inspection.caustic_photon_render_contribution_enabled`
- `inspection.caustic_photon_surface_diagnostics_enabled` (default false;
  writes per-frame landing and reconstructed-query JSONL)
- `inspection.caustic_photon_sample_budget`
  - Surface-only maps support up to 1,048,576 emitted photons and records;
    the owned volume/beam route remains limited to 262,144. Progress exposes
    `photon_budget.requested/effective/limit/limited`. Post-frame photon runtime
    reports emitted count, record capacities/storage bytes, capacity rejections
    and prepared support-neighbor limit in `budget`. These record-storage bytes
    exclude other process memory. Compare requested/effective/emitted counts;
    larger requests are explicitly capped.
  - `k_nearest` uses population-derived sample-centered supports. The explicit
    gather-neighbor setting tunes other gather estimators. Gather max radius is
    normalized to be at least query radius. See
    [bounded quality qualification](photon_quality_presets.md) for measured limits.
- `inspection.caustic_photon_max_path_depth`
- `inspection.caustic_photon_surface_query_radius`
- `inspection.caustic_photon_surface_gather_max_radius`
- `inspection.caustic_photon_surface_gather_neighbors`
- `inspection.caustic_photon_surface_radiance_scale`
- `inspection.caustic_photon_surface_allow_active_medium_receiver` (default
  false; opt in for an opaque receiver reached beneath a single open water
  interface)
- `inspection.caustic_sidecar_enabled` (`disney_v2` legacy alias)
- `inspection.caustic_sidecar_strength` (`disney_v2` only)
- `inspection.volume_tint`

For transparent/glass preview work, `emission_transparency` now supports
bounded per-run sampling budgets through:

- `inspection.preset = "glass_preview"`
- `inspection.secondary_diffuse_samples_3d`
- `inspection.transmission_samples_3d`

Use low preview values such as `4/4` or `8/4` for quick validation, then raise
them for slower review or long-review renders when you want cleaner
transmission.

`glass_preview` defaults the run to `emission_transparency` when
`render.integrator_3d` is omitted, and stamps a low-cost preview budget of
`secondary_diffuse_samples_3d = 8` and `transmission_samples_3d = 4`. Explicit
integrator or sample overrides still win.

`glass_review` defaults the run to `emission_transparency` when
`render.integrator_3d` is omitted, and stamps a slower inspection budget of
`secondary_diffuse_samples_3d = 24` and `transmission_samples_3d = 12`.
Explicit integrator or sample overrides still win.

When the target surface is the live visualizer site rather than local docs:

- convert BMP frame outputs to PNG during visualizer staging
- prefer `skills/codework-visualizer-drop/scripts/stage_visualizer_run.py`
  with BMP-to-PNG normalization instead of ad hoc manual copies
- prefer `ray_tracing/tools/publish_render_outputs.sh --mode visualizer`
  or `--mode both` when you want one higher-level command
- prefer `ray_tracing/tools/publish_latest_render_run.sh` when publishing the
  most recent finished run and the default "last frame is the hero frame"
  policy is acceptable
- keep `drop_id` strict:
  `<program>--<job_type>--<YYYYMMDDTHHMMSSZ>--<nonce>`
- if VPS import reports one success plus one failure, check for a stale invalid
  remote staged drop before assuming the new publish failed
- remove stale staged drops with
  `skills/codework-visualizer-drop/scripts/cleanup_remote_visualizer_drop.sh`
  before rerunning the importer when needed

The headless summary now also emits `object_audit`, which inventories the live
runtime `sceneSettings` slots plus built primitive/triangle counts and a
camera-ray first-hit pixel count per object. Use it when a runtime-scene object
seems missing: if an object is absent from `object_audit`, it never reached the
live render object lane; if it is present with `primitive_count > 0` but
`primary_hit_pixels == 0`, the camera does not actually see it.

The summary also emits diagnostic `timing_breakdown` fields for headless
capacity proofs. Top-level fields separate runtime-scene application, duplicate
scene preflight, native `3D` frame preparation, optional object audit, render
trace, frame analysis/write, optional video encode, and total run time. Nested
`mesh_asset_loader` fields cover runtime-scene read/parse, mesh runtime
document load, cached document copy, cache hit/miss counts, loaded asset bytes,
vertices, and triangles. Nested `scene_builder` fields cover primitive seeding,
mesh append reserve/expansion, BVH rebuild wall time, mesh instance counts, and
expected/appended triangle counts. Treat these fields as diagnostic timing
telemetry for local/private performance proofing, not as promotion gates.

For static file-backed runtime mesh startup proofing, prepared acceleration
summaries also expose persistent BLAS/BVH cache counters:
`blas_persistent_cache_hits`, misses, writes, invalidations, refreshes, read
milliseconds, and write milliseconds. The startup timing matrix mirrors those
as `mesh_local_blas_persistent_cache_read` and
`mesh_local_blas_persistent_cache_write` stages.

The `bvh_summary` object includes diagnostic build microtiming fields for
high-triangle capacity work: allocation, centroid build, inclusive tree build,
range-bounds scans, sort/partition work, node append, final stats, unaccounted
build overhead, range/sort/node call counts, and maximum range sizes. These are
for local performance diagnosis and should not be used as route promotion
criteria.

## Render-Cost and Temporal-Pruning Diagnostics

Use these environment gates when a headless run is meant to explain ray count,
per-ray cost, or temporal-subpass work. Keep baseline beauty renders free of
diagnostic envs unless the comparison explicitly calls for them.

Current default-on render-cost policies:

- pure `tlas_blas` native `3D` renders skip the legacy flattened-BVH build by
  default. Use `RAY_TRACING_NATIVE3D_DISABLE_DEFAULT_TLAS_BVH_SKIP=1` only for
  rollback/parity checks.
- Disney v2 reflected-transmission first-subpass no-hit reuse is default-on.
  Use `RAY_TRACING_DISNEY_V2_REFLECTED_FIRST_SUBPASS_NO_HIT_REUSE_PROBE=0` to
  force old full-trace behavior for regression checks.

Current opt-in diagnostics and candidates:

- `RAY_TRACING_RENDER_TRACE_COST_LEDGER=1` writes the render trace cost ledger
  into the headless summary. Use it for ray-class, material-family,
  direct-light visibility, transmission, and Disney v2 path-cost attribution.
- `RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER=1` writes frame-dataflow and
  ownership diagnostics into the headless summary. Use it for startup,
  scheduler, scratch, TLAS/BLAS, and frame-lifetime accounting.
- `RAY_TRACING_NATIVE_3D_TEMPORAL_RISK_EARLY_STOP=1` enables the R4 per-pixel
  temporal risk early-stop candidate. It is not a default launcher or desktop
  package setting. The representative textured-glass operator-scene proof did
  not justify promotion, so keep this as an operator/debug option unless a
  future narrower policy earns a new proof.
- `RAY_TRACING_NATIVE_3D_TEMPORAL_BUDGET_HEATMAP=1` emits the temporal budget
  heatmap for threshold review. Pair it with the risk early-stop env when the
  goal is to see which pixels were pruned, held by transparent/material risk,
  held by light/edge risk, or kept as high-budget glass/receiver regions.
- `RAY_TRACING_DIRECT_LIGHT_CLEAR_VISIBLE_DECISION_SAMPLE_PROBE=1` enables the
  stricter all-clear direct-light visibility decision probe. It is useful for
  attribution and ray-count study, but it is not a timing win on the restored
  transparent stack and should stay off for default beauty runs.
- `RAY_TRACING_DISNEY_V2_REFLECTED_TRANSMISSION_SAMPLE_CAP=<n>` is an
  experimental reflected-transmission cap override. The current proven safe
  reflected-transmission default is the guarded first-subpass no-hit reuse
  above, not a blind lower cap.

For tiled A/B reviews, run the exact same request and tile renderer settings
with only the candidate env changed. The promotion review should include:

- rendered pixel-subpass delta
- total ray and transmission/direct-light ray deltas
- max channel delta and count of pixels above small thresholds
- internal tile-line and 1-pixel tile-band changed-pixel density
- side-by-side and amplified-diff images
- the headless summary paths and candidate envs used

The current R4 status is intentionally conservative: high-quality synthetic
reviews showed useful subpass reduction and structurally clean tile borders,
but the representative textured-glass operator-scene proof had nontrivial drift
and no timing win. `RAY_TRACING_NATIVE_3D_TEMPORAL_RISK_EARLY_STOP=1` remains
opt-in only.

When a runtime-scene camera seed provides position but no authored orientation,
the native `3D` render path now auto-aims that camera toward the built scene
center. If the runtime scene provides authored camera orientation fields, those
values win. Supported authored fields are:

- `yaw`
- `rotation_z`
- `rotationZ`
- `transform.rotation.z`
- `look_pitch`
- `lookPitch`
- `pitch`
- `rotation_x`
- `rotationX`
- `transform.rotation.x`

For authored moving-camera scenes, prefer
`extensions.ray_tracing.authoring.camera_focus_target` in the source
LineDrawing request. When present, headless runtime-scene sampling keeps the
authored camera-path position/depth motion but recomputes yaw/pitch each sample
to keep the camera aimed at that target. This is a safer lane than hand-authoring
moving `rotation` / `lookPitch` values for each camera-path keyframe.

For multi-frame requests, the current interpolation samples normalized time
from `0.0` to `1.0` across the requested frame count. A single-frame request
uses `render.normalized_t`. Native `3D` direct-light requests now also honor
`render.temporal_frames` instead of silently collapsing back to one subpass.
When the runtime scene carries `extensions.ray_tracing.authoring.light_path`
and/or `camera_path`, headless multi-frame renders now preserve those sampled
authored positions across the render loop instead of collapsing back to the
first light point.

For transparent-surface review, prefer `render.integrator_3d =
emission_transparency`. `direct_light` remains the faster framing/motion tier,
but it is not the correct final readback for transmissive behavior through
transparent objects.

For PhysicsSim volume handoff, the validation flow now authors a room-style
LineDrawing emitter scene with floor, wall planes, a contrast prism, and one
fluid emitter prism, runs `physics_sim_headless --save-volume-frames`, points
RayTracing at the generated `scene_bundle.json`, and writes higher-resolution
BMP frames under:

```text
ray_tracing/build/agent_runs/physics_trio/volume_handoff_image_export/ray_tracing/frames/
```

The summary validates native `3D` route readiness, successful volume
attachment, nonzero VF3D density ingestion, selected first/last volume frame
paths, loaded first/last VF3D frame indices, and whether the final frame export
produced visible pixels. Manifest-backed and scene-bundle-backed rendering now
resolves VF3D or pack sources by requested render frame.

For PhysicsSim water-surface handoff, the validation flow runs
`physics_sim_headless --water-mode --save-volume-frames`, points RayTracing at
the generated `scene_bundle.json`, imports `water_source`, and appends the
selected PhysicsSim Y-up heightfield frame as native `3D` triangle geometry.
The render mesh maps PhysicsSim height `y` into RayTracing scene-up `z`, so the
water surface displays as a horizontal basin plane while preserving the
producer-side `surface_axis: "y"` contract. The current backend path is
headless only; editor controls and LineDrawing liquid-region authoring remain
follow-up work.

```text
ray_tracing/build/agent_runs/physics_trio/water_surface_handoff_image_export/ray_tracing/frames/
```

For single-frame water optics review, the WTR-5.3 validation flow runs
`physics_sim_headless --water-mode --water-review-ripples`, warms the basin for
18 frames, selects the final `water_surface_000017.json` sidecar through
`scene_bundle.json.water_source`, and renders one `emission_transparency` frame
with contrast geometry behind the imported water surface:

```text
ray_tracing/build/agent_runs/physics_trio/water_optics_review_single_frame/ray_tracing/frames/frame_0017.bmp
```

The test asserts that the review sidecar applied ripples, the selected
heightfield has non-flat finite heights/normals, the water material payload is
active, the camera visibly hits the generated `water_surface` object, and the
render traces through transparent water.

For larger-surface visual review, use
`make -C ray_tracing test-ray-tracing-render-headless-water-basin-surface-review`.
This fixture keeps the same PhysicsSim -> `scene_bundle.json.water_source` ->
RayTracing path, but the standalone Water Basin now resolves to a square X/Z
footprint so the imported surface frame is broad instead of a narrow strip. The
review run uses a lighter `32 x 32` water surface, deterministic review
ripples, a plain basin/floor/wall scene, and a closer camera to produce:

```text
ray_tracing/build/agent_runs/physics_trio/water_basin_surface_review_single_frame/ray_tracing/frames/frame_0011.bmp
```

This is still a headless backend optics/review fixture. The current producer
contract remains the PhysicsSim-authored Y-up heightfield, while the RayTracing
builder now remaps that imported heightfield into a Z-up rendered water sheet.

For moving-light multi-frame water optics review, use
`make -C ray_tracing test-ray-tracing-render-headless-water-moving-light-review`.
This WTR-5.4 fixture warms a standalone PhysicsSim Water Basin for `16` frames,
renders four consecutive RayTracing frames (`0008..0011`) through the generated
`scene_bundle.json.water_source`, and samples an authored
`extensions.ray_tracing.authoring.light_path` across normalized render time:

```text
ray_tracing/build/agent_runs/physics_trio/water_moving_light_review/ray_tracing/frames/frame_0008.bmp
ray_tracing/build/agent_runs/physics_trio/water_moving_light_review/ray_tracing/frames/frame_0011.bmp
```

The test asserts that the selected first/last water surface sidecars are both
loaded, the heightfield evolves between those frames, transparent-water
secondary hits remain active, and the rendered BMP sequence has measurable
frame-to-frame pixel deltas from the moving light over the rippled water
surface.

For long-motion sparse-frame review before object displacement work, use
`make -C ray_tracing test-ray-tracing-render-headless-water-long-motion-review`.
This WTR-5.5 fixture runs PhysicsSim Water mode for `201` frames with `4`
simulation steps per exported frame, samples frames `40, 80, 120, 160, 200`,
and renders full RayTracing basin BMP frames from `scene_bundle.json.water_source`:

```text
ray_tracing/build/agent_runs/physics_trio/water_long_motion_review/ray_tracing/frames/frame_0040.bmp
ray_tracing/build/agent_runs/physics_trio/water_long_motion_review/ray_tracing/frames/frame_0200.bmp
ray_tracing/build/agent_runs/physics_trio/water_long_motion_review/ray_tracing/frames/water_long_motion_contact_sheet.bmp
```

This target is a sparse full-RayTracing basin proof for large time separation:
it shades the actual simulated water surfaces with a moving light inside the
same basin-style render composition and asserts height/image deltas across
sparse frames. Full-length video/output throughput remains a later optimization.

For WTR-6 object-water review, use
`make -C ray_tracing test-ray-tracing-render-headless-water-object-coupling-review`.
This fixture runs PhysicsSim Water mode with `--water-object-fixture`, then
renders full RayTracing basin frames `0008`, `0018`, and `0027` through
`scene_bundle.json.water_source` with a visible block object in the basin:

```text
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_review/ray_tracing/frames/frame_0008.bmp
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_review/ray_tracing/frames/frame_0027.bmp
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_review/ray_tracing/frames/water_object_coupling_review.mp4
```

The BMP sequence is the deterministic acceptance artifact. When local `ffmpeg`
is available, the test also encodes the MP4 review helper without replacing the
full RayTracing basin render path.

For WTR-6.4 longer object-water output, use
`make -C ray_tracing test-ray-tracing-render-headless-water-object-coupling-long-review`.
The target is profile-driven:

```bash
# Small validation profile; this is what the make target runs by default.
make -C ray_tracing test-ray-tracing-render-headless-water-object-coupling-long-review

# Expensive review profile requested for long visual inspection.
WTR6_LONG_PROFILE=full \
  make -C ray_tracing test-ray-tracing-render-headless-water-object-coupling-long-review
```

The default `smoke` profile warms up `8` PhysicsSim frames, renders `4`
RayTracing frames at stride `5`, and writes:

```text
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_long_review_smoke/
```

The `full` profile warms up `200` PhysicsSim frames, selects `100` output
frames at stride `5` from the evolved Water Basin (`200..695`), uses a looped
overhead light path, and writes the full BMP/MP4 review under:

```text
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_long_review/
```

Useful overrides:

- `WTR6_LONG_PROFILE=smoke|review|full`
- `WTR6_LONG_WARMUP_FRAMES=<n>`
- `WTR6_LONG_OUTPUT_FRAMES=<n>`
- `WTR6_LONG_FRAME_STRIDE=<n>`
- `WTR6_LONG_SIM_STEPS_PER_FRAME=<n>`
- `WTR6_LONG_GRID=<WxHxD>`
- `WTR6_LONG_WIDTH=<px>`
- `WTR6_LONG_HEIGHT=<px>`
- `WTR6_LONG_TEMPORAL_FRAMES=<n>`
- `WTR6_LONG_INTEGRATOR_3D=<emission_transparency|direct_light|disney_v2>`
- `WTR6_LONG_FPS=<n>`
- `WTR6_LONG_RIPPLE_AMPLITUDE=<meters>`

For WTR-6.6 cache-first preview-matrix planning, use the dry-run planner before
launching more PhysicsSim or RayTracing work:

```bash
python3 ray_tracing/tools/wtr66_preview_matrix_planner.py --overwrite
make -C ray_tracing test-ray-tracing-wtr66-preview-matrix-planner-dry-run
```

The default dry-run writes:

```text
ray_tracing/build/agent_runs/physics_trio/wtr66_preview_matrix_dry_run/
```

It creates `matrix_request.json`, `matrix_summary.json`, one planned
`cache_manifest.json`, named selected-frame lists such as `contact_short` and
`every_10`, plus direct-light and Disney-v2 temporal-2 render request sets over
the same planned PhysicsSim cache. It does not run PhysicsSim or RayTracing.

To execute the first local cache/job-runner validation boundary:

```bash
make -C ray_tracing test-ray-tracing-wtr66-preview-matrix-local-job-runner
```

That explicit gate writes:

```text
ray_tracing/build/agent_runs/physics_trio/wtr66_preview_matrix_local_job_runner/
```

It runs one local PhysicsSim water/object cache and submits direct-light plus
Disney-v2 temporal-2 one-frame jobs over the same selected frame set through
`ray_tracing_job_runner`, then validates job status, render summaries, water
mesh attachment, block visibility, secondary hits, and BMP outputs. The harness
prefers the current clang toolchain job-runner binary before the legacy
`build/<arch>` path to avoid stale local binaries.

The water summary reports whether a water source was found, whether the frame
loaded, whether mesh triangles were attached, requested/loaded first and last
frame indices, selected frame paths, grid dimensions, wet/dry/solid column
counts, surface height statistics, finite-normal status, material IOR and
absorption metadata, the resolved RayTracing water payload, and appended
triangle count. The source `material.absorption_rgb` values are absorption
coefficients; the resolved `payload.tint_rgb` values are the Beer-Lambert
transmittance tint used by the native `3D` material path. Per-render-frame
native `3D` preparation reloads the water heightfield for the requested
absolute render frame, so animated PhysicsSim sidecars can move with the
existing camera/light path sampling.

For WTR-6.7C worker-backed external-cache probes, nonzero render start frames
must keep their absolute frame identity. The repaired worker path publishes
`frame_0120.bmp` for `render.start_frame=120`, and RayTracing imports VF3D and
water-sidecar manifests by exact `frame_index` first. The Linux PC proof
`ray-tracing--wtr67c-cache-direct-probe--20260624T173200Z--wtr67crt05`
verified that frame `120` selected `frame_000120.vf3d` and
`water_surface_000120.json`. Treat this as a plumbing/readiness proof, not a
water-quality proof: before longer direct-light or Disney-v2 reviews, inspect
the PhysicsSim water sidecars and require meaningful temporal height deltas.

The current volume-handoff smoke uses an explicit oblique inspection camera:

- `camera_zoom: 0.95`
- `camera_position: { x: -3.8, y: -7.2, z: 2.2 }`
- `camera_look_at: { x: -0.2, y: 0.8, z: 1.2 }`
- `light_intensity: 2.6`
- `light_radius: 0.10`
- `forward_decay: 220.0`
- `volume_scatter_gain: 3.0`
- `volume_step_scale: 1.0`
- `volume_tint: { r: 0.35, g: 0.65, b: 1.80 }`

That keeps the render in a readable side/oblique room view instead of the old
overhead floor patch, while giving the VF3D plume a readable blue-biased cue
for inspection without changing the persisted runtime scene or default render
behavior.

Volume density reconstruction is now trilinear instead of nearest-cell, and
`inspection.volume_step_scale` can tighten or loosen the single-scatter march
step relative to the default `voxelSize * 0.5` step. Values below `1.0`
produce finer sampling at higher cost.

`volume.debug_overlay=true` renders direct in-scene density contribution for
diagnosis. It uses the request camera, volume source, surface clipping, density
remap, step scale, and albedo/tint controls, but bypasses light-dependent
single scattering so sparse volume structure is visible even when production
lighting washes it out. Summaries read back this request state as
`volume_summary.debug_overlay_enabled`.

The volume material remap controls are request/readback-only inspection
overrides for reinterpreting the same VF3D scalar cache without regenerating
the simulation:

- `volume_density_scale` multiplies sampled scalar density before scatter.
- `volume_density_gamma` applies the post-scale density curve; values below
  `1.0` lift low-density smoke while values above `1.0` concentrate it.
- `volume_scatter_gain` scales single-scatter brightness.
- `volume_absorption_gain` scales extinction independently from scatter.
- `volume_opacity_clamp` caps the remapped density before scatter/extinction.
- `volume_albedo` / `volume_albedo_tint` accepts `{ "r": ..., "g": ..., "b": ... }`
  or `[r, g, b]` and takes precedence over legacy `volume_tint` when both are
  present.

Defaults preserve the previous direct-density behavior: density scale `1.0`,
density gamma `1.0`, scatter gain `1.0`, absorption gain `1.0`, and an
effectively unbounded opacity clamp.

## Detached Runner

Detached requests preserve their CPU budget, denoising override, and video
output settings. Temporal jobs enable durable tile checkpoints by default.
An explicit `"checkpoint": {"enabled": false}` disables those checkpoints
when a long render favors output throughput; completed frames remain durable,
but an interrupted active frame must be rendered again. Disney adaptive tiles
initialize their retained state before the first tile checkpoint, including
those that have not yet received a sample.

The detached runner is the first Phase 1 execution adapter for long-running
headless work:

```bash
ray_tracing/build/arm64/tools/cli/ray_tracing_job_runner submit --request <request.json>
ray_tracing/build/arm64/tools/cli/ray_tracing_job_runner status --job-id <job_id>
ray_tracing/build/arm64/tools/cli/ray_tracing_job_runner cancel --job-id <job_id>
```

The first detached trio chain now reaches this runner through
`bin/run_trio_detached_job_chain.sh`, which waits on detached PhysicsSim output
and submits the RayTracing render automatically once `scene_bundle.json` is
available. That chain now supports named profiles (`preview`, `review`,
`long_review`, `overnight`) and emits both `chain_status.json` and a
top-level `chain_summary.json` manifest with monitoring cadence metadata and
artifact roots. The manifest now also includes
`monitoring.automation_recommendation`, which is the preferred Codex-side
source of truth for heartbeat scheduling decisions.

Phase 2 truth additions:

- `status` now reconciles stored job state with live PID liveness, the render
  progress file, and the presence of a completion summary
- `status` now also classifies a live job as `stalled` when the process remains
  alive but `updated_at_utc` stops moving past the runner stall threshold
  (currently 15 minutes); this is observational only and does not kill the job
- in-flight render stage changes now also refresh `job_status.json` directly,
  so a long-running detached job no longer leaves the top-level status file
  stuck at the launch stage while `render_progress.json` advances
- `job_status.json` now carries:
  - `stage`
  - `overwrite_policy`
  - `requested_start_frame`
  - `requested_frame_count`
  - `effective_start_frame`
  - `effective_frame_count`
  - `frame_index`
  - `frames_completed`
  - `submitted_at_utc`
  - `started_at_utc`
  - `finished_at_utc`
- `render_progress.json` now carries both `stage` and `state`
- `render_progress.json` and `job_status.json` now also carry:
  - `temporal_subpasses_started`
  - `temporal_subpasses_completed`
  - `temporal_subpasses_total`
  - `completed_tiles_in_subpass`
  - `total_tiles_in_subpass`
  - `elapsed_seconds`
  - `estimated_remaining_seconds`
  - `progress_ratio`
- long single-frame renders can now show real temporal-subpass movement while
  still in `rendering_frame`, instead of looking frozen until the frame
  finishes
- active temporal subpasses can now also expose tile-level movement inside the
  current subpass, so status readers can distinguish "slow but moving" from a
  truly stale worker
- interpret those fields carefully:
  - `temporal_subpasses_started` means the renderer has entered that subpass
  - `temporal_subpasses_completed` means the subpass has fully committed
  - `started > completed` is expected while the current subpass is still
    actively rendering
- `started=6`, `completed=5`, `total=6` means "final subpass active", not an
  automatic failure
- a `stalled` state means "alive but no progress timestamp movement past the
  threshold"; if progress resumes on a later `status` read, the runner can move
  the state back to `running`
- current long single-frame limitation: tile-level progress is now available,
  but there is still no bounce-level or per-ray convergence signal beyond the
  active subpass/tile counters
- `--resume` is contiguous-frame resume, not blind overwrite:
  - if no target frames exist, it behaves like a normal clean submit
  - if a structurally valid contiguous prefix exists, the runner stages a
    reduced effective frame window and preserves normalized-time sampling
    through a `sampling` block in the staged request
  - each completed BMP is published through a same-directory temporary file,
    structural BMP validation, durable file sync, atomic rename, and
    parent-directory sync before it counts as resumable evidence
  - corrupt completed-frame candidates or later frames beyond a missing gap
    are rejected as recovery-required rather than silently skipped
  - if all requested frames are valid, submit fails with an explicit
    "nothing to resume" error
  - if outputs already exist and neither `--overwrite` nor `--resume` is set,
    submit fails explicitly instead of silently clobbering or guessing
- recovery-critical request, progress, status, envelope, report, and completion
  metadata are also published atomically through the durable-write boundary
- a job whose recorded worker PID is dead without a valid result summary is
  reconciled as `interrupted`; its status exposes
  `durable_frames_completed`, `resume_from_frame`, and `resume_available`, with
  stage `resumable` only when a valid completed prefix can be continued
- the runner does not automatically restart interrupted jobs. An operator
  submits the recovery attempt explicitly with `--resume`
- every runner invocation first scans the selected persistent jobs root without
  starting work. Interrupted jobs publish
  `ray_tracing_recovery_descriptor.json` with exact request and recovery-anchor
  SHA-256 values for coordinator reconciliation
- `RAY_TRACING_FLEET_JOB=1` makes `--resume` require the coordinator-provided
  `RAY_TRACING_RECOVERY_DESCRIPTOR_PATH`,
  `RAY_TRACING_RESUME_AUTHORITY_PATH`, and
  `RAY_TRACING_RECOVERY_WORKER_ID`; ordinary local explicit resume remains
  available without pretending to hold a fleet lease
- a fleet authority token is consumed once before renderer spawn and binds the
  source job, worker, lease, request/checkpoint digests, expiry, and output
  generation. The worker and all durable render commits fail closed when the
  output fence is revoked or superseded
- multi-subpass native `3D` jobs now enable schema-2 tile-batch checkpoints
  under `<output.root>/checkpoints`. Every quiescent committed batch preserves
  exact tile accumulation, sample counts, and adaptive state behind immutable
  input, runtime, renderer, and sampling identities
- `--resume` restores the newest compatible complete generation. If that
  generation is corrupt or incomplete, restore falls back to the prior
  committed generation; acceleration structures are rebuilt from the bound
  inputs
- direct request authors can opt in with `checkpoint.enabled`,
  `checkpoint.resume`, an absolute `checkpoint.root`, and bounded cadence via
  `tile_batch_size`, `max_tile_batch_size`, and `max_interval_ms`; see
  `docs/temporal_subpass_checkpoints.md`
- detached submissions now use RayTracing worker protocol v1 by default. The
  runner negotiates the sibling `ray_tracing_worker_runtime`, hashes the
  canonical staged request and selected renderer executable, and then launches
  the worker with a durable `worker_request.json`
- restart-visible `job_status.json` includes `worker_protocol_version`,
  `execution_mode`, `request_sha256`, and `renderer_build_sha256`; these are
  rehydrated from durable client/request state even when the renderer updates
  the shared status file
- `job_runner status` emits that merged record directly. A concurrent renderer
  status replacement cannot remove the negotiated mode or protocol identity
  from the CLI response after the reader has recovered them
- protocol events are immutable JSON files under `worker_events/`, including
  progress, full-frame dirty-region notices, completed-frame checkpoint
  references for completed frames and temporal-subpass generations,
  completion, interruption, and cooperative cancellation
- `RAY_TRACING_WORKER_PROTOCOL_MODE=direct` is the explicit compatibility
  fallback. A missing worker executable may also fall back; a present worker
  that fails negotiation or digest validation fails closed
- see `docs/worker_protocol_v1.md` for the complete version-1 message and
  compatibility contract

Submit creates:

```text
ray_tracing/build/agent_runs/jobs/<job_id>/
  job_request.json
  job_status.json
  render_progress.json
  stdout.log
  stderr.log
  pid.txt
  result_summary.json
  worker_capabilities.json
  worker_request.json
  worker_client_state.json
  worker_cancel.json
  worker_events/
```

The `worker_*` paths are protocol-mode artifacts. `worker_cancel.json` exists
only after cancellation is requested, and the explicit direct fallback does
not create a worker request or event directory.

The staged `job_request.json` is canonicalized to absolute scene/volume/output
paths so detached execution does not break on rebased relative paths inside the
job directory.

Detached path policy:

- `--jobs-root` overrides must be absolute existing directories. Relative,
  missing, or traversal-containing override paths are rejected.
- `job_id` values from generated jobs and outer bundles are treated as one safe
  path segment before any job path is built.
- direct request `output.root` remains an explicit caller-selected artifact
  root, but detached submit requires it to resolve to an absolute non-root path
  before rendering starts.
- outer bundle jobs continue to default artifacts to
  `<job_root>/output/artifacts`.

Successful detached renders may still print informational fallback lines to
`stdout.log` when legacy scene or animation config files are absent. Those
messages are non-fatal for request-driven runtime-scene renders. Trust
`job_status.json.state`, `render_progress.json.state`, and
`result_summary.json.diagnostics` over raw stdout wording.

When using heartbeat follow-ups, delete the automation once the job reaches
`completed`, `failed`, or `cancelled`, so finished jobs do not keep waking the
thread on later visits.

## Validation

```bash
make -C ray_tracing ray-tracing-render-headless
make -C ray_tracing ray-tracing-job-runner
make -C ray_tracing test-ray-tracing-render-headless-preflight
make -C ray_tracing test-ray-tracing-render-headless-image-export
make -C ray_tracing test-ray-tracing-render-headless-volume-handoff
make -C ray_tracing test-ray-tracing-render-headless-water-surface-handoff
make -C ray_tracing test-ray-tracing-job-runner-smoke
make -C ray_tracing test-ray-tracing-job-runner-policy
```

## Managed STL scene authoring

For project-owned STL intake, per-instance shading, safe rebuilds and relocation
proof, use [Managed mesh assets](managed_mesh_assets.md). Keep the complete
project for source rebuildability; a runtime-only render export is a different
guarantee. Existing loose scenes keep their current behavior.

## Typed surface capability discovery

See [M6 graph and producer tools](surface_material_m6_contract.md). The headless
renderer exposes `--surface-capabilities` and `--validate-surface-graph <file>`;
normal scene preflight additionally checks graph/source/geometry combinations.
# Isolated preparation checks

Run fixed preparation gates with a fresh, task-owned `BUILD_DIR_BASE`, for example
`make BUILD_DIR_BASE=/absolute/task/output/build run-headless-smoke package-desktop-self-test`.
Generated worker headers, fixture requests/jobs, and the unsigned app bundle stay
under that root. The fixtures select only executable tools in its
`toolchains/clang/<architecture>/tools/cli` directory; they never fall back to
canonical or legacy binaries. Unsafe, overlapping, symlinked, or occupied fixture
roots refuse before replacement. Existing canonical packages and render evidence
are preserved.

For a job-scoped `RELEASE_ROOT`, package preparation defaults its build/staging
root to `<RELEASE_ROOT>.work/build`; the archive remains at the declared release
root. Worker staging refuses occupied package/sidecar paths. Retained release
preparation must bind the selected roots through its owner before resuming;
changing an approved candidate requires supported source reconciliation.

### Continuous V3 volume reconstruction

The optional `participating_volume_v1.reconstruction` selector admits `box`
(default), `continuous`, or `adaptive`. `phase_tolerance` controls bounded angular
caching in continuous modes. See [normalization, admission limits, storage and
verification](spectral_volume_reconstruction.md) before selecting these modes.

See [V3 balanced edge-aware convergence](spectral_sampling.md) for ordinary render, saved-scene and headless sampling controls.

### Camera rays per spectral photon map

Converge-mode spectral requests accept `camera_samples_per_map: 1|2|4|8` inside
`spectral.sampling`. Bundles above one require `camera_sequence: "progressive"`.
For example, `{"mode":"converge","camera_sequence":"progressive",
"camera_samples_per_map":4,"max_passes":32}` reuses each map for four camera
rays while retaining a 32-map cap. One remains the compatible default. Map
counts and camera-ray counts are reported separately; the convergence thresholds
and checkpoint schedule are unchanged. See [sampling](spectral_sampling.md).


### Ordinary V3 linear films and scattering diagnostics

Ordinary V3 final jobs export unexposed float64 XYZ component films by default,
with saved `surface_path_v1.linear_components: false` opt-out. The native editor
and headless path share the export and scattering-order report. See
[the format, exact recombination rule, storage cost, and qualification](spectral_linear_components.md).
