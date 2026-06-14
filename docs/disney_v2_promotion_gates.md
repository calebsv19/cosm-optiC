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
   - Disney v2 does not inherit shipped Disney temporal denoise or pruning
     policy by accident.
2. Transport proof coverage
   - diffuse/specular stochastic participation remains covered by focused
     transport tests.
   - transmission/glass participation remains covered by focused transport
     tests.
   - recursive BSDF path-loop depth and roulette behavior remain covered.
   - finite-radius light-emitter accounting remains covered.
   - ordinary emissive-material surface-hit participation must be added before
     promotion.
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
7. Temporal policy
   - Disney v2 must get an explicit temporal denoise/pruning decision.
   - Current shipped Disney temporal policy must not be copied by default.

## Local Measurement Command

Build the headless CLI first:

```bash
make -C /Users/calebsv/Desktop/CodeWork/ray_tracing build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless
```

Then run the gate measurement:

```bash
/Users/calebsv/Desktop/CodeWork/ray_tracing/tools/measure_disney_v2_promotion_gates.py
```

The command renders the D2.5 primitive and imported-mesh proof scenes through
both `disney` and `disney_v2`, validates the hard machine-checkable gates, and
writes:

- `build/agent_runs/ray_tracing/disney_v2_d26_promotion_gates/promotion_gate_report.json`
- `build/agent_runs/ray_tracing/disney_v2_d26_promotion_gates/promotion_gate_report.md`

The report may pass the local hard gates while still reporting
`promotion_ready=false`. That is expected until the visual-review,
second-imported-mesh, emissive-material, temporal-policy, and performance
threshold blockers are resolved.

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

- add at least one more imported-mesh proof scene and document visible
  improvement or meaningful new behavior over shipped Disney
- decide Disney v2 temporal denoise/pruning policy explicitly
- add focused ordinary emissive-material surface-hit coverage
- set signed-off performance thresholds for promotion scenes
