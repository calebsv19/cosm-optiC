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

- production-quality estimator tuning beyond the first power-heuristic MIS,
  adaptive rough-reflection sample pass, finite-light direct-light PDF
  estimate, material-aware direct-light BSDF/BTDF PDF estimate, and sampled
  emissive-area PDF estimate, especially branch-separated multi-light MIS,
  physically complete BTDF PDFs, and repeated rough reflection/transmission
  convergence
- visual signoff documenting visible improvement or meaningful new behavior
  over shipped Disney for imported-mesh proof scenes
- skull-scale or other external high-triangle scene signoff once large
  sidecar/BVH readiness is portable in the local proof workspace
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

Covered after D2.26:

- Post-ELA4 promotion-gate report passed with hard gates, performance
  thresholds, and quality thresholds true at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_post_ela4_promotion_gates/promotion_gate_report.json`.
- The primitive corridor stayed on the emissive cache path with `2` candidates,
  `1904` selected candidates, `829` visibility rays, `1105` primary area
  samples, `799` recursive area samples, `0` recursive policy skips, and `0`
  full-scan fallbacks.
- Disney v2 now estimates finite-radius runtime light PDF from light radius and
  hit distance through `RuntimeDisneyV2_3D_EstimateDirectLightPdf(...)`; point
  lights and emissive-area-only direct paths keep the conservative `1.0`
  proxy pending fuller area/candidate PDF estimation.
- Primary and recursive Disney v2 MIS use the estimator helper instead of
  open-coding `lightPdf=1.0` whenever direct light exists.
- D2.26 estimator report passed hard gates, performance thresholds, and quality
  thresholds at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_d226_direct_light_pdf_estimator_initial/promotion_gate_report.json`.
- D2.26 Disney v2 threshold metrics: primitive glass corridor `1.125s`, ratio
  `0.666`, secondary `5911`/`1377`, max radiance `0.552`; imported mesh
  material `4.615s`, ratio `0.169`, secondary `18674`/`6667`, max radiance
  `0.522`; imported mesh pressure MRT8 `3.375s`, ratio `0.204`, secondary
  `13894`/`4920`, max radiance `0.484`.

Covered after D2.27:

- Disney v2 now estimates the material BSDF/BTDF PDF toward finite runtime
  light directions through `RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(...)`
  instead of using the sampled support-ray PDF as the direct-light MIS
  comparator.
- Primary and recursive Disney v2 MIS diagnostics record the material-aware
  direct-light BSDF/BTDF PDF while preserving the sampled lobe support PDF for
  path continuation.
- Focused transport tests cover Lambert diffuse PDF, GGX roughness-sensitive
  specular PDF, and bounded opposite-hemisphere transmission PDF behavior.
- D2.27 estimator report passed hard gates, performance thresholds, and quality
  thresholds at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_d227_direct_bsdf_pdf_estimator_initial/promotion_gate_report.json`.
- D2.27 Disney v2 threshold metrics: primitive glass corridor `1.369s`, ratio
  `0.333`, secondary `5919`/`1384`, max radiance `0.553`; imported mesh
  material `4.765s`, ratio `0.121`, secondary `18674`/`6667`, max radiance
  `0.522`; imported mesh pressure MRT8 `4.798s`, ratio `0.235`, secondary
  `13894`/`4920`, max radiance `0.484`.

Covered after D2.28:

- Cached emissive-area samples now report selected sample direction, distance,
  triangle area, receiver/emitter cosines, candidate-selection PDF, area PDF,
  and solid-angle light PDF.
- Disney v2 primary and recursive emissive-area MIS now uses the sampled
  solid-angle light PDF and evaluates the BSDF/BTDF comparator toward the
  selected area-light sample direction when available.
- Focused tests prove the sampled-area solid-angle PDF formula and prove
  emissive-area-only primary/recursive MIS records positive direct-light and
  area-direction BSDF PDFs.
- D2.28 estimator report passed hard gates, performance thresholds, and quality
  thresholds at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_d228_emissive_area_pdf_estimator_initial/promotion_gate_report.json`.
- D2.28 Disney v2 threshold metrics: primitive glass corridor `1.827s`, ratio
  `0.491`, secondary `5917`/`1383`, max radiance `0.553`; imported mesh
  material `3.932s`, ratio `0.120`, secondary `18674`/`6667`, max radiance
  `0.522`; imported mesh pressure MRT8 `5.394s`, ratio `0.267`, secondary
  `13894`/`4920`, max radiance `0.484`.
- D2.28 emissive stress report passed with `0` full-scan fallbacks at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_emissive_stress/d228_area_pdf_initial/emissive_stress_report.json`.

Covered after D2.29:

- Disney v2 now records branch-separated finite runtime-light and
  emissive-area direct-light MIS diagnostics while preserving the legacy
  aggregate MIS fields.
- Primary finite runtime-light direct radiance uses the finite branch weight;
  primary and recursive emissive-area accumulation use the sampled emissive
  area branch weight.
- Focused transport coverage proves finite-only, area-only, mixed
  finite-plus-area, and recursive area-only branch records and power-heuristic
  weights.
- D2.29 estimator report passed hard gates, performance thresholds, and quality
  thresholds at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_d229_branch_separated_direct_light_mis_initial/promotion_gate_report.json`.
- D2.29 Disney v2 threshold metrics: primitive glass corridor `0.722s`, ratio
  `0.433`, secondary `5917`/`1383`, max radiance `4.904`; imported mesh
  material `2.008s`, ratio `0.128`, secondary `18674`/`6667`, max radiance
  `4.780`; imported mesh pressure MRT8 `1.897s`, ratio `0.220`, secondary
  `13894`/`4920`, max radiance `5.310`.
- D2.29 temporal-12 emissive stress report passed with `0` full-scan fallbacks
  at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_emissive_stress/d229_branch_separated_mis_temporal12/emissive_stress_report.json`.
- `promotion_ready` remains false. The next gate must repeat convergence and
  visual signoff, including review of the brighter current max-radiance scale,
  before any route-default decision.

Covered after D2.30:

- Repeated promotion/convergence report passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_stability/d230_branch_mis_repeats_r3/stability_report.json`.
- All three repeats passed hard gates and threshold gates while keeping
  `promotion_ready=false`.
- Disney v2 max radiance and participation counters were deterministic across
  repeats:
  - primitive glass corridor max radiance `4.903733882`
  - imported mesh material max radiance `4.779915613`
  - imported mesh pressure MRT8 max radiance `5.310446615`
- Elapsed timing was still noisy, especially primitive glass corridor
  Disney v2, so this is not yet a release-candidate timing signoff.
- Repeated temporal-12 emissive stress report passed with `0` full-scan
  fallbacks at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_emissive_stress/d230_branch_mis_repeats_r2_temporal12/emissive_stress_report.json`.
- Current D2.30 visual-readiness artifacts:
  - imported mesh route compare:
    `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/imported_mesh_pressure_mrt8/d230_branch_mis_promotion_route_compare/matrix_contact_sheet.png`
  - high-noise emitter denoise ablation:
    `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/high_noise_emitter/d230_branch_mis_denoise_ablation/matrix_contact_sheet.png`
- Visual-readiness interpretation: Disney v2's brighter current output is
  deterministic and reviewable; denoise is not the source of that brightness.
  Human/user visual signoff and physically complete BTDF/PTDF PDF work remain
  blockers before any default-route promotion.

Covered after D2.31:

- Disney v2 now replaces the bounded transmission PDF proxy with a physical
  BTDF/PTDF direct-light PDF estimator in
  `RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(...)`.
- The estimator uses explicit lobe probabilities from normalized principled
  material weights, requires opposite-hemisphere transmission support,
  handles entering/exiting eta, rejects total internal reflection, applies
  dielectric Fresnel transmission probability, and evaluates a GGX refractive
  half-vector PDF with the BTDF Jacobian.
- Focused transport tests cover positive normal refraction PDF, off-axis and
  roughness sensitivity, disabled transmission, and TIR rejection.
- D2.31 promotion report passed hard gates, performance thresholds, and
  quality thresholds at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_d231_physical_btdf_pdf_initial/promotion_gate_report.json`.
- D2.31 Disney v2 threshold metrics: primitive glass corridor `1.391s`, ratio
  `0.462`, secondary `5917`/`1383`, max radiance `0.552737781`; imported mesh
  material `3.787s`, ratio `0.120`, secondary `18674`/`6667`, max radiance
  `0.521580803`; imported mesh pressure MRT8 `3.720s`, ratio `0.237`,
  secondary `13894`/`4920`, max radiance `0.484088724`.
- D2.31 temporal-12 emissive stress passed with `0` full-scan fallbacks at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_emissive_stress/d231_physical_btdf_pdf_temporal12/emissive_stress_report.json`.
- `promotion_ready` remains false. D2.32 later found that the lower D2.31
  promotion-scene radiance scale is cwd-sensitive, so this report is not
  sufficient for route-default signoff.

Covered after D2.32:

- Repeat-3 convergence passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_stability/d232_physical_btdf_repeats_r3/stability_report.json`.
- The recurring `ray_tracing/` cwd repeat lane was deterministic at the bright
  D2.29/D2.30 scale: primitive `4.903733882`, imported material
  `4.779915613`, imported pressure MRT8 `5.310446615`.
- Explicit `CodeWork/` cwd repeats were deterministic at the lower D2.31 scale:
  primitive `0.552737781`, imported material `0.521580803`, imported pressure
  MRT8 `0.484088724`.
- Generated requests differed only by output paths and object audit/material
  ids matched. The remaining promotion blocker is therefore cwd-sensitive
  headless runtime/config resolution, not stochastic convergence noise.
- Refreshed visual review artifacts:
  - imported route compare:
    `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/imported_mesh_pressure_mrt8/d232_physical_btdf_route_compare/matrix_contact_sheet.png`
  - high-noise emitter denoise ablation:
    `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/high_noise_emitter/d232_physical_btdf_denoise_ablation/matrix_contact_sheet.png`
- `promotion_ready` remains false. The next gate must first make headless
  promotion/visual rendering cwd-invariant, then rerun repeat convergence and
  visual review before any route-default decision.

Covered after D2.33:

- Headless config loading is cwd-invariant for promotion/report runs while
  still preserving local runtime/config override precedence. Material presets,
  scene config, and animation config now fall back to stable workspace paths
  when relative cwd paths are absent.
- Paired cwd reports passed hard gates and threshold gates while matching
  Disney v2 max radiance exactly:
  - `CodeWork/` cwd:
    `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_d233_cwd_invariant_codework_confirm_v3/promotion_gate_report.json`
  - `ray_tracing/` cwd:
    `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_d233_cwd_invariant_ray_tracing_confirm_v3/promotion_gate_report.json`
  - primitive glass corridor max radiance `4.903733882`
  - imported mesh material max radiance `4.779915613`
  - imported mesh pressure MRT8 max radiance `5.310446615`
- Repeat-3 convergence from `CodeWork/` cwd passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_stability/d233_cwd_invariant_repeats_r3/stability_report.json`.
- The stable post-D2.33 promotion scale is the bright D2.29/D2.30/D2.32
  `ray_tracing/` cwd scale. The lower D2.31 scale is now classified as a
  cwd-sensitive config artifact.
- `promotion_ready` remains false. The next gate is visual signoff refresh at
  the fixed bright scale, then rough reflection/transmission convergence and
  release-candidate threshold repetition.

Covered after D2.34:

- Imported-mesh pressure MRT8 visual signoff was refreshed at the D2.33
  cwd-invariant bright scale.
- Route comparison artifacts:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/imported_mesh_pressure_mrt8/d233_visual_signoff_refresh_route_compare/`
  - `matrix_report.json` reported `passed=true`
  - `matrix_contact_sheet.png` and
    `comparisons/imported_mesh_disney_vs_disney_v2/side_by_side_disney_disney_v2_diff4x.png`
    are the review images
  - shipped `disney` and experimental `disney_v2` both rendered one frame with
    `16134` BVH triangles and `0` trace overflows
  - Disney v2 stayed on the fixed bright scale with max radiance
    `5.310446615`
  - route diff changed `10638/15360` pixels with max channel delta `100`
- Denoise ablation artifacts:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/imported_mesh_pressure_mrt8/d233_visual_signoff_refresh_denoise_ablation/`
  - `matrix_report.json` reported `passed=true`
  - off/on temporal-12 Disney v2 frames both reported max radiance
    `5.328458129`, `16134` BVH triangles, and `0` trace overflows
  - denoise-on reconstructed `10374` pixels while preserving mirror/glossy
    samples
  - denoise on/off diff changed `1628/15360` pixels, with only `1` pixel above
    delta `8` and max channel delta `12`
- Visual read:
  - Disney v2 is visibly brighter and more directly lit than shipped Disney on
    the imported runtime mesh pressure scene
  - denoise on/off is near-identical at contact-sheet scale with only small,
    localized differences
- Current status:
  - imported-mesh pressure MRT8 route-comparison signoff is refreshed and
    accepted for this proof boundary
  - `promotion_ready` remains false pending rough reflection/transmission
    convergence, release-candidate threshold repetition, and private/manual
    skull or other high-triangle signoff when pressure testing changes.

Covered after D2.35:

- Non-skull promotion blockers were re-run after D2.34 with skull/high-triangle
  proof explicitly skipped by operator direction.
- Focused rough/reflection/transmission-adjacent transport coverage passed:
  `TEST_RUNNER_GROUP=runtime_lighting_materials_transport .../test_runner`
  reported `TEST RESULT: PASS`.
- Baseline candidate promotion gate passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_release_candidate/d235_non_skull_baseline/promotion_gate_report.json`
  - `hard_gates_passed=true`
  - `performance_thresholds.passed=true`
  - `performance_passed=true`
  - `quality_passed=true`
  - Disney v2 scene thresholds passed for primitive glass corridor, imported
    mesh material, and imported mesh pressure MRT8
- Repeat-3 non-skull release-candidate stability passed at:
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_stability/d235_non_skull_release_candidate_repeats_r3/stability_report.json`
  - `passed=true`
  - `repeat_count=3`
  - all three promotion repeats reported `hard_gates_passed=true` and
    `thresholds_passed=true`
  - Disney v2 elapsed spread stayed bounded across the repeated proof scenes:
    primitive glass corridor `0.102`, imported mesh material `0.123`, and
    imported mesh pressure MRT8 `0.085`
- Current status:
  - rough reflection/transmission-focused transport coverage and
    release-candidate threshold repetition are closed for the current
    non-skull evidence lane
  - no renderer/source change was needed for this closeout
  - `promotion_ready` remains false because route-default promotion still
    requires an explicit policy decision and the skull/high-triangle proof was
    intentionally excluded from this slice.
