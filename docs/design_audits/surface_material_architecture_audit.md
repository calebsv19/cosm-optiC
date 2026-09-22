# Independent material architecture audit

Date: 2026-09-21. Baseline: optiC `5afa2b7`, shared authored texture 0.7.0.
Auditor: `material_architecture_audit`; parent verified selected findings separately.
Method: source/contracts/tests inspection; no product edits or GUI session.

## Findings

| ID | Classification | Finding | Evidence |
|---|---|---|---|
| A1 | Functional limitation; highest composition priority | Image sampling, typed graphs and region materials are mutually exclusive. An image albedo/normal with procedural dirt and a region mask cannot be authored as one material. Removing rejection checks alone would create competing material owners. | `src/render/materials/runtime_surface_sampling.inc:91–105`, `runtime_surface_graph.inc:135–172`, `runtime_material_payload_3d.c:560–562` |
| A2 | Reproduced defect | Noise wraps floating coordinates with signed `fmod`, but does not wrap lattice corners. Its documented period has a discontinuity and negative coordinates do not repeat consistently. | `third_party/codework_shared/core/core_authored_texture/src/core_authored_surface_graph.inc:76–90`; parent C probe below |
| A3 | Design opportunity | M6 coordinates have rest/world positions, normals and footprints, but no named UV/chart/tangent identity. The new graph cannot reuse M4/M5 UV/image capabilities. | `third_party/codework_shared/core/core_authored_texture/include/core_authored_surface_graph.h`; `docs/surface_material_m6_contract.md` |
| A4 | Disclosed quality limitation | Replacing each source by its mean before composition is not the mean of the composed result. For a binary checker C, E[C*C]=0.5, while E[C]*E[C]=0.25. M5 RMS roughness and M6 filtered scalar roughness also differ. | `core_authored_surface_graph.inc:133–161`; M6 contract filtering policy |
| A5 | Confirmed diagnostic weakness | Compiler, host parser and scene validator collapse failures into generic messages. Capability discovery describes only the M6 family, rather than the complete material support matrix. | `core_authored_surface_graph.inc:61–64`; `src/render/materials/runtime_surface_graph.inc:13–19,102–107,168–172` |

### Compiled-library reproduction of A2

The independent audit identified the defect by inspecting/reproducing the algorithm.
The parent then ran the actual compiled C library; these numbers supersede the
agents' translated-algorithm numeric examples. For seed 123, Y=0.37, Z=0.51,
P=1,048,576 cells:

| Query | Actual result |
|---|---:|
| N(-0.25) | 0.448972240654709 |
| N(P-0.25) | 0.571298917099888 |
| N(P-0.000001) | 0.587105229578858 |
| N(P+0.000001) | 0.439186254121167 |

At scale_m=1e-6, this seam lies around 1.048576 authored meters. This is not solely
an astronomical-coordinate issue. The existing tests check seed changes and
unbounded means but not wrap continuity/periodicity.

Reproduce from the repo root after building the module:

```sh
make -C third_party/codework_shared/core/core_authored_texture
cc -std=c11 -Wall -Wextra -Werror \
  -Ithird_party/codework_shared/core/core_authored_texture/include \
  docs/design_audits/surface_material_noise_probe.c \
  third_party/codework_shared/core/core_authored_texture/build/libcore_authored_texture.a \
  -lm -o /tmp/optic-noise-period-probe
/tmp/optic-noise-period-probe
```

## Recommended architecture

Keep retained source documents separate from immutable prepared programs/resources.
Preserve the allocation/IO-free shading boundary. Add explicit, versioned coordinate,
resource, composition and output semantics, with adapters retaining old documents'
meaning. The common query should include domain-appropriate positions, named UV and
chart identity, normals, tangent orientation and physical footprint validity.
Evaluate triplanar projections independently; never average coordinates before sampling.

Extend `core_authored_texture` for shared semantics and reuse its mip machinery.
Keep mesh attributes in `core_mesh_asset`/`core_mesh_compile`; use established generic
space/math helpers. optiC owns resource loading/caching, transactions, rendering and
UI. Sculpts/agents emit the same supported declarations. No new universal shading
framework or forced rewriting of existing source documents is justified.

## Independent proposed sequence

1. Noise correctness, structured diagnostics and complete capability description.
2. One named-UV image plus rest/world procedural mask in a versioned program.
3. Explicit region selection/blending and source-preserving migration.
4. Basis-correct normal/bump composition and specified roughness filtering.
5. Secondary footprints and independent composed-filter quality references.

Each step needs backward-compatibility receipts, independent numeric references,
ray/preview agreement, source-preserving undo/save/reopen and bounded preparation.
The synthesis reconciles this sequence with UI and lifecycle dependencies in
[the post-M6 plan](../surface_material_post_m6_plan.md).
