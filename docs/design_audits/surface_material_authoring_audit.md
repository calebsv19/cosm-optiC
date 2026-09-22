# Independent material authoring audit

Date: 2026-09-21. Baseline: optiC `5afa2b7` and committed Sculpts M6 producer.
Auditor: `authoring_workflow_audit`. Read-only source, contracts, producer patch and
existing task-owned typed-inspector screenshot. No GUI launched or tests rerun.

## Findings

| ID | Classification | Finding and consequence | Evidence |
|---|---|---|---|
| U1 | Functional layout gap | Mapped/M6 materials bypass the regular material identity/Response/Textures/Stack/Face/Graph/Proof shell. Changing representation changes the entire editing workflow. | `src/editor/material_editor_compact_render.c:636–639`; regular shell follows near 680 |
| U2 | Missing workflow | The panel edits an existing graph; material creation, node creation/deletion and output wiring require source/agent work. SetSurfaceGraph requires an existing material row; the agent tool asks users to remove incompatible source declarations manually. | `src/editor/scene_editor_document.c:593–602`; `tools/surface_material_m6.py:47–56`; M6 contract |
| U3 | Missing workflow | Working M5 color/roughness/normal/height sampling has no resource/channel authoring UI. | `docs/surface_material_m5_contract.md:62–66` |
| U4 | Confirmed exposed-control mismatch | Graph objects still receive the Scene Surface Mapping panel, offering Legacy/Planar/Axial controls despite M6 rejecting simultaneous surface_mapping. Validation prevents adoption, but the UI offers an unsupported operation. | `src/editor/scene_editor_transform_panel.c:447`; `scene_editor_surface_mapping_panel.c:82`; graph validator |
| U5 | Usability limitation | Node selection cycles storage order; numbered inputs mutate to the next compatible node without showing structure or semantic port names. | `src/editor/scene_editor_surface_graph_panel.inc:14–25,83–92,141–164` |
| U6 | Interaction inconsistency | Material fields start with empty drafts while mapping fields preload values. Semantic graph rejection exits editing rather than keeping the draft available for correction. | `scene_editor_surface_material_panel.c:126–133,157–160`; `scene_editor_surface_mapping_panel.c:161–165` |
| U7 | Feedback weakness | Single-line errors and implementation terms such as “Enable retained source editing”/“Layer: unsupported” offer little recovery guidance. | `scene_editor_surface_material_panel.c:54–71,91`; graph panel status rendering |
| U8 | Preview workflow risk | A stored capture shows Material workspace with Solid viewport. A successful material edit can therefore have no visible material feedback. This is not evidence that the shader is wrong. | Existing M6 typed-inspector capture; document undo also restores preview state |
| U9 | Missing convenience | Scope/layer selection is cyclic; mapping is split between Scene and Materials. Region reset removes both source and mapping, although users may intend only one. | M3 contract; `scene_editor_surface_material_panel.c:15–18,64–90` |
| U10 | Pipeline gap | Sculpts graph preservation and OBJ projection/compilation work, but are separate producer steps rather than a complete desktop prepare/validate/preview/adopt flow. | Sculpts companion patch; M6 producer/UV contract |

## Proposed functional layout

Keep the existing Scene tree and central viewport. All material families share:

1. Assignment header: object/scope, material identity, new/assign/duplicate/replace,
   and local edit versus external provenance.
2. Appearance: supported base color, roughness and surface response, with links to
   their controlling source/node/channel.
3. Sources and composition: stable layer list, graph node outline or image channel
   cards; selected item details and typed connections/outputs.
4. Coordinates: effective mapping for the selected source/layer; UV identity or
   coordinate node; inheritance/override state and geometry restrictions.
5. Preview and validation: actual preview mode/support state, preparation status,
   actionable diagnostics linked to source/node/resource; optional technical detail.

Use named ports such as Color A, Color B, Mask and Coordinates; choose connections
from an explicit typed list. Gate unsupported combinations before interaction.
Numeric controls preload/select existing values and retain failed drafts. A spatial
canvas can later be another view of the same source and commands, not a second model.

## Workflow acceptance

- New procedural: select object → create Noise material → edit scale/colors →
  choose a mix input/output by name → Material preview → undo/redo → save/reopen.
- Image material: import attributed mesh → select UV set → select color/roughness
  files → encoding → normal or height → seam/mirror inspection → save/reopen →
  explicit stale/missing-resource relink. Use existing M5 combinations first.
- Layer/region: select prism face → inspect inheritance → change one source → reset
  source separately from mapping → undo restores their independent states.
- Producer: validate Sculpts candidate → desktop preview → retained local edit →
  save new candidate with producer metadata intact.

## Independent proposed sequence

Common inspector/diagnostics → transactional creation/assignment → complete bounded
graph editing → image/resource authoring → layer/region/coordinate consolidation →
producer/UV adoption flow. Broader combinations depend on actual runtime capability;
do not expose promises in the UI before their execution is implemented.

The existing retained document, revision guards, undo/reopen and provenance handling
are foundations to preserve. Most findings are authoring gaps, not evaluator bugs.
See [the synthesized plan](../surface_material_post_m6_plan.md).
