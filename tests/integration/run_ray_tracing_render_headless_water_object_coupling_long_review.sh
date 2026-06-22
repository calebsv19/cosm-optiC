#!/usr/bin/env bash
set -euo pipefail

RAY_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CODEWORK_ROOT="$(cd "$RAY_DIR/.." && pwd)"
PHYSICS_DIR="$CODEWORK_ROOT/physics_sim"

PROFILE="${WTR6_LONG_PROFILE:-smoke}"
case "$PROFILE" in
  smoke)
    DEFAULT_WARMUP_FRAMES=8
    DEFAULT_OUTPUT_FRAMES=4
    DEFAULT_FRAME_STRIDE=5
    DEFAULT_SIM_STEPS_PER_FRAME=3
    DEFAULT_GRID="32x16x32"
    DEFAULT_WIDTH=240
    DEFAULT_HEIGHT=180
    DEFAULT_TEMPORAL_FRAMES=1
    DEFAULT_FPS=8
    DEFAULT_RIPPLE_AMPLITUDE=0.040
    DEFAULT_RUN_SLUG="water_object_coupling_long_review_smoke"
    ;;
  review)
    DEFAULT_WARMUP_FRAMES=60
    DEFAULT_OUTPUT_FRAMES=24
    DEFAULT_FRAME_STRIDE=5
    DEFAULT_SIM_STEPS_PER_FRAME=4
    DEFAULT_GRID="36x18x36"
    DEFAULT_WIDTH=400
    DEFAULT_HEIGHT=240
    DEFAULT_TEMPORAL_FRAMES=1
    DEFAULT_FPS=12
    DEFAULT_RIPPLE_AMPLITUDE=0.050
    DEFAULT_RUN_SLUG="water_object_coupling_long_review"
    ;;
  full)
    DEFAULT_WARMUP_FRAMES=200
    DEFAULT_OUTPUT_FRAMES=100
    DEFAULT_FRAME_STRIDE=5
    DEFAULT_SIM_STEPS_PER_FRAME=4
    DEFAULT_GRID="40x20x40"
    DEFAULT_WIDTH=640
    DEFAULT_HEIGHT=360
    DEFAULT_TEMPORAL_FRAMES=2
    DEFAULT_FPS=24
    DEFAULT_RIPPLE_AMPLITUDE=0.055
    DEFAULT_RUN_SLUG="water_object_coupling_long_review"
    ;;
  *)
    echo "unknown WTR6_LONG_PROFILE=$PROFILE (expected smoke, review, or full)" >&2
    exit 2
    ;;
esac

WARMUP_FRAMES="${WTR6_LONG_WARMUP_FRAMES:-$DEFAULT_WARMUP_FRAMES}"
OUTPUT_FRAMES="${WTR6_LONG_OUTPUT_FRAMES:-$DEFAULT_OUTPUT_FRAMES}"
FRAME_STRIDE="${WTR6_LONG_FRAME_STRIDE:-$DEFAULT_FRAME_STRIDE}"
SIM_STEPS_PER_FRAME="${WTR6_LONG_SIM_STEPS_PER_FRAME:-$DEFAULT_SIM_STEPS_PER_FRAME}"
GRID="${WTR6_LONG_GRID:-$DEFAULT_GRID}"
WIDTH="${WTR6_LONG_WIDTH:-$DEFAULT_WIDTH}"
HEIGHT="${WTR6_LONG_HEIGHT:-$DEFAULT_HEIGHT}"
TEMPORAL_FRAMES="${WTR6_LONG_TEMPORAL_FRAMES:-$DEFAULT_TEMPORAL_FRAMES}"
FPS="${WTR6_LONG_FPS:-$DEFAULT_FPS}"
RIPPLE_AMPLITUDE="${WTR6_LONG_RIPPLE_AMPLITUDE:-$DEFAULT_RIPPLE_AMPLITUDE}"
RUN_SLUG="${WTR6_LONG_RUN_SLUG:-$DEFAULT_RUN_SLUG}"
WATER_LEVEL="${WTR6_LONG_WATER_LEVEL:-0.58}"
INTEGRATOR_3D="${WTR6_LONG_INTEGRATOR_3D:-emission_transparency}"

if (( OUTPUT_FRAMES < 2 )); then
  echo "WTR6_LONG_OUTPUT_FRAMES must be at least 2" >&2
  exit 2
fi
if (( FRAME_STRIDE < 1 )); then
  echo "WTR6_LONG_FRAME_STRIDE must be positive" >&2
  exit 2
fi
if (( WARMUP_FRAMES < 0 )); then
  echo "WTR6_LONG_WARMUP_FRAMES must be non-negative" >&2
  exit 2
fi

LAST_SELECTED_FRAME=$((WARMUP_FRAMES + (OUTPUT_FRAMES - 1) * FRAME_STRIDE))
PHYSICS_FRAMES=$((LAST_SELECTED_FRAME + 1))

RUN_ROOT="$RAY_DIR/build/agent_runs/physics_trio/$RUN_SLUG"
PHYSICS_OUT="$RUN_ROOT/physics_sim"
RAY_OUT="$RUN_ROOT/ray_tracing"
REQUEST_DIR="$RAY_OUT/requests"
SUMMARY_DIR="$RAY_OUT/summaries"
RUNTIME_SCENE="$RUN_ROOT/runtime_scene.json"
SELECTED_FRAMES_FILE="$RUN_ROOT/selected_frames.txt"
SETTINGS_JSON="$RUN_ROOT/long_review_settings.json"

PHYSICS_TOOL="$PHYSICS_DIR/physics_sim_headless"
RAY_TOOL="$RAY_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"

rm -rf "$RUN_ROOT"
mkdir -p "$RUN_ROOT" "$PHYSICS_OUT" "$RAY_OUT/frames" "$REQUEST_DIR" "$SUMMARY_DIR"

make -C "$PHYSICS_DIR" physics_sim_headless
make -C "$RAY_DIR" BUILD_TOOLCHAIN=clang ray-tracing-render-headless

"$PHYSICS_TOOL" \
  --water-mode \
  --frames "$PHYSICS_FRAMES" \
  --sim-steps-per-frame "$SIM_STEPS_PER_FRAME" \
  --grid "$GRID" \
  --water-level "$WATER_LEVEL" \
  --water-object-fixture \
  --water-review-ripples \
  --water-review-ripple-amplitude "$RIPPLE_AMPLITUDE" \
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

python3 - "$RUN_ROOT" "$RUNTIME_SCENE" "$SCENE_BUNDLE" "$RAY_OUT" "$REQUEST_DIR" "$SUMMARY_DIR" \
  "$SELECTED_FRAMES_FILE" "$SETTINGS_JSON" "$PROFILE" "$WARMUP_FRAMES" "$OUTPUT_FRAMES" \
  "$FRAME_STRIDE" "$SIM_STEPS_PER_FRAME" "$GRID" "$WIDTH" "$HEIGHT" "$TEMPORAL_FRAMES" \
  "$FPS" "$RIPPLE_AMPLITUDE" "$WATER_LEVEL" "$INTEGRATOR_3D" <<'PY'
import json
import math
import os
import sys

(
    run_root,
    runtime_scene_path,
    scene_bundle_path,
    ray_out,
    request_dir,
    summary_dir,
    selected_frames_path,
    settings_path,
    profile,
    warmup_text,
    output_text,
    stride_text,
    sim_steps_text,
    grid,
    width_text,
    height_text,
    temporal_text,
    fps_text,
    ripple_text,
    water_level_text,
    integrator_3d,
) = sys.argv[1:]

warmup = int(warmup_text)
output_frames = int(output_text)
stride = int(stride_text)
width = int(width_text)
height = int(height_text)
temporal_frames = int(temporal_text)
fps = int(fps_text)
selected_frames = [warmup + i * stride for i in range(output_frames)]

def point(x, y, z=None):
    value = {"x": x, "y": y}
    if z is not None:
        value["z"] = z
    return value

def rect(object_id, x, y, z, w, h, d, color, roughness, reflectivity):
    return {
        "object": {
            "object_id": object_id,
            "object_type": "rect_prism_primitive",
            "space_mode_intent": "3d",
            "dimensional_mode": "full_3d",
            "transform": {
                "position": point(x, y, z),
                "rotation": point(0.0, 0.0, 0.0),
                "scale": point(1.0, 1.0, 1.0),
            },
            "primitive": {
                "kind": "rect_prism_primitive",
                "width": w,
                "height": h,
                "depth": d,
            },
        },
        "material": {
            "object_id": object_id,
            "material_id": 0,
            "object_color": color,
            "roughness": roughness,
            "reflectivity": reflectivity,
        },
    }

def plane(object_id, origin, axis_u, axis_v, normal, width_value, height_value, color, roughness, reflectivity):
    return {
        "object": {
            "object_id": object_id,
            "object_type": "plane_primitive",
            "space_mode_intent": "3d",
            "dimensional_mode": "full_3d",
            "transform": {
                "position": point(origin[0], origin[1], origin[2]),
                "rotation": point(0.0, 0.0, 0.0),
                "scale": point(1.0, 1.0, 1.0),
            },
            "primitive": {
                "kind": "plane_primitive",
                "width": width_value,
                "height": height_value,
                "lock_to_construction_plane": False,
                "lock_to_bounds": False,
                "frame": {
                    "origin": point(origin[0], origin[1], origin[2]),
                    "axis_u": point(axis_u[0], axis_u[1], axis_u[2]),
                    "axis_v": point(axis_v[0], axis_v[1], axis_v[2]),
                    "normal": point(normal[0], normal[1], normal[2]),
                },
            },
        },
        "material": {
            "object_id": object_id,
            "material_id": 0,
            "object_color": color,
            "roughness": roughness,
            "reflectivity": reflectivity,
        },
    }

scene_entries = [
    plane("basin_floor", (2.0, 2.0, 0.02), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), 4.35, 4.35, 3365436, 0.62, 0.07),
    rect("basin_back_wall", 2.0, 4.08, 0.42, 4.35, 0.14, 0.82, 6381921, 0.66, 0.06),
    rect("basin_left_wall", -0.08, 2.0, 0.42, 0.14, 4.35, 0.82, 6052708, 0.68, 0.05),
    rect("basin_right_wall", 4.08, 2.0, 0.42, 0.14, 4.35, 0.82, 6052708, 0.68, 0.05),
    plane("clean_backdrop", (2.0, 4.34, 1.08), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, -1.0, 0.0), 4.9, 1.8, 7895945, 0.72, 0.04),
    rect("water_coupled_block", 2.0, 2.0, 0.47, 0.64, 0.64, 0.66, 15115520, 0.38, 0.16),
]

light_radius = 1.35
light_center = (2.0, 2.0)
circle_points = []
for angle in (0.0, math.pi * 0.5, math.pi, math.pi * 1.5, math.pi * 2.0):
    circle_points.append({
        "x": light_center[0] + math.cos(angle) * light_radius,
        "y": light_center[1] + math.sin(angle) * light_radius,
        "rotation": 0.0,
        "handleLink": False,
    })

scene = {
    "schema_family": "codework_scene",
    "schema_variant": "scene_runtime_v1",
    "schema_version": 1,
    "scene_id": "scene_water_object_coupling_long_review",
    "space_mode_default": "3d",
    "unit_system": "meters",
    "world_scale": 1.0,
    "objects": [entry["object"] for entry in scene_entries],
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
                    "points": circle_points,
                },
                "light_path_depth": {
                    "points": [
                        {"z": 2.28, "lookPitch": 0.0} for _ in circle_points
                    ],
                },
                "light_settings": {"intensity": 11.5, "radius": 0.14},
                "environment": {
                    "light_mode": 2,
                    "ambient_strength": 0.12,
                    "top_fill_strength": 0.70,
                },
                "object_materials": [entry["material"] for entry in scene_entries],
            },
        },
    },
}

with open(runtime_scene_path, "w", encoding="utf-8") as f:
    json.dump(scene, f, indent=2)
    f.write("\n")

with open(selected_frames_path, "w", encoding="utf-8") as f:
    for frame in selected_frames:
        f.write(f"{frame}\n")

settings = {
    "profile": profile,
    "warmup_frames": warmup,
    "output_frames": output_frames,
    "frame_stride": stride,
    "selected_first_frame": selected_frames[0],
    "selected_last_frame": selected_frames[-1],
    "physics_frames": selected_frames[-1] + 1,
    "sim_steps_per_frame": int(sim_steps_text),
    "grid": grid,
    "width": width,
    "height": height,
    "temporal_frames": temporal_frames,
    "fps": fps,
    "ripple_amplitude_m": float(ripple_text),
    "water_level": float(water_level_text),
    "integrator_3d": integrator_3d,
    "scene_bundle": scene_bundle_path,
}
with open(settings_path, "w", encoding="utf-8") as f:
    json.dump(settings, f, indent=2)
    f.write("\n")

for i, frame in enumerate(selected_frames):
    normalized_t = 0.0 if output_frames <= 1 else i / float(output_frames - 1)
    request = {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": f"agent_render_water_object_coupling_long_review_{frame:04d}",
        "scene": {"runtime_scene_path": runtime_scene_path},
        "volume": {
            "enabled": True,
            "source_kind": "scene_bundle",
            "source_path": scene_bundle_path,
            "affects_lighting": True,
            "debug_overlay": False,
        },
        "render": {
            "start_frame": frame,
            "frame_count": 1,
            "width": width,
            "height": height,
            "normalized_t": normalized_t,
            "temporal_frames": temporal_frames,
            "integrator_3d": integrator_3d,
        },
        "inspection": {
            "camera_zoom": 0.75,
            "camera_position": {"x": 2.0, "y": -4.75, "z": 2.42},
            "camera_look_at": {"x": 2.0, "y": 1.95, "z": 0.50},
            "environment_brightness": 0.016,
            "ambient_strength": 0.12,
            "environment_light_mode": "ambient",
            "top_fill_strength": 0.70,
            "background_brightness": 0.010,
            "background_color": {"r": 0.004, "g": 0.007, "b": 0.010},
            "light_intensity": 11.5,
            "light_radius": 0.14,
            "forward_decay": 260.0,
            "secondary_diffuse_samples_3d": 3,
            "transmission_samples_3d": 3,
            "object_audit_enabled": True,
            "object_audit_max_dimension": 180,
            "volume_scatter_gain": 1.35,
            "volume_step_scale": 0.85,
            "volume_tint": {"r": 0.28, "g": 0.72, "b": 1.45},
        },
        "output": {"root": ray_out, "overwrite": True},
        "progress": {
            "summary_path": os.path.join(summary_dir, f"render_summary_{frame:04d}.json"),
            "progress_path": os.path.join(summary_dir, f"render_progress_{frame:04d}.json"),
        },
    }
    request_path = os.path.join(request_dir, f"ray_tracing_request_{i:04d}_frame_{frame:04d}.json")
    with open(request_path, "w", encoding="utf-8") as f:
        json.dump(request, f, indent=2)
        f.write("\n")
PY

while IFS= read -r frame_index; do
  output_index=$(((frame_index - WARMUP_FRAMES) / FRAME_STRIDE))
  request="$REQUEST_DIR/ray_tracing_request_$(printf '%04d' "$output_index")_frame_$(printf '%04d' "$frame_index").json"
  summary="$SUMMARY_DIR/render_summary_$(printf '%04d' "$frame_index").json"
  "$RAY_TOOL" --request "$request" --render --summary "$summary" > "$SUMMARY_DIR/stdout_$(printf '%04d' "$frame_index").json"
done < "$SELECTED_FRAMES_FILE"

python3 - "$PHYSICS_OUT/run_summary.json" "$RUN_DIR" "$SUMMARY_DIR" "$RAY_OUT/frames" \
  "$SELECTED_FRAMES_FILE" "$SETTINGS_JSON" "$PROFILE" <<'PY'
import json
import os
import struct
import sys

physics_summary_path, surface_dir, summary_dir, frame_dir, selected_frames_path, settings_path, profile = sys.argv[1:]
with open(selected_frames_path, "r", encoding="utf-8") as f:
    frames = [int(line.strip()) for line in f if line.strip()]
with open(settings_path, "r", encoding="utf-8") as f:
    settings = json.load(f)
expected_integrator = settings.get("integrator_3d", "emission_transparency")
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

def mean_abs_delta(a, b):
    require(a["width"] == b["width"] and a["height"] == b["height"], "frame dimensions changed")
    total = 0
    for pa, pb in zip(a["pixels"], b["pixels"]):
        total += abs(pa[0] - pb[0]) + abs(pa[1] - pb[1]) + abs(pa[2] - pb[2])
    return total / (len(a["pixels"]) * 3.0)

require(physics_summary.get("mode") == "water", "physics run did not use water mode")
require(physics_summary.get("water_object_fixture") is True, "physics run did not enable object fixture")
require(physics_summary.get("water_review_ripples") is True, "physics run did not enable review ripples")

height_sets = []
object_solid_cells = []
object_wet_overlap_cells = []
displacement_deltas = []
for frame in frames:
    surface_path = os.path.join(surface_dir, f"water_surface_{frame:06d}.json")
    surface = load_json(surface_path)
    summary = surface.get("summary") or {}
    obj = summary.get("object_coupling") or {}
    require(surface.get("sample_count", 0) >= 500, f"surface {frame} sample count too low")
    require(summary.get("review_ripples_applied") is True, f"surface {frame} did not apply review ripples")
    require(obj.get("enabled") is True, f"surface {frame} object coupling disabled")
    require(obj.get("fixture_active") is True, f"surface {frame} object fixture inactive")
    require(int(obj.get("object_solid_cells", 0)) > 0, f"surface {frame} has no object solid cells")
    require(int(obj.get("object_wet_overlap_cells", 0)) > 0, f"surface {frame} has no wet object overlap")
    require(float(obj.get("displaced_volume_m3", 0.0)) > 0.0, f"surface {frame} has no displaced volume")
    require(obj.get("displacement_applied") is True, f"surface {frame} has no object displacement")
    object_solid_cells.append(int(obj.get("object_solid_cells", 0)))
    object_wet_overlap_cells.append(int(obj.get("object_wet_overlap_cells", 0)))
    displacement_deltas.append([
        float(obj.get("displacement_delta_min_m", 0.0)),
        float(obj.get("displacement_delta_max_m", 0.0)),
    ])
    heights = [float(value) for value in surface.get("heights_y") or []]
    require(len(heights) == int(surface.get("sample_count", 0)), f"surface {frame} height count mismatch")
    height_sets.append(heights)

height_deltas = []
for left, right in zip(height_sets, height_sets[1:]):
    height_deltas.append(sum(abs(a - b) for a, b in zip(left, right)) / len(left))
first_last_height_delta = sum(abs(a - b) for a, b in zip(height_sets[0], height_sets[-1])) / len(height_sets[0])
require(max(height_deltas) > 0.0008, f"long object-water height deltas too small: {height_deltas}")

image_metrics = []
object_hits = []
secondary_hits = []
for frame in frames:
    summary = load_json(os.path.join(summary_dir, f"render_summary_{frame:04d}.json"))
    require(summary.get("rendered_frames") is True, f"render {frame} did not complete")
    require(summary.get("integrator_3d") == expected_integrator,
            f"render {frame} used {summary.get('integrator_3d')} not {expected_integrator}")
    water = summary.get("water_surface") or {}
    require(water.get("loaded") is True, f"render {frame} did not load water")
    require(water.get("mesh_attached") is True, f"render {frame} did not attach water mesh")
    require(water.get("loaded_first_frame_index") == frame, f"render {frame} loaded wrong water frame")
    stats = summary.get("render_stats") or {}
    secondary = int(stats.get("secondary_hits", 0))
    require(secondary > 0, f"render {frame} had no secondary water hits")
    secondary_hits.append(secondary)
    audit = summary.get("object_audit") or []
    block = next((entry for entry in audit if entry.get("object_id") == "water_coupled_block"), None)
    require(block is not None, f"render {frame} missing object audit for block")
    require(int(block.get("primary_hit_pixels", 0)) > 0, f"render {frame} block not visible")
    object_hits.append(int(block.get("primary_hit_pixels", 0)))
    image_path = os.path.join(frame_dir, f"frame_{frame:04d}.bmp")
    width, height, pixels = read_bmp_rgb(image_path)
    image_metrics.append({
        "frame": frame,
        "width": width,
        "height": height,
        "pixels": pixels,
        "nonzero": sum(1 for p in pixels if p != (0, 0, 0)),
        "max_rgb": max(max(p) for p in pixels),
    })

for metric in image_metrics:
    require(metric["nonzero"] > 0, f"frame {metric['frame']} is blank")
require(max(metric["max_rgb"] for metric in image_metrics) > 40, "sequence has no readable light response")

image_deltas = [mean_abs_delta(a, b) for a, b in zip(image_metrics, image_metrics[1:])]
first_last_image_delta = mean_abs_delta(image_metrics[0], image_metrics[-1])
require(max(image_deltas) > 0.25, f"long object-water image deltas too small: {image_deltas}")

report = {
    "profile": profile,
    "settings": settings,
    "frames": frames,
    "object_solid_cells": object_solid_cells,
    "object_wet_overlap_cells": object_wet_overlap_cells,
    "displacement_deltas": displacement_deltas,
    "height_deltas": height_deltas,
    "first_last_height_delta": first_last_height_delta,
    "object_primary_hit_pixels": object_hits,
    "secondary_hits": secondary_hits,
    "image_deltas": image_deltas,
    "first_last_image_delta": first_last_image_delta,
    "frame_dir": frame_dir,
}
summary_path = os.path.join(os.path.dirname(settings_path), "long_review_summary.json")
with open(summary_path, "w", encoding="utf-8") as f:
    json.dump(report, f, indent=2)
    f.write("\n")
print(json.dumps(report, indent=2))
PY

if command -v ffmpeg >/dev/null 2>&1; then
  ffmpeg -y -hide_banner -loglevel error -framerate "$FPS" \
    -pattern_type glob -i "$RAY_OUT/frames/frame_*.bmp" \
    -pix_fmt yuv420p "$RAY_OUT/frames/water_object_coupling_long_review.mp4"
else
  cat > "$RAY_OUT/frames/mp4_conversion_command.txt" <<EOF
ffmpeg -y -framerate '$FPS' -pattern_type glob -i '$RAY_OUT/frames/frame_*.bmp' -pix_fmt yuv420p '$RAY_OUT/frames/water_object_coupling_long_review.mp4'
EOF
fi

echo "ray tracing water object-coupling long review passed ($PROFILE): $RAY_OUT/frames"
