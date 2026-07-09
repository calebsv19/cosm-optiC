#!/usr/bin/env bash
set -euo pipefail

RAY_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CODEWORK_ROOT="$(cd "$RAY_DIR/.." && pwd)"
PHYSICS_DIR="$CODEWORK_ROOT/physics_sim"

RUN_ROOT="$RAY_DIR/build/agent_runs/physics_trio/water_moving_light_review"
PHYSICS_OUT="$RUN_ROOT/physics_sim"
RAY_OUT="$RUN_ROOT/ray_tracing"
RUNTIME_SCENE="$RUN_ROOT/runtime_scene.json"
REQUEST="$RUN_ROOT/ray_tracing_request.json"
SUMMARY="$RAY_OUT/render_summary.json"
STDOUT_SUMMARY="$RAY_OUT/stdout_summary.json"
PHYSICS_FRAMES=16
START_FRAME=8
RENDER_FRAMES=4
END_FRAME=$((START_FRAME + RENDER_FRAMES - 1))

PHYSICS_TOOL="$PHYSICS_DIR/physics_sim_headless"
RAY_TOOL="$RAY_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"

rm -rf "$RUN_ROOT"
mkdir -p "$RUN_ROOT" "$PHYSICS_OUT" "$RAY_OUT"

make -C "$PHYSICS_DIR" physics_sim_headless
make -C "$RAY_DIR" BUILD_TOOLCHAIN=clang ray-tracing-render-headless

"$PHYSICS_TOOL" \
  --water-mode \
  --frames "$PHYSICS_FRAMES" \
  --sim-steps-per-frame 3 \
  --grid 32x16x32 \
  --water-level 0.62 \
  --water-review-ripples \
  --water-review-ripple-amplitude 0.040 \
  --output-root "$PHYSICS_OUT" \
  --summary "$PHYSICS_OUT/run_summary.json" \
  --progress "$PHYSICS_OUT/run_progress.json" \
  --overwrite \
  --save-volume-frames

RUN_DIR="$PHYSICS_OUT/volume_frames/Water Basin"
SCENE_BUNDLE="$RUN_DIR/scene_bundle.json"
FIRST_SURFACE_FRAME="$RUN_DIR/water_surface_$(printf '%06d' "$START_FRAME").json"
LAST_SURFACE_FRAME="$RUN_DIR/water_surface_$(printf '%06d' "$END_FRAME").json"
if [[ ! -f "$SCENE_BUNDLE" ]]; then
  echo "missing PhysicsSim scene_bundle.json at $SCENE_BUNDLE" >&2
  exit 1
fi
if [[ ! -f "$FIRST_SURFACE_FRAME" || ! -f "$LAST_SURFACE_FRAME" ]]; then
  echo "missing requested water surface frame range $START_FRAME..$END_FRAME" >&2
  exit 1
fi

cat > "$RUNTIME_SCENE" <<JSON
{
  "schema_family": "codework_scene",
  "schema_variant": "scene_runtime_v1",
  "schema_version": 1,
  "scene_id": "scene_water_moving_light_review",
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
      "object_id": "subsurface_reference_block",
      "object_type": "rect_prism_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": {
        "position": { "x": 2.82, "y": 2.78, "z": 0.24 },
        "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 },
        "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
      },
      "primitive": { "kind": "rect_prism_primitive", "width": 0.54, "height": 0.42, "depth": 0.20 }
    }
  ],
  "materials": [],
  "lights": [],
  "cameras": [],
  "constraints": [],
  "hierarchy": [],
  "extensions": {
    "ray_tracing": {
      "authoring": {
        "light_path": {
          "mode": "BEZIER_CUBIC",
          "points": [
            {
              "x": 0.82,
              "y": 1.22,
              "rotation": 0.0,
              "handleLink": false,
              "velocity1": { "vx": 0.42, "vy": -0.10 }
            },
            {
              "x": 3.18,
              "y": 2.38,
              "rotation": 0.0,
              "handleLink": false,
              "velocity2": { "vx": -0.42, "vy": 0.10 }
            }
          ]
        },
        "light_path_depth": {
          "points": [
            { "z": 2.08, "lookPitch": 0.0, "velocity1": { "vz": 0.0 } },
            { "z": 2.08, "lookPitch": 0.0, "velocity2": { "vz": 0.0 } }
          ]
        },
        "light_settings": { "intensity": 9.4, "radius": 0.15 },
        "environment": { "light_mode": 2, "ambient_strength": 0.16, "top_fill_strength": 0.90 },
        "object_materials": [
          { "object_id": "basin_floor", "material_id": 0, "object_color": 3365436, "roughness": 0.62, "reflectivity": 0.07 },
          { "object_id": "basin_back_wall", "material_id": 0, "object_color": 6381921, "roughness": 0.66, "reflectivity": 0.06 },
          { "object_id": "basin_left_wall", "material_id": 0, "object_color": 6052708, "roughness": 0.68, "reflectivity": 0.05 },
          { "object_id": "basin_right_wall", "material_id": 0, "object_color": 6052708, "roughness": 0.68, "reflectivity": 0.05 },
          { "object_id": "clean_backdrop", "material_id": 0, "object_color": 7895945, "roughness": 0.72, "reflectivity": 0.04 },
          { "object_id": "subsurface_reference_block", "material_id": 0, "object_color": 14455296, "roughness": 0.46, "reflectivity": 0.12 }
        ]
      }
    }
  }
}
JSON

cat > "$REQUEST" <<JSON
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "run_id": "agent_render_water_moving_light_review",
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
    "frame_count": $RENDER_FRAMES,
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
    "environment_brightness": 0.018,
    "ambient_strength": 0.14,
    "environment_light_mode": "ambient",
    "top_fill_strength": 0.90,
    "background_brightness": 0.012,
    "background_color": { "r": 0.004, "g": 0.007, "b": 0.010 },
    "light_intensity": 9.4,
    "light_radius": 0.15,
    "forward_decay": 240.0,
    "secondary_diffuse_samples_3d": 2,
    "transmission_samples_3d": 2,
    "volume_scatter_gain": 1.35,
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
grep -q '"route_native_3d": true' "$SUMMARY"
grep -q '"rendered_frames": true' "$SUMMARY"
grep -q '"frames_rendered": 4' "$SUMMARY"
grep -q '"integrator_3d": "emission_transparency"' "$SUMMARY"

python3 - "$PHYSICS_OUT/run_summary.json" "$FIRST_SURFACE_FRAME" "$LAST_SURFACE_FRAME" "$SUMMARY" "$RAY_OUT/frames" "$START_FRAME" "$END_FRAME" <<'PY'
import json
import os
import struct
import sys

physics_summary_path, first_surface_path, last_surface_path, ray_summary_path, frame_dir, start_text, end_text = sys.argv[1:8]
start_frame = int(start_text)
end_frame = int(end_text)
with open(physics_summary_path, "r", encoding="utf-8") as f:
    physics_summary = json.load(f)
with open(first_surface_path, "r", encoding="utf-8") as f:
    first_surface = json.load(f)
with open(last_surface_path, "r", encoding="utf-8") as f:
    last_surface = json.load(f)
with open(ray_summary_path, "r", encoding="utf-8") as f:
    ray_summary = json.load(f)

def require(condition, message):
    if not condition:
        raise SystemExit(message)

def read_bmp_rgb(path):
    with open(path, "rb") as f:
        data = f.read()
    require(data[:2] == b"BM", f"{path} is not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    require(bpp in (24, 32), f"{path} is not a 24-bit or 32-bit BMP")
    bytes_per_pixel = bpp // 8
    row_stride = ((width * bytes_per_pixel + 3) // 4) * 4
    pixels = []
    for y in range(abs(height)):
        row_start = offset + y * row_stride
        for x in range(width):
            pixel_start = row_start + x * bytes_per_pixel
            b, g, r = data[pixel_start: pixel_start + 3]
            pixels.append((r, g, b))
    return width, abs(height), pixels

def image_metrics(path):
    width, height, pixels = read_bmp_rgb(path)
    return {
        "width": width,
        "height": height,
        "pixels": pixels,
        "nonzero": sum(1 for p in pixels if p != (0, 0, 0)),
        "max_rgb": max(max(p) for p in pixels),
    }

def mean_abs_delta(a, b):
    require(a["width"] == b["width"] and a["height"] == b["height"], "frame dimensions changed")
    total = 0
    for pa, pb in zip(a["pixels"], b["pixels"]):
        total += abs(pa[0] - pb[0]) + abs(pa[1] - pb[1]) + abs(pa[2] - pb[2])
    return total / (len(a["pixels"]) * 3.0)

require(physics_summary.get("mode") == "water", "physics run did not use water mode")
require(physics_summary.get("water_review_ripples") is True, "physics summary did not enable review ripples")
for surface, label in ((first_surface, "first"), (last_surface, "last")):
    summary = surface.get("summary") or {}
    require(surface.get("sample_count", 0) >= 900, f"{label} surface sample count too low")
    require(summary.get("review_ripples_applied") is True, f"{label} surface did not apply review ripples")
    require(float(summary.get("max_slope", 0.0)) > 0.02, f"{label} surface too flat")

first_heights = [float(h) for h in first_surface.get("heights_y") or []]
last_heights = [float(h) for h in last_surface.get("heights_y") or []]
require(len(first_heights) == len(last_heights) and len(first_heights) > 0, "surface frame sample mismatch")
height_delta = sum(abs(a - b) for a, b in zip(first_heights, last_heights)) / len(first_heights)
require(height_delta > 0.00025, f"surface did not evolve across selected frames: {height_delta}")

water = ray_summary.get("water_surface") or {}
require(water.get("loaded") is True, "ray summary did not load water surface")
require(water.get("mesh_attached") is True, "ray summary did not attach water mesh")
require(water.get("loaded_first_frame_index") == start_frame, "ray did not load first requested water frame")
require(water.get("loaded_last_frame_index") == end_frame, "ray did not load last requested water frame")
require(int(water.get("triangle_count", 0)) > 1200, "water surface mesh too small")
payload = water.get("payload") or {}
require(payload.get("applied") is True, "ray summary did not apply water material payload")
require(abs(float(payload.get("ior", 0.0)) - 1.333) < 0.01, "unexpected water payload IOR")

render_stats = ray_summary.get("render_stats") or {}
require(int(render_stats.get("visible_pixels", 0)) > 0, "render has no visible pixels")
require(int(render_stats.get("secondary_hits", 0)) > 0, "render did not trace through transparent water")
require(max(render_stats.get("max_rgb") or [0, 0, 0]) > 80, "render contrast too low")

metrics = []
for frame_index in range(start_frame, end_frame + 1):
    frame_path = os.path.join(frame_dir, f"frame_{frame_index:04d}.bmp")
    require(os.path.getsize(frame_path) > 0, f"missing rendered frame {frame_path}")
    metrics.append(image_metrics(frame_path))

first_to_last_delta = mean_abs_delta(metrics[0], metrics[-1])
adjacent_deltas = [mean_abs_delta(a, b) for a, b in zip(metrics, metrics[1:])]
require(first_to_last_delta > 2.0, f"moving-light sequence changed too little: {first_to_last_delta}")
require(max(adjacent_deltas) > 1.0, f"adjacent moving-light frames changed too little: {adjacent_deltas}")
require(all(m["nonzero"] > 0 for m in metrics), "one or more frames are blank")
require(max(m["max_rgb"] for m in metrics) > 80, "sequence has no bright light response")

print(json.dumps({
    "height_delta": height_delta,
    "first_to_last_pixel_delta": first_to_last_delta,
    "adjacent_pixel_deltas": adjacent_deltas,
}, indent=2))
PY

for frame_index in $(seq "$START_FRAME" "$END_FRAME"); do
  frame_path="$RAY_OUT/frames/frame_$(printf '%04d' "$frame_index").bmp"
  test -s "$frame_path"
  test "$(dd if="$frame_path" bs=1 count=2 2>/dev/null)" = "BM"
done

echo "ray tracing water moving-light multi-frame review passed: $RAY_OUT/frames/frame_$(printf '%04d' "$START_FRAME").bmp..frame_$(printf '%04d' "$END_FRAME").bmp"
