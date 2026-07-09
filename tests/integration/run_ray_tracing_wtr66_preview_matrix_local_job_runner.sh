#!/usr/bin/env bash
set -euo pipefail

RAY_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CODEWORK_ROOT="$(cd "$RAY_DIR/.." && pwd)"
PHYSICS_DIR="$CODEWORK_ROOT/physics_sim"
RUN_ROOT="$RAY_DIR/build/agent_runs/physics_trio/wtr66_preview_matrix_local_job_runner"
JOBS_ROOT="$RUN_ROOT/jobs"
RUNNER="$RAY_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_job_runner"
if [[ ! -x "$RUNNER" ]]; then
  RUNNER="$RAY_DIR/build/$(uname -m)/tools/cli/ray_tracing_job_runner"
fi

if [[ ! -x "$RUNNER" ]]; then
  echo "missing ray_tracing_job_runner at $RUNNER" >&2
  exit 1
fi

make -C "$PHYSICS_DIR" physics_sim_headless

python3 "$RAY_DIR/tools/wtr66_preview_matrix_planner.py" \
  --matrix-root "$RUN_ROOT" \
  --matrix-slug wtr66_preview_matrix_local_job_runner \
  --overwrite

python3 - "$RUN_ROOT/matrix_summary.json" <<'PY'
import json
import subprocess
import sys

summary_path = sys.argv[1]
with open(summary_path, "r", encoding="utf-8") as f:
    summary = json.load(f)

caches = summary.get("physics_caches") or []
if len(caches) != 1:
    raise SystemExit("expected exactly one planned cache")

command = caches[0].get("planned_command") or []
if not command:
    raise SystemExit("cache manifest missing planned_command")

subprocess.run(command, check=True)
PY

wait_for_job() {
  local job_id="$1"
  local status_json
  for _ in $(seq 1 120); do
    status_json="$("$RUNNER" status --job-id "$job_id" --jobs-root "$JOBS_ROOT")"
    if printf '%s' "$status_json" | grep -q '"state": "completed"'; then
      return 0
    fi
    if printf '%s' "$status_json" | grep -q '"state": "failed"'; then
      echo "$status_json" >&2
      return 1
    fi
    sleep 1
  done
  echo "timed out waiting for job $job_id" >&2
  return 1
}

submit_request() {
  local request="$1"
  local submit_output
  submit_output="$("$RUNNER" submit --request "$request" --jobs-root "$JOBS_ROOT" --overwrite)"
  printf '%s' "$submit_output" | sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p'
}

mkdir -p "$JOBS_ROOT"
rm -f "$RUN_ROOT/render_request_manifest.tsv" "$RUN_ROOT/submitted_jobs.tsv"

python3 - "$RUN_ROOT/matrix_summary.json" >"$RUN_ROOT/render_request_manifest.tsv" <<'PY'
import json
import pathlib
import sys

summary_path = pathlib.Path(sys.argv[1])
summary = json.loads(summary_path.read_text(encoding="utf-8"))
for variant in summary.get("render_variants") or []:
    request_dir = pathlib.Path(variant["request_dir"])
    for request in sorted(request_dir.glob("ray_tracing_request_*.json")):
        print(f"{variant['variant_slug']}\t{request}")
PY

while IFS=$'\t' read -r _variant request; do
  [[ -n "$request" ]]
  job_id="$(submit_request "$request")"
  if [[ -z "$job_id" ]]; then
    echo "failed to submit $request" >&2
    exit 1
  fi
  printf '%s\t%s\n' "$request" "$job_id" >>"$RUN_ROOT/submitted_jobs.tsv"
  wait_for_job "$job_id"
done <"$RUN_ROOT/render_request_manifest.tsv"

python3 - "$RUN_ROOT" "$JOBS_ROOT" <<'PY'
import json
import pathlib
import sys

run_root = pathlib.Path(sys.argv[1])
jobs_root = pathlib.Path(sys.argv[2])
summary = json.loads((run_root / "matrix_summary.json").read_text(encoding="utf-8"))

def require(condition, message):
    if not condition:
        raise SystemExit(message)

def load(path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)

submitted = {}
for line in (run_root / "submitted_jobs.tsv").read_text(encoding="utf-8").splitlines():
    request, job_id = line.split("\t")
    submitted[pathlib.Path(request)] = job_id

cache = summary["physics_caches"][0]
scene_bundle = pathlib.Path(cache["scene_bundle_path"])
water_manifest = pathlib.Path(cache["water_manifest_path"])
require(scene_bundle.exists(), "executed cache missing scene_bundle.json")
require(water_manifest.exists(), "executed cache missing water_manifest_v1.json")

variant_reports = []
for variant in summary["render_variants"]:
    request_dir = pathlib.Path(variant["request_dir"])
    frames = []
    object_hits = []
    secondary_hits = []
    job_ids = []
    for request_path in sorted(request_dir.glob("ray_tracing_request_*.json")):
        request = load(request_path)
        frame = int(request["render"]["start_frame"])
        job_id = submitted.get(request_path)
        require(job_id, f"missing submitted job for {request_path}")
        job_root = jobs_root / job_id
        job_status = load(job_root / "job_status.json")
        result = load(job_root / "result_summary.json")
        require(job_status["state"] == "completed", f"{job_id} not completed")
        require(job_status["overwrite_policy"] == "overwrite", f"{job_id} not overwrite policy")
        require(result["rendered_frames"] is True, f"{job_id} did not render")
        require(result["integrator_3d"] == variant["integrator_3d"], f"{job_id} wrong integrator")
        require(result["render"]["temporal_frames"] == variant["temporal_frames"],
                f"{job_id} wrong temporal frame count")
        water = result.get("water_surface") or {}
        require(water.get("loaded") is True, f"{job_id} did not load water")
        require(water.get("mesh_attached") is True, f"{job_id} did not attach water mesh")
        require(water.get("loaded_first_frame_index") == frame, f"{job_id} loaded wrong water frame")
        audit = result.get("object_audit") or []
        block = next((entry for entry in audit if entry.get("object_id") == "water_coupled_block"), None)
        require(block is not None, f"{job_id} missing block audit")
        hit_pixels = int(block.get("primary_hit_pixels", 0))
        require(hit_pixels > 0, f"{job_id} block not visible")
        stats = result.get("render_stats") or {}
        secondary = int(stats.get("secondary_hits", 0))
        require(secondary > 0, f"{job_id} missing secondary hits")
        frame_path = pathlib.Path(variant["frame_dir"]) / f"frame_{frame:04d}.bmp"
        require(frame_path.exists(), f"{job_id} missing frame {frame_path}")
        frames.append(frame)
        object_hits.append(hit_pixels)
        secondary_hits.append(secondary)
        job_ids.append(job_id)
    variant_report = {
        "variant_slug": variant["variant_slug"],
        "cache_slug": variant["cache_slug"],
        "job_ids": job_ids,
        "frames": frames,
        "integrator_3d": variant["integrator_3d"],
        "temporal_frames": variant["temporal_frames"],
        "object_primary_hit_pixels": object_hits,
        "secondary_hits": secondary_hits,
        "frame_dir": variant["frame_dir"],
    }
    variant_reports.append(variant_report)
    variant_summary_path = pathlib.Path(run_root / "render_variants" / variant["variant_slug"] / "variant_summary.json")
    variant_summary = load(variant_summary_path)
    variant_summary["execution_mode"] = "local_job_runner"
    variant_summary["status"] = "completed"
    variant_summary["job_ids"] = job_ids
    variant_summary["object_primary_hit_pixels"] = object_hits
    variant_summary["secondary_hits"] = secondary_hits
    variant_summary_path.write_text(json.dumps(variant_summary, indent=2) + "\n", encoding="utf-8")

require(len(variant_reports) >= 2, "expected at least two completed variants")
cache_usage = summary["cache_reuse"]["wtr65_reference_cache_120f"]
require(cache_usage["variant_count"] >= 2, "cache reuse not recorded")

execution_summary = {
    "schema": "wtr66_local_job_runner_matrix_execution_summary_v1",
    "status": "completed",
    "matrix_root": str(run_root),
    "jobs_root": str(jobs_root),
    "cache_slug": cache["cache_slug"],
    "scene_bundle_path": str(scene_bundle),
    "water_manifest_path": str(water_manifest),
    "render_variant_count": len(variant_reports),
    "render_variants": variant_reports,
    "readback_contract": {
        "job_status_completed": True,
        "result_summary_rendered_frames": True,
        "integrator_validated": True,
        "temporal_frames_validated": True,
        "water_mesh_attached": True,
        "block_primary_hits_nonzero": True,
        "secondary_hits_nonzero": True,
    },
}
(run_root / "local_job_runner_execution_summary.json").write_text(
    json.dumps(execution_summary, indent=2) + "\n", encoding="utf-8")

summary["execution_mode"] = "local_job_runner"
summary["status"] = "completed"
summary["local_job_runner_execution_summary"] = str(run_root / "local_job_runner_execution_summary.json")
(run_root / "matrix_summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

print(json.dumps(execution_summary, indent=2))
PY
