# Render Review Sets

Repo-local detached render-review artifacts for `ray_tracing` live here.

This lane is for local docs and review writeups inside the repository. It is
not the live visualizer website pipeline. Live website publication goes through
the `visualizer-run/v1` staging/import flow owned by
`skills/codework-visualizer-drop/`.

Use this lane for:

- one authored scene state
- one detached render request
- one selected output frame for repo-doc inspection
- one copied render summary for downstream review

Typical contents per set:

- `request.json`
- `preview.bmp`
- optional `preview.png`
- `summary.json`
- `index.md`

These sets are intended to mirror one completed detached run in a stable
repo-doc form without keeping the full private run root exposed.

For multi-cell proof matrices, use the private visual-matrix lane instead of
copying every generated frame into repo docs. The runner is:

```bash
ray_tracing/tests/integration/run_ray_tracing_visual_matrix.py \
  --manifest ray_tracing/tests/fixtures/disney_v2_visual_matrix/<matrix_id>/matrix_manifest.json
```

The Disney v2 private matrix outputs are written under:

```text
_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/
```

Each matrix package contains `matrix_report.json`, `index.md`, a contact sheet,
per-cell PNGs, copied requests/summaries, and comparison diff metrics.

Current private matrix ids include:

- `primitive_glass_corridor`
- `transparent_interior_stack`
- `mirror_glossy_preservation`
- `high_noise_emitter`
- `imported_mesh_pressure_mrt8`
- `mirror_surface_unification`
- `skull_high_triangle_local`

The skull/high-triangle matrix is prepared into the private artifact tree and
is not a shipped repo-local fixture. Recurring gate coverage should stay on the
low/moderate imported-mesh matrices unless a specific pressure test needs the
skull scene.

Current published sets:

- `disney_v2_d218_denoise_on_off_visual_proof/`
- `disney_v2_d218_denoise_visual_proof/`
- `disney_v2_d25_canonical_proofs/`
- `grime_screen_motion_review_v2/`
