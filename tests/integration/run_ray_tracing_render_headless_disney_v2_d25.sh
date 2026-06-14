#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"
if [[ ! -x "$CLI" ]]; then
  CLI="$ROOT_DIR/build/$(uname -m)/tools/cli/ray_tracing_render_headless"
fi

PROOF_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/disney_v2_d25"
PRIMITIVE_REQUEST="$ROOT_DIR/tests/fixtures/disney_v2_d25/primitive_glass_corridor_request.json"
MESH_REQUEST="$ROOT_DIR/tests/fixtures/disney_v2_d25/imported_mesh_material_request.json"
PRIMITIVE_OUT="$PROOF_ROOT/primitive_glass_corridor"
MESH_OUT="$PROOF_ROOT/imported_mesh_material"

mkdir -p "$PRIMITIVE_OUT" "$MESH_OUT"
"$CLI" --request "$PRIMITIVE_REQUEST" --render --summary "$PRIMITIVE_OUT/render_summary.json" > "$PRIMITIVE_OUT/stdout_summary.json"
"$CLI" --request "$MESH_REQUEST" --render --summary "$MESH_OUT/render_summary.json" > "$MESH_OUT/stdout_summary.json"

python3 - "$PRIMITIVE_OUT/render_summary.json" "$MESH_OUT/render_summary.json" <<'PY'
import json
import sys

primitive_summary_path, mesh_summary_path = sys.argv[1], sys.argv[2]

def load(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def assert_common(summary, run_id):
    assert summary["schema_version"] == "ray_tracing_headless_summary_v1"
    assert summary["run_id"] == run_id
    assert summary["integrator_3d"] == "disney_v2"
    assert summary["scene_applied"] is True
    assert summary["route_native_3d"] is True
    assert summary["prepared_frame"] is True
    assert summary["rendered_frames"] is True
    assert summary["frames_rendered"] == 1
    assert summary["render_stats"]["visible_pixels"] > 0
    assert summary["render_stats"]["nonzero_pixels"] > 0
    assert summary["outputs"]["first_frame_path"].endswith("frame_0000.bmp")
    assert len(summary["object_audit"]) > 0

primitive = load(primitive_summary_path)
assert_common(primitive, "disney_v2_d25_primitive_glass_corridor")
primitive_audit = {entry["object_id"]: entry for entry in primitive["object_audit"]}
for object_id in ("plane_floor", "prism_center", "prism_offset"):
    assert object_id in primitive_audit, object_id
    assert primitive_audit[object_id]["primary_hit_pixels"] >= 0
assert primitive_audit["prism_offset"]["alpha"] < 1.0
assert primitive["bvh_summary"]["triangle_count"] >= 0

mesh = load(mesh_summary_path)
assert_common(mesh, "disney_v2_d25_imported_mesh_material")
mesh_bvh = mesh["bvh_summary"]
assert mesh_bvh["ready"] is True
assert mesh_bvh["triangle_count"] > 0, mesh_bvh
assert mesh_bvh["node_count"] > 1, mesh_bvh
assert mesh_bvh["leaf_count"] > 1, mesh_bvh
assert mesh_bvh["trace_calls"] > 0, mesh_bvh
assert mesh_bvh["trace_overflows"] == 0, mesh_bvh
mesh_audit = {entry["object_id"]: entry for entry in mesh["object_audit"]}
assert mesh_audit["obj_sphere_pressure"]["triangle_count"] > 0
assert mesh_audit["obj_sphere_pressure"]["triangle_count"] < mesh_bvh["triangle_count"]
assert mesh_audit["obj_sphere_pressure"]["primary_hit_pixels"] > 0
assert mesh_audit["obj_sphere_pressure"]["roughness"] <= 0.30
PY

test -s "$PRIMITIVE_OUT/frames/frame_0000.bmp"
test -s "$MESH_OUT/frames/frame_0000.bmp"
test "$(dd if="$PRIMITIVE_OUT/frames/frame_0000.bmp" bs=1 count=2 2>/dev/null)" = "BM"
test "$(dd if="$MESH_OUT/frames/frame_0000.bmp" bs=1 count=2 2>/dev/null)" = "BM"
