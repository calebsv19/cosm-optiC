# Retained atmosphere and existing cache attachments

This is a Main Edit development feature. The source baseline is optiC 0.18.0,
worker 0.9.0; this document does not announce another release.

## Editor workflow

Open a saved scene and select **Environment**. Attach a VF3D file, VF3D pack,
cache manifest, or existing PhysicsSim scene bundle. Import copies the complete
contained dependency set into `assets/atmosphere/<content digest>/`. Source files
are preserved. Missing files, symlinks and traversing dependency paths are refused.
The active scene must be saved to disk before importing assets.

For a starting atmosphere, cycle the preset selector and choose **Create static
atmosphere**. Room haze, floor fog, cloud bank, mist patch and plume are bounded,
repeatable initial fields. Generation does not run a fluid solver or PhysicsSim.
First attachment fits visible geometry bounds, selects the atmosphere, and frames
its volume. Cloud/mist/plume use uniform scale at 40 percent of the largest scene
span; room haze fits the room dimensions, and floor fog fits the floor footprint
with a shallow height. Empty scenes use a bounded ten-metre reference region.

There is one attachment. **Replace with static preset** and **Replace atmosphere**
preserve existing placement and appearance. Playback resets to static for the new
source. **Fit volume to scene** is an explicit undoable re-fit for existing
attachments; opening a scene never resizes it automatically. Imported sparse
sequences use the same actual-frame selection policy before first-load fitting.

Environment defaults to the normal geometric viewport, with the shared evaluated
playhead. Its Wire/Solid/Material selector continues to control scene geometry.
A loaded atmosphere shows a coloured wire box, centre marker and bounded density
guide. The guide indicates occupied cache regions; it is not a shaded cloud or a
prediction of final lighting. Disabled/hidden volumes keep muted editable bounds.
The resource panel reports cache loaded/unavailable, actual frame and dimensions.

Click **Select atmosphere**, its box edges or centre marker to edit it. The
Inspector provides **Move (W)**, **Rotate (R)**, **Scale (E)**, **Frame atmosphere
(F)**, centre XYZ, rotation, scale, and physical XYZ size. Drag labelled axes or
the uniform scale handle; release applies one command and Escape cancels. Context
changes, focus loss, resize, stale revisions and active authoring operations cancel
gestures. Rotation and scale preserve the volume centre. Inspector values display
the live drag preview, while retained scene bytes change only on release.
Editing pauses timeline playback. The existing transform-handle visibility toggle
and snapping policy apply. Atmosphere axes use the existing world-axis handles.

Appearance and cache timing remain separate collapsible groups in Scene resources.
The **Field shape** selector creates/replaces the VF3D density field. The **Light
cloud**, **Soft mist**, and **Smoke** buttons instead change how the existing field
renders; they preserve its source, position, size, rotation, playback and visibility.
The selected appearance is highlighted; manually edited settings read **Custom**.
New static cloud banks start with Light cloud, plumes with Smoke, and other static
shapes with Soft mist. Existing attachments keep their saved appearance, including
when their field shape is replaced.

Expand **Appearance** to edit:

- **Transparency (%)**: 0 keeps the full chosen density; 100 removes both scattering
  and extinction without removing the attachment. Intermediate values multiply the
  remapped density, so perceived transparency still depends on volume thickness and
  lighting. This is not a flat image alpha.
- **Hue (0–360 degrees)** and **Saturation (%)**: recolor the scattering tint. Hue
  editing on a neutral tint starts at 35% saturation so the hue change is visible;
  set saturation to zero for neutral white/grey. Tint brightness is preserved.
- **Density**: controls the field's density independently of transparency.
- **Light absorption (darkening)** and **Light scattering (brightness)**: control
  attenuation and the light scattered toward the camera. Cloud/mist looks use lower
  absorption than Smoke; their appearance still requires suitable scene lighting.

**Advanced appearance** exposes density gamma, sampling step and direct Tint RGB
entry. These are art-directed single-scattering looks, not a multiple-scattering
cloud model or a guarantee of photorealistic clouds. Use Disney V2 renders to judge
the result; the geometric density guide remains an editing aid.

Click a numeric field, type its new value, then press Enter. XYZ and RGB fields
accept three comma-separated numbers; Escape cancels. Source FPS accepts an
integer or exact `N/D` fraction, such as `30000/1001`. Volume placement lives in
the Inspector, and centre/size edits convert back into the compatible cache-origin
placement fields used by the renderer and MCP. Invalid negative size/scale is
refused without changing the document. Volume transforms do not move bundled
water geometry.

Attachment changes are ordinary scene commands: Undo/Redo work across UI and
agents, and **File Save** retains the declaration. Remove releases the attachment;
Undo restores it. Published cache files remain available for redo. Agent mutations
are refused during active numeric edits or volume gestures. A missing source or
invalid sample reports an error instead of substituting an arbitrary frame.

The native preview framework remains available through the manual **Direct light
preview** toggle. It is off by default. It uses the authored scene camera and native
Direct Light integrator at 160 by 100 pixels, rendered in small row batches; it
replaces the geometric view only when explicitly enabled. The header then says
Direct light. Choosing a geometric display mode, entering another workspace or
opening a scene returns to geometric editing. This manual diagnostic has no final
quality or every-frame playback guarantee; higher-quality interactive preview
remains deferred. Use final rendering to judge scattering, lighting and materials.

## Playback and identity

There is one retained attachment under
`extensions.ray_tracing.authoring.atmosphere_attachment`, with schema
`ray_atmosphere_attachment_v1`. Absence preserves the previous legacy source
selection. A declaration, including a disabled declaration, owns selection and
prevents a legacy volume from appearing in its place.
The optional `appearance.opacity` value is in [0,1] and defaults to 1 for old
scenes. It multiplies density after density-scale/gamma remapping for both scattering
and extinction. UI, retained JSON and `atmosphere.set` use the same command path.

Manifest IDs are explicit, sorted actual `frame_index` values. They are not array
positions. Animated playback computes:

```
floor(source_start + (scene_frame_with_subframe - timeline_start)
      * source_fps / scene_fps)
```

Hold selects the predecessor and clamps outside the sequence. Error refuses a
missing exact frame. Static manifests hold their selected source frame. A single
VF3D or pack is static and uses its stored header frame ID; timing offsets do not
select another frame inside a single file.

The shared evaluated scene records the source frame and full content SHA-256.
For a water bundle, identity includes the selected volume and water payloads;
manifest revision also includes the water manifest. Preparation rereads identity
and refuses a changed cache. A manifest ID that disagrees with its payload/header
is rejected. Scrubbing and reverse/direct seeks do not step a solver or accumulate
time.

## Placement, flags and existing water

Volume position is expressed in authored scene units. Rotation uses XYZ degrees
(Rx, then Ry, then Rz), and positive XYZ scale pivots around the cache origin.
Ray clipping and density sampling use the inverse placement; march spacing uses
the smallest world voxel dimension.

Regular volume rendering and ordinary bootstrap/lens-transport volume caches
support placement. Cache deposits and samples map world points into the placed
grid; spherical and directional footprint kernels use world distances under
unequal XYZ scale. Diagnostics report world bounds and centroids. Existing
identity-placement behavior is preserved.

Photon-volume cache families still require identity placement: their separate
beam normalization has not been verified for transformed grids. Those combinations
are explicitly refused before population/rendering. Disable photon-volume
caustics or restore identity placement to use those modes.

Enabled controls the complete attachment. Visible volume controls its volume
component; bundled water retains its existing independent mesh behavior. Affects
lighting controls the existing volume lighting/sampling policy. The attachment
does not change water materials or move water with the volume-placement controls.

Existing bundles with a fluid source and optional `water_source` are supported.
Water uses the same clock and timing settings, selecting from its own actual frame
IDs. Existing water geometry, boundary and optical material adapters consume the
selected payload. Disabling the attachment removes both components from prepared
render geometry. Water-only authoring and water-placement editing are outside
this slice. No PhysicsSim source changes, solver runs or CFD work are involved.

## Live agent MCP

`tools/scene_editor_mcp.py` is a stdio MCP server. The local Codex registration is
named `optic_scene_editor`. Connect explicitly with `scene_connect`, supplying the
absolute path of a scene already open in optiC. Closed scenes are refused; this
server has no offline scene overwrite path. A client with this registration can
start without a selected document.

The ten tools are `scene_connect`, `scene_read`, `atmosphere_import`,
`atmosphere_generate`, `atmosphere_set`, `atmosphere_remove`, `scene_seek`,
`scene_undo`, `scene_redo`, and `scene_save`. Read state before each mutation and
supply its exact `expected_revision`. Set uses the typed attachment schema,
including appearance, placement and rational playback fields. Read reports dirty
state, playhead and evaluated cache identity.

UI and MCP call the same validators, command history and File Save operation.
An active atmosphere numeric edit, transform operation or close dialog refuses
agent mutations. A stale revision or invalid/missing dependency preserves the
active document. The main thread owns application; agents do not directly write
an open scene. The local socket and scene descriptor are owner-only. Unknown/live
endpoint ownership is preserved; idle connections expire.

For another MCP client:

```sh
python3 /absolute/path/to/ray_tracing/tools/scene_editor_mcp.py
# Optional initial target:
python3 /absolute/path/to/ray_tracing/tools/scene_editor_mcp.py --scene /absolute/scene_runtime.json
```

New Codex connections must load the newly registered server; registration does
not retroactively add tools to an already running conversation.

## Portable snapshots and verification

The scene workflow collects complete manifest/bundle dependencies relative to
their owning files. It freezes the selected runtime rendering defaults, rewrites
runtime/output roots, and seals file sizes/hashes. Static and animated retained
attachments can be relocated with their dependencies. Legacy request-level
simulation/project exports continue to use their existing Physics Trio lane.

Native UI controls, revision/missing-source refusal, Undo/Redo, tuning restoration,
actual File Save and fresh-process reopen pass. Real MCP initialize/list/call,
static generation/import, rational rates and save parity pass. Native preview
seeks change pixels, density edits change the image, Undo restores exact pixels,
completed images match an independent native render, and playback completes images. Sparse cache seeks,
full digests, water triangles, disable, placement image changes, sealed exports
and exact relocated images pass. Header identity is checked for raw VF3D and packs.
Codec timing/refusal and placed-cache metric/energy tests pass with ASan/UBSan.
Native bootstrap/lens-transport rendering, ordinary placed-cache frame preparation
and photon-placement refusal pass. Relevant timeline, evaluated-scene
preview and volume/import regressions pass. The full renderer/material/worker
matrix and human scene acceptance are separate.
