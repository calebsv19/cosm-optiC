# optiC Current Truth

Last updated: 2026-05-24

## Program Identity
- Repository directory: `ray_tracing/`
- Public product name: `optiC`
- Internal/repo/runtime identifiers still use `ray_tracing` and `RayTracing`
  in launcher, log, binary, and source-level contracts where required
- The compiler-units rollout now has its first explicit RayTracing customer
  lane:
  - dual-toolchain `clang-build` / `fisics-build`
  - explicit `PACKAGE_TOOLCHAIN` package source selection
  - sema targets on:
    - `src/import/runtime_scene_bridge.c`
    - `src/import/runtime_scene_bridge_authoring.c`
    - `src/app/animation_fluid_scene.c`
    - `src/render/runtime_light_emitter_3d.c`
    - `src/render/runtime_volume_3d_sampling.c`
    - `src/render/runtime_native_3d_sampling.c`
    - `src/render/runtime_volume_3d.c`
    - `src/render/runtime_volume_3d_integrate.c`
    - `src/render/runtime_volume_3d_scatter.c`
    - `src/render/runtime_direct_light_3d.c`
    - `src/render/runtime_visibility_3d.c`
    - `src/render/runtime_camera_3d_rays.c`
    - `src/render/runtime_ray_3d.c`
  - current honest units boundary covers:
    - runtime-scene object positions and primitive dimensions
    - authoring path coordinates, light/camera seed positions, focus target,
      scene bounds, and construction-plane offset
    - fluid-manifest world-fit camera placement, default orbit extents, and
      import placement interpolation into world space
    - emitter radius, falloff distance, and hit-distance attenuation checks
    - volume bounds clip distances and world-position-to-voxel normalization
    - volume grid voxel size, bounds extents, and timestep metadata
    - volume transmittance march-step and segment-length semantics
    - volume single-scatter light-path distance and sample-step semantics
    - direct-light light-path distance and attenuation-falloff semantics
    - visibility segment-distance, light-distance, and target-distance semantics
    - camera near-plane and projected camera-depth semantics
    - ray offset epsilon and first-hit distance semantics
  - support-only sema coverage now also records:
    - native `3D` unitless subpass/jitter sampling in
      `src/render/runtime_native_3d_sampling.c`
  - `extensions.ray_tracing.authoring.light_settings.radius` was checked and
    intentionally left on its existing renderer-local scalar contract
- Primary runtime entry:
  - `src/app/animation.c` (`main()` delegates through `ray_tracing_app_main(...)`)
  - wrapper shell: `include/ray_tracing/ray_tracing_app_main.h`, `src/app/ray_tracing_app_main.c`

## Current Shipped State
- Legacy `2D` rendering and editor flows remain present.
- Native `3D` runtime ladder is shipped through:
  - `Direct Light`
  - `Diffuse Bounce`
  - `Material`
  - `Emission / Transparency`
  - `Disney`
- The shipped native `3D` direct-light tier now averages a small deterministic
  finite-radius light sample set, so penumbra and transparent-shadow softening
  track authored `light.radius` instead of behaving like a strict point-light
  shortcut.
- Native `3D` output is RGB-aware through the full shipped ladder.
- Runtime UI text measurement now routes through one guarded font contract:
  - only fonts still owned by the live runtime font cache are considered usable
  - HUD/menu measurement and line-height helpers no longer call `SDL_ttf`
    sizing APIs directly on unchecked font pointers
  - when a stale font handle is encountered, text draw/measure now fails closed
    instead of dereferencing the dead handle
- Native `3D` support layers now include:
  - tile preview
  - dirty-rect preview updates
  - shared-frame preview reconstruction
  - explicit `Nearest` / `Bilinear` upscale policy selection
  - tile occupancy culling
  - temporal accumulation upgrades
  - stratified + blue-noise sampling support
  - Disney-only denoise
  - optional top-fill lighting
- Native `3D` low-resolution presentation no longer has one hardcoded upscale policy:
  - preview dirty-rect redraw and completed-frame resolve both route through
    `runtime_native_3d_preview_reconstruction.*`
  - the persisted/menu/runtime contract now exposes `upscaleMode3D` through a
    renderer-controls button and the runtime `U` key cycle
  - `OFF` preserves a raw non-smoothed low-resolution present path
  - `Nearest` preserves crisp pixel structure at low render scales
  - `Bilinear` keeps the smoother reconstruction path available as an
    explicit mode instead of an accidental default
- Native `3D` material payloads now support opt-in procedural material texture overlays:
  - `textureId=1` rust overlay
  - `textureId=2` fog overlay
  - per-object texture pan/scale/strength fields persisted on scene objects
  - per-object node-ready texture parameters persisted on scene objects: pattern mode, coverage, grain, edge softness, contrast, flow, color depth, surface damage, and seed
  - path-traced/native hit anchoring currently uses triangle barycentric hit coordinates so overlays remain attached to surfaces as view/light state changes
  - generated Material face-group texture overrides now feed the native `3D` material payload when a runtime hit carries a local triangle ordinal, so rust/fog overrides and their parameter blocks can change actual BSDF roughness, reflectivity/specular weight, diffuse response, color, and transparency in the simulation path
- Native `3D` material payloads also support the first v2 layered material texture stack:
  - bounded ordered stacks carry durable layer ids, base/overlay roles, blend modes, placement, procedural parameters, enabled state, opacity, and material influence fields
  - base layer kinds include solid, brushed metal, wood, brick, concrete, and stone in the stack/evaluator contract
  - overlay layer kinds include rust, fog, grime, and oil in the current Material editor surface, with scratches and edge wear reserved in the enum contract
  - runtime scene authoring save/load persists `material_texture_stack` data additively beside legacy `procedural_texture` compatibility fields
  - the Material preview and native payload path both evaluate through `RuntimeMaterialTextureStackEvaluatePlacedUV(...)`, so focused editor preview and simulation material response share the same stack math
- Native `3D` material payloads now also support object-bound authored bitmap texture sets for the current texture-authoring roundtrip:
  - `drawing_program` exports separate per-face RGBA PNG files plus one JSON manifest
  - `ray_tracing` reads that manifest through app-local authored-texture import code
  - bindings are restored by stable `object_id` under `extensions.ray_tracing.authoring.object_materials[*].authored_texture`
  - the same `extensions.ray_tracing.authoring.object_materials[*]` lane now also
    supports object-wide review-time overrides for:
    - `object_color`
    - `alpha` / `transparency`
    - `reflectivity`
    - `roughness`
    - `emissive_strength`
  - runtime defaults now treat omitted `emissive_strength` as `0.0` rather than
    implicitly emissive
  - authored transparent objects should use `material_id = 5`
  - `material_id = 4` remains the explicit emissive preset lane
    - object-wide procedural texture shorthand:
      - `texture_id`
      - `texture_strength`
      - `texture_scale`
      - `texture_offset_u`
      - `texture_offset_v`
      - `texture_seed`
      - `texture_pattern_mode`
      - `texture_coverage`
      - `texture_grain`
      - `texture_edge_softness`
      - `texture_contrast`
      - `texture_flow`
      - `texture_color_depth`
      - `texture_surface_damage`
  - the first non-raytraced material-preview service is now shipped through
    `ray_tracing_material_preview_headless`:
    - loads the same compiled runtime scene contract as the render CLI
    - resolves one target object by `object_id` or `scene_object_index`
    - evaluates the same object-wide procedural/material stack without running
      the native `3D` integrators
    - writes a deterministic BMP swatch or contact-sheet preview plus an
      optional JSON summary of effective per-variant values
  - runtime-scene authoring persistence now writes authored-texture manifest paths back relative to the runtime-scene file when the manifest lives beside that scene, so reopen does not depend on a machine-local absolute export path
  - runtime-scene authoring persistence now writes explicit cleared authored-texture state (`null`) when an object no longer has a bound authored texture, so overlay merge removes stale bindings instead of preserving them by omission
  - invalid authored-texture binds now record a bounded object-local invalid state instead of silently collapsing to unbound
  - the selected-object Material editor now surfaces that invalid state explicitly with:
    - attempted manifest path
    - bounded validation failure reason
  - runtime-scene authoring persistence now preserves invalid authored-texture entries by writing manifest path plus binding mode instead of erasing them to `null`
  - reopen now replays persisted invalid authored-texture entries and resurfaces the same invalid state when validation still fails
  - supported face-role mapping is explicit for planes and generated rectangular-prism faces
  - hit-time payload resolution now samples authored bitmap color and alpha before procedural fallback when a bound face is hit
  - the authored/procedural coexistence rule is now explicit:
    - authored bitmap sampling establishes the substrate/base color and alpha response for the hit
    - procedural base-role texture-stack layers do not replace that authored substrate
    - procedural overlay-role texture-stack layers may still modulate above it
    - legacy rust/fog `textureId` overlays remain compatible because they still enter through overlay-role evaluation
  - runtime-scene authoring persistence retains the manifest path/binding metadata rather than embedding image payload bytes
  - the authored-texture manifest reader now supports the first narrow dual-lane runtime contract:
    - base-lane faces load from `base_surfaces` when present, otherwise fall back to legacy `surfaces`
    - overlay-lane faces load from optional `overlay_surfaces`
    - top-level `overlay_material_intent_kind` is now preserved for runtime material-response mapping
  - dual-lane authored sampling now behaves as:
    - authored base lane establishes the substrate/base color and alpha response
    - authored overlay lane samples independently by the same face-role mapping
    - overlay alpha blends over the authored base before the remaining downstream material policy runs
    - overlay material intent is now mapped through the existing runtime texture-stack taxonomy so `grime`, `oil`, `rust`, and `fog` can modify BSDF/material response instead of acting as pure color decals
  - authored-texture manifests now also expose finer-grained per-surface material-intent metadata to the runtime:
    - `layer_material_intent_stable_ids` is read from manifest schema `v5`
    - runtime face metadata now derives bounded face-level base and overlay material-intent stable ids from those exported values
    - for the current lane-faithful export transition, the bounded face-metadata getter now merges overlay-face overlay intent into the returned face metadata instead of assuming overlay intent still appears in base-surface semantic arrays
    - explicit per-face summary fields are now the primary runtime contract:
      - `base_material_intent_kind`
      - `overlay_material_intent_kind`
    - current runtime compatibility behavior is now explicit:
      - when those summary fields are present, loader and payload logic prefer them over the raw `layer_material_intent_stable_ids` array
      - when they are absent, older manifests still fall back to the previous bounded one-per-family derivation from the raw array
    - the durable authored-texture runtime rule is now explicit:
      - one effective base/substrate intent per face
      - one effective overlay/environment intent per face
      - raw per-layer `layer_material_intent_stable_ids` arrays remain auxiliary/editor-facing metadata and are not a richer mixed-material runtime BSDF contract
    - hit-time payload resolution now consumes that richer metadata narrowly:
      - face-level base/substrate intent may modulate BSDF response after authored base-lane sampling
      - face-level overlay/environment intent may modulate BSDF response after authored overlay-lane sampling
      - face-level overlay intent now takes priority over the older coarse binding-wide `overlay_material_intent_kind`, which remains only as fallback
  - authored-texture manifest validation is now explicit instead of permissive:
    - bind only accepts explicitly allowed schema versions
    - schema `v5` manifests must provide a valid `emitted_output_kind`
    - `export_binding_kind` must be `SEPARATE_FACES`
    - supported authored-texture primitive kinds remain bounded to `PLANE` and `RECT_PRISM`
    - face-array loading is strict:
      - malformed entries fail the bind
      - duplicate face-role declarations fail the bind
      - unreadable or missing face image files fail the bind
    - base-lane completeness is enforced for the declared primitive kind
    - declared bounded overlay lanes must also be complete
    - invalid authored-texture binds now leave the object fully unbound instead of partially active
  - the authored-texture loader/runtime lane now also partially adopts shared `core_authored_texture >= 0.1.1`:
    - schema/version, binding/output/primitive vocabulary, face-role semantics, and manifest-contract validation now come from the shared core module
    - the cutover is intentionally narrow:
      - JSON parsing
      - image loading
      - invalid-binding UX
      - runtime-scene persistence policy
      remain app-local
    - current schema `v5` manifests now expect the strict shared lane shape:
      - `base_surfaces`
      - optional `overlay_surfaces`
      - no mixed legacy `surfaces` field in the current `v5` output path
    - `core_authored_texture >= 0.1.2` is now the actual runtime floor for this lane:
      - `FLATTENED_ONLY` manifests that still declare an overlay lane are rejected by the shared validator
      - runtime validation coverage now also explicitly rejects the mixed `schema v5` drift shape (`surfaces` plus `base_surfaces`)
      - semantic-net validation now also routes through the shared module
  - the authored-texture manifest reader now accepts optional semantic net metadata per face:
    - net layout kind
    - canonical net slot
    - orientation
    - ordered corner ids
    - ordered edge ids
    - adjacent face-role hints
    - manual layout offsets
  - semantic-net metadata is no longer best-effort:
    - plane faces now require:
      - `net_layout_kind = PLANE`
      - `net_slot = FRONT`
      - `corner_ids = [255,255,255,255]`
      - `edge_ids = [255,255,255,255]`
      - `adjacent_face_roles = [NONE,NONE,NONE,NONE]`
    - rect-prism faces now require:
      - `net_layout_kind = PRISM_CROSS`
      - `net_slot` matching the face role
      - orientation in `R0/R90/R180/R270`
      - four unique `corner_ids` in `0..7`
      - four unique `edge_ids` in `0..11`
      - four unique valid adjacent prism face roles that are neither `NONE` nor the face's own role
    - malformed semantic-net metadata now fails authored-texture bind instead of degrading silently
    - compatibility remains bounded:
      - legacy neutral adjacency aliases `SURFACE` and `UNSPECIFIED` still normalize to canonical `NONE`
  - that richer manifest metadata is now available through a bounded runtime face-metadata getter, while current face sampling still resolves by explicit face-role mapping
- Scene editor mode routing now has four top-level modes:
  - Path / Bézier
  - Objects
  - Camera
  - Material
- Material editor mode is the first focused object material lane:
  - focuses the current selected object or the most recently selected object
  - shows an object-only focused viewport layer in 2D and retained native `3D` digest lanes
  - defaults native `3D` Material mode to an object-centered projector so the focused object orbits around its own center rather than its scene placement
  - retains the old scene-placement object-only projector path as an internal view mode for a later UI toggle
  - frames and wheel-zooms against the focused object's extents in object-centered native `3D` Material mode, with a much wider accumulated zoom range for close texture inspection and far-distance preview
  - renders the focused native `3D` object as filled projected triangles in Material mode, using a preview-safe capped version of the authored object color and drawing triangle separation lines
  - samples procedural material texture overlays directly in the flat Material preview, so texture type, strength, scale, U offset, and V offset are visible before path-traced rendering
  - previews v2 object stacks, including base patterns plus ordered rust/fog/grime/oil overlays, even when the legacy single `textureId` field is `None`
  - lets the base layer be toggled off in the layer list; the preview then draws a dim neutral substrate so overlay-only behavior can be inspected without changing the authored object color
  - keeps editor rendering bounded: no-texture focused objects use a fast solid fill path, while active rust/fog previews use capped block sampling instead of unbounded per-pixel draw-point emission
  - exposes a `Solid Faces` preview toggle that defaults on; the solid path uses opaque fill and editor-local depth buffering so rear triangles do not show through glass/fog/rust material previews
  - in Solid Faces mode, depth-checks triangle edge pixels so visible face diagonals render for per-triangle material inspection while rear triangle edges remain hidden
  - uses the native `3D` builder mesh as preview geometry truth, so planes resolve to `2` triangles and rectangular prisms resolve to `12`
  - can pick the nearest visible focused-object preview triangle from a Material canvas click and stores a volatile selected-triangle set for the current editor session
  - selects whole face groups by default in Material mode: plain click selects the face group under the cursor, and Shift-click toggles that face group
  - shows selected triangle count, selected face count, and selected face group ids in the Material left pane
  - renders selected faces with a depth-aware perimeter outline in the focused preview so the selected face texture remains visible
  - uses a compact Material pane summary and tighter texture controls so the `Face Groups` list owns more of the left pane
  - renders a compact bounded `Face Groups` list below the object-wide texture controls; the list inventories all focused-object face groups, shows selected triangles over total triangles per face, and clicking any row selects that whole face group as active
  - draws a scrollbar and routes pane wheel events to the group list when focused objects have more face groups than visible rows
  - keeps the group-list UI above the existing global editor buttons so Material-specific controls do not collide with shared `Add`, `Delete`, and `Select` actions
  - treats generated preview face groups as default texture islands: planes start as one two-triangle island and rectangular prisms start as six two-triangle islands
  - samples rust/fog preview textures from a cohesive face-space UV island across both triangles of a generated face, using a stable face-group seed so the texture does not restart at the diagonal split
  - lets active face groups override texture kind independently, so a generated face can be `None`, `Rust`, or `Fog` without changing the object-wide default or neighboring generated faces
  - routes strength, scale, Offset U, and Offset V sliders through the active face group when one is active, while preserving object-wide fallback controls when no group is active
  - routes the texture kind buttons through the active face group when one is active, while preserving object-wide texture kind controls when no group is active
  - object-default mode exposes the v2 layer stack immediately when no face is selected; layer rows support add, mute/enable, move up/down, delete, row selection, and row-level `On`/`Off` visibility toggles
  - now includes an object-wide `Authored Texture` section above the procedural layer stack; the section shows compact bound/unbound status, manifest basename plus face count when bound, and explicit `Pick/Replace Manifest` plus `Clear Binding` actions for the selected object
  - authored-texture bind state now survives the full selected-object workflow: bind in the Material editor, persist the runtime scene, reopen it later, and recover the same manifest path / binding mode / face-count summary for the selected object
  - the same selected-object workflow now also supports replacing one authored-texture manifest with another for the same object, persisting that replacement, clearing the binding later, and reopening back into an unbound state
  - active v2 base layers expose `Solid`, `Metal`, `Wood`, and `Brick` buttons in the current editor UI, while active v2 overlay layers expose `Rust`, `Fog`, `Grime`, and `Oil`
  - the active authored-texture follow-on lane in `drawing_program` now explicitly aligns its layer-role recipe to this runtime taxonomy instead of inventing a second label family:
    - role recipe:
      - `Base`
      - `Material Detail`
      - `Decal`
      - `Grime`
      - `Damage`
    - runtime intent families remain:
      - base/substrate:
        - `solid`
        - `metal`
        - `wood`
        - `brick`
        - `concrete`
        - `stone`
      - overlay/environment:
        - `rust`
        - `fog`
        - `grime`
        - `oil`
    - role and runtime intent are now deliberately separate, so later dual-lane authored layers can preserve cases like oily contamination without collapsing that intent into color-only paint
  - exposes compact texture parameter controls for active procedural overlays: pattern mode (`Default`, `Speck`, `Patch`, `Flow`), coverage, grain, edge softness, contrast, flow, color depth, and surface damage
  - placement controls remain sliders; secondary procedural parameters render as bounded knob cells with vertical drag and numeric readback
  - routes texture parameter controls through the active face group when one is active, while preserving object-wide parameter controls when no group is active
  - samples face-local texture overrides in the Material preview even when the object-wide texture kind is `None`
  - shows whether the active face is using object defaults or a volatile face override, and exposes `Reset Face` to clear the active override
  - exposes `Copy to Selected` when multiple face groups are selected; it copies the active face group's effective texture kind, placement, and parameter block to every other selected generated face group
  - persists object-wide procedural texture defaults and parameters plus generated face-group texture kind, placement, and parameters through ray-tracing runtime-scene `object_materials[].procedural_texture`
  - persists generated face-group texture kind, placement, and parameters for config-backed scenes through per-object `materialFacePlacements`
  - leaves group naming, custom group topology, and durable group controls for the next material-editor slices
- Native `3D` config persistence is now split cleanly from legacy `2D` state:
  - `integratorMode3D`
  - `temporalFrames3D`
  - `bounceDepth3D`
  - `rouletteThreshold3D`
  - top-fill and denoise toggles
  - runtime-scene and optional atmosphere-source paths
- Deep-render export now supports:
  - absolute start-frame selection
  - resume from highest existing saved frame
  - shared absolute-frame truth across output numbering and path sampling
- Export/video workflow state:
  - `frameDir` remains frame export root
  - `videoOutputRoot` remains persisted runtime config state
  - menu exposes grouped Data I/O + batch actions
- Scene-editor digest truth now preserves pickable guide-only helper overlays without promoting them into native `3D` render geometry.
- Headless agent rendering now has a Phase 4 request-driven CLI:
  - request schema: `ray_tracing_agent_render_request_v1`
  - command target: `make -C ray_tracing ray-tracing-render-headless`
  - preflight command: `ray_tracing_render_headless --request <request.json> --preflight`
  - frame export command: `ray_tracing_render_headless --request <request.json> --render`
  - summary schema: `ray_tracing_headless_summary_v1`
  - current scope: runtime-scene apply, optional VF3D source validation, native `3D` route readiness, prepared-frame validation, and BMP frame export under `<output.root>/frames/`
  - PhysicsSim `scene_bundle.json` handoff is covered by `test-ray-tracing-render-headless-volume-handoff`, which now uses a room-style LineDrawing emitter fixture (floor, wall planes, contrast prism, emitter prism), then runs PhysicsSim headless VF3D export and RayTracing BMP export into `ray_tracing/build/agent_runs/physics_trio/volume_handoff_image_export/`
  - volume handoff summaries now report VF3D channel/grid/density debug fields, including `volume_summary.density_non_zero_cell_count`
  - generic runtime-scene light/camera seeds from `transform.position` are promoted into the native `3D` route, matching LineDrawing-generated scene shape
  - runtime-scene camera seeds now also accept authored orientation fields (`yaw` / `rotation_z` / `transform.rotation.z`, plus optional pitch fields), and when no camera orientation is authored the native `3D` render auto-aims the seeded camera toward the built scene center so headless runtime-scene exports do not fall into all-black horizontal views
  - authored moving-camera scenes now also accept `extensions.ray_tracing.authoring.camera_focus_target = { x, y, z }`; when present, headless runtime-scene sampling preserves authored camera-path translation/depth motion but recomputes yaw/pitch toward that focus target each sample, which is safer than hand-authoring moving camera orientation curves
  - render summaries now also report render-visibility truth through `render_stats.hit_pixels`, `render_stats.visible_pixels`, `render_stats.nonzero_pixels`, `render_stats.max_radiance`, and `render_stats.max_rgb`
  - render summaries now also expose `object_audit`, which reports per-object runtime-slot presence, built primitive/triangle counts, and primary camera-ray hit pixels for headless diagnosis
  - headless requests now accept additive inspection-only render tuning fields: `preset`, `camera_zoom`, `camera_position`, `camera_look_at`, `environment_brightness`, `light_intensity`, `light_radius`, `forward_decay`, `volume_scatter_gain`, `volume_step_scale`, `secondary_diffuse_samples_3d`, `transmission_samples_3d`, and `volume_tint`
  - the shipped `glass_preview` inspection preset defaults to `emission_transparency` plus a bounded low-cost preview budget (`secondary_diffuse_samples_3d = 8`, `transmission_samples_3d = 4`) unless explicit request fields replace those values
  - the shipped `glass_review` inspection preset defaults to `emission_transparency` plus a slower review budget (`secondary_diffuse_samples_3d = 24`, `transmission_samples_3d = 12`) unless explicit request fields replace those values
  - runtime-scene apply now skips authoring helper objects (`point_set`, `curve_path`, `edge_set`) when populating live render object slots, so helper records do not crowd out later renderable primitives such as thin transparent review slabs
  - the current volume-handoff smoke now uses an explicit oblique camera rig (`camera_zoom=0.95`, `camera_position=(-3.8,-7.2,2.2)`, `camera_look_at=(-0.2,0.8,1.2)`) plus moderated lighting/scatter (`light_intensity=2.6`, `light_radius=0.10`, `forward_decay=220.0`, `volume_scatter_gain=3.0`, `volume_step_scale=1.0`) and a blue-biased inspection tint (`volume_tint=(0.35,0.65,1.80)`) so the room, emitter prism, and plume region read as a side-view `3D` scene instead of an overhead floor patch
  - runtime VF3D density reconstruction is now trilinear instead of nearest-cell lookup, reducing block-edge artifacting in headless and runtime volume shading
  - native `3D` direct-light renders now honor `temporalFrames3D` / `render.temporal_frames`; the old forced-single-subpass path is gone
  - detached local supervision now has a first RayTracing adapter through `make -C ray_tracing ray-tracing-job-runner`, with `submit`, `status`, and `cancel` commands that stage canonicalized absolute-path request files into `build/agent_runs/jobs/<job_id>/`, then supervise `ray_tracing_render_headless` through `job_status.json`, `render_progress.json`, `stdout.log`, `stderr.log`, `pid.txt`, and `result_summary.json`
  - the first trio detached chain adapter now routes into that runner through
    `bin/run_trio_detached_job_chain.sh`, which waits on detached PhysicsSim
    completion and auto-submits the RayTracing child once the exported
    `scene_bundle.json` exists
  - `test-ray-tracing-job-runner-smoke` proves one detached `diffuse_bounce` render can complete from file-backed status alone without holding a live PTY
  - detached Phase 2 truth now adds explicit submit policies (`fail_if_exists`, `overwrite`, `resume`), truthful `job_status.json` timing/frame fields, state/stage reconciliation on `status`, and contiguous-frame resume for already-rendered prefix frames via a staged `sampling` window that preserves normalized-time sampling
  - detached live-job supervision now also refreshes `job_status.json` directly
    on render-stage transitions (`loading_scene`, `preflight_ready`,
    `rendering_frame`, `writing_frame`, `completed`, `failed`) instead of only
    relying on `render_progress.json`; this makes thread-heartbeat follow-ups
    and direct file inspection trustworthy without a live PTY
  - detached live-job supervision now also surfaces temporal-subpass progress
    through `temporal_subpasses_started`, `temporal_subpasses_completed`,
    `temporal_subpasses_total`, and `progress_ratio` in both
    `render_progress.json` and `job_status.json`, so a single expensive frame
    can show real progress while still inside `rendering_frame`
  - detached live-job supervision now distinguishes "entered current subpass"
    from "committed current subpass"; a status like `started=6`,
    `completed=5`, `total=6` is now explicitly "final subpass active", not an
    implied failed render
  - detached status reconciliation now also emits a non-destructive `stalled`
    state when a process remains alive but `updated_at_utc` does not move for
    15 minutes; resumed progress can move that state back to `running`
  - `make -C ray_tracing ray-tracing-material-preview-headless` now ships a
    non-raytraced material swatch/contact-sheet lane that evaluates the same
    object-level material + procedural texture stack as runtime shading, but
    writes deterministic BMP previews and optional JSON summaries without
    running native `3D` integrators
  - material preview requests now support a sheet-level `background_color`
    (`0xRRGGBB`) plus per-variant synthetic `preview_overlay` blocks for fast
    grime/oil sweeps without rewriting the authored scene stack
  - `ray_tracing/tools/publish_material_preview_set.sh` now publishes one
    generated request into `ray_tracing/docs/material_preview_sets/<set_id>/`
    with `request.json`, `preview.bmp`, optional `preview.png`, `summary.json`,
    and an `index.md` that records left-to-right / top-to-bottom variant order
  - local repo-doc publication and live website publication are now explicitly
    separate lanes:
    - `ray_tracing/tools/publish_render_review_set.sh` and
      `ray_tracing/tools/publish_material_preview_set.sh` publish only into the
      repo docs surface
    - live visualizer publication goes through staged `visualizer-run/v1`
      drops under `_private_workspace_artifacts/codework_visualizer_runs/`
      plus the VPS import flow owned by `skills/codework-visualizer-drop/`
    - `ray_tracing/tools/publish_render_outputs.sh` is now the higher-level
      helper that can target `local`, `visualizer`, or `both`, defaulting to
      the visualizer lane and optionally staging the full frame sequence
    - `ray_tracing/tools/publish_latest_render_run.sh` now sits one layer above
      that wrapper and can infer the latest completed run root, its last frame,
      a default set id, and a humanized title for routine publish-after-render
      work
    - the visualizer staging script now rejects malformed drop ids instead of
      letting bad lowercase timestamp separators survive until VPS import time
    - remote staging cleanup now has an explicit helper script for stale invalid
      drops that would otherwise keep polluting importer results
  - missing legacy scene/animation config files now log as informational
    fallback messages during headless request-driven runs instead of looking
    like hard render failures in stdout
  - remaining long-frame supervision limit: there is still no tile-level,
    bounce-level, or non-temporal percent signal once a render is inside one
    long temporal subpass
  - `test-ray-tracing-job-runner-policy` now covers: clean submit, collision rejection without flags, partial contiguous resume after removing the final frame, and explicit overwrite rerender
  - multi-frame image export currently samples normalized time from `0.0` to `1.0` across the requested frame count
  - manifest-backed volume rendering currently imports one representative VF3D frame; animated VF3D frame selection remains a follow-up contract
- Menu/control-surface implementation is now split across:
  - `src/ui/menu/sdl_menu_render.c` for orchestration/layout pass ownership
  - `src/ui/menu/sdl_menu_render_controls.c` for shared text/button/control rendering helpers

## Recommended Scene-Authoring / Render Loop
- Treat `line_drawing` as the canonical source for room/object/camera/light
  authoring and `ray_tracing` as the downstream preview, render, and publish
  lane.
- The intended operator flow is:
  - export or compile `scene_authoring.json` and `scene_runtime.json` from the
    upstream request
  - use headless preview first for framing/material validation
  - use `ray_tracing_material_preview_headless` when the question is surface
    treatment rather than full lighting transport
  - use `glass_preview` / `glass_review` only when transmission quality is the
    actual thing under review
  - use detached job runner plus publish helpers only after preview/framing are
    stable
  - for worker-backed multi-frame review, use the proven seed ->
    `start_stage = ray_tracing` continuation -> optional publish backfill flow
    described in `docs/headless_continuation_visualizer_workflow.md`
- Current authoring contract reminders:
  - `material_id = 5` is the transparent/glass preset
  - `material_id = 4` remains the explicit emissive preset
  - omitted `emissive_strength` now means `0.0`, not an implicitly lit object
  - layered object treatment should prefer `material_texture_stack` over
    widening more flat one-off object fields

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Support lanes: `config/`, `assets/`, `data/`, `tmp/`
- Active source subsystems:
  - `app`, `camera`, `config`, `editor`, `engine`, `export`, `geo`, `import`, `material`, `path`, `render`, `scene`, `tools`, `ui`

## Verification Contract
- Build:
  - `make -C ray_tracing clean && make -C ray_tracing`
- Runtime-scene bridge contract:
  - `make -C ray_tracing test-runtime-scene-bridge-contract`
  - runs the focused core/apply and writeback/additive-merge bridge groups
    without requiring the full generic `test` lane
- Stable tests:
  - `make -C ray_tracing test-stable`
- Smoke wording note:
  - `make -C ray_tracing run-headless-smoke`
  - currently routes through `test-stable` rather than a separate runtime-only lane
- Build-only readiness:
  - `make -C ray_tracing visual-harness`
- Current native `3D` regression coverage is also decomposed into narrower families under the stable lane:
  - config-animation source/volume and settings/export persistence
  - prepared-render parity and scatter-preview proofs
  - scene-geometry builder and trace contracts
- Legacy lane:
  - `make -C ray_tracing test-legacy`

## Release and Packaging Snapshot
- Release-readiness and desktop packaging lanes are active and maintained.
- Standard package flow is available through `package-desktop*` targets.
- Release flow includes contract/audit/sign/notary/staple/verify/distribute gates.

## Runtime Data Policy
- Tracked defaults remain under `config/`.
- Runtime/generated state remains under `data/runtime` and other ignored runtime lanes.
- Export/video roots are runtime-config driven and menu-editable.

## Current Boundary
- Do not reopen closed `I5`/`I6` slices.
- Treat procedural material-texture sampling plus focused Material editor mode as the active post-`I6` material authoring foundation.
- Preserve Material mode's object-focused viewport controls while the next authoring controls land: `F` should frame the focused object, and wheel zoom should accumulate around that fit instead of resetting on every scroll tick.
- Keep the next material/material-authoring slice focused on authoring-side layer-role conventions, then coexistence polish between procedural overlays and authored bitmap bindings, plus durable group/preset controls on top of the now-live manifest path.
- Keep deep-render start/resume behavior stable while adjacent runtime-scene buckets settle.
- Keep the new menu-render, digest-pick, and native `3D` test-family seams aligned with their current helper/file boundaries while larger file-split work continues.
- Defer VF3D / `physics_sim` ingestion expansion until the next internal renderer boundary is chosen.

## History and Deep Lane References
- Full phase-by-phase details and archived slices are in private docs:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/ray_tracing/`
- This file is the compressed public current-state contract.
