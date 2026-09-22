# T3 surface composition contract

Status: implemented, verified and adopted in Main Edit on September 22, 2026.
Implementation checkpoint `fab3972`; test-only quantization correction `808b2b6`.
This is source/native verification, not an installed release or human hands-on
acceptance.

## Ownership and adoption boundary

Canonical and vendored `core_authored_texture` are **0.8.0**. Following explicit
user approval, exactly five module files were synchronized with before/after
SHA-256 guards, and three prepared shared adoption notes were applied. Unrelated
canonical shared changes were preserved. Canonical unit and ASan/UBSan tests pass.

Main Edit advanced by guarded clean fast-forward from `ea6fcdf` to `fab3972`, then
`808b2b6`. App/native/headless/compiler targets rebuilt there. No package version,
release, installed app refresh, publication or remote deployment is part of T3.

Shared core owns typed graph preparation and evaluation. optiC owns retained
JSON, resource files and cache lifetimes, mappings, per-hit image sampling,
primitive-face selection, final material response, and editor commands.

## Retained schema

The graph remains in the object's retained material row at
`extensions.ray_tracing.authoring.object_materials[].surface_graph`.
Composition requires all three declarations:

- Graph `version: 2`, `required_capability: "optic.surface_composition_v1"`,
  and `color_space: "linear"`.
- The object's `extensions.ray_tracing.surface_mapping`.
- The object's `extensions.ray_tracing.surface_sampling`, using the existing
  version 1 sampling capability and channel declarations.

Graph version 1 with `optic.surface_graph_v1` retains its exclusive-source
contract and existing seven node kinds. The version/capability pair must match;
new image and roughness-mixture kinds are unavailable to version 1. Additional
`required_features` remain rejected. Capability discovery retains the original
v1 fields and adds a `composition` object describing this extension.

Composition adds three node kinds:

| Kind | Inputs | Resource / value semantics | Output type |
| --- | --- | --- | --- |
| `image_color` | None | `resource: "base_color"`; straight linear RGB | Color |
| `image_scalar` | None | `resource: "roughness"`, `"height"`, or `"base_color_alpha"` | Scalar |
| `roughness_mix` | `[a, b, mask]` | RMS material-mixture rule | Scalar |

The seven existing kinds remain `scalar`, `color`, `coordinate`, `noise3d`,
`triplanar_checker`, `multiply`, and `mix`. Existing color `mix` takes two colors
and a scalar factor. `multiply` remains scalar multiplication. Programs are
bounded to 32 nodes, with unique IDs, typed edges, no cycles, a required
`outputs.base_color`, and optional `outputs.roughness`.

An illustrative graph fragment, with matching object mapping and sampling
channel declarations supplied separately:

```json
{
  "version": 2,
  "required_capability": "optic.surface_composition_v1",
  "color_space": "linear",
  "nodes": [
    {"id": "paint", "kind": "image_color", "resource": "base_color"},
    {"id": "bare", "kind": "color", "value": [0.1, 0.15, 0.2]},
    {"id": "rest", "kind": "coordinate", "space": "object_rest", "scale_m": 0.2, "offset": [0, 0, 0]},
    {"id": "mask", "kind": "noise3d", "inputs": ["rest"], "seed": 7},
    {"id": "color", "kind": "mix", "inputs": ["bare", "paint", "mask"]},
    {"id": "rough", "kind": "image_scalar", "resource": "roughness"},
    {"id": "bare_rough", "kind": "scalar", "value": 0.8},
    {"id": "rough_mix", "kind": "roughness_mix", "inputs": ["bare_rough", "rough", "mask"]}
  ],
  "outputs": {"base_color": "color", "roughness": "rough_mix"}
}
```

Every referenced image channel must exist. `base_color_alpha` references the
base-color channel, rather than a separate file. Normal and height response
resources are still mutually exclusive. Existing conflicting material graphs,
texture stacks, authored-texture bindings, material-region bindings and
procedural solid asset references are rejected rather than silently combined.

## Independent image and procedural coordinates

All images on one composition object share **one object surface mapping** and
its sampling periods. Supported image charts are the existing planar mapping
(version 1) and named authored UV mapping (version 3). Axial mapping is outside
this composition contract. Image nodes do not accept XYZ coordinate inputs or
per-node mapping overrides.

Planar mappings retain their object-rest/world choice and physical dimensions.
Named UV mappings retain `uv_set_id`, `uv_scale`, `uv_offset`, and rotation; the
hit must carry the requested UV set. Image sampling uses the mapped coordinates
divided by `period_tiles`, with repeat addressing and the existing derivative
based mip filtering.

Procedural `coordinate` nodes independently choose `object_rest` or `world`,
with `scale_m` in meters and dimensionless offsets. Thus a named-UV image can
be mixed by a rest-space noise mask or a world-space checker without converting
the image chart into XYZ coordinates. One object's image chart remains shared
even when several procedural coordinate nodes use different spaces.

## Primitive regions

Composition may declare:

```json
"regions": [
  {"face_role": "front", "outputs": {"base_color": "paint"}},
  {"face_role": "back", "outputs": {"roughness": "bare_rough"}}
]
```

Each region overrides only its named channels. An omitted channel inherits the
default graph output; if default roughness is absent, the underlying material's
roughness remains the starting value. Regions reference the same node set and
resources; they do not create independent charts or image files.

Planes support `front` only. Rectangular prisms support `front`, `back`, `left`,
`right`, `top`, and `bottom` using the existing primitive face-role convention.
Roles must be unique and each override must name at least one supported output.
Mesh face-role overrides are rejected. Runtime selects the prepared per-face
program using the existing primitive-island helper and geometric normal.

## Linear color, alpha, roughness, and response

Base-color PNGs declare sRGB or linear encoding. Preparation converts sRGB to
linear and builds premultiplied-alpha mip data. Graph `image_color` safely
unpremultiplies the filtered sample, returning zero RGB for effectively zero
alpha. There is no implicit image-over-base operation in the graph. An explicit
`image_scalar` with `resource: "base_color_alpha"` supplies filtered alpha for
an ordinary color mix. Legacy M5 image-over-base behavior is unchanged.

Roughness image pyramids retain the second moment. `image_scalar` with resource
`roughness` returns `sqrt(max(0, filtered_second_moment))`. Height is linear data.
`roughness_mix` returns:

```text
sqrt((1 - mask) * a * a + mask * b * b)
```

This is an explicit RMS material-mixture convention. It does not claim exact
covariance filtering for spatially correlated source and mask fields. Existing
scalar arithmetic and version 1 roughness outputs retain their previous meaning.

Normal/height response runs once, after graph color and roughness. Procedural
coordinates and response preparation use the unperturbed shading normal when
available. Tangent-space normal application uses the selected image chart's
frame. Composition normal decoding centers the two tangent components exactly
at byte 128: `max(-1, (byte - 128) / 127)`; the third component uses the existing
`2 * byte / 255 - 1` conversion. Decoded vectors are normalized before mip
construction. M5 retains its original decode policy.

Filtered normal-vector length supplies coherence. Roughness is increased by
`sqrt(min(1, roughness² + normal_strength² * (1 - coherence)))` before the final
material update. A zero-strength or neutral composition normal does not replace
the shading normal. Height response uses filtered height gradients and the
existing meter-valued `height_m` strength. Degenerate tangent frames do not
apply a perturbation. Color remains linear through material evaluation and
receives the existing display conversion in preview.

## Resource preparation and cache

Graph image nodes reference semantic channels, not file paths in shared core.
The host samples immutable pyramids into `CoreSurfaceGraphInputs.nodes[]`, indexed
by compiled node index. Each sample must have the expected image-node kind,
validity flag, and finite components in [0, 1]. Missing or mismatched samples
fail. The old shared evaluation entry point delegates without external inputs;
it continues to execute old graphs and rejects graphs requiring image inputs.

Composition reuses T2's digest- and interpretation-keyed resource cache. The
composition normal decode policy has a distinct image-cache key, preventing
reuse of incompatible M5 normal pyramids. Composition does not bake a procedural
material pyramid: its procedural nodes evaluate in the shared bounded graph.
Consequently the procedural-pyramid-allocation test fault is not exercised by
composition's no-bake preparation path.

Resource lifetimes remain reference counted across current and staged bindings;
unused entries may be evicted under the existing unique-resource budget. File
pins are checked at preparation even on warm cache hits. Per-hit shading performs
no resource decoding, file IO, cache mutation, or allocation. Scene-relative
paths retain T2's explicit scene-parent context and path-confinement behavior.

## Editor command boundary

The retained `composition` preset creates the v2 graph and required declarations.
Mesh composition requires an existing named UV chart; supported primitives can
use the planar route. The material panel exposes graph editing and image
resources, with coordinate editing keeping image mapping separate from
procedural coordinate-node parameters.

Existing revision-guarded graph commands create/delete nodes, connect named
ports, and change default outputs. The region-output command changes one
primitive face/channel; clearing an override restores inheritance. Commands
validate the complete candidate and use the existing document rollback and undo
boundary. Deletion must account for region references as well as default outputs
and graph edges. Resource import/relink, pin validation, and immutable asset
ownership continue through the T2 document resource commands.

For composition, Reset mapping resets object image-chart placement while
preserving UV-set identity, physical tile sizes, sampling, and resources.
Procedural coordinate nodes remain authored in Sources. Reset source resets
known scalar/color/noise/checker parameters while retaining image references,
wiring, regions, and metadata. Version 1 reset behavior is unchanged.

## Verification receipts

All artifacts below are under the ignored local `build/surface_material_t3/`
root. The source checkpoint includes the test scripts; generated images and logs
are not shipped as release artifacts.

| Gate | Evidence | Result |
| --- | --- | --- |
| Full app, native host, headless and mesh compiler | `build-02.log`, `build-authoring-fix.log`, `build-final.log` | Builds pass; known linker alignment warning only |
| Shared graph unit suite | `core-tests.log` | Pass |
| Shared ASan + UBSan with warnings as errors | `core-sanitizer.log`, `core_graph_sanitizer` | Pass |
| T3 runtime and retained source | `acceptance-04/acceptance.json` | 14 cases, each with fresh-process reopen; 10 malformed-scene rejections |
| Real inspector workflow | `authoring-03-native/acceptance.json` | Preset, resources, all three node kinds, wiring, face override/inherit, Undo, Save/reopen pass |
| Portable composition and UV candidate | `composition-tools-01/acceptance.json` | 9 pure tests plus real compilation, candidate preflight/render and relocated bundle rendering pass |
| Graph diagnostic compatibility | `diagnostics-01/` | Pass |
| Existing material/recovery regressions | `regression-results.json`, `legacy-check.log` | All 11 M5 cases, four M6 source/space cases, T1/T2 authoring/cache, curved mapping, T0 failure recovery and eight frozen legacy render hashes pass |
| Documentation consistency | `docs-verify.log` | 93 documents scanned; no stale references found |

The runtime fixtures cover explicit alpha, named UV seams, mirrored/nonuniform
transforms, primitive face ray and preview values, independent reconstructed
rest/world positions and known image-input bytes. Constant RGB/RMS references
use a `2e-6` numeric tolerance; maximum measured adapter error is below `3e-16`.
Rest/world tests independently establish coordinate transport and image inputs;
they do not claim an independent implementation of the shared noise evaluator.

The original isolated directional renders exactly matched independently
calculated constant graph outputs. Fresh Main Edit acceptance exposed a single
RGB channel rounding difference in the variance case: float32 image moments
versus double-precision reference constants. Its comparison now requires maximum
8-bit error <= 1 and mean <= 0.0001; observed mean is 0.00000771605. All other
constant-reference comparisons remain exact. Flat normal, zero strength, constant height and zero height match the
absent-response render. Tilted/mirrored normal and height ramp change the
render, with separate independently calculated basis/gradient normal checks.
Mirrored flattened and TLAS/BLAS render bytes match. The variance fixture
independently checks the far-mip coherence term after graph RMS and stable
response on re-entry. Same-process M5/T3 switching verifies distinct decoder
cache entries and later reuse without further decoding. These fixtures do not
claim every intermediate mip transition or T4 secondary-ray quality.

The authoring test found and fixed a null optional-region-array access on first
face selection. It now cycles every empty prism scope before creating overrides.
A primitive preview adapter omission was also fixed: it now carries the linear
color flag and final world normal. PNG import uses the document command rather
than automating the operating-system chooser; its receipt explicitly records
`native_file_chooser: false`.

Reproduce the focused native acceptance after building:

```sh
python3 tests/integration/test_surface_composition_t3.py --output-root <new-output>/runtime
python3 tests/integration/test_surface_composition_authoring_t3.py --output-root <new-output>/authoring
```

Native editor tests require an available macOS display session. Sandbox-only SDL
initialization failures are retained separately from successful native runs.

Canonical shared synchronization and Main Edit adoption are complete; package
release remains separate. T4 is the next implementation slice:
secondary texture footprints/reference quality and measured interactive costs.

## Fresh Main Edit adoption receipts

`build/surface_material_t3_adoption/adoption.json` binds the fresh execution to
`808b2b6` (implementation remains `fab3972`). It records T3 runtime/reference
renders, all 14 cases with fresh reopen and 10 negative preflights, native T3
creation/edit/Undo/Save/reopen, T1 authoring, T2 resources/cache and T0 recovery.
The first variance-reference failure and its deterministic repeat remain in the
original `runtime/` and `variance-repeat.log`; the corrected full run is
`runtime-02/`. Numeric payload tolerance was unchanged. Flat/zero-response and
flattened/TLAS comparisons remain byte-exact. No renderer change was needed.
