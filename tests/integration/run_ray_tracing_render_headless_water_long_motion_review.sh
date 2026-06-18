#!/usr/bin/env bash
set -euo pipefail

RAY_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CODEWORK_ROOT="$(cd "$RAY_DIR/.." && pwd)"
PHYSICS_DIR="$CODEWORK_ROOT/physics_sim"

RUN_ROOT="$RAY_DIR/build/agent_runs/physics_trio/water_long_motion_review"
PHYSICS_OUT="$RUN_ROOT/physics_sim"
RAY_OUT="$RUN_ROOT/ray_tracing"
SUMMARY_DIR="$RAY_OUT/summaries"
RUNTIME_SCENE="$RUN_ROOT/runtime_scene.json"
PHYSICS_FRAMES=201
SELECTED_FRAMES=(40 80 120 160 200)
LIGHT_T=(0.0 0.25 0.50 0.75 1.0)

PHYSICS_TOOL="$PHYSICS_DIR/physics_sim_headless"
RAY_TOOL="$RAY_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"

rm -rf "$RUN_ROOT"
mkdir -p "$RUN_ROOT" "$PHYSICS_OUT" "$RAY_OUT" "$SUMMARY_DIR"

make -C "$PHYSICS_DIR" physics_sim_headless
make -C "$RAY_DIR" BUILD_TOOLCHAIN=clang ray-tracing-render-headless

"$PHYSICS_TOOL" \
  --water-mode \
  --frames "$PHYSICS_FRAMES" \
  --sim-steps-per-frame 4 \
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
if [[ ! -f "$SCENE_BUNDLE" ]]; then
  echo "missing PhysicsSim scene_bundle.json at $SCENE_BUNDLE" >&2
  exit 1
fi
for frame_index in "${SELECTED_FRAMES[@]}"; do
  surface_path="$RUN_DIR/water_surface_$(printf '%06d' "$frame_index").json"
  if [[ ! -f "$surface_path" ]]; then
    echo "missing selected sparse water surface frame at $surface_path" >&2
    exit 1
  fi
done

cat > "$RUNTIME_SCENE" <<JSON
{
  "schema_family": "codework_scene",
  "schema_variant": "scene_runtime_v1",
  "schema_version": 1,
  "scene_id": "scene_water_long_motion_review",
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

mkdir -p "$RAY_OUT/frames"

for i in "${!SELECTED_FRAMES[@]}"; do
  frame_index="${SELECTED_FRAMES[$i]}"
  normalized_t="${LIGHT_T[$i]}"
  request="$RUN_ROOT/ray_tracing_request_$(printf '%04d' "$frame_index").json"
  summary="$SUMMARY_DIR/render_summary_$(printf '%04d' "$frame_index").json"
  progress="$SUMMARY_DIR/render_progress_$(printf '%04d' "$frame_index").json"
  cat > "$request" <<JSON
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "run_id": "agent_render_water_long_motion_review_$(printf '%04d' "$frame_index")",
  "scene": { "runtime_scene_path": "$RUNTIME_SCENE" },
  "volume": {
    "enabled": true,
    "source_kind": "scene_bundle",
    "source_path": "$SCENE_BUNDLE",
    "affects_lighting": true,
    "debug_overlay": false
  },
  "render": {
    "start_frame": $frame_index,
    "frame_count": 1,
    "width": 320,
    "height": 240,
    "normalized_t": $normalized_t,
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
  "output": { "root": "$RAY_OUT", "overwrite": true },
  "progress": { "summary_path": "$summary", "progress_path": "$progress" }
}
JSON
  if [[ ! -x "$RAY_TOOL" ]]; then
    make -C "$RAY_DIR" BUILD_TOOLCHAIN=clang ray-tracing-render-headless
  fi
  "$RAY_TOOL" --request "$request" --render --summary "$summary" > "$SUMMARY_DIR/stdout_$(printf '%04d' "$frame_index").json"
done

python3 - "$PHYSICS_OUT/run_summary.json" "$RUN_DIR" "$SUMMARY_DIR" "$RAY_OUT/frames" "${SELECTED_FRAMES[@]}" <<'INNERPY'
import json
import os
import struct
import sys

physics_summary_path, surface_dir, summary_dir, frame_dir, *frame_texts = sys.argv[1:]
frames = [int(value) for value in frame_texts]

with open(physics_summary_path, "r", encoding="utf-8") as f:
    physics_summary = json.load(f)

def require(condition, message):
    if not condition:
        raise SystemExit(message)

def load_surface(frame_index):
    path = os.path.join(surface_dir, f"water_surface_{frame_index:06d}.json")
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def load_summary(frame_index):
    path = os.path.join(summary_dir, f"render_summary_{frame_index:04d}.json")
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def read_bmp_rgb(path):
    with open(path, "rb") as f:
        data = f.read()
    require(data[:2] == b"BM", f"{path} is not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = abs(struct.unpack_from("<i", data, 22)[0])
    bpp = struct.unpack_from("<H", data, 28)[0]
    require(bpp in (24, 32), f"{path} is not a 24-bit or 32-bit BMP")
    bytes_per_pixel = bpp // 8
    row_stride = ((width * bytes_per_pixel + 3) // 4) * 4
    pixels = []
    for y in range(height):
        row_start = offset + y * row_stride
        for x in range(width):
            pixel_start = row_start + x * bytes_per_pixel
            b, g, r = data[pixel_start: pixel_start + 3]
            pixels.append((r, g, b))
    return width, height, pixels

def write_bmp_rgb(path, width, height, pixels):
    row_stride = ((width * 3 + 3) // 4) * 4
    image_size = row_stride * height
    header_size = 54
    with open(path, "wb") as f:
        f.write(b"BM")
        f.write(struct.pack("<IHHI", header_size + image_size, 0, 0, header_size))
        f.write(struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, image_size, 2835, 2835, 0, 0))
        padding = b"\0" * (row_stride - width * 3)
        for y in range(height):
            row = pixels[y * width:(y + 1) * width]
            for r, g, b in row:
                f.write(bytes((b, g, r)))
            f.write(padding)

def image_metrics(frame_index):
    path = os.path.join(frame_dir, f"frame_{frame_index:04d}.bmp")
    require(os.path.getsize(path) > 0, f"missing rendered frame {path}")
    width, height, pixels = read_bmp_rgb(path)
    return {
        "frame": frame_index,
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

surfaces = [load_surface(frame) for frame in frames]
heights = [[float(h) for h in surface.get("heights_y") or []] for surface in surfaces]
for frame, surface, frame_heights in zip(frames, surfaces, heights):
    summary = surface.get("summary") or {}
    require(surface.get("sample_count", 0) >= 900, f"surface {frame} sample count too low")
    require(summary.get("review_ripples_applied") is True, f"surface {frame} did not apply review ripples")
    require(float(summary.get("max_slope", 0.0)) > 0.02, f"surface {frame} too flat")
    require(len(frame_heights) == int(surface.get("sample_count", 0)), f"surface {frame} height count mismatch")

height_deltas = []
for left, right in zip(heights, heights[1:]):
    require(len(left) == len(right), "surface sample counts differ across sparse frames")
    height_deltas.append(sum(abs(a - b) for a, b in zip(left, right)) / len(left))
first_last_height_delta = sum(abs(a - b) for a, b in zip(heights[0], heights[-1])) / len(heights[0])
require(first_last_height_delta > 0.006, f"long sparse water height delta too small: {first_last_height_delta}")
require(max(height_deltas) > 0.004, f"adjacent sparse water height deltas too small: {height_deltas}")

metrics = [image_metrics(frame) for frame in frames]
for frame, summary in ((frame, load_summary(frame)) for frame in frames):
    require(summary.get("rendered_frames") is True, f"render {frame} did not complete")
    water = summary.get("water_surface") or {}
    require(water.get("loaded") is True, f"render {frame} did not load water")
    require(water.get("mesh_attached") is True, f"render {frame} did not attach water mesh")
    require(water.get("loaded_first_frame_index") == frame, f"render {frame} loaded wrong first water frame")
    require(water.get("loaded_last_frame_index") == frame, f"render {frame} loaded wrong last water frame")
    render_stats = summary.get("render_stats") or {}
    require(int(render_stats.get("secondary_hits", 0)) > 0, f"render {frame} did not trace through transparent water")

image_deltas = [mean_abs_delta(a, b) for a, b in zip(metrics, metrics[1:])]
first_last_image_delta = mean_abs_delta(metrics[0], metrics[-1])
require(first_last_image_delta > 2.0, f"long sparse image delta too small: {first_last_image_delta}")
require(max(image_deltas) > 1.0, f"adjacent sparse image deltas too small: {image_deltas}")
require(all(m["nonzero"] > 0 for m in metrics), "one or more frames are blank")
require(max(m["max_rgb"] for m in metrics) > 80, "sequence has no bright light response")

width = metrics[0]["width"]
height = metrics[0]["height"]
label_h = 24
sheet_width = width * len(metrics)
sheet_height = height + label_h
sheet = [(8, 10, 12)] * (sheet_width * sheet_height)
for index, metric in enumerate(metrics):
    x_offset = index * width
    for y in range(height):
        for x in range(width):
            sheet[(y + label_h) * sheet_width + x_offset + x] = metric["pixels"][y * width + x]
contact_path = os.path.join(frame_dir, "water_long_motion_contact_sheet.bmp")
write_bmp_rgb(contact_path, sheet_width, sheet_height, sheet)

print(json.dumps({
    "frames": frames,
    "height_deltas": height_deltas,
    "first_last_height_delta": first_last_height_delta,
    "image_deltas": image_deltas,
    "first_last_image_delta": first_last_image_delta,
    "contact_sheet": contact_path,
}, indent=2))
INNERPY

echo "ray tracing water long-motion sparse-frame review passed: $RAY_OUT/frames"
