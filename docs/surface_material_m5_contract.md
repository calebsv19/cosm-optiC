# M5 sampling and response fidelity

M5 adds an explicitly requested sampling path to the M1 planar, M2 axial and M4
named-UV mappings. It prepares bounded linear/data mip pyramids, measures primary
ray and viewport footprints, and evaluates tangent normal or physical-height
response in the same material adapter. Documents without the new declaration
retain their prior sampling and color interpretation. This is an isolated source
checkpoint; Main Edit adoption, installed builds and publication are separate.

## Declaration and supported sources

Place this next to `surface_mapping` in an object's `extensions.ray_tracing`:

```json
"surface_sampling": {
  "version": 1,
  "required_capability": "optic.surface_sampling_v1",
  "filter": "trilinear",
  "address": "repeat",
  "color_space": "linear",
  "period_tiles": [8, 8],
  "normal_strength": 1,
  "channels": {
    "base_color": {
      "path": "/absolute/path/base-color.png",
      "sha256": "<64 hexadecimal characters>",
      "color_space": "srgb"
    },
    "roughness": {
      "path": "/absolute/path/roughness.png",
      "sha256": "<64 hexadecimal characters>",
      "color_space": "data"
    },
    "normal": {
      "path": "/absolute/path/normal.png",
      "sha256": "<64 hexadecimal characters>",
      "color_space": "data"
    }
  }
}
```

The enclosing mapping and brick/solid material stack are required. `channels` is
optional; an empty or absent channel object filters the procedural stack alone.
The first sampling capability supports one object-level chart and stack. It
rejects M3 named/region bindings, material graphs, authored-texture manifests and
procedural solid material references combined with this declaration. Those M3/M4
features remain supported by their existing paths without this declaration.
Broader graph/source sampling is an explicit follow-up, not an automatic fallback.

`period_tiles` contains two integers from 1 through 32. This is an intentional
repeating chart, not an infinite procedural surface: mapped coordinates divided
by these periods address the textures and procedural bake. Axial U must equal
the mapping's integer circumference repeat count. Mapping offsets/rotations and
UV-set matching keep the M1–M4 meanings. A 256×256, eight-channel bake represents
the procedural chart; detail smaller than that bake's texels is not reconstructed.
Images have independent power-of-two dimensions from 1 through 1024, including
rectangular images. PNG files must be at most 16 MiB, and the scene's prepared
pyramids must fit a 128 MiB budget. The current adapter uses absolute SHA-256-pinned
paths; portable asset bundles and UDIMs are outside this capability.

The inspector reports `Sampling: filtered / linear color`. Existing mapping
controls edit the retained declaration through normal document commands. Undo,
redo, save and fresh-process reopen preserve channel pins and producer metadata.
Image authoring and channel-selection controls are not added to the UI in M5.
Unknown required capabilities, unsupported combinations, invalid parameters,
missing/stale images and invalid encoding declarations fail validation.
Preparation failure clears the incomplete prepared state and aborts the load.

## Filtering, encoding and footprint policy

The shared `core_authored_texture` 0.6.0 API owns immutable float pyramids,
repeat/bilinear lookup, trilinear interpolation, conservative major-axis LOD,
sRGB decoding, tangent normal conversion and physical bump math. The app owns
JSON/PNG IO, resource pinning/budgets, inverse object frames, footprints, material
binding and prepared-state lifetime. No image IO or source rebaking occurs during
shading or orbit.

Base color declares `srgb` or `linear`; the transfer function is applied before
mip construction. Alpha remains linear, and color is premultiplied before
filtering and composited over the procedural stack. Embedded PNG gamma/profile
metadata does not override the declaration. Roughness, normal and height require
`data`; their values never pass through the color transfer function. Data-channel
alpha is ignored. Roughness uses RMS filtering (`sqrt(mean(r*r))`). Normal vectors
are decoded and normalized before mip construction; filtered vector length adds
a bounded normal-variance term to roughness, scaled by normal strength squared.
This is a stability approximation, not a measured microfacet-distribution fit.

Primary camera rays carry adjacent pixel directions. Intersections project those
rays onto the geometric tangent plane. The viewport computes world-space pixel
derivatives from its projected triangle, then uses the same mapping and sampler.
The major singular value of the texel-space footprint selects LOD. This isotropic
filter deliberately blurs the narrow axis at grazing angles; it is not an
anisotropic/EWA filter. A grazing footprint crossing the tangent-plane horizon
uses the coarsest level. Secondary offset rays currently use the chart average
because transported secondary differentials are not implemented. This is stable
but loses reflected/refracted texture detail. Point-query APIs without a pixel
footprint use level zero. These policies are explicit and covered by tests.

Viewport inspection lighting converts linear response to sRGB display bytes.
Final rendering retains its existing scene lighting and tone curve. Matching
material values does not imply byte-identical viewport and final images.

## Surface response

Normal maps use positive-Z tangent space, with RGB decoded from [0,1] to [-1,1].
Their vectors must be nonzero and have nonnegative Z. `normal_strength` defaults
to 1 and is bounded to [0,2]. Zero strength disables normal variance as well as
XY tilt. The frame follows increasing chart U, is orthogonalized against the
unperturbed shading normal, and derives handedness from increasing chart V.
Mirrored charts, negative object scales, UV rotation and nonuniform transforms
therefore preserve the intended direction. Repeated material resolution retains
the original normal rather than applying the tilt repeatedly. Disney secondary
vertices consume the prepared shading normal as well as primary/direct shading.

For height response, replace `normal` with a `height` image entry and add
`"height_m": 0.1`. Height is the red data channel times that meter amplitude;
`height_m` defaults to zero and is bounded to [0,1]. Normal and height channels
are mutually exclusive. Height gradients are taken at the selected mip level
and transformed with the dual surface basis in world meters. This changes
shading only, never topology, silhouette or intersection position.

Degenerate UVs retain color/data coordinates and disable tangent response.
Axial poles retain the M2 fade to the object's base material and do not fabricate
a frame. Normals crossing the geometric hemisphere are rejected in favor of the
base normal. The M4 derivative basis is not MikkTSpace. Directional lighting
follows the chart-aligned normal; a general anisotropic/brushed-metal BRDF is
not introduced by this phase.

## Preview detail, cost and verification

UV assets retain all source triangles in settled and interactive preview LOD.
Explicit UV/sampling scenes use the full mesh loader: the ordinary 1 MiB preview
skip cannot silently remove a required chart. This also makes missing required
assets fail in the native editor. Memory/load cost can therefore increase for
large attributed scenes; attribute-aware reduction remains future work.

Run from the repository root, using fresh output directories:

```sh
make -j4 all scene-editor-workspace-visual-test ray-tracing-render-headless smooth-mesh-runtime-compile-tool
make -C third_party/codework_shared/core/core_authored_texture test
python3 tests/integration/test_surface_mapping_m5.py --output-root build/m5-proof
python3 tests/integration/test_surface_sampling_curved_m5.py --output-root build/m5-curved-proof
```

The native fixtures cover mirrored/rotated/degenerate UVs, sRGB versus linear
color, premultiplied alpha, RMS roughness, normal variance/zero strength, height,
procedural-only filtering, primary/horizon/secondary footprints, independent frame
oracles, ray/preview material parity, document round trips and an 8,192-triangle
exact-LOD orbit. Curved fixtures cover a planar primitive, cylinder and sphere,
including pole fallback. Rendered flattened/TLAS routes are compared byte for
byte. Negative preflights cover capability, encoding, image pins/dimensions,
response conflicts and aggregate preparation budget. Shared math is also tested
with AddressSanitizer and UndefinedBehaviorSanitizer.

M0–M4 regressions and the eight frozen legacy fixtures remain separate gates.
Acceptance outputs record channel errors, motion variation, prepared bytes,
orbit time and rebuild counts. Performance measurements qualify those fixtures
on the tested host; they do not establish a general real-time mesh budget.

Next: M6 expands typed graph editing, source families (including 3D/triplanar),
producer/unwrap tools and capability reporting. Broader sampling combinations,
secondary differential transport and attribute-aware LOD need their own bounded
proofs. See [the roadmap](surface_material_mapping_plan.md).
