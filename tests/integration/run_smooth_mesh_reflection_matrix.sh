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
report = {
    "schema": "smooth_mesh_reflection_matrix_report_v1",
    "smooth_tlas_sha256": summaries[("smooth", "tlas_blas_parity")][1],
    "flat_tlas_sha256": summaries[("flat", "tlas_blas_parity")][1],
    "smooth_flattened_sha256": summaries[("smooth", "flattened_bvh")][1],
    "flat_flattened_sha256": summaries[("flat", "flattened_bvh")][1],
    "triangle_count": summaries[("smooth", "tlas_blas_parity")][0]["bvh_summary"]["triangle_count"],
}
(root / "matrix_report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
print("[smooth-mesh-reflection-matrix] PASS")
PY
