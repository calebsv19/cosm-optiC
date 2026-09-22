# Independent material runtime and proof audit

Date: 2026-09-21. Baseline: optiC `5afa2b7`.
Auditor: `runtime_quality_audit`. Read-only source and existing receipts; no product
edits, GUI launch or expensive suite rerun. Existing timing/test results are prior
measurements, not freshly rerun benchmarks.

## Findings

| ID | Classification | Finding | Evidence |
|---|---|---|---|
| R1 | Verified control-flow hazard; user-visible corruption not reproduced | Material preparation destroys prior state before success. Mapping resets, then sampling failure can short-circuit graph preparation, retaining old graph state against replaced scene objects. Rollback reapply failure is ignored. | `src/render/materials/runtime_surface_mapping.c:491–531`; `runtime_surface_sampling.inc:175–178`; `src/import/runtime_scene_bridge.c:881–891`; `src/editor/scene_editor_document.c:318–327` |
| R2 | Reproduced semantic defect | Noise period/negative-coordinate inconsistency, independently found by architecture audit and verified by parent against compiled C. | See [architecture reproduction](surface_material_architecture_audit.md) for authoritative compiled-library numbers |
| R3 | Disclosed visible limitation | All offset secondary rays lose bounded footprint detail. M5 uses chart means; M6 substitutes source means. Sharp reflected texture detail is lost and correlated composition is biased. | `src/render/runtime_ray_3d.c:211–218`; `runtime_specular_reflection_3d.c:65`; `runtime_surface_sampling.inc:356–359`; shared graph evaluator |
| R4 | Confirmed scalability limitation | Pyramids are owned per object. Validation prepares/frees image data, load prepares it again, and full scene application rebuilds material state. No orbit rebuild does not imply cheap edits. | `runtime_surface_sampling.inc:8–18,150–193`; `scene_editor_document.c:153–204` |
| R5 | Confirmed bounded performance limitation | UVs force exact geometry copies; interactive and settled meshes both allocate. Earlier M5 Material orbit costs are 38–58ms for two triangles and 102ms for 8,192 triangles, indicating geometry reduction alone cannot solve the cost. | `core_mesh_preview/src/core_mesh_preview_lod.c:70–75,283`; `src/editor/scene_editor_mesh_preview_store.c:115–124`; prior M5 acceptance receipt |
| R6 | Confirmed proof gap | M6 adapter agreement compares R/G, omits B, uses constant roughness and the same evaluator for its coordinate oracle. Good plumbing proof does not establish independent evaluator correctness or final-lighting fidelity. | `tests/scene_editor_surface_graph_m6.h:33–74` |
| R7 | Portability limitation | M5 requires absolute hash-pinned resources. Local save/reopen proof does not establish movable projects. | `runtime_surface_sampling.inc:32–40` |

## Required acceptance improvements

- Inject allocation/image-read failures at each preparation boundary, including
  failed rollback. Preserve a complete last-good generation or install a clearly
  empty/error generation; never combine old programs with new object identities.
- Test periodicity and continuity on every noise axis, negative shifts, seeds and
  small physical scales, with an explicit compatibility decision.
- Add independent full-RGB, nonconstant roughness, projected-normal weights and
  high-sample filtering oracles. Keep same-evaluator adapter agreement separately.
- Establish reflected checker/normal scenes. Start with delta-specular differential
  transport; rough/diffuse cone approximations are separate quality decisions.
- For 100 instances sharing one pinned image, decode/build the image once per
  content/encoding version. Unrelated transforms/camera edits must not decode it.
  Measure cold/warm loads, edit p50/p95, bytes, build counts and eviction behavior.
- Profile rasterization/shading separately from geometry at fixed output resolution.
  Benchmark 1/10/100 instances and 8K/100K/1M triangles before choosing LOD work.
  Reduction must preserve chart boundaries/handedness within a measured error limit.
- Test project relocation and explicit relink, with missing/corrupt resources and
  interrupted reload, before claiming portable producer adoption.

## Design boundaries to preserve

Static capability validation already rejects many unsupported combinations. Normal
and height are mutually exclusive in M5, so an internal overwrite is not an exposed
normal-plus-height bug. M5 RMS/normal variance and M6 scalar roughness are intentionally
different today and need an explicit composition policy.

Prepared graph shading is fixed-size and allocation/IO-free. Improve preparation
ownership/invalidation around it rather than replacing it. Extend existing
`core_authored_texture` and `core_mesh_preview` where semantics belong there; app
resource caches, document transactions and render generations remain optiC-owned.

Independent sequence: reliability → independent proof → resource preparation and
measured performance → wider composition → Main Edit workflow acceptance. The parent
reconciles timing of safe adoption and UI work in [the next-step plan](../surface_material_post_m6_plan.md).
