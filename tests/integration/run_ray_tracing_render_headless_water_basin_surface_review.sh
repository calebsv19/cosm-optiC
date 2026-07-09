#!/usr/bin/env bash
set -euo pipefail

RAY_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CODEWORK_ROOT="$(cd "$RAY_DIR/.." && pwd)"
PHYSICS_DIR="$CODEWORK_ROOT/physics_sim"

RUN_ROOT="$RAY_DIR/build/agent_runs/physics_trio/water_basin_surface_review_single_frame"
PHYSICS_OUT="$RUN_ROOT/physics_sim"
RAY_OUT="$RUN_ROOT/ray_tracing"
RUNTIME_SCENE="$RUN_ROOT/runtime_scene.json"
REQUEST="$RUN_ROOT/ray_tracing_request.json"
SUMMARY="$RAY_OUT/render_summary.json"
STDOUT_SUMMARY="$RAY_OUT/stdout_summary.json"
PHYSICS_FRAMES=12
START_FRAME=$((PHYSICS_FRAMES - 1))
RENDER_FRAME_NAME="frame_$(printf '%04d' "$START_FRAME").bmp"

PHYSICS_TOOL="$PHYSICS_DIR/physics_sim_headless"
RAY_TOOL="$RAY_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"

rm -rf "$RUN_ROOT"
mkdir -p "$RUN_ROOT" "$PHYSICS_OUT" "$RAY_OUT"

make -C "$PHYSICS_DIR" physics_sim_headless
make -C "$RAY_DIR" BUILD_TOOLCHAIN=clang ray-tracing-render-headless

"$PHYSICS_TOOL" \
  --water-mode \
  --frames "$PHYSICS_FRAMES" \
  --sim-steps-per-frame 2 \
  --grid 32x16x32 \
  --water-level 0.62 \
  --water-review-ripples \
  --water-review-ripple-amplitude 0.045 \
  --output-root "$PHYSICS_OUT" \
  --summary "$PHYSICS_OUT/run_summary.json" \
  --progress "$PHYSICS_OUT/run_progress.json" \
  --overwrite \
  --save-volume-frames

RUN_DIR="$PHYSICS_OUT/volume_frames/Water Basin"
SCENE_BUNDLE="$RUN_DIR/scene_bundle.json"
SURFACE_FRAME="$RUN_DIR/water_surface_$(printf '%06d' "$START_FRAME").json"
if [[ ! -f "$SCENE_BUNDLE" ]]; then
  echo "missing PhysicsSim scene_bundle.json at $SCENE_BUNDLE" >&2
  exit 1
fi
if [[ ! -f "$SURFACE_FRAME" ]]; then
  echo "missing final water surface frame at $SURFACE_FRAME" >&2
  exit 1
fi

cat > "$RUNTIME_SCENE" <<JSON
{
  "schema_family": "codework_scene",
  "schema_variant": "scene_runtime_v1",
  "schema_version": 1,
  "scene_id": "scene_water_basin_surface_review_single_frame",
  "space_mode_default": "3d",
  "unit_system": "meters",
  "world_scale": 1.0,
  "objects": [
    {
      "object_id": "basin_floor",
      "object_type": "plane_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": {
        "position": { "x": 2.0, "y": 2.0, "z": 0.02 },
        "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 },
        "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
      },
      "primitive": {
        "kind": "plane_primitive",
        "width": 4.35,
        "height": 4.35,
        "lock_to_construction_plane": false,
        "lock_to_bounds": false,
        "frame": {
          "origin": { "x": 2.0, "y": 2.0, "z": 0.02 },
          "axis_u": { "x": 1.0, "y": 0.0, "z": 0.0 },
          "axis_v": { "x": 0.0, "y": 1.0, "z": 0.0 },
          "normal": { "x": 0.0, "y": 0.0, "z": 1.0 }
        }
      }
    },
    {
      "object_id": "basin_back_wall",
      "object_type": "rect_prism_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": {
        "position": { "x": 2.0, "y": 4.08, "z": 0.42 },
        "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 },
        "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
      },
      "primitive": { "kind": "rect_prism_primitive", "width": 4.35, "height": 0.14, "depth": 0.82 }
    },
    {
      "object_id": "basin_left_wall",
      "object_type": "rect_prism_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": {
        "position": { "x": -0.08, "y": 2.0, "z": 0.42 },
        "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 },
        "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
      },
      "primitive": { "kind": "rect_prism_primitive", "width": 0.14, "height": 4.35, "depth": 0.82 }
    },
    {
      "object_id": "basin_right_wall",
      "object_type": "rect_prism_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": {
        "position": { "x": 4.08, "y": 2.0, "z": 0.42 },
        "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 },
        "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
      },
      "primitive": { "kind": "rect_prism_primitive", "width": 0.14, "height": 4.35, "depth": 0.82 }
    },
    {
      "object_id": "clean_backdrop",
      "object_type": "plane_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": {
        "position": { "x": 2.0, "y": 4.34, "z": 1.08 },
        "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 },
        "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
      },
      "primitive": {
        "kind": "plane_primitive",
        "width": 4.9,
        "height": 1.8,
        "lock_to_construction_plane": false,
        "lock_to_bounds": false,
        "frame": {
          "origin": { "x": 2.0, "y": 4.34, "z": 1.08 },
          "axis_u": { "x": 1.0, "y": 0.0, "z": 0.0 },
          "axis_v": { "x": 0.0, "y": 0.0, "z": 1.0 },
          "normal": { "x": 0.0, "y": -1.0, "z": 0.0 }
        }
      }
    },
    {
      "object_id": "small_depth_reference",
      "object_type": "rect_prism_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": {
        "position": { "x": 2.85, "y": 2.65, "z": 0.22 },
        "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 },
        "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
      },
      "primitive": { "kind": "rect_prism_primitive", "width": 0.44, "height": 0.36, "depth": 0.18 }
    }
  ],
  "materials": [],
  "lights": [
    { "position": { "x": 2.0, "y": 1.65, "z": 2.15 } }
  ],
  "cameras": [],
  "constraints": [],
  "hierarchy": [],
  "extensions": {
    "ray_tracing": {
      "authoring": {
        "light_settings": { "intensity": 8.8, "radius": 0.16 },
        "environment": { "light_mode": 2, "ambient_strength": 0.20, "top_fill_strength": 1.20 },
        "object_materials": [
          { "object_id": "basin_floor", "material_id": 0, "object_color": 3561532, "roughness": 0.60, "reflectivity": 0.08 },
          { "object_id": "basin_back_wall", "material_id": 0, "object_color": 6381921, "roughness": 0.66, "reflectivity": 0.06 },
          { "object_id": "basin_left_wall", "material_id": 0, "object_color": 6052708, "roughness": 0.68, "reflectivity": 0.05 },
          { "object_id": "basin_right_wall", "material_id": 0, "object_color": 6052708, "roughness": 0.68, "reflectivity": 0.05 },
          { "object_id": "clean_backdrop", "material_id": 0, "object_color": 7895945, "roughness": 0.72, "reflectivity": 0.04 },
          { "object_id": "small_depth_reference", "material_id": 0, "object_color": 13734733, "roughness": 0.48, "reflectivity": 0.12 }
        ]
      }
    }
  }
}
JSON

cat > "$REQUEST" <<JSON
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "run_id": "agent_render_water_basin_surface_review_single_frame",
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
    "start_frame": $START_FRAME,
    "frame_count": 1,
    "width": 320,
    "height": 240,
    "normalized_t": 0.0,
    "temporal_frames": 1,
    "integrator_3d": "emission_transparency"
  },
  "inspection": {
    "camera_zoom": 0.78,
    "camera_position": { "x": 2.0, "y": -4.65, "z": 2.35 },
    "camera_look_at": { "x": 2.0, "y": 1.95, "z": 0.47 },
    "environment_brightness": 0.025,
    "ambient_strength": 0.18,
    "environment_light_mode": "ambient",
    "top_fill_strength": 1.20,
    "background_brightness": 0.015,
    "background_color": { "r": 0.004, "g": 0.007, "b": 0.010 },
    "light_intensity": 8.8,
    "light_radius": 0.16,
    "forward_decay": 240.0,
    "secondary_diffuse_samples_3d": 1,
    "transmission_samples_3d": 1,
    "volume_scatter_gain": 1.25,
    "volume_step_scale": 0.9,
    "volume_tint": { "r": 0.30, "g": 0.72, "b": 1.40 }
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
grep -q '"water_surface_source_found": true' "$SUMMARY"
grep -q '"water_surface_loaded": true' "$SUMMARY"
grep -q '"water_surface_mesh_attached": true' "$SUMMARY"
grep -q '"surface_axis": "y"' "$SUMMARY"
grep -q '"route_native_3d": true' "$SUMMARY"
grep -q '"prepared_frame": true' "$SUMMARY"
grep -q '"rendered_frames": true' "$SUMMARY"
grep -q '"frames_rendered": 1' "$SUMMARY"
grep -q '"integrator_3d": "emission_transparency"' "$SUMMARY"

python3 - "$PHYSICS_OUT/run_summary.json" "$SURFACE_FRAME" "$SUMMARY" "$START_FRAME" <<'PY'
import json
import math
import sys

physics_summary_path, surface_path, ray_summary_path, start_frame_text = sys.argv[1:5]
start_frame = int(start_frame_text)
with open(physics_summary_path, "r", encoding="utf-8") as f:
    physics_summary = json.load(f)
with open(surface_path, "r", encoding="utf-8") as f:
    surface = json.load(f)
with open(ray_summary_path, "r", encoding="utf-8") as f:
    ray_summary = json.load(f)

def require(condition, message):
    if not condition:
        raise SystemExit(message)

require(physics_summary.get("mode") == "water", "physics run did not use water mode")
require(physics_summary.get("water_review_ripples") is True, "physics summary did not enable review ripples")
require(int(surface.get("grid_w", 0)) >= 30, "water surface grid_w did not use review resolution")
require(int(surface.get("grid_d", 0)) >= 30, "water surface grid_d did not expand to square basin footprint")
require(int(surface.get("sample_count", 0)) >= 900, "water surface has too few samples for review")
require(float(surface.get("sample_origin_z", 0.0)) + float(surface.get("sample_spacing_z", 0.0)) * (int(surface.get("grid_d", 0)) - 1) > 3.5,
        "water surface Z footprint is still narrow")

surface_summary = surface.get("summary") or {}
require(surface_summary.get("review_ripples_applied") is True, "surface frame did not apply review ripples")
require(abs(float(surface_summary.get("review_ripple_amplitude_m", 0.0)) - 0.045) < 0.0005,
        "surface frame did not preserve configured ripple amplitude")
surface_range = float(surface_summary.get("surface_max_y", 0.0)) - float(surface_summary.get("surface_min_y", 0.0))
delta_range = float(surface_summary.get("review_ripple_delta_max_m", 0.0)) - float(surface_summary.get("review_ripple_delta_min_m", 0.0))
require(surface_range > 0.018, f"surface range too flat: {surface_range}")
require(delta_range > 0.018, f"review ripple delta range too flat: {delta_range}")
require(float(surface_summary.get("max_slope", 0.0)) > 0.02, "surface normals did not capture a visible slope")
heights = surface.get("heights_y") or []
require(len(heights) == int(surface.get("sample_count", 0)), "height sample count mismatch")
require(len({round(float(h), 5) for h in heights}) > 16, "heightfield has too few distinct water heights")
require(all(math.isfinite(float(h)) for h in heights), "heightfield contains non-finite values")

water = ray_summary.get("water_surface") or {}
require(water.get("loaded") is True, "ray summary did not load water surface")
require(water.get("mesh_attached") is True, "ray summary did not attach water mesh")
require(water.get("loaded_first_frame_index") == start_frame, "ray did not load requested first water frame")
require(water.get("loaded_last_frame_index") == start_frame, "ray did not load requested last water frame")
require(int(water.get("grid_w", 0)) >= 30, "ray summary lost review grid_w")
require(int(water.get("grid_d", 0)) >= 30, "ray summary lost expanded grid_d")
require(int(water.get("triangle_count", 0)) > 1200, "water surface mesh too small for basin review")
require(float(water.get("max_slope", 0.0)) > 0.02, "ray summary lost water surface slope")
payload = water.get("payload") or {}
require(payload.get("applied") is True, "ray summary did not apply water material payload")
require(abs(float(payload.get("ior", 0.0)) - 1.333) < 0.01, "unexpected water payload IOR")

render_stats = ray_summary.get("render_stats") or {}
require(int(render_stats.get("visible_pixels", 0)) > 0, "render has no visible pixels")
require(int(render_stats.get("nonzero_pixels", 0)) > 0, "render has no nonzero pixels")
require(int(render_stats.get("secondary_hits", 0)) > 0, "render did not trace through transparent water")
require(max(render_stats.get("max_rgb") or [0, 0, 0]) > 60, "render contrast too low for visual review")
object_audit = ray_summary.get("object_audit") or []
water_hits = [
    int(obj.get("primary_hit_pixels", 0))
    for obj in object_audit
    if obj.get("object_type") == "water_surface"
]
require(water_hits and max(water_hits) > 2000, "render camera did not make the basin water surface dominant")
PY

test -s "$RAY_OUT/frames/$RENDER_FRAME_NAME"
test "$(dd if="$RAY_OUT/frames/$RENDER_FRAME_NAME" bs=1 count=2 2>/dev/null)" = "BM"

echo "ray tracing water basin surface review single-frame export passed: $RAY_OUT/frames/$RENDER_FRAME_NAME"
