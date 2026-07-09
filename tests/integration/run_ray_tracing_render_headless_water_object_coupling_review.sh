#!/usr/bin/env bash
set -euo pipefail

RAY_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CODEWORK_ROOT="$(cd "$RAY_DIR/.." && pwd)"
PHYSICS_DIR="$CODEWORK_ROOT/physics_sim"

RUN_ROOT="$RAY_DIR/build/agent_runs/physics_trio/water_object_coupling_review"
PHYSICS_OUT="$RUN_ROOT/physics_sim"
RAY_OUT="$RUN_ROOT/ray_tracing"
SUMMARY_DIR="$RAY_OUT/summaries"
RUNTIME_SCENE="$RUN_ROOT/runtime_scene.json"
PHYSICS_FRAMES=28
SELECTED_FRAMES=(8 18 27)
LIGHT_T=(0.10 0.55 0.95)

PHYSICS_TOOL="$PHYSICS_DIR/physics_sim_headless"
RAY_TOOL="$RAY_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"

rm -rf "$RUN_ROOT"
mkdir -p "$RUN_ROOT" "$PHYSICS_OUT" "$RAY_OUT" "$SUMMARY_DIR"

make -C "$PHYSICS_DIR" physics_sim_headless
make -C "$RAY_DIR" BUILD_TOOLCHAIN=clang ray-tracing-render-headless

"$PHYSICS_TOOL" \
  --water-mode \
  --frames "$PHYSICS_FRAMES" \
  --sim-steps-per-frame 3 \
  --grid 32x16x32 \
  --water-level 0.58 \
  --water-object-fixture \
  --water-review-ripples \
  --water-review-ripple-amplitude 0.028 \
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

cat > "$RUNTIME_SCENE" <<JSON
{
  "schema_family": "codework_scene",
  "schema_variant": "scene_runtime_v1",
  "schema_version": 1,
  "scene_id": "scene_water_object_coupling_review",
  "space_mode_default": "3d",
  "unit_system": "meters",
  "world_scale": 1.0,
  "objects": [
    {
      "object_id": "basin_floor",
      "object_type": "plane_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": { "position": { "x": 2.0, "y": 2.0, "z": 0.02 }, "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 }, "scale": { "x": 1.0, "y": 1.0, "z": 1.0 } },
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
      "transform": { "position": { "x": 2.0, "y": 4.08, "z": 0.42 }, "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 }, "scale": { "x": 1.0, "y": 1.0, "z": 1.0 } },
      "primitive": { "kind": "rect_prism_primitive", "width": 4.35, "height": 0.14, "depth": 0.82 }
    },
    {
      "object_id": "basin_left_wall",
      "object_type": "rect_prism_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": { "position": { "x": -0.08, "y": 2.0, "z": 0.42 }, "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 }, "scale": { "x": 1.0, "y": 1.0, "z": 1.0 } },
      "primitive": { "kind": "rect_prism_primitive", "width": 0.14, "height": 4.35, "depth": 0.82 }
    },
    {
      "object_id": "basin_right_wall",
      "object_type": "rect_prism_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": { "position": { "x": 4.08, "y": 2.0, "z": 0.42 }, "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 }, "scale": { "x": 1.0, "y": 1.0, "z": 1.0 } },
      "primitive": { "kind": "rect_prism_primitive", "width": 0.14, "height": 4.35, "depth": 0.82 }
    },
    {
      "object_id": "clean_backdrop",
      "object_type": "plane_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": { "position": { "x": 2.0, "y": 4.34, "z": 1.08 }, "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 }, "scale": { "x": 1.0, "y": 1.0, "z": 1.0 } },
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
      "object_id": "water_coupled_block",
      "object_type": "rect_prism_primitive",
      "space_mode_intent": "3d",
      "dimensional_mode": "full_3d",
      "transform": { "position": { "x": 2.0, "y": 2.0, "z": 0.47 }, "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 }, "scale": { "x": 1.0, "y": 1.0, "z": 1.0 } },
      "primitive": { "kind": "rect_prism_primitive", "width": 0.64, "height": 0.64, "depth": 0.66 }
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
            { "x": 0.80, "y": 1.18, "rotation": 0.0, "handleLink": false, "velocity1": { "vx": 0.44, "vy": -0.10 } },
            { "x": 3.20, "y": 2.42, "rotation": 0.0, "handleLink": false, "velocity2": { "vx": -0.44, "vy": 0.10 } }
          ]
        },
        "light_path_depth": {
          "points": [
            { "z": 2.10, "lookPitch": 0.0, "velocity1": { "vz": 0.0 } },
            { "z": 2.10, "lookPitch": 0.0, "velocity2": { "vz": 0.0 } }
          ]
        },
        "light_settings": { "intensity": 9.6, "radius": 0.15 },
        "environment": { "light_mode": 2, "ambient_strength": 0.16, "top_fill_strength": 0.90 },
        "object_materials": [
          { "object_id": "basin_floor", "material_id": 0, "object_color": 3365436, "roughness": 0.62, "reflectivity": 0.07 },
          { "object_id": "basin_back_wall", "material_id": 0, "object_color": 6381921, "roughness": 0.66, "reflectivity": 0.06 },
          { "object_id": "basin_left_wall", "material_id": 0, "object_color": 6052708, "roughness": 0.68, "reflectivity": 0.05 },
          { "object_id": "basin_right_wall", "material_id": 0, "object_color": 6052708, "roughness": 0.68, "reflectivity": 0.05 },
          { "object_id": "clean_backdrop", "material_id": 0, "object_color": 7895945, "roughness": 0.72, "reflectivity": 0.04 },
          { "object_id": "water_coupled_block", "material_id": 0, "object_color": 15115520, "roughness": 0.38, "reflectivity": 0.16 }
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
  "run_id": "agent_render_water_object_coupling_review_$(printf '%04d' "$frame_index")",
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
    "camera_zoom": 0.76,
    "camera_position": { "x": 2.0, "y": -4.65, "z": 2.35 },
    "camera_look_at": { "x": 2.0, "y": 1.95, "z": 0.50 },
    "environment_brightness": 0.018,
    "ambient_strength": 0.14,
    "environment_light_mode": "ambient",
    "top_fill_strength": 0.90,
    "background_brightness": 0.012,
    "background_color": { "r": 0.004, "g": 0.007, "b": 0.010 },
    "light_intensity": 9.6,
    "light_radius": 0.15,
    "forward_decay": 240.0,
    "secondary_diffuse_samples_3d": 2,
    "transmission_samples_3d": 2,
    "object_audit_enabled": true,
    "object_audit_max_dimension": 180,
    "volume_scatter_gain": 1.25,
    "volume_step_scale": 0.9,
    "volume_tint": { "r": 0.30, "g": 0.72, "b": 1.40 }
  },
  "output": { "root": "$RAY_OUT", "overwrite": true },
  "progress": { "summary_path": "$summary", "progress_path": "$progress" }
}
JSON
  "$RAY_TOOL" --request "$request" --render --summary "$summary" > "$SUMMARY_DIR/stdout_$(printf '%04d' "$frame_index").json"
done

python3 - "$PHYSICS_OUT/run_summary.json" "$RUN_DIR" "$SUMMARY_DIR" "$RAY_OUT/frames" "${SELECTED_FRAMES[@]}" <<'PY'
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

def load_json(path):
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
    require(bpp in (24, 32), f"{path} has unsupported BMP depth")
    bpp_bytes = bpp // 8
    row_stride = ((width * bpp_bytes + 3) // 4) * 4
    pixels = []
    for y in range(height):
        row = offset + y * row_stride
        for x in range(width):
            b, g, r = data[row + x * bpp_bytes: row + x * bpp_bytes + 3]
            pixels.append((r, g, b))
    return width, height, pixels

def mean_abs_delta(a, b):
    require(a[0] == b[0] and a[1] == b[1], "render dimensions changed")
    total = 0
    for pa, pb in zip(a[2], b[2]):
        total += abs(pa[0] - pb[0]) + abs(pa[1] - pb[1]) + abs(pa[2] - pb[2])
    return total / (len(a[2]) * 3.0)

require(physics_summary.get("mode") == "water", "physics run did not use water mode")
require(physics_summary.get("water_object_fixture") is True, "physics object fixture disabled")

for frame in frames:
    surface = load_json(os.path.join(surface_dir, f"water_surface_{frame:06d}.json"))
    obj = ((surface.get("summary") or {}).get("object_coupling") or {})
    require(obj.get("fixture_active") is True, f"surface {frame} fixture inactive")
    require(int(obj.get("object_solid_cells", 0)) > 0, f"surface {frame} has no object solid cells")
    require(int(obj.get("object_wet_overlap_cells", 0)) > 0, f"surface {frame} has no wet overlap")
    require(float(obj.get("displaced_volume_m3", 0.0)) > 0.0, f"surface {frame} has no displaced volume")
    require(obj.get("displacement_applied") is True, f"surface {frame} has no displacement")

images = []
object_hit_counts = []
for frame in frames:
    summary = load_json(os.path.join(summary_dir, f"render_summary_{frame:04d}.json"))
    require(summary.get("rendered_frames") is True, f"render {frame} did not complete")
    water = summary.get("water_surface") or {}
    require(water.get("loaded") is True, f"render {frame} did not load water")
    require(water.get("mesh_attached") is True, f"render {frame} did not attach water mesh")
    require(water.get("loaded_first_frame_index") == frame, f"render {frame} loaded wrong water frame")
    stats = summary.get("render_stats") or {}
    require(int(stats.get("secondary_hits", 0)) > 0, f"render {frame} had no secondary water hits")
    audit = summary.get("object_audit") or []
    block = next((entry for entry in audit if entry.get("object_id") == "water_coupled_block"), None)
    require(block is not None, f"render {frame} missing object audit for block")
    require(int(block.get("triangle_count", 0)) >= 12, f"render {frame} block triangles missing")
    require(int(block.get("primary_hit_pixels", 0)) > 0, f"render {frame} block not visible")
    object_hit_counts.append(int(block.get("primary_hit_pixels", 0)))
    image_path = os.path.join(frame_dir, f"frame_{frame:04d}.bmp")
    require(os.path.getsize(image_path) > 0, f"missing frame {image_path}")
    image = read_bmp_rgb(image_path)
    require(sum(1 for p in image[2] if p != (0, 0, 0)) > 0, f"frame {frame} blank")
    images.append(image)

deltas = [mean_abs_delta(a, b) for a, b in zip(images, images[1:])]
require(max(deltas) > 0.5, f"object-water sequence image deltas too small: {deltas}")

print(json.dumps({
    "frames": frames,
    "object_primary_hit_pixels": object_hit_counts,
    "image_deltas": deltas,
    "frame_dir": frame_dir,
}, indent=2))
PY

if command -v ffmpeg >/dev/null 2>&1; then
  ffmpeg -y -hide_banner -loglevel error -framerate 8 \
    -pattern_type glob -i "$RAY_OUT/frames/frame_*.bmp" \
    -pix_fmt yuv420p "$RAY_OUT/frames/water_object_coupling_review.mp4"
else
  cat > "$RAY_OUT/frames/mp4_conversion_command.txt" <<EOF
ffmpeg -y -framerate 8 -pattern_type glob -i '$RAY_OUT/frames/frame_*.bmp' -pix_fmt yuv420p '$RAY_OUT/frames/water_object_coupling_review.mp4'
EOF
fi

echo "ray tracing water object-coupling review passed: $RAY_OUT/frames"
