#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
CLI="$(ray_tracing_tool_path ray_tracing_render_headless "$ROOT_DIR")"

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_preflight_request.json"
WORK_ROOT="$(ray_tracing_test_reset_work_root preflight_smoke "$ROOT_DIR")"
SUMMARY="$WORK_ROOT/render_summary.json"
OBJECT_MOTION_SUMMARY="$WORK_ROOT/object_motion_summary.json"
LEDGER_ENABLED_SUMMARY="$WORK_ROOT/frame_dataflow_enabled_summary.json"
LEDGER_TLAS_SKIP_SUMMARY="$WORK_ROOT/frame_dataflow_tlas_skip_summary.json"
LEDGER_TLAS_SKIP_DISABLED_SUMMARY="$WORK_ROOT/frame_dataflow_tlas_skip_disabled_summary.json"
LEDGER_PARITY_SKIP_SUMMARY="$WORK_ROOT/frame_dataflow_parity_skip_summary.json"
LEDGER_FLATTENED_SKIP_SUMMARY="$WORK_ROOT/frame_dataflow_flattened_skip_summary.json"
STDOUT_SUMMARY="$WORK_ROOT/stdout_summary.json"
NEGATIVE_ROOT="$WORK_ROOT/negative_requests"
DIAG_ROOT="$WORK_ROOT/diagnostics"
ROUTE_ROOT="$WORK_ROOT/route_requests"

mkdir -p "$NEGATIVE_ROOT" "$DIAG_ROOT" "$ROUTE_ROOT"
"$CLI" --request "$REQUEST" --preflight --summary "$SUMMARY" > "$STDOUT_SUMMARY"

grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY"
grep -q '"scene_applied": true' "$SUMMARY"
grep -q '"route_native_3d": true' "$SUMMARY"
grep -q '"prepared_frame": true' "$SUMMARY"
grep -q '"integrator_3d": "emission_transparency"' "$SUMMARY"
grep -q '"preset": "glass_preview"' "$SUMMARY"
grep -q '"has_secondary_diffuse_samples_3d_override": true' "$SUMMARY"
grep -q '"secondary_diffuse_samples_3d": 8' "$SUMMARY"
grep -q '"has_transmission_samples_3d_override": true' "$SUMMARY"
grep -q '"transmission_samples_3d": 4' "$SUMMARY"
grep -q '"has_background_brightness_override": true' "$SUMMARY"
grep -q '"background_brightness": 0.420000000' "$SUMMARY"
grep -q '"environment_lighting": {' "$SUMMARY"
grep -q '"mode": "ambient"' "$SUMMARY"
grep -q '"preset": "sky"' "$SUMMARY"
grep -q '"background_brightness_source": "background_brightness"' "$SUMMARY"
grep -q '"background_miss_contributes": true' "$SUMMARY"
grep -q '"registered_lights": {' "$SUMMARY"
grep -q '"light_count": 2' "$SUMMARY"
grep -q '"enabled_count": 2' "$SUMMARY"
grep -q '"shape_counts": { "point": 0, "sphere": 1, "disk": 0, "rect": 1, "mesh_emissive": 0 }' "$SUMMARY"
grep -q '"source_counts": { "authored": 0, "compatibility": 1, "material_emitter": 1 }' "$SUMMARY"
grep -q '"object_motion": {' "$SUMMARY"
grep -q '"has_object_motion_tracks": false' "$SUMMARY"
grep -q '"total_tracks": 0' "$SUMMARY"
grep -q '"diagnostics": "object_motion_tracks_missing"' "$SUMMARY"
grep -q '"active_trace_route": "tlas_blas"' "$SUMMARY"
grep -q '"requested_trace_route": "tlas_blas"' "$SUMMARY"
grep -q '"trace_context_stats_owned": true' "$SUMMARY"
grep -q '"trace_context_callback_bound": true' "$SUMMARY"
grep -q '"route_trace_calls": 0' "$SUMMARY"
grep -q '"route_parity_mismatches": 0' "$SUMMARY"
grep -q '"object_motion_acceleration": {' "$SUMMARY"
grep -q '"authored_rigid_motion_tracks": 0' "$SUMMARY"
grep -q '"moving_object_tlas_policy": "static_scene_or_prepared_cache_reuse"' "$SUMMARY"
grep -q '"flattened_fallback_available": true' "$SUMMARY"

python3 - "$REQUEST" "$WORK_ROOT/object_motion_scene_runtime.json" "$WORK_ROOT/object_motion_request.json" "$WORK_ROOT/object_motion_output" <<'PY'
import json
import pathlib
import sys

request_path, scene_dst, request_dst, output_root = sys.argv[1:5]
request = json.load(open(request_path, "r", encoding="utf-8"))
scene_path = pathlib.Path(request["scene"]["runtime_scene_path"])
if not scene_path.is_absolute():
    scene_path = (pathlib.Path(request_path).parent / scene_path).resolve()
scene = json.load(open(scene_path, "r", encoding="utf-8"))
objects = scene.get("objects") or []
matched_id = objects[0].get("object_id") if objects else "object_motion_fixture_object"
scene.setdefault("extensions", {}).setdefault("ray_tracing", {}).setdefault("authoring", {})[
    "object_motion_tracks"
] = [
    {
        "object_id": matched_id,
        "enabled": True,
        "mode": "authored_path",
        "timing": {"domain": "normalized_t", "wrap": "hold"},
        "path": {
            "mode": "BEZIER_CUBIC",
            "points": [
                {"x": 0.0, "y": 0.0, "z": 0.0},
                {"x": 0.25, "y": 0.0, "z": 0.0},
            ],
        },
        "rotation_keyframes": [
            {"t": 0.0, "yaw_degrees": 0.0, "pitch_degrees": 0.0, "roll_degrees": 0.0},
            {"t": 1.0, "yaw_degrees": 5.0, "pitch_degrees": 0.0, "roll_degrees": 0.0},
        ],
    },
    {
        "object_id": "missing_object_motion_fixture",
        "enabled": True,
        "mode": "authored_path",
        "timing_domain": "frame",
        "wrap": "clamp",
    },
]
with open(scene_dst, "w", encoding="utf-8") as f:
    json.dump(scene, f, indent=2)
    f.write("\n")
request["scene"]["runtime_scene_path"] = scene_dst
request["render"]["frame_count"] = 3
request["output"]["root"] = output_root
request["progress"]["summary_path"] = f"{output_root}/render_summary.json"
request["progress"]["progress_path"] = f"{output_root}/render_progress.json"
with open(request_dst, "w", encoding="utf-8") as f:
    json.dump(request, f, indent=2)
    f.write("\n")
PY

"$CLI" --request "$WORK_ROOT/object_motion_request.json" --preflight \
  --summary "$OBJECT_MOTION_SUMMARY" > "$WORK_ROOT/object_motion_stdout_summary.json"

grep -q '"object_motion": {' "$OBJECT_MOTION_SUMMARY"
grep -q '"has_object_motion_tracks": true' "$OBJECT_MOTION_SUMMARY"
grep -q '"total_tracks": 2' "$OBJECT_MOTION_SUMMARY"
grep -q '"enabled_tracks": 2' "$OBJECT_MOTION_SUMMARY"
grep -q '"matched_tracks": 1' "$OBJECT_MOTION_SUMMARY"
grep -q '"unmatched_tracks": 1' "$OBJECT_MOTION_SUMMARY"
grep -q '"position_path_tracks": 1' "$OBJECT_MOTION_SUMMARY"
grep -q '"rotation_keyframe_tracks": 1' "$OBJECT_MOTION_SUMMARY"
grep -q '"sampled_tracks": 1' "$OBJECT_MOTION_SUMMARY"
grep -q '"has_executable_motion": true' "$OBJECT_MOTION_SUMMARY"
grep -q '"diagnostics": "ok"' "$OBJECT_MOTION_SUMMARY"
grep -q '"object_motion_acceleration": {' "$OBJECT_MOTION_SUMMARY"
grep -q '"authored_rigid_motion_tracks": 1' "$OBJECT_MOTION_SUMMARY"
grep -q '"authored_rigid_primitive_motion_tracks": 1' "$OBJECT_MOTION_SUMMARY"
grep -q '"moving_object_tlas_policy": "rebuild_tlas_per_sample_until_refit_contract_lands"' "$OBJECT_MOTION_SUMMARY"
grep -q '"prepared_scene_time_dependent_required": true' "$OBJECT_MOTION_SUMMARY"
grep -q '"tlas_rebuild_required": true' "$OBJECT_MOTION_SUMMARY"
grep -q '"tlas_refit_available": false' "$OBJECT_MOTION_SUMMARY"
grep -q '"normalized_t_changes_across_frames": true' "$OBJECT_MOTION_SUMMARY"

python3 - "$REQUEST" "$WORK_ROOT/object_motion_mesh_scene_runtime.json" "$WORK_ROOT/object_motion_mesh_request.json" "$WORK_ROOT/object_motion_mesh_output" "$ROOT_DIR" <<'PY'
import json
import pathlib
import sys

request_path, scene_dst, request_dst, output_root, root_dir = sys.argv[1:6]
mesh_path = pathlib.Path(root_dir) / "tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json"
request = json.load(open(request_path, "r", encoding="utf-8"))
scene = {
    "schema_family": "codework_scene",
    "schema_variant": "scene_runtime_v1",
    "schema_version": 1,
    "scene_id": "scene_object_motion_mesh_accel_policy",
    "unit_system": "meters",
    "world_scale": 1.0,
    "space_mode_default": "3d",
    "objects": [
        {
            "object_id": "mesh_glider",
            "object_type": "mesh_asset_instance",
            "dimensional_mode": "full_3d",
            "transform": {
                "position": {"x": 0.0, "y": 0.0, "z": 0.0},
                "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                "scale": {"x": 0.5, "y": 0.5, "z": 0.5},
            },
            "geometry_ref": {"kind": "mesh_asset", "id": "asset_sphere_8x4"},
            "extensions": {"line_drawing": {"runtime_mesh_path": str(mesh_path)}},
        }
    ],
    "materials": [],
    "lights": [],
    "cameras": [],
    "constraints": [],
    "extensions": {
        "ray_tracing": {
            "authoring": {
                "object_motion_tracks": [
                    {
                        "object_id": "mesh_glider",
                        "enabled": True,
                        "mode": "authored_path",
                        "timing": {"domain": "normalized_t", "wrap": "hold"},
                        "path": {
                            "mode": "BEZIER_CUBIC",
                            "points": [
                                {"x": 0.0, "y": 0.0, "z": 0.0},
                                {"x": 1.0, "y": 0.0, "z": 0.0},
                            ],
                        },
                    }
                ]
            }
        }
    },
}
with open(scene_dst, "w", encoding="utf-8") as f:
    json.dump(scene, f, indent=2)
    f.write("\n")
request["scene"]["runtime_scene_path"] = scene_dst
request["render"]["frame_count"] = 3
request["output"]["root"] = output_root
request["progress"]["summary_path"] = f"{output_root}/render_summary.json"
request["progress"]["progress_path"] = f"{output_root}/render_progress.json"
with open(request_dst, "w", encoding="utf-8") as f:
    json.dump(request, f, indent=2)
    f.write("\n")
PY

"$CLI" --request "$WORK_ROOT/object_motion_mesh_request.json" --preflight \
  --summary "$WORK_ROOT/object_motion_mesh_summary.json" \
  > "$WORK_ROOT/object_motion_mesh_stdout_summary.json"

grep -q '"authored_rigid_motion_tracks": 1' "$WORK_ROOT/object_motion_mesh_summary.json"
grep -q '"authored_rigid_mesh_motion_tracks": 1' "$WORK_ROOT/object_motion_mesh_summary.json"
grep -q '"moving_mesh_asset_instances": 1' "$WORK_ROOT/object_motion_mesh_summary.json"
grep -q '"mesh_local_blas_identity_reusable": true' "$WORK_ROOT/object_motion_mesh_summary.json"
grep -q '"mesh_local_blas_policy": "reuse_mesh_asset_local_blas_for_rigid_instance_motion"' "$WORK_ROOT/object_motion_mesh_summary.json"
grep -q '"static_mesh_asset_instances": 0' "$WORK_ROOT/object_motion_mesh_summary.json"
grep -q '"normalized_t_changes_across_frames": true' "$WORK_ROOT/object_motion_mesh_summary.json"

RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER=1 \
  "$CLI" --request "$REQUEST" --preflight --summary "$LEDGER_ENABLED_SUMMARY" \
  > "$WORK_ROOT/frame_dataflow_enabled_stdout_summary.json"

RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER=1 \
RAY_TRACING_NATIVE3D_SKIP_FLATTENED_BVH_ON_TLAS=1 \
  "$CLI" --request "$REQUEST" --preflight --summary "$LEDGER_TLAS_SKIP_SUMMARY" \
  > "$WORK_ROOT/frame_dataflow_tlas_skip_stdout_summary.json"

RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER=1 \
RAY_TRACING_NATIVE3D_DISABLE_DEFAULT_TLAS_BVH_SKIP=1 \
  "$CLI" --request "$REQUEST" --preflight --summary "$LEDGER_TLAS_SKIP_DISABLED_SUMMARY" \
  > "$WORK_ROOT/frame_dataflow_tlas_skip_disabled_stdout_summary.json"

python3 - "$REQUEST" "$ROUTE_ROOT/parity_request.json" tlas_blas_parity "$ROOT_DIR/config/samples/ps4d_runtime_scene_visual_test.json" "$WORK_ROOT/parity_route_output" <<'PY'
import json
import sys

src, dst, route, scene_path, output_root = sys.argv[1:6]
request = json.load(open(src, "r", encoding="utf-8"))
request["scene"]["runtime_scene_path"] = scene_path
request.setdefault("inspection", {})["trace_route"] = route
request["output"]["root"] = output_root
request["progress"]["summary_path"] = f"{output_root}/render_summary.json"
request["progress"]["progress_path"] = f"{output_root}/render_progress.json"
with open(dst, "w", encoding="utf-8") as f:
    json.dump(request, f, indent=2)
    f.write("\n")
PY

python3 - "$REQUEST" "$ROUTE_ROOT/flattened_request.json" flattened_bvh "$ROOT_DIR/config/samples/ps4d_runtime_scene_visual_test.json" "$WORK_ROOT/flattened_route_output" <<'PY'
import json
import sys

src, dst, route, scene_path, output_root = sys.argv[1:6]
request = json.load(open(src, "r", encoding="utf-8"))
request["scene"]["runtime_scene_path"] = scene_path
request.setdefault("inspection", {})["trace_route"] = route
request["output"]["root"] = output_root
request["progress"]["summary_path"] = f"{output_root}/render_summary.json"
request["progress"]["progress_path"] = f"{output_root}/render_progress.json"
with open(dst, "w", encoding="utf-8") as f:
    json.dump(request, f, indent=2)
    f.write("\n")
PY

RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER=1 \
RAY_TRACING_NATIVE3D_SKIP_FLATTENED_BVH_ON_TLAS=1 \
  "$CLI" --request "$ROUTE_ROOT/parity_request.json" --preflight \
  --summary "$LEDGER_PARITY_SKIP_SUMMARY" \
  > "$WORK_ROOT/frame_dataflow_parity_skip_stdout_summary.json"

RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER=1 \
RAY_TRACING_NATIVE3D_SKIP_FLATTENED_BVH_ON_TLAS=1 \
  "$CLI" --request "$ROUTE_ROOT/flattened_request.json" --preflight \
  --summary "$LEDGER_FLATTENED_SKIP_SUMMARY" \
  > "$WORK_ROOT/frame_dataflow_flattened_skip_stdout_summary.json"

python3 - "$SUMMARY" "$LEDGER_ENABLED_SUMMARY" "$LEDGER_TLAS_SKIP_SUMMARY" "$LEDGER_TLAS_SKIP_DISABLED_SUMMARY" "$LEDGER_PARITY_SKIP_SUMMARY" "$LEDGER_FLATTENED_SKIP_SUMMARY" <<'PY'
import json
import sys

disabled = json.load(open(sys.argv[1], "r", encoding="utf-8"))
enabled = json.load(open(sys.argv[2], "r", encoding="utf-8"))
tlas_skip = json.load(open(sys.argv[3], "r", encoding="utf-8"))
tlas_skip_disabled = json.load(open(sys.argv[4], "r", encoding="utf-8"))
parity_skip = json.load(open(sys.argv[5], "r", encoding="utf-8"))
flattened_skip = json.load(open(sys.argv[6], "r", encoding="utf-8"))

disabled_ledger = disabled["frame_dataflow_state_ledger"]
enabled_ledger = enabled["frame_dataflow_state_ledger"]
tlas_skip_ledger = tlas_skip["frame_dataflow_state_ledger"]
tlas_skip_disabled_ledger = tlas_skip_disabled["frame_dataflow_state_ledger"]
parity_skip_ledger = parity_skip["frame_dataflow_state_ledger"]
flattened_skip_ledger = flattened_skip["frame_dataflow_state_ledger"]
disabled_dataflow = disabled_ledger["prepare_and_scene_state"]["prepared_scene_copy_bind_dataflow"]
enabled_dataflow = enabled_ledger["prepare_and_scene_state"]["prepared_scene_copy_bind_dataflow"]
tlas_skip_dataflow = tlas_skip_ledger["prepare_and_scene_state"]["prepared_scene_copy_bind_dataflow"]
tlas_skip_disabled_dataflow = tlas_skip_disabled_ledger["prepare_and_scene_state"]["prepared_scene_copy_bind_dataflow"]
parity_skip_dataflow = parity_skip_ledger["prepare_and_scene_state"]["prepared_scene_copy_bind_dataflow"]
flattened_skip_dataflow = flattened_skip_ledger["prepare_and_scene_state"]["prepared_scene_copy_bind_dataflow"]
enabled_scratch = enabled_ledger["scratch_and_output_estimates"]["render_unit_scratch_state"]
enabled_scheduler = enabled_ledger["tile_and_temporal_state"]["scheduler_lifetime_and_cancellation"]
enabled_presentation = enabled_ledger["presentation_and_output_state"]
enabled_snapshot = enabled_ledger["render_request_snapshot_state"]

assert disabled_ledger["enabled"] is False
assert disabled_ledger["schema_version"] == "frame_dataflow_state_ledger_v1"
assert disabled_ledger["activation_env"] == "RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER"
assert disabled_dataflow["stats_enabled"] is False

assert enabled_ledger["enabled"] is True
assert enabled_ledger["flattened_bvh_skip_on_tlas_force_env"] == "RAY_TRACING_NATIVE3D_SKIP_FLATTENED_BVH_ON_TLAS"
assert enabled_ledger["flattened_bvh_skip_on_tlas_default_disable_env"] == "RAY_TRACING_NATIVE3D_DISABLE_DEFAULT_TLAS_BVH_SKIP"
assert enabled_ledger["render_request"]["pixel_count_per_frame"] == 160 * 96
assert enabled_snapshot["snapshot_type"] == "RuntimeNative3DRenderRequestSnapshot"
assert enabled_snapshot["valid"] is True
assert enabled_snapshot["generation_bound"] is False
assert enabled_snapshot["generation"] == 0
assert enabled_snapshot["dimensions"]["output_width"] == 160
assert enabled_snapshot["dimensions"]["output_height"] == 96
assert enabled_snapshot["dimensions"]["render_width"] == 160
assert enabled_snapshot["dimensions"]["render_height"] == 96
assert enabled_snapshot["dimensions"]["host_width"] == 160
assert enabled_snapshot["dimensions"]["host_height"] == 96
assert enabled_snapshot["sampling"]["sampling_bound"] is True
assert enabled_snapshot["sampling"]["frame_count"] == 1
assert enabled_snapshot["sampling"]["temporal_frames"] == 1
assert enabled_snapshot["sampling"]["tile_size"] > 0
assert enabled_snapshot["sampling"]["integrator_3d"] == "emission_transparency"
assert enabled_snapshot["resource_budget"]["bound"] is False
assert enabled_snapshot["scene_snapshot"]["prepared_frame_bound"] is True
assert enabled_snapshot["scene_snapshot"]["prepared_frame_valid"] is True
assert enabled_snapshot["scene_snapshot"]["prepared_frame_width"] == 160
assert enabled_snapshot["scene_snapshot"]["prepared_frame_height"] == 96
assert enabled_snapshot["scene_snapshot"]["prepared_primitive_count"] > 0
assert enabled_snapshot["scene_snapshot"]["prepared_triangle_count"] > 0
assert enabled_snapshot["scene_snapshot"]["material_snapshot_bound"] is True
assert enabled_snapshot["scene_snapshot"]["material_count"] >= 0
assert enabled_snapshot["scene_snapshot"]["material_object_binding_count"] > 0
assert enabled_snapshot["scene_snapshot"]["light_snapshot_bound"] is True
assert enabled_snapshot["scene_snapshot"]["enabled_light_count"] == 2
assert enabled_snapshot["acceleration_snapshot"]["scene_acceleration_bound"] is True
assert enabled_snapshot["acceleration_snapshot"]["trace_route"] == "tlas_blas"
assert enabled_snapshot["acceleration_snapshot"]["tlas_instance_count"] > 0
assert enabled_snapshot["acceleration_snapshot"]["trace_context_callback_bound"] is True
assert enabled_snapshot["volume_water_snapshot"]["volume_enabled"] is False
assert enabled_snapshot["volume_water_snapshot"]["volume_attached"] is False
assert enabled_snapshot["diagnostics_and_output_snapshot"]["frame_dataflow_ledger_enabled"] is True
assert enabled_snapshot["diagnostics_and_output_snapshot"]["output_root_bound"] is True
assert enabled_snapshot["diagnostics_and_output_snapshot"]["summary_destination_bound"] is True
assert enabled_snapshot["diagnostics_and_output_snapshot"]["progress_destination_bound"] is True
assert enabled_snapshot["cancellation_snapshot"]["cancel_token_type"] == "RuntimeNative3DTileSchedulerCancelToken"
assert enabled_snapshot["cancellation_snapshot"]["cancel_token_bound"] is False
assert enabled_snapshot["cancellation_snapshot"]["cancel_generation"] == 0
assert enabled_ledger["prepare_and_scene_state"]["prepared_frame"] is True
assert enabled_ledger["prepare_and_scene_state"]["cached_primitive_count"] > 0
assert enabled_ledger["scratch_and_output_estimates"]["render_pixel_buffer_bytes_per_frame"] > 0
assert enabled_scratch["reuse_disable_env"] == "RAY_TRACING_NATIVE3D_DISABLE_RENDER_UNIT_SCRATCH_REUSE"
assert enabled_scratch["owner_count"] == 0
assert enabled_scratch["setup_calls"] == 0
assert enabled_scratch["radiance_capacity_bytes_max"] == 0
assert enabled_scheduler["cancel_token_type"] == "RuntimeNative3DTileSchedulerCancelToken"
assert enabled_scheduler["job_array_owners"] == 0
assert enabled_scheduler["parent_metric_array_owners"] == 0
assert enabled_scheduler["progress_tile_array_owners"] == 0
assert enabled_scheduler["completion_queue_owners"] == 0
assert enabled_scheduler["worker_pool_owners"] == 0
assert enabled_scheduler["cancel_token_bound"] == 0
assert enabled_scheduler["cancel_checks"] == 0
assert enabled_scheduler["cancel_requested"] == 0
assert enabled_scheduler["cancel_before_dispatch"] == 0
assert enabled_scheduler["cancel_during_wait"] == 0
assert enabled_scheduler["cancel_before_final_resolve"] == 0
assert enabled_scheduler["final_resolve_blocked_by_cancel"] == 0
assert enabled_scheduler["worker_drain_shutdowns"] == 0
assert enabled_scheduler["worker_cancel_shutdowns"] == 0
assert enabled_scheduler["cancel_generation"] == 0
assert enabled_presentation["pixel_stride_bytes"] == 4
assert enabled_presentation["requested_output_frames"] == 1
assert enabled_presentation["written_output_frames"] == 0
assert enabled_presentation["render_pixel_buffer_bytes_per_frame"] == 160 * 96 * 4
assert enabled_presentation["requested_output_pixel_bytes_total"] == 160 * 96 * 4
assert enabled_presentation["measured_output_pixel_bytes_written"] == 0
assert enabled_presentation["dirty_preview_host_pixels"] == 0
assert enabled_presentation["dirty_preview_host_bytes"] == 0
assert enabled_presentation["final_resolve_host_pixels"] == 0
assert enabled_presentation["final_resolve_host_bytes"] == 0
assert enabled_presentation["history_seed_host_bytes"] == 0
assert enabled_presentation["history_promote_host_bytes"] == 0
assert enabled_presentation["final_preview_present_host_bytes"] == 0
assert enabled_presentation["measured_preview_host_movement_bytes"] == 0
assert enabled_ledger["known_global_state_sources"]["prepared_scene_cache"] is True
assert enabled_ledger["acceleration_state"]["trace_context_stats_owned"] is True
assert enabled_ledger["acceleration_state"]["trace_context_callback_bound"] is True
assert enabled_dataflow["stats_enabled"] is True
assert enabled_dataflow["prepare_calls"] >= 1
assert enabled_dataflow["copy_calls"] >= 1
assert enabled_dataflow["bind_after_copy_calls"] >= 1
assert enabled_dataflow["final_frame_bind_calls"] >= 1
assert enabled_dataflow["last_copied_estimated_bytes"] > 0
assert enabled_dataflow["flattened_bvh_skip_on_tlas_enabled"] is True
assert enabled_dataflow["flattened_bvh_skip_on_tlas_default_enabled"] is True
assert enabled_dataflow["flattened_bvh_skip_on_tlas_force_enabled"] is False
assert enabled_dataflow["flattened_bvh_skip_on_tlas_default_disabled"] is False
assert enabled_dataflow["frame_bvh_tlas_readiness_checks"] >= 1
assert enabled_dataflow["frame_bvh_ensure_calls"] == 0
assert enabled_dataflow["frame_bvh_build_calls"] == 0
assert enabled_dataflow["frame_bvh_skip_for_tlas_calls"] >= 1
assert enabled_dataflow["last_frame_bvh_required"] is False
assert enabled_dataflow["last_frame_bvh_skipped_for_tlas"] is True
assert enabled_dataflow["last_tlas_ready_for_frame_bvh_skip"] is True
assert enabled_dataflow["last_frame_bvh_ready"] is False
assert enabled_dataflow["last_frame_bvh_node_count"] == 0
assert enabled_dataflow["last_frame_bvh_total_bytes"] == 0
assert enabled_dataflow["last_frame_bvh_skip_decision"] == "skipped_tlas_ready"

assert tlas_skip_ledger["enabled"] is True
assert tlas_skip_dataflow["stats_enabled"] is True
assert tlas_skip_dataflow["flattened_bvh_skip_on_tlas_enabled"] is True
assert tlas_skip_dataflow["flattened_bvh_skip_on_tlas_default_enabled"] is True
assert tlas_skip_dataflow["flattened_bvh_skip_on_tlas_force_enabled"] is True
assert tlas_skip_dataflow["flattened_bvh_skip_on_tlas_default_disabled"] is False
assert tlas_skip_dataflow["frame_bvh_tlas_readiness_checks"] >= 1
assert tlas_skip_dataflow["frame_bvh_skip_for_tlas_calls"] >= 1
assert tlas_skip_dataflow["frame_bvh_ensure_calls"] == 0
assert tlas_skip_dataflow["last_frame_bvh_required"] is False
assert tlas_skip_dataflow["last_frame_bvh_skipped_for_tlas"] is True
assert tlas_skip_dataflow["last_tlas_ready_for_frame_bvh_skip"] is True
assert tlas_skip_dataflow["last_frame_bvh_ready"] is False
assert tlas_skip_dataflow["last_frame_bvh_skip_decision"] == "skipped_tlas_ready"
assert tlas_skip_ledger["acceleration_state"]["trace_route"] == "tlas_blas"

assert tlas_skip_disabled_ledger["enabled"] is True
assert tlas_skip_disabled_dataflow["stats_enabled"] is True
assert tlas_skip_disabled_dataflow["flattened_bvh_skip_on_tlas_enabled"] is False
assert tlas_skip_disabled_dataflow["flattened_bvh_skip_on_tlas_default_enabled"] is False
assert tlas_skip_disabled_dataflow["flattened_bvh_skip_on_tlas_force_enabled"] is False
assert tlas_skip_disabled_dataflow["flattened_bvh_skip_on_tlas_default_disabled"] is True
assert tlas_skip_disabled_dataflow["frame_bvh_tlas_readiness_checks"] == 0
assert tlas_skip_disabled_dataflow["frame_bvh_skip_for_tlas_calls"] == 0
assert tlas_skip_disabled_dataflow["frame_bvh_ensure_calls"] >= 1
assert tlas_skip_disabled_dataflow["frame_bvh_build_calls"] >= 1
assert tlas_skip_disabled_dataflow["last_frame_bvh_required"] is True
assert tlas_skip_disabled_dataflow["last_frame_bvh_skipped_for_tlas"] is False
assert tlas_skip_disabled_dataflow["last_tlas_ready_for_frame_bvh_skip"] is False
assert tlas_skip_disabled_dataflow["last_frame_bvh_ready"] is True
assert tlas_skip_disabled_dataflow["last_frame_bvh_node_count"] > 0
assert tlas_skip_disabled_dataflow["last_frame_bvh_total_bytes"] > 0
assert tlas_skip_disabled_dataflow["last_frame_bvh_skip_decision"] == "disabled_by_env"
assert tlas_skip_disabled_ledger["acceleration_state"]["trace_route"] == "tlas_blas"

for ledger, dataflow, route in (
    (parity_skip_ledger, parity_skip_dataflow, "tlas_blas_parity"),
    (flattened_skip_ledger, flattened_skip_dataflow, "flattened_bvh"),
):
    assert ledger["enabled"] is True
    assert dataflow["stats_enabled"] is True
    assert dataflow["flattened_bvh_skip_on_tlas_enabled"] is True
    assert dataflow["flattened_bvh_skip_on_tlas_default_enabled"] is False
    assert dataflow["flattened_bvh_skip_on_tlas_force_enabled"] is True
    assert dataflow["flattened_bvh_skip_on_tlas_default_disabled"] is False
    assert dataflow["frame_bvh_skip_for_tlas_calls"] == 0
    assert dataflow["frame_bvh_ensure_calls"] >= 1
    assert dataflow["last_frame_bvh_required"] is True
    assert dataflow["last_frame_bvh_skipped_for_tlas"] is False
    assert dataflow["last_frame_bvh_ready"] is True
    assert dataflow["last_frame_bvh_node_count"] > 0
    assert dataflow["last_frame_bvh_total_bytes"] > 0
    assert dataflow["last_frame_bvh_skip_decision"] == "route_requires_flattened_bvh"
    assert ledger["acceleration_state"]["trace_route"] == route
PY

expect_preflight_reject() {
  local label="$1"
  local request_path="$2"
  local expected="$3"
  set +e
  "$CLI" --request "$request_path" --preflight \
    >"$DIAG_ROOT/$label.out" 2>"$DIAG_ROOT/$label.err"
  status=$?
  set -e
  if [ "$status" -eq 0 ]; then
    echo "expected preflight rejection for $label" >&2
    exit 1
  fi
  grep -q "$expected" "$DIAG_ROOT/$label.err"
}

cat >"$NEGATIVE_ROOT/missing_scene_path.json" <<'JSON'
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "scene": {},
  "render": {
    "frame_count": 1,
    "width": 64,
    "height": 64
  },
  "output": {
    "root": "../negative_output"
  }
}
JSON

cat >"$NEGATIVE_ROOT/invalid_output_root.json" <<'JSON'
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "scene": {
    "runtime_scene_path": "../../../config/samples/ps4d_runtime_scene_visual_test.json"
  },
  "render": {
    "frame_count": 1,
    "width": 64,
    "height": 64
  },
  "output": {
    "root": ""
  }
}
JSON

expect_preflight_reject \
  missing_scene_path \
  "$NEGATIVE_ROOT/missing_scene_path.json" \
  "field=scene.runtime_scene_path"

expect_preflight_reject \
  invalid_output_root \
  "$NEGATIVE_ROOT/invalid_output_root.json" \
  "field=output.root"
