# Material viewport parity diagnostic — 2026-09-20

## Result

Material display is currently an approximate appearance aid, not a reliable
final-render material-layout preview. The brick floor in the earlier viewport
proof was a temporary in-memory material variation; it was not a saved-scene
viewport/final-render parity proof. This diagnostic supplies that missing test
using controlled saved scenes rather than claiming the old screenshot matched.

Eight isolated cases were evaluated: a centered 4 x 2 plane with baseline brick,
scale 2, U offset 0.23, V offset 0.19, rotation 0.4; a 4 x 2 x 1 prism with
uniform placement and with independent face placements; and an editor mutation,
save, fresh-process reopen case.

All eight native viewport runs and all eight 640 x 480 single-frame
`direct_light` renders completed. This is successful diagnostic execution,
**not a material parity pass**. Application rendering code was not changed.

## Evidence and limits

Artifacts: `build/editor_ui_recovery/material-parity-02/` (ignored local output).
Every case contains its actual saved scene, request, native `viewport.ppm`,
ray-hit/pre-cache albedo projections, logs, and completed renderer summary/BMP.
`diagnostic.json` records metrics and changed-sample counts.

- Baseline plane: 65,536 surface samples; 81.4758% differ by more than 3/255
  in at least one RGB channel. Mean absolute RGB error is 19.648814/255.
- Scale, U offset, V offset, and rotation each change both albedo paths and
  native viewport pixels. Their resulting patterns do not agree across paths.
- Scale 2 and offsets 0.23/0.19 applied through the editor control mutation
  adapter survive authoring persistence and fresh-process reopen exactly;
  the full sampled report also matches. This does not test mouse-drag ergonomics,
  undo/redo, or every material parameter.
- Prism: all six faces receive 65,536 ray samples. The control case leaves
  face group 0 unchanged. Overrides change each of the other five final-render
  face projections, but change zero viewport albedo samples and zero native
  viewport pixels. Face-specific placement is therefore not previewed.
- Independent mirror/wood/brick material families on individual faces are
  **not established by this test**. The placement override structure changes
  placement and texture parameters, not arbitrary per-face BSDF ownership.
  A separate authored-texture/face-intent path exists in the renderer; it needs
  its own saved-scene, editing, and viewport parity proof.

The albedo comparison evaluates the same physical surface locations using the
viewport's ideal pre-cache object evaluator and actual runtime ray-hit material
payloads. It excludes lighting, tone mapping, interpolation, and raster aliasing.
It is not a pixel comparison of final shaded screenshots. Native captures and
separate headless renders provide the additional end-to-end visual evidence.
Their cameras and lighting differ and should not be compared pixel-for-pixel.
The diagnostic is limited to axis-aligned primitive plane/prism fixtures.

## Source explanation

`scene_editor_mesh_preview_surface.c` normalizes primitive/mesh positions into
object-space 0..1 box-projection UVs. `scene_editor_viewport_material.c` bakes an
object-level 128 x 128 material grid using the object seed (or index + 1).

`runtime_material_payload_3d.c` resolves primitive face islands, grounds their
coordinates through `scene_editor_material_face_metrics.c` using physical face
dimensions/orientation, uses a different face seed, and applies face overrides.
Thus shared procedural texture functions alone do not guarantee equal results:
the inputs differ. The viewport also uses analytic studio lighting rather than
scene light transport, explaining additional expected appearance differences.

## Next implementation and acceptance order

1. Establish one surface-material query used by both paths: stable object and
   face identity, local surface coordinates, physical scale, UV orientation,
   placement, procedural seed, effective layers and face overrides. Resolve
   shared-library ownership before extracting any reusable API. Keep editor
   input adapters out of renderer policy. Do not change saved scale semantics
   merely to make the viewport resemble its current image.
2. Feed this query into the viewport cache. Cache by material revision and
   surface mapping/face identity as necessary; camera changes must not rebake
   object-space patterns. Retain fast camera movement and bounded resolution.
3. Turn these diagnostics into acceptance gates. At identical surface points,
   require matching base color, roughness, reflectivity, opacity and material
   identity to numerical tolerance. Separately set an image/filtering tolerance
   for the bounded viewport cache. Test cache invalidation after edits and
   zero stale frames after undo, redo, save and reopen.
4. Expand geometry tests to translated/rotated/nonuniformly scaled prisms,
   meshes, UV seams, mirrored transforms, face selection and duplicated objects.
   Use a six-face prism with unique colors first, then different material
   families, and ensure editing one face does not alter the other five.
5. Validate optical response separately: diffuse reference, roughness sweep,
   mirror reflecting a known colored card, brushed-metal orientation under a
   moving light, and glass transmission/IOR. Use a transport-capable final
   integrator for reflection/transmission tests. A direct-light brick test
   does not prove physically correct mirror or glass behavior. Describe the
   viewport's supported lighting approximations explicitly.

The existing Preview window is outside this diagnostic and implementation plan.
This concerns the shared Material display mode available in every workspace.

## Reproduction

From the checkout root (requires a native graphical desktop session):

```sh
make -j4 BUILD_TOOLCHAIN=clang scene-editor-workspace-visual-test ray-tracing-render-headless
python3 tests/integration/test_material_viewport_parity.py \
  --output-root build/editor_ui_recovery/material-parity-new
```

The output directory must not exist. Scene/editor writes are confined to new
fixture copies. The harness invokes the same editor placement mutation and
persistence adapters as the controls, captures the native renderer, and verifies
that the separate headless renderer reports one completed output frame.
