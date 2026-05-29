# RayTracing Headless Agent Render CLI

Status: Phase 4 volume handoff image export contract landed, with runtime-scene camera fallback for native `3D` renders, additive colored volume inspection tint support, and a first detached RayTracing local job runner.

Environment-light inspection overrides now also support the shipped three-way
renderer lane:
- `off`
- `top_fill`
- `ambient`

Use `ambient_strength` for the ambient-mode surface-fill amount (`0.0..1.0`)
and `top_fill_strength` for the top-fill lane (`0.0..20.0`). The older
`environment_brightness` override remains available as a compatibility knob for
the underlying byte-domain environment brightness state.

The first RayTracing agent command is:

```bash
make -C ray_tracing ray-tracing-render-headless
ray_tracing/build/arm64/tools/cli/ray_tracing_render_headless \
  --request ray_tracing/tests/fixtures/agent_render_preflight_request.json \
  --preflight
```

To render BMP frame images:

```bash
make -C ray_tracing ray-tracing-render-headless
ray_tracing/build/arm64/tools/cli/ray_tracing_render_headless \
  --request ray_tracing/tests/fixtures/agent_render_image_export_request.json \
  --render
```

On non-Darwin builds the binary may be under:

```bash
ray_tracing/build/tools/cli/ray_tracing_render_headless
```

Detached submit/status/cancel now also exist:

```bash
make -C ray_tracing ray-tracing-job-runner
ray_tracing/build/arm64/tools/cli/ray_tracing_job_runner \
  submit --request ray_tracing/tests/fixtures/agent_render_job_runner_request.json
```

Optional submit policy flags:

- `--overwrite`: rerender even when target frames already exist
- `--resume`: continue from the first missing contiguous frame when earlier
  frames already exist

## Publication Lanes

There are two different post-render publication targets:

1. local repo docs review sets
   - helper: `ray_tracing/tools/publish_render_review_set.sh`
   - output root: `ray_tracing/docs/render_review_sets/<set_id>/`
   - purpose: local documentation and repo-side artifact inspection
2. live visualizer website runs
   - helper lane: `skills/codework-visualizer-drop/`
   - contract: `visualizer-run/v1`
   - local staging root:
     `_private_workspace_artifacts/codework_visualizer_runs/<drop_id>/`
   - purpose: actual website/VPS-facing visualizer publication
3. unified publish helper
   - helper: `ray_tracing/tools/publish_render_outputs.sh`
   - mode switch: `--mode local|visualizer|both`
   - defaults to the visualizer lane when `--mode` is omitted
   - can also include the full frame sequence in the staged visualizer drop
     with `--include-all-frames`
4. latest-run convenience helper
   - helper: `ray_tracing/tools/publish_latest_render_run.sh`
   - can infer the latest run root, latest frame, default set id, and title
   - useful immediately after a completed detached render when you do not want
     to spell out every path manually

`publish_render_review_set.sh` is not a live website deploy step. Use it for
local docs only.

## Request Schema

The request root must declare:

```json
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "run_id": "example_run",
  "scene": {
    "runtime_scene_path": "scene_runtime.json"
  },
  "volume": {
    "enabled": false,
    "source_kind": "auto",
    "source_path": "scene_bundle.json",
    "affects_lighting": true,
    "debug_overlay": false
  },
  "render": {
    "start_frame": 0,
    "frame_count": 1,
    "width": 640,
    "height": 360,
    "normalized_t": 0.0,
    "temporal_frames": 1,
    "integrator_3d": "direct_light"
  },
  "inspection": {
    "preset": "glass_preview",
    "camera_zoom": 0.95,
    "camera_position": { "x": -3.8, "y": -7.2, "z": 2.2 },
    "camera_look_at": { "x": -0.2, "y": 0.8, "z": 1.2 },
    "environment_light_mode": "ambient",
    "ambient_strength": 0.25,
    "light_intensity": 2.6,
    "light_radius": 0.10,
    "forward_decay": 220.0,
    "volume_scatter_gain": 3.0,
    "volume_step_scale": 1.0,
    "secondary_diffuse_samples_3d": 8,
    "transmission_samples_3d": 4,
    "volume_tint": { "r": 0.35, "g": 0.65, "b": 1.80 }
  },
  "output": {
    "root": "ray_tracing",
    "overwrite": false
  },
  "progress": {
    "summary_path": "ray_tracing/render_summary.json",
    "progress_path": "ray_tracing/render_progress.json"
  }
}
```

Supported `volume.source_kind` values:
- `auto`
- `manifest`
- `scene_bundle`
- `raw_vf3d`
- `vf3d`
- `pack`
- `none`

Supported `render.integrator_3d` values:
- `direct_light`
- `diffuse_bounce`
- `material`
- `emission_transparency`
- `disney`

Relative paths resolve from the request file directory.

Optional `inspection` fields are headless-only tuning overrides. They do not
change the persisted runtime scene:

- `camera_zoom`
- `camera_position`
- `camera_look_at`
- `environment_light_mode`
- `ambient_strength`
- `top_fill_strength`
- `environment_brightness`
- `light_intensity`
- `light_radius`
- `forward_decay`
- `volume_scatter_gain`
- `volume_step_scale`
- `preset`
- `secondary_diffuse_samples_3d`
- `transmission_samples_3d`
- `volume_tint`

Preferred environment-light override contract:
- `environment_light_mode = "off"` disables the extra environment fill lane
- `environment_light_mode = "top_fill"` uses `top_fill_strength`
- `environment_light_mode = "ambient"` uses `ambient_strength`
- `environment_brightness` still maps directly to the persisted
  `0..255` environment brightness value and should be treated as a lower-level
  compatibility override rather than the first-choice authored request field

## Current Behavior

`--preflight`:

1. parses the request schema
2. applies the runtime scene through the existing bridge
3. optionally validates and attaches the configured VF3D volume source
4. resolves the native 3D route
5. prepares a native 3D frame in memory
6. writes `ray_tracing_headless_summary_v1`

`--render` runs the same readiness path and then writes BMP frames to:

```text
<output.root>/frames/frame_0000.bmp
```

When `progress.progress_path` is set, the render CLI now also writes a
`ray_tracing_render_progress_v1` JSON file with stage transitions such as
`loading_scene`, `preflight_ready`, `rendering_frame`, `writing_frame`, and
`completed`.

The frame index starts at `render.start_frame` and writes `render.frame_count`
frames. The summary reports:

- `rendered_frames`
- `frames_rendered`
- `outputs.frame_dir`
- `outputs.first_frame_path`
- `outputs.last_frame_path`
- `volume_summary_built`
- `volume_summary.density_non_zero_cell_count`
- `render_stats.hit_pixels`
- `render_stats.visible_pixels`
- `render_stats.nonzero_pixels`
- `render_stats.max_radiance`
- `render_stats.max_rgb`
- `object_audit[*].object_id`
- `object_audit[*].material_id`
- `object_audit[*].alpha`
- `object_audit[*].reflectivity`
- `object_audit[*].roughness`
- `object_audit[*].emissive_strength`
- `object_audit[*].texture_id`
- `object_audit[*].texture_strength`
- `object_audit[*].texture_scale`
- `object_audit[*].texture_offset_u`
- `object_audit[*].texture_offset_v`
- `object_audit[*].texture_seed`
- `object_audit[*].texture_pattern_mode`
- `object_audit[*].texture_coverage`
- `object_audit[*].texture_grain`
- `object_audit[*].texture_edge_softness`
- `object_audit[*].texture_contrast`
- `object_audit[*].texture_flow`
- `object_audit[*].texture_color_depth`
- `object_audit[*].texture_surface_damage`
- `object_audit[*].packed_color`
- `object_audit[*].primitive_count`
- `object_audit[*].triangle_count`
- `object_audit[*].primary_hit_pixels`
- `object_audit[*].center_screen`
- `inspection.camera_zoom`
- `inspection.camera_position`
- `inspection.camera_look_at`
- `inspection.environment_brightness`
- `inspection.light_intensity`
- `inspection.light_radius`
- `inspection.forward_decay`
- `inspection.volume_scatter_gain`
- `inspection.volume_step_scale`
- `inspection.preset`
- `inspection.secondary_diffuse_samples_3d`
- `inspection.transmission_samples_3d`
- `inspection.volume_tint`

For transparent/glass preview work, `emission_transparency` now supports
bounded per-run sampling budgets through:

- `inspection.preset = "glass_preview"`
- `inspection.secondary_diffuse_samples_3d`
- `inspection.transmission_samples_3d`

Use low preview values such as `4/4` or `8/4` for quick validation, then raise
them for slower review or long-review renders when you want cleaner
transmission.

`glass_preview` defaults the run to `emission_transparency` when
`render.integrator_3d` is omitted, and stamps a low-cost preview budget of
`secondary_diffuse_samples_3d = 8` and `transmission_samples_3d = 4`. Explicit
integrator or sample overrides still win.

`glass_review` defaults the run to `emission_transparency` when
`render.integrator_3d` is omitted, and stamps a slower inspection budget of
`secondary_diffuse_samples_3d = 24` and `transmission_samples_3d = 12`.
Explicit integrator or sample overrides still win.

When the target surface is the live visualizer site rather than local docs:

- convert BMP frame outputs to PNG during visualizer staging
- prefer `skills/codework-visualizer-drop/scripts/stage_visualizer_run.py`
  with BMP-to-PNG normalization instead of ad hoc manual copies
- prefer `ray_tracing/tools/publish_render_outputs.sh --mode visualizer`
  or `--mode both` when you want one higher-level command
- prefer `ray_tracing/tools/publish_latest_render_run.sh` when publishing the
  most recent finished run and the default "last frame is the hero frame"
  policy is acceptable
- keep `drop_id` strict:
  `<program>--<job_type>--<YYYYMMDDTHHMMSSZ>--<nonce>`
- if VPS import reports one success plus one failure, check for a stale invalid
  remote staged drop before assuming the new publish failed
- remove stale staged drops with
  `skills/codework-visualizer-drop/scripts/cleanup_remote_visualizer_drop.sh`
  before rerunning the importer when needed

The headless summary now also emits `object_audit`, which inventories the live
runtime `sceneSettings` slots plus built primitive/triangle counts and a
camera-ray first-hit pixel count per object. Use it when a runtime-scene object
seems missing: if an object is absent from `object_audit`, it never reached the
live render object lane; if it is present with `primitive_count > 0` but
`primary_hit_pixels == 0`, the camera does not actually see it.

When a runtime-scene camera seed provides position but no authored orientation,
the native `3D` render path now auto-aims that camera toward the built scene
center. If the runtime scene provides authored camera orientation fields, those
values win. Supported authored fields are:

- `yaw`
- `rotation_z`
- `rotationZ`
- `transform.rotation.z`
- `look_pitch`
- `lookPitch`
- `pitch`
- `rotation_x`
- `rotationX`
- `transform.rotation.x`

For authored moving-camera scenes, prefer
`extensions.ray_tracing.authoring.camera_focus_target` in the source
LineDrawing request. When present, headless runtime-scene sampling keeps the
authored camera-path position/depth motion but recomputes yaw/pitch each sample
to keep the camera aimed at that target. This is a safer lane than hand-authoring
moving `rotation` / `lookPitch` values for each camera-path keyframe.

For multi-frame requests, the current interpolation samples normalized time
from `0.0` to `1.0` across the requested frame count. A single-frame request
uses `render.normalized_t`. Native `3D` direct-light requests now also honor
`render.temporal_frames` instead of silently collapsing back to one subpass.
When the runtime scene carries `extensions.ray_tracing.authoring.light_path`
and/or `camera_path`, headless multi-frame renders now preserve those sampled
authored positions across the render loop instead of collapsing back to the
first light point.

For transparent-surface review, prefer `render.integrator_3d =
emission_transparency`. `direct_light` remains the faster framing/motion tier,
but it is not the correct final readback for transmissive behavior through
transparent objects.

For PhysicsSim volume handoff, the validation flow now authors a room-style
LineDrawing emitter scene with floor, wall planes, a contrast prism, and one
fluid emitter prism, runs `physics_sim_headless --save-volume-frames`, points
RayTracing at the generated `scene_bundle.json`, and writes higher-resolution
BMP frames under:

```text
ray_tracing/build/agent_runs/physics_trio/volume_handoff_image_export/ray_tracing/frames/
```

The summary validates native `3D` route readiness, successful volume
attachment, nonzero VF3D density ingestion, and whether the final frame export
produced visible pixels. Current manifest-backed rendering resolves one
representative VF3D frame from the scene bundle; animated per-render-frame VF3D
playback remains a follow-up contract.

The current volume-handoff smoke uses an explicit oblique inspection camera:

- `camera_zoom: 0.95`
- `camera_position: { x: -3.8, y: -7.2, z: 2.2 }`
- `camera_look_at: { x: -0.2, y: 0.8, z: 1.2 }`
- `light_intensity: 2.6`
- `light_radius: 0.10`
- `forward_decay: 220.0`
- `volume_scatter_gain: 3.0`
- `volume_step_scale: 1.0`
- `volume_tint: { r: 0.35, g: 0.65, b: 1.80 }`

That keeps the render in a readable side/oblique room view instead of the old
overhead floor patch, while giving the VF3D plume a readable blue-biased cue
for inspection without changing the persisted runtime scene or default render
behavior.

Volume density reconstruction is now trilinear instead of nearest-cell, and
`inspection.volume_step_scale` can tighten or loosen the single-scatter march
step relative to the default `voxelSize * 0.5` step. Values below `1.0`
produce finer sampling at higher cost.

## Detached Runner

The detached runner is the first Phase 1 execution adapter for long-running
headless work:

```bash
ray_tracing/build/arm64/tools/cli/ray_tracing_job_runner submit --request <request.json>
ray_tracing/build/arm64/tools/cli/ray_tracing_job_runner status --job-id <job_id>
ray_tracing/build/arm64/tools/cli/ray_tracing_job_runner cancel --job-id <job_id>
```

The first detached trio chain now reaches this runner through
`bin/run_trio_detached_job_chain.sh`, which waits on detached PhysicsSim output
and submits the RayTracing render automatically once `scene_bundle.json` is
available. That chain now supports named profiles (`preview`, `review`,
`long_review`, `overnight`) and emits both `chain_status.json` and a
top-level `chain_summary.json` manifest with monitoring cadence metadata and
artifact roots. The manifest now also includes
`monitoring.automation_recommendation`, which is the preferred Codex-side
source of truth for heartbeat scheduling decisions.

Phase 2 truth additions:

- `status` now reconciles stored job state with live PID liveness, the render
  progress file, and the presence of a completion summary
- `status` now also classifies a live job as `stalled` when the process remains
  alive but `updated_at_utc` stops moving past the runner stall threshold
  (currently 15 minutes); this is observational only and does not kill the job
- in-flight render stage changes now also refresh `job_status.json` directly,
  so a long-running detached job no longer leaves the top-level status file
  stuck at the launch stage while `render_progress.json` advances
- `job_status.json` now carries:
  - `stage`
  - `overwrite_policy`
  - `requested_start_frame`
  - `requested_frame_count`
  - `effective_start_frame`
  - `effective_frame_count`
  - `frame_index`
  - `frames_completed`
  - `submitted_at_utc`
  - `started_at_utc`
  - `finished_at_utc`
- `render_progress.json` now carries both `stage` and `state`
- `render_progress.json` and `job_status.json` now also carry:
  - `temporal_subpasses_started`
  - `temporal_subpasses_completed`
  - `temporal_subpasses_total`
  - `progress_ratio`
- long single-frame renders can now show real temporal-subpass movement while
  still in `rendering_frame`, instead of looking frozen until the frame
  finishes
- interpret those fields carefully:
  - `temporal_subpasses_started` means the renderer has entered that subpass
  - `temporal_subpasses_completed` means the subpass has fully committed
  - `started > completed` is expected while the current subpass is still
    actively rendering
- `started=6`, `completed=5`, `total=6` means "final subpass active", not an
  automatic failure
- a `stalled` state means "alive but no progress timestamp movement past the
  threshold"; if progress resumes on a later `status` read, the runner can move
  the state back to `running`
- current long single-frame limitation: stage updates are truthful, but there is
  still no tile-level or bounce-level progress signal beyond temporal subpass
  completion
- `--resume` is contiguous-frame resume, not blind overwrite:
  - if no target frames exist, it behaves like a normal clean submit
  - if a contiguous prefix exists, the runner stages a reduced effective frame
    window and preserves normalized-time sampling through a `sampling` block in
    the staged request
  - if all requested frames already exist, submit fails with an explicit
    "nothing to resume" error
  - if outputs already exist and neither `--overwrite` nor `--resume` is set,
    submit fails explicitly instead of silently clobbering or guessing

Submit creates:

```text
ray_tracing/build/agent_runs/jobs/<job_id>/
  job_request.json
  job_status.json
  render_progress.json
  stdout.log
  stderr.log
  pid.txt
  result_summary.json
```

The staged `job_request.json` is canonicalized to absolute scene/volume/output
paths so detached execution does not break on rebased relative paths inside the
job directory.

Successful detached renders may still print informational fallback lines to
`stdout.log` when legacy scene or animation config files are absent. Those
messages are non-fatal for request-driven runtime-scene renders. Trust
`job_status.json.state`, `render_progress.json.state`, and
`result_summary.json.diagnostics` over raw stdout wording.

When using heartbeat follow-ups, delete the automation once the job reaches
`completed`, `failed`, or `cancelled`, so finished jobs do not keep waking the
thread on later visits.

## Validation

```bash
make -C ray_tracing ray-tracing-render-headless
make -C ray_tracing ray-tracing-job-runner
make -C ray_tracing test-ray-tracing-render-headless-preflight
make -C ray_tracing test-ray-tracing-render-headless-image-export
make -C ray_tracing test-ray-tracing-render-headless-volume-handoff
make -C ray_tracing test-ray-tracing-job-runner-smoke
make -C ray_tracing test-ray-tracing-job-runner-policy
```
