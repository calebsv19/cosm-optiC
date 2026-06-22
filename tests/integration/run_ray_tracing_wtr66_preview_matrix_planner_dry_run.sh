#!/usr/bin/env bash
set -euo pipefail

RAY_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
RUN_ROOT="$RAY_DIR/build/agent_runs/physics_trio/wtr66_preview_matrix_dry_run"

python3 "$RAY_DIR/tools/wtr66_preview_matrix_planner.py" \
  --matrix-root "$RUN_ROOT" \
  --matrix-slug wtr66_preview_matrix_dry_run \
  --overwrite

python3 - "$RUN_ROOT" <<'PY'
import json
import pathlib
import sys

run_root = pathlib.Path(sys.argv[1])
summary_path = run_root / "matrix_summary.json"
request_path = run_root / "matrix_request.json"
runtime_scene_path = run_root / "runtime_scene.json"

def require(condition, message):
    if not condition:
        raise SystemExit(message)

def load(path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)

summary = load(summary_path)
request = load(request_path)

require(summary["schema"] == "wtr66_preview_matrix_summary_v1", "wrong summary schema")
require(summary["execution_mode"] == "dry_run", "summary is not dry_run")
require(summary["physics_cache_count"] == 1, "expected one planned cache")
require(summary["render_variant_count"] >= 2, "expected at least two variants")
require(runtime_scene_path.exists(), "runtime_scene.json missing")
require(request["schema"] == "wtr66_preview_matrix_request_v1", "wrong request schema")

cache_reuse = summary["cache_reuse"].get("wtr65_reference_cache_120f")
require(cache_reuse is not None, "missing cache reuse record")
require(cache_reuse["variant_count"] >= 2, "expected at least two variants over one cache")

cache = summary["physics_caches"][0]
cache_root = pathlib.Path(cache["cache_root"])
cache_manifest_path = cache_root / "cache_manifest.json"
require(cache_manifest_path.exists(), "cache_manifest.json missing")
require(cache["status"] == "planned_not_run", "cache should not have run")
require(cache["frames_requested"] == 120, "unexpected planned cache frame count")
require(cache["review_ripple_amplitude_m"] == 0.035, "unexpected ripple amplitude")
require("--water-mode" in cache["planned_command"], "planned command missing water mode")
require("--save-volume-frames" in cache["planned_command"], "planned command missing volume export")

selected_sets = cache["selected_frame_sets"]
require(selected_sets["contact_short"]["frames"] == [12, 18, 24], "bad contact_short frames")
require(selected_sets["every_10"]["frames"] == list(range(10, 120, 10)), "bad every_10 frames")
for frame_set in selected_sets.values():
    require(pathlib.Path(frame_set["path"]).exists(), f"selected frame file missing: {frame_set['path']}")

variants = {variant["variant_slug"]: variant for variant in summary["render_variants"]}
for slug in ("direct_light_t1_contact_short", "disney_v2_t2_contact_short"):
    require(slug in variants, f"missing variant {slug}")
    variant = variants[slug]
    require(variant["status"] == "planned_not_run", f"{slug} should not have run")
    require(variant["cache_slug"] == "wtr65_reference_cache_120f", f"{slug} uses wrong cache")
    require(variant["selected_frames"] == [12, 18, 24], f"{slug} selected frames changed")
    request_dir = pathlib.Path(variant["request_dir"])
    request_files = sorted(request_dir.glob("ray_tracing_request_*.json"))
    require(len(request_files) == 3, f"{slug} should have 3 request files")
    first_request = load(request_files[0])
    require(first_request["render"]["start_frame"] == 12, f"{slug} first request frame mismatch")
    require(first_request["volume"]["source_kind"] == "scene_bundle", f"{slug} not scene_bundle-backed")
    require(first_request["volume"]["source_path"] == cache["scene_bundle_path"],
            f"{slug} does not reuse planned cache scene_bundle")
    require(first_request["scene"]["runtime_scene_path"] == str(runtime_scene_path),
            f"{slug} runtime scene path mismatch")

direct = variants["direct_light_t1_contact_short"]
disney = variants["disney_v2_t2_contact_short"]
require(direct["integrator_3d"] == "emission_transparency", "direct variant integrator mismatch")
require(direct["temporal_frames"] == 1, "direct variant temporal mismatch")
require(disney["integrator_3d"] == "disney_v2", "disney variant integrator mismatch")
require(disney["temporal_frames"] == 2, "disney variant temporal mismatch")

print(json.dumps({
    "status": "passed",
    "matrix_summary": str(summary_path),
    "cache_manifest": str(cache_manifest_path),
    "variant_count": summary["render_variant_count"],
}, indent=2))
PY
