# Disney V2 Promotion Gates

Status: experimental gate contract for the separate `disney_v2` route.

This document defines the minimum criteria before Disney v2 can be considered
for replacing or changing defaults around the shipped `disney` route. It is not
a promotion decision. The shipped `disney` integrator remains the default
Disney-like native `3D` route.

## Hard Gates

Disney v2 cannot be promoted unless all of these are true:

1. Route isolation
   - `render.integrator_3d = "disney"` still resolves to the shipped Disney
     route.
   - `render.integrator_3d = "disney_v2"` resolves only to the experimental
     route.
   - Disney v2 does not inherit shipped Disney temporal pruning policy by
     accident.
   - Disney v2 denoise remains mapped to the v2 edge-safe final-resolve policy,
     not the shipped Disney blur path.
2. Transport proof coverage
   - diffuse/specular stochastic participation remains covered by focused
     transport tests.
   - transmission/glass participation remains covered by focused transport
     tests.
   - recursive BSDF path-loop depth and roulette behavior remain covered.
   - recursive BSDF lobe resampling must be local to each newly hit material
     vertex, not copied from the primary hit.
   - recursive mirror/glossy reflection depth and rough-sample diagnostics must
     remain covered beyond the bounded primary-reflection proof.
   - transparent camera-through accumulation, thin-walled versus solid glass,
     solid interior return, alpha-only versus physical transmission, and
     medium-stack diagnostics must remain covered.
   - finite-radius light-emitter accounting remains covered.
   - ordinary emissive-material surface-hit participation remains covered and
     counted separately from finite-radius light-emitter hits.
   - primary-vertex emissive-area sampling from ordinary emissive material
     triangles remains covered and counted separately from BSDF endpoint hits.
   - recursive-vertex emissive-area sampling from ordinary emissive material
     triangles remains covered and counted separately from finite-radius
     light-emitter hits and BSDF endpoint emissive hits.
3. Runtime-scene proof health
   - at least one primitive proof scene renders through `disney_v2`.
   - at least two imported-mesh material proof scenes render through
     `disney_v2`.
   - each proof records a render summary, object/material audit, BVH summary,
     and selected frame path.
   - imported-mesh proofs must show positive primary-hit pixels on imported
     mesh objects and zero BVH trace overflows.
4. Shipped-Disney comparison
   - each promotion proof scene must render through both `disney` and
     `disney_v2`.
   - matching scenes must report the same BVH triangle count for both routes.
   - route labels in summaries must remain distinct.
5. Visual review
   - at least two imported-mesh proof scenes must show visible improvement or
     meaningful new behavior over shipped Disney.
   - visible differences must be recorded with side-by-side frame paths and a
     short written review.
6. Performance and stability
   - elapsed render time must be measured for both routes on every proof scene.
   - Disney v2 cost thresholds must be explicit before default-route changes.
   - measurement reports must include route labels, frame paths, render stats,
     BVH stats, and timing ratios.
   - candidate threshold results must split performance and
     quality/convergence outcomes and remain separate from human promotion
     signoff.
7. Temporal policy
   - Disney v2 has an explicit edge-safe final-resolve denoise decision.
   - Current shipped Disney temporal pruning policy must not be copied by
     default.
   - Denoise/reconstruction must preserve clean visual edges, stable
     same-surface triangle interiors, transparent/interior contribution, and
     mirror/glossy contribution in canonical proof scenes.
   - The D2.18 primitive glass-corridor on/off matrix is a first proof cell,
     not full signoff:
     `docs/render_review_sets/disney_v2_d218_denoise_on_off_visual_proof/`.

## Local Measurement Command

Build the headless CLI first:

```bash
make -C /Users/calebsv/Desktop/CodeWork/ray_tracing build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless
```

Then run the gate measurement:

```bash
/Users/calebsv/Desktop/CodeWork/ray_tracing/tools/measure_disney_v2_promotion_gates.py
```

Candidate thresholds are loaded from:

```text
/Users/calebsv/Desktop/CodeWork/ray_tracing/tests/fixtures/disney_v2_promotion_thresholds.json
```

The command renders the D2.5 primitive and imported-mesh proof scenes through
both `disney` and `disney_v2`, validates the hard machine-checkable gates, and
writes:

- `build/agent_runs/ray_tracing/disney_v2_d26_promotion_gates/promotion_gate_report.json`
- `build/agent_runs/ray_tracing/disney_v2_d26_promotion_gates/promotion_gate_report.md`

The report may pass the local hard gates while still reporting
`promotion_ready=false`. That is expected until estimator quality and
release-candidate performance-threshold signoff are resolved.

## Current D2.6 Baseline

Generated on 2026-06-13:

- report JSON:
  `build/agent_runs/ray_tracing/disney_v2_d26_promotion_gates/promotion_gate_report.json`
- report Markdown:
  `build/agent_runs/ray_tracing/disney_v2_d26_promotion_gates/promotion_gate_report.md`
- `hard_gates_passed=true`
- `promotion_ready=false`
- machine-checkable gates passing:
  - route isolation
  - render health
  - audit health
  - BVH health
  - imported mesh participation
  - performance measurement
  - same-scene geometry parity
- measured elapsed ratios:
  - primitive glass corridor, `disney_v2 / disney = 0.176`
  - imported mesh material, `disney_v2 / disney = 0.077`

Current promotion blockers:

- post-ELA4 promotion-gate rerun using the recursive emissive-area policy
  counters before release-candidate performance interpretation
- production-quality estimator tuning beyond the first power-heuristic MIS
  and adaptive rough-reflection sample pass, especially BRDF-evaluated rough
  reflection/transmission direct-light quality
- repeated release-candidate convergence-threshold reports before any
  route-default discussion

Covered after D2.19:

- focused ordinary emissive-material surface-hit coverage for both first BSDF
  secondary hits and bounded recursive-loop endpoints

Covered after D2.20:

- second imported-mesh scene in the machine-checkable promotion report
- candidate threshold JSON and per-scene pass/fail threshold reporting
- imported-mesh pressure MRT8 private visual matrix with shipped Disney versus
  Disney v2 route comparison and Disney v2 denoise on/off proof

Covered after D2.21:

- user visual signoff for the imported-mesh pressure MRT8 matrix
- portable private skull/high-triangle scene package with relative sidecar
  paths
- first skull/high-triangle direct-light versus Disney-v2 smoke matrix at
  `171,272` BVH triangles and zero trace overflows

Covered after D2.22:

- repeat-run helper for D2.20 candidate thresholds and D2.21 skull smoke
- first `--repeat 2` stability report passed
- skull/high-triangle proof policy is settled as private/manual, not shipped,
  and direct-light versus Disney v2 only

Covered after D2.23:

- Disney v2 primary and recursive MIS weights now use a power heuristic
  (`power=2`) instead of the earlier balance-style PDF ratio
- rough reflection recursion now has bounded adaptive sample counts:
  tiny canonical proofs can use 4 samples, imported/medium scenes are capped
  lower, and skull-scale scenes cap to one rough probe
- focused tests prove power-heuristic weights and multi-sample rough-reflection
  diagnostics
- first post-change D2.20/D2.21 stability guard passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_stability/d223_estimator_quality_adaptive_smoke/stability_report.json`

Covered after D2.24:

- Disney v2 directly samples ordinary emissive material triangles from the
  primary visible vertex as area lights through the shared emissive-direct
  helper
- focused coverage proves emissive-area contribution without a finite runtime
  light and without classifying the triangle as a BSDF endpoint emissive hit
- result diagnostics separate emissive-area radiance/sample counters from
  endpoint emissive material hits and finite-radius runtime light hits
- post-change D2.20/D2.21 stability guard passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_stability/d224_emissive_area_smoke/stability_report.json`

Covered after D2.25:

- Disney v2 recursively samples ordinary emissive material triangles as direct
  area lights at bounded recursive transport vertices
- reflected mirror/glossy recursion preserves recursive emissive-area radiance
  and per-vertex light-branch diagnostics when merging reflected geometry back
  into the primary result
- the shared emissive-direct helper uses derived scene capabilities to skip
  mesh-emission support when a scene proves no emissive surfaces exist
- candidate threshold reports now split `performance_passed` and
  `quality_passed`; quality thresholds include visible pixels, secondary
  rays/hits, and bounded max radiance
- guarded threshold report passed with `hard_gates_passed=true`,
  `performance_thresholds=true`, `quality_thresholds=true`, and
  `promotion_ready=false` at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_d225_recursive_area_quality_thresholds_guarded/promotion_gate_report.json`
- post-change D2.20/D2.21 stability guard passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_stability/d225_recursive_area_quality_smoke/stability_report.json`

Covered after ELA1/ELA2:

- Disney v2 emissive-area direct lighting now uses a cached
  `RuntimeEmissiveLightSet3D` candidate list built during runtime scene
  capability refresh instead of scanning all scene triangles per receiver hit
  on the normal cache-valid path.
- Headless `render_summary.json` and promotion-gate extraction now include
  emissive candidate count, selected candidates, visibility rays, primary area
  samples, recursive area samples, and invalid-cache full-scan fallback count.
- Focused primary and recursive emissive-area transport tests assert the cache
  path and zero full-scan fallback while preserving D2.24/D2.25 behavior.
- Post-ELA two-repeat promotion-only report passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_stability/ela12_post_accel_repeats_r2/stability_report.json`.
- `primitive_glass_corridor/disney_v2` improved from the prior D2.25
  two-repeat mean `0.845s` to `0.633s` while reporting `2` emissive
  candidates, `1872` selected candidate samples, `761` visibility rays, and
  `0` full-scan fallbacks.
- Imported mesh material and pressure MRT8 currently report `0` emissive-area
  candidates, so they are not stress proofs for this specific acceleration
  path.

Covered after SU4:

- plane, thin-prism, and runtime-mesh mirror walls now have a canonical Disney
  v2 visual matrix under
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/mirror_surface_unification/matrix_review/`.
- no-denoise plane/prism/mesh runs all matched mirror diagnostics:
  `mirror_dominant_pixels=2017`, `mirror_reflection_hit_pixels=537`,
  `mirror_emitter_reflection_pixels=22`, and
  `mirror_geometry_reflection_pixels=515`.
- headless summaries and visual matrix reports now serialize mirror-dominant,
  base-attenuated, reflection-hit, emitter-reflection, geometry-reflection, and
  mirror radiance diagnostics.
- Focused emitter stress helper now exists at
  `tests/integration/run_ray_tracing_disney_v2_emissive_stress_report.py`.
  Its first report passed with `0` fallback scans for `emitter_off`,
  `single_emitter`, and `many_emitters`; the generated `many_emitters` case
  raised candidate count from `12` to `96` without changing the bounded
  selected-candidate shape.

Covered after ELA4:

- Disney v2 now separates primary and recursive emissive-area direct-light
  sampling policy. Primary visible-vertex sampling remains active, while
  recursive direct area samples are capped above `16` cached emissive
  candidates or `8192` runtime triangles.
- Endpoint emissive-material BSDF hits remain separate from this recursive
  direct-light cap.
- `render_summary.json`, promotion-gate extraction, and the focused emissive
  stress helper now include `emissive_area_recursive_policy_skips`,
  `emissive_area_recursive_candidate_cap_skips`,
  `emissive_area_recursive_triangle_cap_skips`,
  `emissive_area_recursive_candidate_cap`, and
  `emissive_area_recursive_triangle_cap`.
- Post-ELA4 focused stress report passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_emissive_stress/ela4_recursive_caps_initial/emissive_stress_report.json`.
- The `single_emitter` case preserved recursive behavior with `12` candidates,
  `41878` recursive area samples, and `0` recursive policy skips.
- The generated `many_emitters` case reported `96` candidates, kept primary
  samples active at `219107`, suppressed recursive direct area samples to `0`,
  and recorded `371325` recursive policy skips with `0` full-scan fallbacks.
