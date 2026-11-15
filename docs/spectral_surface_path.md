# V3 ordinary spectral surface and fog transport

Ordinary local Disney V3 uses `surface_path_v1` for supported opaque indirect
lighting in native rendering, editor jobs and v2 headless requests. It reuses the evaluated native camera, planes, boxes,
triangle meshes, spectral source sampler, CIE observer and GGX sampling code.
It traces camera paths with direct-light sampling and multiple importance
sampling (MIS); it does not gather or blur photon maps. The existing glass
photon-map and two-plane calibration routes remain separate controls.

## Default quality and explicit reductions

Selecting this route activates the versioned `surface_path_v1` quality profile.
No separate switch is needed for indirect reflection, wavelength-aware response,
pixel jitter, direct-light MIS or scattering in a supplied medium. Omission of
`quality_profile` means `surface_path_v1`; explicitly naming it pins that policy.
Unknown profile names and malformed settings fail validation.

| Setting | Default | Explicit override |
| --- | --- | --- |
| Camera samples per pixel | 512 | `surface_path_v1.samples`: 1–8192 |
| Maximum surface interactions | 12 | `surface_path_v1.max_depth`: 1–12; 1 is direct-only surface transport |
| Camera sampler | `hashed_v1` | `scrambled_path_v1` is experimental; see the [sampling audit](spectral_error_diagnostics.md#camera-sampling-and-sample-attribution) |
| Medium sampler | `analog` | `surface_path_v1.medium_sampling: "split_first"` is experimental; see [bounded estimators](spectral_estimator.md) |
| Continuation roulette | `throughput` | `fog_survival` is experimental; see [qualification](spectral_estimator.md#november-13-continuation-roulette-after-fog-scattering) |
| Repeated-volume direction/light proposal | `phase` | `surface_path_v1.volume_direction_sampling`: `light_mis` is experimental and `light_only` is its angular-light-only ablation, and `rectangle_mis` tests a tighter solid-angle proposal; see [qualification](spectral_estimator.md#november-13-repeated-scatter-angular-light-and-direction-mis) |
| Seed | 19 | `surface_path_v1.seed`: 0–2147483647 |
| Exposure | 1 | `spectral.exposure`: positive finite value |
| Display | `spectral_soft_v1` | `spectral.display_transform: "legacy_clip"` |
| Displayed component | `total` | `direct`, `surface_indirect`, `volume` |

Russian roulette starts after the fourth path event with survival compensation.
A hard limit of 32 surface/volume events also applies. Finite depth and sample
limits are approximations. A finished fixed-budget image has no automatic
`converged` status. The request work guard is
`width * height * samples * max_depth * maximum_camera_branches <= 1200000000`; it refuses excess work
rather than silently reducing quality.

`spectral_quality`, `spectral_execution` and `spectral_surface_path` in the
summary report the effective profile, seed, exposure, display, sampling,
component and depth/sample counts. Keep the request, scene and mesh assets with
the summary when archiving a result. A seed alone cannot reproduce changed
geometry or spectra. The profile is deterministic for the same build and inputs;
cross-platform bit equality is not promised.

Selecting Disney V3 activates these defaults. Saved scene policy, native RGB
source/material adapters, editor job controls and shared diagnostics follow the
[transport backend contract](spectral_transport_backend.md). The glass caustic
estimator remains separate. Historical calibration and V2/Direct Light behavior
are unchanged.

## Scene and source contract

Use a static meter-scale runtime scene with `world_scale: 1`
and 1–32 native planes, rectangular prisms or triangle mesh instances. When
`materials` is supplied, each primitive requires one explicit material matched
by `object_id`; unused or missing materials fail. Omission uses the native RGB
approximation. Spectral materials override this route's native RGB
appearance. Mesh references use the normal native asset loader.

The path configuration may contain `light`, `materials`, `samples`,
`max_depth`, `seed`, `probes`, `path_trace`, `sample_audit`, `camera_sampler`,
`medium`, `medium_sampling`, `continuation_roulette`, `volume_direction_sampling`, `output_component`, `caustics`, `linear_components`. Omitted
`light` uses the native incident sources; a supplied explicit source requires
an empty scene light list. An explicit source is one of:

```json
{
  "type": "area",
  "origin_m": [0, 0.75, 1.96],
  "axis_u": [1, 0, 0],
  "axis_v": [0, -1, 0],
  "width_m": 0.65,
  "height_m": 0.65,
  "spectrum": {"continuum": [[380, 0.018], [780, 0.018]]}
}
```

An area source emits from the `axis_u × axis_v` side with constant radiance.
Axes must be nonzero and orthogonal. Alternatively use
`{"type":"sky_cap","axis":[0,0,1],"half_angle_deg":90,"spectrum":...}`.
The cap is uniform radiance inside its cone and dark outside; 180 degrees gives
an all-sphere reference environment. It is prescribed spectral illumination,
not an atmospheric sunset simulation.

Source `lines` are up to 16 `[vacuum_nm, integrated_radiance]` pairs, in
W/(m² sr) per line. `continuum` is 2–32 piecewise-linear
`[vacuum_nm, radiance_per_nm]` knots, in W/(m² sr nm). Both may coexist.
Wavelengths must be strictly increasing in 380–780 nm; strengths are nonnegative.
Elastic reflection/scattering preserves the sampled wavelength.

Materials use `{"object_id":"wall","model":"diffuse","reflectance":...}`.
Reflectance is a piecewise-linear curve with 2–32 knots including 380 and 780 nm,
bounded in [0,1]. The cosine-sampled Lambertian continuation multiplies throughput
by reflectance once. Optional `two_sided:true` models an opaque thin sheet;
the default is one-sided.

`model:"ggx"` additionally requires `roughness` in [0.03,1]. Its reflectance
curve supplies spectral Schlick F0. This is a single specular microfacet lobe,
not a complete coated dielectric, measured conductor or full Disney material.
The scene testbench includes a matte sphere and a neutral GGX reflector.

## Smooth glass and surface caustics

Add one smooth glass material to the saved `surface_path_v1.materials` list.
Its object must be a closed runtime mesh. The other objects in this profile
are matte planes. Use an explicit rectangular spectral area light and start
the camera and emitter outside the glass.

```json
{"object_id":"glass_ball","model":"smooth_glass"}
```

The material defaults to clear SCHOTT N-BK7. Set `"glass":"constant","ior":1.52`
for a wavelength-independent index, or set `"absorption_per_m":2.5` for
achromatic Beer–Lambert absorption. The camera and forward photons use the
same wavelength-dependent Snell and Fresnel interfaces, including internal
reflection. Camera radiance and photon power use their respective transport
weights. The editor loads the full optical mesh when applying this profile.

One map is built for the prepared render and is shared by the film. It covers
light from the area emitter through a chain of smooth specular events to the
first matte plane. The camera handles direct light and other paths, and
continues from the map contribution across diffuse bounces. A reverse camera
hit through the same specular chain is assigned to the map once. Each diffuse
plane receives photons by geometry and side, so the scene needs no receiver
list.

Omit `caustics` to use four batches of 16,384 photons, seed 19, 48 continuum
bands, and a fixed support radius equal to 3% of the glass mesh's largest
world-space extent. Optional controls are saved under `surface_path_v1`.

```json
{"caustics":{"passes":16,"photons_per_pass":16384,"seed":19,"bins":48}}
```

The budget is capped at 262,144 photons per map. The passes extend one
deterministic launch stream into one cumulative map. Camera samples are a
separate budget. The default reconstruction uses fixed spatial support and
source-weighted continuum bands, with separate exact line atoms. A fixed
budget has finite spatial and wavelength resolution. For a camera-only
comparison set `passes` to zero. A saved `radius_m` overrides the geometric
default.

Two independent saved sampling choices are available in the same
`caustics` object:

```json
{
  "caustics": {
    "passes": 16,
    "photons_per_pass": 16384,
    "seed": 19,
    "bins": 48,
    "photon_launch": "glass_guided",
    "camera_fresnel": "split_first"
  }
}
```

`glass_guided` mixes a proposal aimed at a sphere enclosing the glass with
the original cosine hemisphere sampler. Each photon carries the ratio of
physical cosine density to proposal density. Launches whose origin lies
inside the enclosing sphere use the cosine sampler. The map key includes the
proposal, and the weighted launch-power ledger is an estimate of emitted flux.

`split_first` traces both Fresnel branches when the camera ray starts by
hitting the smooth glass. Each branch carries its Fresnel probability, and
subsequent interfaces use roulette. This choice preserves the photon map key.
The transport report separates primary camera paths from extra split
continuations. The work limit counts up to two branches per camera sample.

`split_bounded` additionally splits up to three internal interfaces and avoids
roulette on split specular chains. It is experimental: see [weights, ownership,
branch limits and qualification](spectral_estimator.md). Combined with the
independent medium split it admits at most six branches per primary sample.

`split_camera_exit` reserves one additional exit split for an unscattered
camera path, with at most four branches when first-segment medium splitting
is enabled. It remains experimental: equal-time beam/edge gates still fail.

Omitting the choices selects `cosine` and `roulette`, which also work as
explicit values. File Save retains both choices for subsequent ordinary jobs.

`output_component` can display `caustic` or `caustic_bounce` for this profile.
The first is gathered on the first matte surface of a camera path. The second
is gathered after a prior matte interaction, such as light bouncing from a
focused spot on a floor onto a wall. The normal `total` combines both with
the ordinary path components. The transport report records the effective
radius, map identity, launch and deposit counts, power by band, glass events
and reverse source hits assigned to the map.

The bounded profile supports one closed smooth dielectric, one rectangular
area light, diffuse plane receivers, and an optional homogeneous exterior
medium. Rough transmission, nested dielectrics, and general mesh receivers
require additional qualification. The existing specialized photon instrument
continues to handle its documented configurations.

## Medium and diagnostics

Optional `medium` describes one homogeneous axis-aligned box:

```json
{
  "bounds_min_m": [-0.99, 0.01, 0.001],
  "bounds_max_m": [0.99, 1.99, 1.99],
  "sigma_a_per_m": [[380, 0.015], [780, 0.015]],
  "sigma_s_per_m": [[380, 0.25], [780, 0.25]],
  "g": 0.25
}
```

Coefficient curves cover 380–780 nm and are nonnegative. Pure absorption uses
Beer–Lambert transmission. Camera continuation uses exponential free flight,
analog absorption/scattering selection and Henyey–Greenstein directions,
including subsequent surface/volume interactions. The exterior medium stops
at the smooth dielectric boundary. Its coefficients do not apply inside the
solid. VF3D, nested media, curves and the existing atmosphere provider use
other routes.

With smooth glass, the forward pass attenuates the ballistic deposits using
wavelength-dependent extinction. It also deposits the probability-weighted
first medium scatter reached through a specular chain. The camera integrates
that volume estimate with the phase function and camera-side extinction. It
samples ordinary direct-lit fog and continued surface/volume paths separately.
Reverse emitter hits through a specular chain are assigned once to the
appropriate map. The fog cell width is 0.04 m by default. Override it with
`medium.caustic_cell_m` from 0.005 to 1 m. The existing surface radius and
spectral bin settings still control the surface map.

`output_component: "volume_caustic"` displays the focused fog contribution,
which the report includes as `volume_caustic_subset` inside `volume`. A
nonzero medium changes the weighted map key. The `ballistic_launch_key`
identifies the dry launch schedule for matched comparisons. Zero-density fog
preserves the dry surface map and image. Per-band diagnostics account for
medium absorption and scattering alongside receiver, escape and glass power.
The volume estimate uses cell averaging at finite spatial and spectral
resolution.

Up to eight probes record `{name,x,y}`. Linear, unexposed XYZ is partitioned into
`direct_xyz` (no medium scatter, no prior surface continuation),
`surface_indirect_xyz` (additional surface interactions, no medium scatter), and
`volume_xyz` (any path with a medium scatter). These sum to `total_xyz`.
`surface_order_Y` resolves non-volume contributions by additional surface count.
Changing displayed component leaves the sampled paths and probe totals intact.
The standard-error field is a sample-second-moment estimate; stratified spectral
samples and a single seed do not establish a confidence interval. Event counters
are diagnostics, not an emitted-watt conservation ledger.

## Reproduce

For a matched visible-source and selective-reflector matrix through saved scenes,
including same-white spectra and direct-shadow controls, see the
[spectral material response bench](spectral_material_response.md).

From the checkout, build the normal headless CLI and run:

```sh
make ray-tracing-render-headless
python3 tools/spectral_room_testbench.py \
  --cli build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless \
  --output /tmp/optic-room-demo --width 256 --height 192 --samples 1024
make test-headless-spectral-surface-path
```

Use the actual CLI path printed by the build on other toolchains/platforms.
Use `--view shadow`, `--wall orange|neutral|black`, `--component direct|surface_indirect|volume|total` and `--fog` for matched diagnostics. `--seed`, `--exposure` and `--max-depth` are explicit overrides. Keep camera, source, exposure, seed and depth fixed when comparing wall colors or components.

The tool stages a portable request, runtime scene and existing native sphere
mesh. It writes the summary, native BMP and lossless PNG. Choose a fresh output
root; existing case directories are preserved. No denoising or reconstruction
change is involved.

Qualification covers an independent four-dimensional colored-panel integral,
source scaling and spectral selection, black controls, analytical absorption,
multiple-scattering constant-radiance equilibrium, surface→fog and fog→surface
controls, broadband source × selective-material response, and twelve GGX
hemisphere references. These are scoped physical checks, not whole-engine
certification or proof that a fixed budget converges in arbitrary scenes.

The ordinary summary also exposes disjoint non-scattered indirect path classes
and a surface-light PDF consistency ledger; see
[the diagnostic contract](spectral_error_diagnostics.md#non-scattered-indirect-classes-and-sampling-balance).
These observational subsets do not change the beauty-film component schema or
require a scene opt-in. Experimental volume light proposals apply at volume
vertices, never at later surface vertices merely because the path contains fog.
