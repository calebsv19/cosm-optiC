# src › editor


The current shell separates document menus from pane controls.
`scene_editor_workspace_layout.c` computes document-bar and center-header bounds;
`scene_editor_workspace_profile.c` owns workspace/document/Add popup focus and
routes commands through existing editor callbacks. `scene_editor_pane_host.c`
reserves one document bar and a bottom status strip, including timeline sizing.
The shell draws pane separators and mode-specific header controls. Popup input
is consumed before viewport tools to prevent click-through. Changes to the
chrome-layout struct require rebuilding all consumers before native acceptance.
The Add > Import mesh task is rendered above the viewport by
`scene_editor_transform_panel.c`, which retains the existing managed mesh job
and picker state. Its early event route provides Cancel and Escape recovery while
the chooser is pending; import setup no longer appears among object properties.

Render authoring uses `scene_editor_render_authoring.c` for subject/task
selection and the themed control surface. Camera and the retained animated light
remain selectable before timeline setup. `scene_editor_camera_inspector.c`
serves the selected camera/light point or handle; numeric edits use the existing
retained authoring commands. Timeline transport/evaluation stays in
`scene_editor_timeline.c`; the Render controller owns no clock or scene copy.
Key rows start visible together, with optional temporal curve editing. Setup
retains an unambiguous legacy light path and timing in one undoable command;
missing/ambiguous targets remain visible errors. The ordinary-scene native
acceptance entrypoint is `scene_editor_workspace_visual_test <scratch>
<copied-scene> --render-authoring`. It saves only the supplied copied scene.

Interactive tooling for shaping the scene.

- `bezier_editor.c` – Adds/removes Bézier control points, manipulates velocity handles, and renders the path using the current camera margin so edits match the live viewport.
- `object_editor.c` – Adds, selects, transforms, and deletes scene objects; manages polygon creation workflows and shows the camera frustum while editing.
- `object_editor_selection_tracker.c` – Small selected/last-selected object tracker shared by object and material editor modes without coupling the material editor to the full object editor.
- `material_editor.c` – Focused object material editor mode. It opens on the selected or most recently selected object, defaults native `3D` framing to the focused object's center, retains an internal scene-placement view mode, owns compact existing controls grouped as Base Layer, Texture Binding, Physical Response, Face Override, and Preview & Readback surfaces, routes the active-face detail preview pane, and stores selected-triangle state, face override group list UI, row selection, list scrolling, active-face texture/placement/parameter control routing, reset, and copy-to-selected placement.
- `material_editor_mutation.c` – Material-mode mutation facade. It routes UI-triggered layer, texture kind, placement, pattern, parameter, Glass response, Mirror response, Metal stack response, Glass overlay shortcut, face reset, and copy-to-selected operations through explicit destination labels for material stack, selected-layer response, object-color compatibility, face override, and legacy object texture fallback paths, and exposes the internal panel-group labels used by Material editor rendering while preserving existing behavior.
- `material_editor_proof_readback.c` – Material-mode M4/M9/M10/M11 proof route readback helper. It builds focused-object request/readback labels for the `headless_material_preview` route, records the current mutation destination and panel group, maps Glass clear/frosted/tinted/dirty states to existing M4 proof packages, maps Mirror default/tinted/rough/illuminated states to SU4, mirror/glossy, or M10-S4 proof coverage, maps default/tinted/polished/damaged Metal states to existing M4/Disney-v2 evidence plus missing-proof labels, and keeps the editor affordance request-shape-only without launching proof generation or writing proof packages.
- `material_editor_graph_actions.c` – Material-mode M8 graph action helper. It creates/syncs an object-local graph from the focused material stack, adds supported layer and `roughness.scalar` channel-output nodes, clears graph sidecars, and keeps every edit compiling through the existing stack fallback path.
- `material_editor_graph_readback.c` – Material-mode graph integration readback helper. It reports focused-object graph identity, node count, channel refs, compiled stack fallback state, and the current M8 graph MVP status for the compact Graph pane.
- `material_editor_material_readback.c` – Material-mode M8 material identity/readback helper. It labels the focused material as preset, customized, authored-texture-backed, or graph-backed, exposes the deferred save-preset request label, and reports the active stack layer so dense response controls can name the layer being edited.
- `material_editor_recipe.c` – Material-mode M8 recipe helper. It builds the persistent `Material | Surface | Finish` header readback, owns the compact compatibility matrix for family/surface/finish choices including Glass overlay finishes such as fog, grime, scratches, and oil, and applies anchored dropdown menu selections into existing preset and editable stack state without introducing preset persistence.
- `material_editor_response_readback.c` – Material-mode M9/M10/M11 response-family readback helper. It classifies the focused material into Generic, Glass, Mirror, Metal, or Emissive response families, builds compact row/state/field readback for the active `Resp` pane, gives Glass its family-specific transmission/roughness/IOR/reflect/specular/tint/absorption/thin-walled surface, gives Mirror editable reflect/rough/spec/tint rows with dominance/base readbacks, gives Rough Metal editable stack-backed rough/reflect/spec/tint rows with guarded metallic and base readbacks, and keeps remaining families on a generic fallback until their own panes are promoted.
- `material_editor_texture_channel_readback.c` – Material-mode M8/M9 texture/channel readback helper. It classifies authored texture channels as visual, physical scalar, future normal/bump, or deferred displacement, reports the active procedural/placement source, and adds Glass-specific mapping labels for tint, clarity/frost, coverage, guarded transmission intent, and future scratch/frost detail without changing renderer sampling.
- `material_editor_face_region_readback.c` – Material-mode M8 face/region readback helper. It reports active face group, selected/focused group counts, layer or object-fallback context, object-face versus layer-specific override state, and reset/copy availability for the compact Face pane.
- `material_editor_compact_layout.c` – Material-mode M8 compact layout/state foundation. It defines the small sub-pane vocabulary, persistent identity disclosure state, and deterministic header/tab/content/popover rects used to build the laptop-friendly Material workspace without moving existing controls yet.
- `material_editor_compact_render.c` – Material-mode M8 compact shell renderer. It renders the persistent identity header, disclosure popover, compact sub-pane tabs, one active content area, and the compact Glass overlay shortcut strip in `Resp` while reusing existing stack, response, texture, face, graph, and proof controls without changing material semantics.
- `material_editor_authored_texture_binding.c` – Material-mode authored texture binding readback helper. It resolves the focused object's authored manifest binding, shows manifest/face-count state, and now exposes compact channel summaries from authored texture metadata without turning those references into renderer behavior.
- `material_editor_face_preview.c` – Dedicated Material-mode active-face detail preview pane. It renders one selected face group at higher quality through the shared non-ray-traced material surface evaluator, preserves extreme face aspect ratios in the right-pane preview, and lets inspection ignore or honor layer alpha.
- `material_preview_surface_eval.c` – Shared editor/headless material surface evaluation and lightweight preview shading seam. It resolves object/face material stacks plus face overrides into per-pixel surface state for detail preview and headless material swatches, including object-local Glass transport and Mirror response overrides and runtime stack-layer response influences.
- `scene_editor_material_face_metrics.c` – Material-mode face metrics/grounding helper. It resolves real plane/rect-prism face dimensions from retained primitive seeds, prefers world `+Z` as preview-up when a face contains a vertical direction, falls back to a stable planar `X/Y` orientation for horizontal faces, and expands face UVs into grounded, dimension-aware coordinates before procedural texture sampling.
- `scene_editor_material_face_placement.c` – App-local face texture placement owner for Material mode. It lets generated face groups inherit object-wide defaults, records active-face texture kind, placement, and parameter overrides, exposes save/load iteration helpers, normalizes slider values, and resolves cohesive face-island UVs across split triangles.
- `scene_editor_material_graph.c` – App-local per-object material graph sidecar. It stores bounded M7 graph documents and compiles them on set into the existing material stack fallback so scene-config/runtime-scene persistence can carry graph authoring state while old stack consumers keep reading `RuntimeMaterialTextureStack`.
- `scene_editor_digest_overlay_projector.c` – Digest overlay extents/projector math, including the Material-mode focused-object projector used to center the active object without linking tests to the full overlay renderer stack.
- `scene_editor_viewport3d_bridge.c` – Thin adapter between RayTracing's durable double target / radian projector state and shared `core_viewport3d >= 0.1.0`. It converts the Ray basis convention to the canonical right/screen-down/forward basis and routes pan, anchor zoom, orbit, frame, and resize transitions while projector construction and zoom-domain policy stay local.
- `scene_editor_viewport_nav_zoom.c` – Native `3D` digest viewport policy over the shared state-transition layer. Frame operations establish durable zoom limits; bounded reciprocal wheel/trackpad deltas preserve the pointer anchor without re-deriving limits from incidental selection, while focused Material mode retains its wider inspection range.
- `scene_editor_material_preview.c` – Material-mode focused-object triangle preview. It reuses the native `3D` builder mesh, fills projected triangles with fast solid fill when no texture is active or capped barycentric block sampling for rust/fog procedural texture color, samples generated face groups as cohesive texture islands with the same texture parameter block as the native payload path, supports Solid Faces opaque/depth-buffered preview with visible triangle edges on front-facing faces, exposes click picking for nearest visible focused-object triangles, and draws selected face-group highlights.
- `scene_editor.c` – Hosts the editor window, routes events to the active editor mode (cycle with Tab/Shift+Tab), saves settings, and draws shared HUD elements.
- `scene_editor_document.c` – Retained runtime-scene document owner for complete JSON preservation, stable-ID typed edits, bounded undo/redo, exact-base conflict detection, managed-candidate adoption, preview rehydration, and atomic durable scene publication.
- `scene_editor_transform_panel.c` – Object-mode right-pane inspector for numeric XYZ position/degree rotation/per-axis scale, rename/duplicate/two-press delete, undo/redo, explicit-unit managed STL intake, and per-instance shading/crease controls.
- `scene_editor_transform_ergonomics.c` – App-local World/Local presentation state and optional Move/Rotate/Scale quantization used by the Scene transform controls.
- `scene_editor_mesh_preview_contract.h` – RayTracing-owned Bounds/Wire/Solid/Material editor vocabulary plus invalidation policy. Geometry and view-direction changes may restart preview quality; zoom, pan, projection, appearance, hover, and selection retain the established geometry tier.
- `scene_editor_mesh_preview_store.c` – Thin editor-only consumer of shared `core_mesh_preview` `0.5.0` coherent indexed LODs. It makes a per-object LOD-or-bounds presentation decision, retains validated runtime/sidecar local bounds independently of LOD success, and exposes observable fallback status while native RayTracing rendering, materials, camera, overlays, final geometry, and BVHs remain authoritative.
- `scene_editor_mesh_preview_render.c` – RayTracing scene-editor mesh-preview orchestrator for Bounds/Wire/Solid/Material controls, structural wire, overlay cages, fallback GPU fill, and picking.
- `scene_editor_mesh_preview_surface.c` / `scene_editor_mesh_preview_shading.c` – Ray-local depth-raster/cache and lighting contract for smooth Solid/Material previews. It preserves floating-point projected vertices, keeps a reduced interactive tier, promotes settled frames to full viewport resolution, uses authored source normals when an exact LOD can address them, and otherwise builds angle-weighted normals in each LOD's own vertex space so high-frequency meshes do not become flat-facet noise. Solid mode uses the same calm base color across objects while Material mode retains authored colors. The surface uses the projector's orthogonal view-depth component and larger-is-front convention shared with picking, then composes primitives plus imported meshes through one owner/depth buffer. Guide-only primitives bypass the filled pass, successful surface composition suppresses later raw primitive wires, and shared `kit_viewport3d` supplies the same silhouette/depth/object-boundary threshold as LineDrawing.
- `scene_editor_primitive_preview_geometry.c` – Ray-local plane/rect-prism tessellation for the editor preview only. It feeds primitive triangles into the same Solid/Material depth surface as imported mesh LODs without changing retained scene meaning or final renderer geometry.
- `scene_editor_mesh_preview_outline.c` – Thin SDL adapter over optional shared `kit_viewport3d >= 0.1.0` silhouette/depth/object-owner outline semantics, including selected-over-hover priority. Buffer production, picking, texture upload, and draw ownership stay Ray-local.
- `camera_editor.c` – Fully interactive camera tooling: click-drag to pan, scroll/± to zoom, adjust the saved margin, edit the camera’s Bézier path (add/delete/toggle cubic/quadratic), and preview both light and camera paths through the active camera.
- `editor_mode_router.c` – Canonical editor mode routing layer that centralizes mode clamp/cycle policy, backend-routed view-context construction, and lane-aware `3D` capability labeling across compat-fallback and bounded native routes until full 3D edit math lands.
- `scene_editor_control_surface.c` – Shared control-surface provider that maps backend route + digest state into lane-aware scene-editor shell labels/status/action enablement. Native runtime `3D` still uses the retained digest viewport/editing contract here until a dedicated native editor control provider exists.
- `scene_editor_digest_overlay_objects.c` – Scene-digest overlay renderer for retained native `3D` primitives, guide-only helper outlines, and bounded digest visual state.
- `scene_editor_digest_overlay_object_pick.c` – Thin whole-object fallback over shared `core_screen_pick >= 0.1.0`. Mesh hover/click first uses the coherent preview LOD triangle hit test; this fallback projects primitive and mesh-instance authored origins into the current Ray projector and uses the shared radius/ranking contract.
- `scene_editor_object_list.c` – Virtualized, clipped object rows backed by vendored `kit_ui >= 0.11.2` wheel evaluation and top-anchor scroll sizing; RayTracing owns row meaning, SDL drawing, scrollbar paint, and selection.
- `scene_editor_surface_render.c` – Shared left/right pane render adapter for the scene-editor shell so mode summaries and status flow stay out of the core event/router file.

## Scene workspace presentation

The scene-digest overlay requests a wire reference across the five task
profiles. Materials alone displays the selected-object Bounds/Wire/Solid/Material
toolbar; unselected objects remain wire context. Workspace switching preserves
the scene view until the user explicitly frames a selection. This policy stays
RayTracing-owned; shared viewport and mesh-preview libraries provide geometry.

`scene_editor_workspace_layout.c` calculates the three-row document, workspace
and viewport-tool header. The document row owns identity/history/save/output/
leave, the workspace row is a direct five-segment navigator, and the tool row
owns selection, creation, transforms, framing, camera, paths and lights.
`scene_editor_pane_host.c` owns side-pane expansion/restoration and splitter state.
`scene_editor_chrome_shell.c` renders and maps the header; chrome actions update
layout without document commands and supplies the normalized task-status line.
Workspace changes clear stale action results so non-Scene profiles can state
their purpose. The retained document and transform inspector remain separate
owners. See `docs/editor_workspace.md` for the bounded slice and acceptance.

The E0/E1 continuation adds `scene_editor_workspace_profile.c` for the five task
profiles and `scene_editor_sidebar.c` for clipped scroll containers, Objects/Library
tabs and search focus. Outliner names come from the retained document, and filtering
never changes selected identity. `scene_editor_transform_panel.c` shares its managed
import between the picker and SDL file drop; no-selection import controls remain
available. App-local label helpers intersect parent clips so scrolling cannot
escape a pane. No shared module was extended.

The legacy-named object move gizmo owns Move/Rotate/Scale transient gesture state,
stable object/document binding and one-command release. The transform handles
module owns projection, picking and axis/ring rendering; transform preview owns
read-only mesh/primitive copies and handle origins; transform feedback owns the
plain-language task status and inspector group labels. Rendering consumes these adapters without changing
retained documents or runtime geometry. Workspace changes cancel the gesture.
Native acceptance lives in `tests/scene_editor_move_acceptance.h` and
`tests/scene_editor_transform_acceptance.h` within the workspace harness.

U2.3 object identity and commands:

- `scene_editor_document_objects.c` enumerates retained objects and provides
  copied readback, flags and names through the document's private transaction
  seam. It does not introduce another history owner.
- `scene_editor_object_commands.c` provides revision-checked object actions and
  selection/dirty/history readback for UI and agent callers.
- `object_editor_selection_tracker.c` stores stable IDs for retained documents;
  runtime indices are resolved on read. Its legacy index path is restricted to
  non-retained scenes.
- `scene_editor_object_list.c` owns document-backed rows and clipped per-control
  hitboxes. `scene_editor_sidebar.c` owns sticky Inspector identity and disclosure.
  New row/header strings have persistent frame backing storage.
- Primitive, mesh and curve importers omit explicitly hidden objects using the
  same rule, preserving runtime slot alignment across their separate passes.

## Render timeline dock

Render uses a full-width bottom dock below the tools, viewport, and inspector.
The shared `core_pane` / `kit_pane` splitter resizes it; height survives workspace
switches within the editor session. The dock defaults to 240 logical pixels.

- `scene_editor_timeline.c` owns the retained timeline/session adapter and
  document commands. It does not draw controls or interpret pointer positions.
- `scene_editor_timeline_view.c` owns presentation-only visible time, compact
  control geometry, frame/pixel conversion, and 1/2/5-based ruler intervals.
- `scene_editor_timeline_ui.c` owns grouped target rows, selection, focus,
  numeric entry, scrubbing, pan/zoom, and release/cancel behavior.
- `scene_editor_timeline_render.c` draws toolbar, channel hierarchy, ruler,
  key diamonds, selected-key controls, scrollbar, and hover help.
- `scene_editor_timeline_curve.c` draws and edits the selected scalar curve in
  the same full-height grid and visible-time window. Point frame/value movement
  and temporal-handle movement commit through the same retained command owner.

- `scene_editor_timeline_selection.c` owns independent key selection, matching-
  channel clipboard and atomic batch commands. `scene_editor_timeline_key_inspector.c`
  presents selected-key frame/value fields and explicit editing actions.

Click a channel to select camera/light animation and its inspector. Click an
entity header to select spatial authoring; its disclosure arrow collapses rows.
The **playhead** is the time being previewed. A **selected key** is a saved value
at a particular frame. They are independent: clicking a diamond selects it;
dragging the ruler scrubs without losing that selection. Selected diamonds are
highlighted. Previous/Next key explicitly select a neighboring key and seek it.

For a first edit, select Camera > Path progress, seek a frame with Playhead,
then edit **At playhead** to create/update a key. Alternatively, Add key records
the evaluated value there. Select a diamond and edit **Key frame** or **Key value**
to change that existing key while keeping the preview at its current time.
Path progress is a unitless position along the spatial path; it does not move
the path's control points. Edit those separately in Path authoring.

Shift-click adds/removes keys in the current channel. Drag any selected diamond
to move the group with its spacing preserved; Escape cancels. The inspector's
**Anchor frame** is the primary (last-added) key; changing it shifts the whole
group. **Set all values** explicitly assigns the same value to every selected key.
Copy then seek and Paste aligns the earliest copied key to the playhead. Dup here
copies and pastes in one action. Paste requires a matching property and unit.
Collisions, range violations and invalid values reject the entire edit; they do
not overwrite existing keys. Each group edit creates one undoable command.
At least one key must remain in a channel.

With timeline focus, arrows step the playhead (Shift: ten frames); Alt+arrows
retime the selected keys. Up/Down select and seek the previous/next key.
Ctrl/Cmd+A selects all keys in the channel; C/V/D copy/paste/duplicate.
Space toggles playback, Home/End navigate the animation range, and F fits it.
Ctrl/Cmd+wheel zooms around the pointer; Shift+wheel or middle drag pans time.
Ordinary wheel scrolls channels. The visible scrollbar also supports track clicks.
Fit channel shows its first through last key. Snapping is always to whole frames.

Footer frame/value, interpolation and Delete edit selected keys. Interpolation
controls the segment leaving each selected key: Hold keeps its value until the
next key, Linear changes evenly, and Bezier ease exposes temporal handles in
Curves. Select one curve point to edit time/value or its handles. Resizing, focus
loss, Escape, or stale document revision cancel uncommitted gestures. Undo/redo
and external document edits conservatively clear selection to avoid targeting a
replacement key at the same frame. Selection and navigation create no history.

Multi-key selection is limited to one channel. Marquee/cross-channel selection,
editable playback range/FPS, lifetime clips and events remain later scope. No MCP
server is added. The public selection API supplies semantic commands and readback;
named control/track geometry supports native UI acceptance.

Verification: `make BUILD_TOOLCHAIN=clang test-scene-editor-timeline-view
 test-scene-editor-pane-host-contract test-scene-editor-foundation-a` (one command),
plus isolated workspace visual modes `--timeline-dock`, `--render-authoring`, and
`--timeline` and `--timeline-selection`. The selection mode covers independent
playhead/keys, selected inspector edits, grouped operations, clipboard rejection,
undo/redo, save/reopen and compact inspector reachability. The dock mode exercises native routing, navigation without history,
key/curve edits, cancellation, undo/redo, save/reopen, and compact window layout.

The toolbar separates transport/key creation (left), Keys/Curves (center), and
Fit/Zoom (right), with non-overlap guards for compact windows. The footer and
animation inspector distinguish **Key** from **Sample**: a sample is the evaluated
value at a frame without a key. Entering a value creates a key there; entering a
value at an existing key updates it. The inspector's Playhead field seeks time,
not a key's frame. Interpolation belongs to the segment after a key. Editing
spatial path points changes the route rather than automatically inserting a
temporal key. This pass changes presentation only, not evaluation or history.

## Animate an existing object's position

Select an object in Scene, enter Render, and choose **Set up scene animation** if
needed. **Animate object: <name>** adds Position X/Y/Z channels together, seeded
from its saved placement; repeating the action selects the existing channels.
Choose an axis row, seek using Playhead, and enter an absolute position in
**At playhead**. Use Key frame/Key value to adjust an existing diamond.

For move/pause/resume, put a different position at frame 20, the same position at
frame 40, then another position at frame 60. Linear interpolation holds the value
between identical keys. Hold interpolation instead keeps a value until an abrupt
change at the next key. Curves can ease each axis independently.

Render displays the evaluated mesh/primitive position while Scene retains base
placement. This first slice uses numeric position authoring, not a viewport gizmo
that automatically inserts keys. Scroll the timeline channel list to reach Y/Z.
Select another object in Scene to add its channels. Rotation, scale, object paths,
emitters and lifetime clips remain later slices.
