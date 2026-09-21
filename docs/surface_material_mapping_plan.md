# Cohesive surface material mapping

Status: M0–M5 implemented in the isolated surface-material lane. See the
[M1 contract](surface_material_m1_contract.md), [M2 checkpoint](surface_material_m2_contract.md)
[M3 document/region contract](surface_material_m3_contract.md) and
[M4 explicit UV contract](surface_material_m4_contract.md) and
[M5 sampling/response contract](surface_material_m5_contract.md).
Main Edit adoption and package publication remain separate. The broader roadmap
below includes work beyond this checkpoint.

## User outcome

A material belongs to a coherent object surface or an intentional surface region,
not to each tessellation triangle independently. A brick course should continue
around a sphere or curved wall with predictable horizontal alignment; vertical
joints should follow the chosen surface direction. Triangle refinement, camera
movement, and viewport workspace changes must not restart or move the pattern.
Scale, offset, rotation, face overrides and saved scene identity must agree in
Material viewport display and actual render.

## Verified starting point

Read [the parity diagnostic](material_viewport_parity.md) before changing code.
It proves a viewport/runtime mapping mismatch and missing viewport face overrides.
It does not prove that every renderer path independently pastes a whole texture
onto every triangle. Current viewport mapping chooses a dominant projection axis
per triangle from normalized object-space coordinates. Primitive rendering uses
face islands and physical face dimensions. Different seeds are another source
of disagreement. Mesh, primitive and authored-UV paths must be traced separately.

The existing Material workspace edits properties. Bounds/Wire/Solid/Material is
a shared display selector across workspaces. The separate Preview window remains
a later consumer, not the starting point for this lane.

## Bounded implementation sequence

1. **Inventory and freeze reference behavior.** Trace geometry identity, topology,
   connected regions, UV/attribute availability and material evaluation across
   imported meshes, primitives and derived surfaces. Reuse existing shared mesh,
   scene, math and surface-region contracts before adding another representation.
   Retain the existing eight-case diagnostic and add a low/high tessellation
   sphere pair with a labeled grid and brick pattern.
2. **Define a common surface coordinate contract.** Specify mapping mode, stable
   object/region identity, local/world frame, physical units, seed, seam handling,
   offsets and transform order. Preserve existing files through an explicit
   legacy default or migration; do not silently reinterpret saved scale. Decide
   which semantics belong in shared core and which adapters remain in optiC so
   Sculpts can later consume the same scene contract.
3. **Implement one useful curved-surface path first.** For a sphere/cylinder brick
   test, evaluate longitude around a chosen axis and a height/latitude coordinate
   independent of triangle IDs. Compare height-based courses against angular
   courses and explicitly choose the intended behavior. Use the same query in
   viewport and renderer; accelerate/copy results without changing semantics.
4. **Extend by surface type.** Planar mapping for floors/walls; authored UVs for
   assets with meaningful unwraps; cylindrical/spherical mappings where suitable;
   connected-region charts or unwraps for arbitrary meshes. Triplanar blending can
   be useful for nondirectional noise, but is not automatically a solution for
   coherent brick joints. Treat intentional hard edges and seams explicitly.
5. **Complete authoring and persistence.** Expose mapping mode/axis, physical tile
   size, offsets, rotation and seam orientation through existing inspector rows.
   Add surface-region assignment only after selection/identity is reliable.
   Validate undo/redo, duplication, save/reopen, transform and material changes.
6. **Prove response separately.** Match base color, masks, roughness, reflection
   and opacity at identical surface points before comparing shaded output.
   Then test mirrors, brushed-metal tangents and glass with suitable transport.
   Keep real-time lighting approximations explicit and measure orbit performance.

A sphere cannot receive a seamless, undistorted flat rectangular grid everywhere.
The chosen mapping must address a wrap seam, pole convergence and distortion.
Horizontal rings and longitudinal joints are achievable, but physical brick
width changes toward the poles unless rows adapt or a special cap is used.
A general mesh has analogous chart/seam choices; no automatic mapping should
claim to eliminate them universally.

## Acceptance fixtures

- Plane and prism: current scale/offset/rotation parity, six-face identity,
  per-face material/placement isolation, saved-scene reopen.
- Sphere: low/high tessellation and alternate triangulation produce the same
  pattern at common surface points; courses wrap predictably; seam/pole policy
  is visible and documented.
- Cylinder and bent wall: continuous rows around curvature and intentional
  transitions across hard edges; stable physical tile size where defined.
- Imported mesh: rotations, nonuniform scale, UV seams, mirrored transforms,
  disconnected islands and derived geometry preserve defined material identity.
- Runtime: bounded cache cost, no stale edits, camera movement does not rebake
  object-space mapping; material display remains available in every workspace.

## Suggested new-task brief

Continue from clean ray_tracing Main Edit after the UI closeout. Read
`docs/material_viewport_parity.md`, this plan and the current Git state. Start
with the mapping inventory and sphere/plane reference fixtures. Prove one shared
surface-coordinate path in the viewport and final renderer before broadening
material controls. Keep VERSION and WORKER_VERSION unchanged unless separately
requested. Do not redesign the editor shell or the existing Preview window.
