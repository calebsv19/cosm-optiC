#!/usr/bin/env bash
set -euo pipefail

RAY_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CODEWORK_ROOT="$(cd "$RAY_DIR/.." && pwd)"
PHYSICS_DIR="$CODEWORK_ROOT/physics_sim"

RUN_ROOT="$RAY_DIR/build/agent_runs/physics_trio/water_surface_handoff_image_export"
PHYSICS_OUT="$RUN_ROOT/physics_sim"
RAY_OUT="$RUN_ROOT/ray_tracing"
RUNTIME_SCENE="$RUN_ROOT/runtime_scene.json"
REQUEST="$RUN_ROOT/ray_tracing_request.json"
SUMMARY="$RAY_OUT/render_summary.json"
STDOUT_SUMMARY="$RAY_OUT/stdout_summary.json"

PHYSICS_TOOL="$PHYSICS_DIR/physics_sim_headless"
RAY_TOOL="$RAY_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"

rm -rf "$RUN_ROOT"
mkdir -p "$RUN_ROOT" "$PHYSICS_OUT" "$RAY_OUT"

make -C "$PHYSICS_DIR" physics_sim_headless
make -C "$RAY_DIR" BUILD_TOOLCHAIN=clang ray-tracing-render-headless

"$PHYSICS_TOOL" \
  --water-mode \
  --frames 2 \
  --sim-steps-per-frame 1 \
  --grid 16x12x8 \
  --water-level 0.42 \
  --output-root "$PHYSICS_OUT" \
  --summary "$PHYSICS_OUT/run_summary.json" \
  --progress "$PHYSICS_OUT/run_progress.json" \
  --overwrite \
  --save-volume-frames

SCENE_BUNDLE="$(find "$PHYSICS_OUT/volume_frames" -name scene_bundle.json -print | sort | head -n 1)"
if [[ -z "$SCENE_BUNDLE" || ! -f "$SCENE_BUNDLE" ]]; then
  echo "missing PhysicsSim scene_bundle.json under $PHYSICS_OUT/volume_frames" >&2
  exit 1
fi

cat > "$RUNTIME_SCENE" <<JSON
{
  "schema_family": "codework_scene",
  "schema_variant": "scene_runtime_v1",
  "schema_version": 1,
  "scene_id": "scene_water_surface_handoff_smoke",
  "space_mode_default": "3d",
  "unit_system": "meters",
  "world_scale": 1.0,
  "objects": [
    {
      "object_id": "floor_y_up",
      "object_type": "plane_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": {
        "position": { "x": 0.0, "y": -0.05, "z": 0.0 },
        "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 },
        "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
      },
      "flags": {
        "visible": true,
        "locked": false,
        "selectable": true
      },
      "primitive": {
        "kind": "plane_primitive",
        "width": 6.0,
        "height": 5.0,
        "lock_to_construction_plane": false,
        "lock_to_bounds": false,
        "frame": {
          "origin": { "x": 0.0, "y": -0.05, "z": 0.0 },
          "axis_u": { "x": 1.0, "y": 0.0, "z": 0.0 },
          "axis_v": { "x": 0.0, "y": 0.0, "z": 1.0 },
          "normal": { "x": 0.0, "y": 1.0, "z": 0.0 }
        }
      }
    }
  ],
  "materials": [],
  "lights": [],
  "cameras": [],
  "constraints": [],
  "hierarchy": [],
  "extensions": {}
}
JSON

cat > "$REQUEST" <<JSON
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "run_id": "agent_render_water_surface_handoff_smoke",
  "scene": {
    "runtime_scene_path": "$RUNTIME_SCENE"
  },
  "volume": {
    "enabled": true,
    "source_kind": "scene_bundle",
    "source_path": "$SCENE_BUNDLE",
    "affects_lighting": true,
    "debug_overlay": false
  },
  "render": {
    "start_frame": 0,
    "frame_count": 2,
    "width": 320,
    "height": 320,
    "normalized_t": 0.0,
    "temporal_frames": 1,
    "integrator_3d": "direct_light"
  },
  "inspection": {
    "camera_zoom": 0.95,
    "camera_position": { "x": -2.8, "y": -4.6, "z": 2.4 },
    "camera_look_at": { "x": 0.0, "y": 0.35, "z": 0.0 },
    "environment_brightness": 0.0,
    "light_intensity": 3.2,
    "light_radius": 0.12,
    "forward_decay": 180.0,
    "volume_scatter_gain": 2.4,
    "volume_step_scale": 1.0,
    "volume_tint": { "r": 0.35, "g": 0.70, "b": 1.60 }
  },
  "output": {
    "root": "$RAY_OUT",
    "overwrite": true
  },
  "progress": {
    "summary_path": "$SUMMARY",
    "progress_path": "$RAY_OUT/render_progress.json"
  }
}
JSON

"$RAY_TOOL" --request "$REQUEST" --render --summary "$SUMMARY" > "$STDOUT_SUMMARY"

grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY"
grep -q '"scene_applied": true' "$SUMMARY"
grep -q '"volume_attached": true' "$SUMMARY"
grep -q '"volume_summary_built": true' "$SUMMARY"
grep -q '"water_surface_source_found": true' "$SUMMARY"
grep -q '"water_surface_loaded": true' "$SUMMARY"
grep -q '"water_surface_mesh_attached": true' "$SUMMARY"
grep -q '"route_native_3d": true' "$SUMMARY"
grep -q '"prepared_frame": true' "$SUMMARY"
grep -q '"rendered_frames": true' "$SUMMARY"
grep -q '"frames_rendered": 2' "$SUMMARY"
grep -q '"surface_axis": "y"' "$SUMMARY"
grep -q '"selected_first_frame_path": ' "$SUMMARY"
grep -q '"selected_last_frame_path": ' "$SUMMARY"
grep -Eq '"triangle_count": [1-9][0-9]*' "$SUMMARY"
grep -Eq '"grid_w": [1-9][0-9]*' "$SUMMARY"
grep -Eq '"grid_d": [1-9][0-9]*' "$SUMMARY"
grep -Eq '"sample_count": [1-9][0-9]*' "$SUMMARY"
grep -Eq '"water_cells": [1-9][0-9]*' "$SUMMARY"
grep -Eq '"ior": 1\.33' "$SUMMARY"
grep -q '"payload": {' "$SUMMARY"
grep -q '"applied": true' "$SUMMARY"
grep -Eq '"transparency": 0\.92' "$SUMMARY"
grep -q '"tint_rgb":' "$SUMMARY"
grep -Eq '"tint_rgb": \[0\.67[0-9]*, 0\.86[0-9]*, 0\.94[0-9]*\]' "$SUMMARY"
grep -Eq '"visible_pixels": [1-9][0-9]*' "$SUMMARY"
grep -Eq '"nonzero_pixels": [1-9][0-9]*' "$SUMMARY"

test -s "$RAY_OUT/frames/frame_0000.bmp"
test -s "$RAY_OUT/frames/frame_0001.bmp"
test "$(dd if="$RAY_OUT/frames/frame_0000.bmp" bs=1 count=2 2>/dev/null)" = "BM"
test "$(dd if="$RAY_OUT/frames/frame_0001.bmp" bs=1 count=2 2>/dev/null)" = "BM"

echo "ray tracing water surface handoff image export passed: $RAY_OUT/frames"
