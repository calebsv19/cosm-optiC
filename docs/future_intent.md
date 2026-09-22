# Ray Tracing Future Intent

Last updated: 2026-08-23

## Direction

Keep `ray_tracing` stable as a hybrid editor/runtime while treating the shipped native `3D` RGB ladder as the current product truth, not as a future experiment.

## Surface material mapping follow-up — September 21, 2026

The [independent post-M6 audit and plan](surface_material_post_m6_plan.md) now
controls the next-step order. T0 correctness, failure handling and Main Edit source
adoption are complete; see the [T0 contract](surface_material_t0_contract.md).
Next is T1 common authoring and complete editing of existing graph capabilities,
then T2 resource workflows/preparation and T3 composition. The broader authoring,
resource and performance findings remain planned work.


The adopted surface-material source completes the bounded M0–M6 implementation. The
[M4 explicit UV contract](surface_material_m4_contract.md) records per-corner UVs,
named sets, tangent frames, import/storage/cache transport and protected exact LOD.
Mapped geometry-field preview now evaluates the same compiled program as ray hits.
The original [parity diagnostic](material_viewport_parity.md) remains historical;
current mapped acceptance and unchanged legacy behavior have separate proof.
Main Edit source adoption now has fresh retained-workflow verification.

M5 adds explicitly declared filtering, color/data encoding and tangent/height
response; see the [M5 sampling contract](surface_material_m5_contract.md) for
measured cost and scope. The [M6 contract](surface_material_m6_contract.md) adds
typed graph editing, 3D/triplanar sources, bounded producer/unwrap tools and
executable capability reporting. T0 hardening and source integration are complete;
user hands-on acceptance remains separate from scripted native proof. Broader
sampling of graph/region/manifest bindings, transported secondary differentials,
measured anisotropic response and attribute-aware LOD remain explicit extensions.
The full-detail M5 fixture measures 102 ms per full-quality Material orbit frame
at 8,192 triangles; larger scenes and faster interaction need separate proof.
Preview-window redesign and release/version work remain separate.

## Near-Term Intent

`RT-MIRROR-1` is the active proof-first renderer fix plan. Preserve a
deterministic direct-versus-reflected and nested-mirror baseline before changing
behavior, then unify Disney-v2 reflected hits with the full path-vertex surface
evaluator, replace hidden triangle-count rough-reflection sample cliffs with an
explicit quality budget, and add reflection-aware reconstruction. The detailed
contract, phases, verification ladder, and completion gates live in
`docs/smooth_mesh_reflection_quality.md`.

1. Choose the next post-`I6` renderer lane cleanly.
   - The shipped native `3D` ladder is already closed through `Disney`.
   - New work should extend that ladder or its support systems intentionally, not reopen completed `I5`/`I6` slices.

2. Preserve deep-render export usability.
   - Keep absolute start-frame and resume-from-existing controls stable.
   - Keep frame numbering and path sampling locked to the same absolute-frame contract.

3. Keep menu/editor/runtime truth aligned.
   - Preserve current object color/material separation.
   - Preserve current RGBA object-authoring controls, object-level transparency/emissive multipliers, and the current runtime-scene persistence behavior.
   - Build on the object-centered Material editor mode instead of overloading the top-level object editor.
   - Expose the retained scene-placement Material view only when there is a real workflow for comparing object origin framing against in-scene placement.
   - Preserve the broader object-focused Material zoom range so texture work can be inspected close up and previewed from a distance.
   - Build on the texture-sampled filled triangle Material preview, all-face-group list, active-face texture kind and placement controls, copy-to-selected placement, generated-face persistence, and the v2 layered material stack by growing toward custom per-triangle/per-plane material texture placement.
   - Keep the current v2 stack contract as the intermediate representation for later node-graph authoring instead of making the node UI the first runtime format.
   - Preserve the split between geometry scene selection and optional atmosphere attachment.
   - Keep menu-render control helpers and scene-digest picking helpers as dedicated seams instead of collapsing them back into monolithic host files.
   - Preserve the new authored-bitmap texture roundtrip:
     - keep the object-bound manifest/PNG contract stable
   - keep authored bitmap binding metadata scene-relative instead of embedding texture bytes into runtime-scene files
   - build coexistence rules between authored bitmap faces and procedural overlays intentionally instead of letting the two routes drift
   - Treat imported STL objects as ordinary authored scene objects once they
     have been converted into `mesh_asset_runtime_v1` sidecars and referenced
     by `mesh_asset_instance`.
   - Build the next AI-agent scene-authoring material flow around the existing
     object `material_texture_stack`: prompts like a skull with a brushed metal
     base, rough layer, and grime overlay should compile into layered material
     stack entries rather than one-off STL-specific material fields.
   - Use `ray_tracing_material_preview_headless` as the bounded preview/tuning
     loop for imported mesh material presets before running expensive full
     native `3D` render proofs.
   - Preserve the now-routed per-object editor presentation contract: use the
     shared coherent LOD when available, otherwise draw an exact-transform
     12-edge AABB only when validated runtime or sidecar bounds exist. Keep
     Bounds/Wire/Solid/Material controls, triangle picking, native GPU mesh
     submission, and per-frame-slot cache without moving renderer, material,
     camera, overlay, final-geometry, fallback policy, or BVH authority into
     shared core.
   - Complete hands-on packaged skull/dragon visual acceptance when native UI
     capture is available, then tune only renderer-local feature thresholds or
     appearance if the proof exposes a concrete readability defect.

4. Build the next native `3D` lighting pass around an authored lighting model
   rather than one-off inspection overrides.
   - Keep `environment_light_mode`, `ambient_strength`, `top_fill_strength`,
     and `environment_brightness` compatible with existing headless requests.
   - Add a clearer lighting preset/authoring layer for scene work, including
     readable ambient contribution and optional sky-tinted background/fill
     behavior.
   - Treat background color/brightness and ambient contribution as related but
     separately inspectable controls so headless summaries can explain why a
     scene is bright, dark, or sky-tinted.
   - Preserve the current direct-light, diffuse-bounce, material, emission,
     Disney, and Disney-v2 route identities while centralizing shared
     environment-light normalization.

5. Finish production photon mapping through explicit product boundaries.
   - Treat completed PPM-23 transport, nested media, attenuation, and
     fail-closed acceptance as current implementation truth.
   - Implement PPM-24 estimator selection and convergence/rejected-energy
     diagnostics before visual tuning or default promotion.
   - Connect existing caustics and production photon mapping through one
     explicit user-facing mode contract and deterministic output matrix; keep
     simultaneous operation disabled initially unless independent accounting
     proves a clear use case.
   - Decide direct Disney v2 photon/beam-map sampling in PPM-25 only after the
     cache-conversion bridge and estimator contracts are measurable.
   - Keep production/default promotion in PPM-26 behind numeric, visual, cost,
     lifecycle, and regression acceptance.

6. Preserve bounded VF3D / `physics_sim` ingestion while renderer quality work
   proceeds.
   - Existing volume and water sidecar ingestion is current behavior, not
     deferred intent.
   - Avoid broadening ingestion formats until photon/caustic estimator and
     product-mode boundaries are stable.

## Structural Intent

- Keep native `3D` renderer work seam-driven:
  - tier docs remain under the active integrator lane
  - completed sub-slices should archive instead of accumulating under active
- Keep large host files focused on orchestration:
  - `src/app/animation.c`
  - `src/render/pipeline/ray_tracing2.c`
  - `src/ui/menu/sdl_menu*.c`
- Keep verification families decomposed when the behavior boundary is already clear:
  - config persistence/export
  - prepared native `3D` parity/scatter behavior
  - runtime scene `3D` geometry builder vs trace contracts
- Keep support-layer changes shared across the shipped native `3D` ladder when the behavior is truly common:
  - temporal accumulation
  - sampling
  - denoise
  - preview/update behavior
- Keep procedural texture work and the authored-bitmap manifest reader app-local until a stable cross-app material graph/schema exists; current layered material stacks and bitmap face bindings are native `3D` renderer behavior, not shared-core contracts.
- Keep the Material editor's object-wide layer stack as the fallback while custom group topology, durable group controls, and per-layer face overrides catch up to the generated-face placement schema.
- Keep imported STL mesh material expansion object-wide first. Per-face or
  semantic-region material authoring should wait until the imported mesh
  pipeline has stable group/selection metadata beyond raw triangle ordinals.

## Non-Goals

- No reopening of closed grayscale-to-RGB migration slices.
- No broad rewrite that merges active renderer work with unrelated editor polish.
- No ingestion-expansion push ahead of the next internal renderer proof boundary.

## Private Planning Reference

Detailed execution history and archived support lanes live in:
- `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/ray_tracing/`
