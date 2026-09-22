# T0 surface-material correctness and recovery

T0 hardens the existing M0–M6 capabilities. It does not add material creation,
image assignment UI or mixed image/procedural graph composition. Those remain
T1–T3 in the [post-M6 plan](surface_material_post_m6_plan.md).

## Periodic noise compatibility

`core_authored_texture` 0.7.1 wraps signed coordinates and each neighboring
lattice corner modulo 2^20 on all three axes. Positive interior cells retain
0.7.0 values. Negative and period-boundary cells intentionally change to correct
periodicity and continuity in the unshipped M6 candidate before Main Edit adoption.
Graph document version/capability remain v1. Archived M6 images and receipts are
retained as 0.7.0 evidence; they are not replacement goldens for the corrected noise.

Permanent shared tests use literal independent reference values derived with
60-digit Decimal arithmetic, three seeds, negative/positive XYZ coordinates,
period shifts from -2 through +2, all RGB channels and varying roughness. Sample
agreement is bounded by 2e-14. Boundary tests on every axis at -period/0/+period
use epsilons 1e-3 and 1e-4, require fine error below 1e-6 and at most 0.02 times
coarse error plus 1e-12. They also run at meter scale 1e-6. Separate checks cover
footprint fade, unbounded mean and independently weighted triplanar projections.
These tests fail against the original noise implementation and pass with the fix.

## One coherent runtime generation

Material preparation stages candidate mapping frames, sampled image/material
pyramids and graph programs without replacing published material tables. Success
transfers resource ownership and publishes all three tables together at the existing
synchronous scene-update boundary. No allocation, JSON access or IO enters shading.
This is not a new concurrent scene-publication API. Staging can temporarily retain
both old and candidate resource budgets; shared caching/incremental preparation
remain T2.

The bridge applies a defined failure policy across the entire runtime:

| Failure boundary | Result |
|---|---|
| File read, asset loading, parsing or validation before scene replacement | Previous objects, assets, IDs and materials remain active |
| Material allocation/read/decode/preparation after replacement begins | Empty runtime: objects, IDs, scaffold/digest/primitive/light seeds, motion, mesh/curve assets, mappings, sampled resources and graphs are cleared together |
| Editor command apply/history failure | Retained document rolls back using a snapshot allocated before mutation, then reapplies that document |
| Reapplication also fails | Runtime remains explicitly empty; diagnostic begins with the restore failure; retained source and history remain available for recovery |

An empty failed runtime has no scene path. A later early rejection cannot attach a
rejected candidate path to it. Candidate adoption reserves history before changing
runtime or disk, prepares runtime before saving, and rolls back on preparation/save
failure. Successful adoption remains one undo command. An early rejected command
does not advance revision or dirty/history state.

The deterministic `*FailNextForTests` APIs are disabled by default. Permanent native
acceptance injects candidate allocation, image read, image decode, pyramid allocation,
snapshot, history and restore failures. It also checks candidate-adoption failure,
successful adoption/undo/save, repeated recovery and late-clear followed by early
rejection. Original fixture files are hash-checked and never edited by the runner.

## Diagnostic and control contract

`RuntimeSurfaceGraphParseDetailed` returns `RuntimeSurfaceGraphDiagnostic` with
stable `code`, `object_id`, `node_id`, `property` and a human-readable `message`.
Input ports are identified by property paths such as `inputs[0]`. Missing scope is
an empty identifier. Failure clears the output program; success clears diagnostic
fields. The existing bool/string parser remains compatible and formats the same
identity fields. Scene validation supplies object identity and retains node details;
headless preflight summaries retain validation details when scene application fails.

Examples include `parameter_range` at node `position`, property `scale_m`,
`input_type` at a named node input, `cycle`, `missing_reference`, `mixed_source`,
and `transform_range` at `transform.scale.x`. Required feature declarations,
including explicit null declarations, fail closed. JSON identity and actionable
host restrictions remain app-owned; the shared compiler owns executable programs.

Graph objects explain that their coordinates belong to material graph nodes.
Competing Legacy/Planar/Axial, space, axis and numeric mapping controls are absent.
Input handling rechecks source capability before using cached hit rectangles, so
stale clicks and drafts cannot mutate graph-incompatible mappings.

## Verification and remaining acceptance

The verification ladder covers shared unit and graph ASan/UBSan tests; application,
headless and native-host builds; 29 graph/scene diagnostic cases; native lifecycle
faults; M0–M6 compatibility; mesh pack/builder and preview outline/shading tests;
procedural-solid material runtime; headless preflight; and eight unchanged legacy
image hashes. M6 adapter parity separately checks all RGB and nonconstant roughness,
with actual inspector input, duplicate/undo/redo/save/fresh-process reopen and
matching flattened/TLAS renders. Producer metadata and Sculpts output are retained.

Local receipts are under ignored `build/surface_material_t0/`, including
`regressions.json`, `diagnostics-final/acceptance.json`, `lifecycle-complete/acceptance.json`
and `m6-final/acceptance.json`. The [plan](surface_material_post_m6_plan.md) records
source adoption status. Scripted native proof and inspected captures are not a claim
of the user's hands-on acceptance. App/worker versions and installed packages are
unchanged by this source work.

Reproduce focused checks from the repository root:

```sh
make -C third_party/codework_shared/core/core_authored_texture test
make -j4 all scene-editor-workspace-visual-test ray-tracing-render-headless smooth-mesh-runtime-compile-tool
python3 tests/integration/test_surface_graph_diagnostics.py --output-root build/t0-diagnostics-new
python3 tests/integration/test_surface_lifecycle_t0.py \
  --graph-scene <graph-fixture>/scene_runtime.json \
  --sampling-scene <sampling-fixture>/scene_runtime.json \
  --output-root build/t0-lifecycle-new
```

Native acceptance requires a working macOS display session. The lifecycle runner
copies existing M5/M6 fixture directories before operating on them.
