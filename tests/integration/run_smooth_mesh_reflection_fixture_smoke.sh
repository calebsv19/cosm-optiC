#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GENERATOR="$ROOT/tools/smooth_mesh_reflection/generate_fixtures.py"
COMPILER="$ROOT/build/toolchains/clang/$(uname -m)/tools/smooth_mesh_reflection/compile_runtime_fixture"
OUT="${TMPDIR:-/private/tmp}/ray_tracing_smooth_mesh_reflection_fixture_smoke"

mkdir -p "$OUT/a" "$OUT/b"
for family in analytic_sphere icosphere organic_blob crease; do
  python3 "$GENERATOR" --family "$family" --tier unit --output "$OUT/a/$family.stl" \
    --authoring-output "$OUT/a/$family.authoring.json" >/dev/null
  python3 "$GENERATOR" --family "$family" --tier unit --output "$OUT/b/$family.stl" \
    --authoring-output "$OUT/b/$family.authoring.json" >/dev/null
  cmp "$OUT/a/$family.stl" "$OUT/b/$family.stl"
  cmp "$OUT/a/$family.summary.json" "$OUT/b/$family.summary.json"
  "$COMPILER" "$OUT/a/$family.authoring.json" / \
    "smooth_reflection_${family}_unit_runtime" "$OUT/a/$family.runtime.json" >/dev/null
done

python3 - "$OUT/a" <<'PY'
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
expected = {"analytic_sphere": 1280, "icosphere": 1280, "organic_blob": 1280, "crease": 12}
for family, triangles in expected.items():
    summary = json.loads((root / f"{family}.summary.json").read_text())
    if summary["triangle_count"] != triangles:
        raise SystemExit(f"{family}: expected {triangles} triangles, got {summary['triangle_count']}")
    if summary["stl_bytes"] <= 84:
        raise SystemExit(f"{family}: empty STL")
    runtime = json.loads((root / f"{family}.runtime.json").read_text())
    mesh = runtime["mesh"]
    if mesh["normal_count"] != mesh["vertex_count"]:
        raise SystemExit(f"{family}: incomplete runtime normals")
    expected_provenance = "generated_crease_aware" if family == "crease" else "generated_smooth"
    if mesh["normal_provenance"] != expected_provenance:
        raise SystemExit(f"{family}: unexpected provenance {mesh['normal_provenance']}")
print("[smooth-mesh-reflection-fixture-smoke] PASS")
PY
