# Ray Tracing Current Truth

Last updated: 2026-05-07

## Program Identity
- Repository directory: `ray_tracing/`
- Public product name: `RayTracing Project`
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
- Native `3D` output is RGB-aware through the full shipped ladder.
- Native `3D` support layers now include:
  - tile preview
  - dirty-rect preview updates
  - tile occupancy culling
  - temporal accumulation upgrades
  - stratified + blue-noise sampling support
  - Disney-only denoise
  - optional top-fill lighting
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
  - supported face-role mapping is explicit for planes and generated rectangular-prism faces
  - hit-time payload resolution now samples authored bitmap color and alpha before procedural fallback when a bound face is hit
  - runtime-scene authoring persistence retains the manifest path/binding metadata rather than embedding image payload bytes
  - the authored-texture manifest reader now accepts optional semantic net metadata per face:
    - net layout kind
    - canonical net slot
    - orientation
    - ordered corner ids
    - ordered edge ids
    - adjacent face-role hints
    - manual layout offsets
  - that richer manifest metadata is now available through a bounded runtime face-metadata getter, but current face sampling still resolves by explicit face-role mapping
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
  - active v2 base layers expose `Solid`, `Metal`, `Wood`, and `Brick` buttons in the current editor UI, while active v2 overlay layers expose `Rust`, `Fog`, `Grime`, and `Oil`
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
- Menu/control-surface implementation is now split across:
  - `src/ui/menu/sdl_menu_render.c` for orchestration/layout pass ownership
  - `src/ui/menu/sdl_menu_render_controls.c` for shared text/button/control rendering helpers

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Support lanes: `config/`, `assets/`, `data/`, `tmp/`
- Active source subsystems:
  - `app`, `camera`, `config`, `editor`, `engine`, `export`, `geo`, `import`, `material`, `path`, `render`, `scene`, `tools`, `ui`

## Verification Contract
- Build:
  - `make -C ray_tracing clean && make -C ray_tracing`
- Smoke/harness:
  - `make -C ray_tracing run-headless-smoke`
  - `make -C ray_tracing visual-harness`
- Stable tests:
  - `make -C ray_tracing test-stable`
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
- Keep the next material/material-authoring slice focused on coexistence polish between procedural overlays and authored bitmap bindings, plus durable group/preset controls on top of the now-live manifest path.
- Keep deep-render start/resume behavior stable while adjacent runtime-scene buckets settle.
- Keep the new menu-render, digest-pick, and native `3D` test-family seams aligned with their current helper/file boundaries while larger file-split work continues.
- Defer VF3D / `physics_sim` ingestion expansion until the next internal renderer boundary is chosen.

## History and Deep Lane References
- Full phase-by-phase details and archived slices are in private docs:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/ray_tracing/`
- This file is the compressed public current-state contract.
