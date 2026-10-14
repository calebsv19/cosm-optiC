# Optical response through shared UI and MCP commands

`material_response_read` and `material_response_set` operate on a stable
`object_id`. Read the current revision before each patch and provide
`expected_revision`. They use one retained command owner with document validation,
locks, runtime publication, Undo/Redo, and File Save. They preserve source graphs,
mapping, resource declarations and unrelated producer metadata.

`settings` is a nonempty patch. Omitted settings keep their resolved meaning.

| Setting | Meaning and accepted values |
|---|---|
| `base_preset` | `"glass"` opts the object into the existing glass material family. Required when editing a different family. Preserves its source graph/mapping. |
| `transmission` | 0–1 physical glass transmission weight; distinct from legacy alpha/display coverage. |
| `ior` | 1–2.5, matching the existing runtime/UI bounds. |
| `absorption_distance` | 0.25–8 in existing renderer world-distance units. |
| `thin_walled` | Boolean; existing thin-sheet versus solid-medium policy. |
| `base_roughness` | 0–1 base fallback. Graph output/region roughness stays authoritative at a hit. |
| `interface_tint` | Packed RGB24 integer 0–16777215, or null to clear the override. |
| `absorption_color` | Packed RGB24 integer 0–16777215, or null to clear the override. |

Colors use the existing packed-color interpretation, not a new spectral/color
model. Native validation is authoritative, including nullable color types.
Malformed, unsupported, nonfinite/out-of-range, stale, locked and active-edit
requests fail without applying a partial patch. Selecting glass is explicit;
readback never opts legacy objects in or synthesizes a row. Editing a rowless
object creates a retained row; Undo restores its original omission.

Readback returns an immutable retained material copy and object-level resolved
transport values. `base_roughness` is not a sampled final roughness. The source
label identifies graph/hit ownership; graph output and primitive-region overrides,
image sampling, and existing layers may change the final surface response.
Transport edits seed omitted transport settings from current resolved meaning,
then save canonical glass field names. Unknown metadata is preserved.

For graph-backed or mapped materials, **Materials → Appearance** provides glass
selection and optical controls through this owner. Source wiring remains in
Sources and mapping remains in Coordinates. Existing legacy glass transport
controls use the same retained owner when the editor has the corresponding
runtime-scene document open. Runtime-only legacy/config workflows retain their
existing runtime mutation behavior. Existing selected-layer roughness controls
retain their original meaning; MCP's `base_roughness` explicitly edits the base.

Use `scene_undo`, `scene_redo`, `scene_save`, and `scene_open` for history and
persistence. Cached clients use a fresh `editor_catalog` and `editor_command`;
no bridge/chat reset is needed. Optical edits do not alter scene attachments.

Verification uses actual graph Appearance UI clicks, legacy response controls,
named and cached stdio dispatch, per-hit graph/base transport values, typed/stale/
locked refusals, graph/producer/legacy/attachment preservation, shared history,
Direct Light and Disney V2 renders, Save/reopen, and exact fixed-sampling pixels
in a fresh process. This is authoring/integration parity, not measured glass
calibration, arbitrary mesh closed-volume qualification or caustic validation.
No new graph outputs or renderer scattering model are introduced.
