#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${SMOOTH_MESH_REFLECTION_MATRIX_OUT:-$ROOT/build/agent_runs/ray_tracing/smooth_mesh_reflection_matrix}"
GENERATOR="$ROOT/tools/smooth_mesh_reflection/generate_fixtures.py"
PREPARE="$ROOT/tools/smooth_mesh_reflection/prepare_reflection_matrix.py"
COMPILER="$ROOT/build/toolchains/clang/$(uname -m)/tools/smooth_mesh_reflection/compile_runtime_fixture"
RENDERER="$ROOT/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"

python3 "$PREPARE" --out-root "$OUT" --generator "$GENERATOR" --compiler "$COMPILER" --tier high >/dev/null
for mode in smooth flat; do
  for route in tlas_blas_parity flattened_bvh; do
    request="$OUT/request_${mode}_${route}.json"
    if [[ "$mode" == flat ]]; then
      RAY_TRACING_MESH_SHADING_MODE=flat "$RENDERER" --request "$request" --render \
        --summary "$OUT/renders/${mode}_${route}/render_summary.json" >/dev/null
    else
      "$RENDERER" --request "$request" --render \
        --summary "$OUT/renders/${mode}_${route}/render_summary.json" >/dev/null
    fi
  done
done

python3 - "$OUT" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
summaries = {}
for mode in ("smooth", "flat"):
    for route in ("tlas_blas_parity", "flattened_bvh"):
        run = root / "renders" / f"{mode}_{route}"
        summary = json.loads((run / "render_summary.json").read_text())
        frame = run / "frames" / "frame_0000.bmp"
        if not summary["rendered_frames"] or not frame.exists() or frame.stat().st_size <= 2:
            raise SystemExit(f"incomplete render: {mode}/{route}")
        if summary["prepared_acceleration"]["route_parity_mismatches"] != 0:
            raise SystemExit(f"route parity mismatch: {mode}/{route}")
        summaries[(mode, route)] = (summary, hashlib.sha256(frame.read_bytes()).hexdigest())
if summaries[("smooth", "tlas_blas_parity")][1] == summaries[("flat", "tlas_blas_parity")][1]:
    raise SystemExit("smooth and flat reflection images are unexpectedly identical")
smooth_render_ms = summaries[("smooth", "tlas_blas_parity")][0]["timing_breakdown"]["render_frames_ms"]
flat_render_ms = summaries[("flat", "tlas_blas_parity")][0]["timing_breakdown"]["render_frames_ms"]
assets = {}
for family in ("analytic_sphere", "icosphere", "organic_blob", "crease"):
    runtime_path = next((root / "assets" / "mesh_assets").glob(
        f"smooth_reflection_{family}_*.runtime.json"))
    mesh = json.loads(runtime_path.read_text())["mesh"]
    assets[family] = {
        "vertex_count": mesh["vertex_count"],
        "normal_count": mesh.get("normal_count", 0),
        "triangle_count": mesh["triangle_count"],
        "normal_provenance": mesh.get("normal_provenance", "none"),
    }
report = {
    "schema": "smooth_mesh_reflection_matrix_report_v1",
    "smooth_tlas_sha256": summaries[("smooth", "tlas_blas_parity")][1],
    "flat_tlas_sha256": summaries[("flat", "tlas_blas_parity")][1],
    "smooth_flattened_sha256": summaries[("smooth", "flattened_bvh")][1],
    "flat_flattened_sha256": summaries[("flat", "flattened_bvh")][1],
    "triangle_count": summaries[("smooth", "tlas_blas_parity")][0]["bvh_summary"]["triangle_count"],
    "assets": assets,
    "route_parity_mismatches": {
        f"{mode}_{route}": summary["prepared_acceleration"]["route_parity_mismatches"]
        for (mode, route), (summary, _) in summaries.items()
    },
    "timing_ms": {
        "smooth_render": smooth_render_ms,
        "flat_render": flat_render_ms,
        "smooth_total": summaries[("smooth", "tlas_blas_parity")][0]["timing_breakdown"]["total_run_ms"],
        "flat_total": summaries[("flat", "tlas_blas_parity")][0]["timing_breakdown"]["total_run_ms"],
    },
    "smooth_vs_flat_render_delta_percent": (
        ((smooth_render_ms - flat_render_ms) / flat_render_ms) * 100.0
        if flat_render_ms > 0.0 else None
    ),
}
(root / "matrix_report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
print("[smooth-mesh-reflection-matrix] PASS")
PY
