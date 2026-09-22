# T1 material inspector and retained authoring

Source checkpoint: `d7f948e`. Main Edit adopted T1 and T2 through `3819af1`
on September 22, 2026, with a clean fast-forward and fresh native authoring,
reopen, resource and recovery checks. Implementation and source adoption are
complete; installed-release and human acceptance remain separate.

T1 adds a common material inspector and completes editing of the existing bounded
procedural graph. It reuses the M6 graph compiler and T0 document transactions;
there is no new graph schema or shared evaluator version.

## Inspector workflow

Every selected object uses the same Assignment header, explicit Solid/Material
preview toggle, and Appearance, Sources, Coordinates and Preview sections.
Assignment can collapse to leave more space for editing. Appearance identifies
controlling outputs on typed sources; existing legacy response controls remain
available for legacy materials. Sources owns graph values and connections.
Coordinates selects coordinate nodes by kind, regardless of storage order, or
shows the retained chart/UV controls for mapped sources. A constant graph has no
coordinate source. Preview reports the current source family and outputs.

- **New** creates a Noise, Triplanar checker or Solid color source on an object
  without an explicit retained source. A material row is created when necessary.
- **Assign** copies a selected scene object's supported source, response and mapping
  into an independent material on the selected object. It does not create a shared
  mutable material link.
- **Duplicate** gives the selected object's source a fresh material identity without
  duplicating geometry. Values and producer metadata remain unchanged.
- **Replace** confirms an explicit preset replacement. Prior source, mapping and
  response declarations are archived under `material_authoring.replaced_sources`.

New, Assign, Replace and the two resets require an in-inspector confirmation;
Cancel changes nothing. Duplicate is one immediate undoable command. Successful
commands retain unknown metadata, produce a fresh `material_authoring.source_id`
when creating/copying/replacing a source, and use the existing undo/redo owner.
Assignment clears conflicting importer-owned recipient response overrides before
copying the source's values; unrelated recipient metadata stays active. Conflicting
source metadata is recoverable in provenance. Save/reopen retains these identities
and archives rather than reconstructing them from runtime data.

## Supported graph editing

The node list has search and supports all seven existing kinds: scalar, color,
coordinate, noise3d, triplanar_checker, multiply and mix. Existing values can be
edited; new nodes get valid defaults and unique IDs. Coordinate space, scale in
meters and offsets, color components, scalar values, integer noise seed and checker
sharpness are editable. Numeric fields preload the current value; first typing or
Select All replaces it. Invalid drafts remain editable after rejection.

Named input pickers expose Coordinates, Color A/B, Mask and Scalar A/B. Candidate
nodes are filtered by type, and cycles are unavailable. Base-color and roughness
outputs can be rewired; roughness can return to the base response. Deletion reports
node/output dependents rather than silently disconnecting them. Node creation,
deletion, connections and output changes each use one retained document transaction.
Runtime graph validation remains authoritative for UI and document API commands.

Source reset changes source values while preserving graph wiring and coordinate
nodes. Mapping reset changes coordinate values while preserving other nodes and
producer metadata. On legacy layers, source reset covers common response, opacity
and influence values; it does not invent defaults for arbitrary procedural families.
Chart mapping reset restores placement but preserves physical tile sizes, axes,
named UV-set identity and chart identity.

## Bounds and failure behavior

Supported graph replacement targets are plane/rect-prism primitives and mesh
instances. Procedural-solid asset geometry retains its asset authoring boundary.
Object-pinned manifests or surface documents cannot simply be assigned to another
object: validation rejects an invalid rebind and restores the whole document.
Image-channel file selection/relinking and mixed image/procedural composition are
T2/T3 work, respectively.

Stale revisions, locked targets, incompatible graphs and unsupported geometry fail
without accepting a partial command. Rejected numeric drafts stay visible. A valid
identical reset/connection may still create an undo entry. Preview mode survives
material apply/undo/redo within the session; a new process uses its existing default.

Graph lists and mapping values scroll within the available inspector. Hidden or
clipped controls have no hit targets. The existing viewport-only fallback remains
at very small window sizes; restore the workspace after enlarging the window to
resume inspector editing. Supported constrained-layout acceptance uses 1024×620;
800×600 verifies hidden controls are inert.

## Verification

Build the application and native test host, then run from the repository root with
fresh output directories:

```sh
make -j4 all scene-editor-workspace-visual-test ray-tracing-render-headless
python3 tests/integration/test_material_authoring_t1.py --output-root build/t1-acceptance
python3 tests/integration/test_surface_mapping_m3.py --output-root build/t1-m3
python3 tests/integration/test_surface_graph_m6.py --output-root build/t1-m6
```

T1 acceptance starts with ordinary geometry lacking an authored source and creates
noise/checker materials using real inspector events. It verifies all seven node
kinds, edits/connections/outputs, source identity, independent resets, cancellation,
rejection, exact undo, rowless creation, assignment response/provenance, preview
state, save/fresh-process reopen and constrained layout. It captures the normal
and constrained inspector for visual review. M3 additionally exercises real
Material→Coordinates numeric input, rejected drafts, correction, undo and focus
release. Scripted native proof is separate from user hands-on acceptance.

The September 22 source checkpoint passes the T1 native workflow and fresh-process
reopen, including historical identity noncollision and editing a retained row with
70,000 characters of unknown producer metadata. Normal 1280×800 and constrained
1024×620 captures were inspected. M0 fixtures, 29 diagnostic cases, M1–M6 retained
workflows, eight unchanged legacy render hashes, editor foundation, pane-host and
preview-shading checks pass. Full workspace acceptance also passes, including
legacy source subtabs, preset assignment and popup dismissal without click-through. The four M6 source/space cases retain ray/preview
agreement within 2.1e-14 and identical flattened/TLAS render hashes. These are
compatibility checks; T1 makes no rendering-performance improvement claim.

T0 fault-injected generation, undo/restore and save-publication recovery also pass
on the T1 source. Local verification artifacts are under
`build/surface_material_t1/`, including `acceptance-closeout/acceptance.json`,
`workspace-verified/acceptance.json`, `lifecycle-closeout/acceptance.json`, and the
M1–M6/legacy logs. Installed builds and package publication are separate.
