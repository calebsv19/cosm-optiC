# RayTracing Headless Agent Render CLI

Status: Phase 4 volume handoff image export contract landed, with runtime-scene camera fallback for native `3D` renders, additive colored volume inspection tint support, first PhysicsSim water-surface sidecar ingestion, and a first detached RayTracing local job runner.

Environment-light inspection overrides now also support the shipped three-way
renderer lane:
- `off`
- `top_fill`
- `ambient`

Use `ambient_strength` for the ambient-mode surface-fill amount (`0.0..1.0`)
and `top_fill_strength` for the top-fill lane (`0.0..20.0`). Ambient mode now
resolves through an authored native `3D` environment model:
`environment_preset` selects `sky`, `warm_sky`, or `neutral`;
`background_brightness` controls miss-pixel/background radiance independently
from ambient surface fill; and `background_color` tints the preset gradient.
The older `environment_brightness` override remains available as a
compatibility knob for the underlying byte-domain environment brightness state.

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

For the baseline unattended visual proof, use the make target:

```bash
make -C ray_tracing visual-artifact
```

The target renders a deterministic first frame, validates that the BMP is
parseable and nonblank, writes metrics to
`ray_tracing/visual_artifacts/source_first_frame/artifact_validation.json`, and
prints:

```text
ray_tracing visual artifact ready: <repo>/ray_tracing/visual_artifacts/source_first_frame/frames/frame_0000.bmp
```

`visual_artifacts/` is ignored and is separate from the `build/agent_runs/`
test-output roots.

For the canonical RT-R2 agent demo pack, including the richer mesh-asset spheres
scene and local queue-bundle dry run, see `docs/AGENT_DEMO_PACK.md`.

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

## Portable Worker Queue Fixture Export

RayTracing can now create a local, queue-root-style fixture package for the
Desktop VPS worker queue without opening the UI or submitting remotely:

```bash
python3 ray_tracing/tools/export_worker_queue_fixture.py \
  --fixture \
  --output-root ray_tracing/visual_artifacts/worker_queue_exports \
  --item-name ray-tracing-portable-fixture-20260624a
python3 bin/vps_worker_job_queue.py \
  --queue-root ray_tracing/visual_artifacts/worker_queue_exports \
  validate \
  --item-name ray-tracing-portable-fixture-20260624a
```

The first supported mode is `scene-only`: it exports a runtime scene, referenced
`assets/mesh_assets/*.runtime.json` sidecars, `bundle/render_request.json`,
queue-compatible `bundle/request/job.json`, `bundle/request/payload/...`, and a
manifest. VF3D and PhysicsSim bundle attachment modes are intentionally deferred
until the sidecar/package shape is clearer.

The same helper can export explicit runtime scene and render-request paths:

```bash
python3 ray_tracing/tools/export_worker_queue_fixture.py \
  --scene-runtime ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/scene_runtime_pressure_mrt8.json \
  --render-request ray_tracing/tests/fixtures/agent_render_mesh_asset_sphere_pressure_mrt8_request.json \
  --mesh-asset-root ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets \
  --output-root ray_tracing/visual_artifacts/worker_queue_exports \
  --item-name ray-tracing-explicit-scene-only-20260624a \
  --job-id ray-tracing--explicit-scene-only--20260624T000003Z--rtbundle04
```

Use `--scene-authoring <path>` when an authoring-state JSON should be copied
alongside the runtime scene as `bundle/scene_authoring.json`.

`bundle/render_request.json` is the portable render request for review and later
direct tooling. Current Desktop queue prepare still expects
`--ray-inspection-file` to be an inspection-settings JSON object, so the fixture
also writes `bundle/presets/inspection_settings.json` and points queue
`submit_args` at that file.

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

Publish helpers require publish run roots to be absolute existing directories
and treat `set_id`, `drop_id`, `job_type`, and selected frame names as single
path segments. Use letters, numbers, dot, underscore, or dash; slashes,
traversal segments, and leading dash values are rejected before staging.

`publish_render_review_set.sh` is not a live website deploy step. Use it for
local docs only. Its public `request.json` and `summary.json` copies redact
local/private paths before writing into `docs/render_review_sets/<set_id>/`;
the full source artifacts remain in the private run root.

The current D2.18 denoise ablation review set is:

```text
ray_tracing/docs/render_review_sets/disney_v2_d218_denoise_on_off_visual_proof/
```

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
    "visible": true,
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
    "integrator_3d": "direct_light",
    "denoise_enabled": true
  },
  "resources": {
    "cpu_percent": 50,
    "max_workers": 2,
    "reserve_cpu_count": 1
  },
  "inspection": {
    "preset": "glass_preview",
    "camera_zoom": 0.95,
    "camera_position": { "x": -3.8, "y": -7.2, "z": 2.2 },
    "camera_look_at": { "x": -0.2, "y": 0.8, "z": 1.2 },
    "environment_light_mode": "ambient",
    "ambient_strength": 0.25,
    "environment_preset": "sky",
    "background_brightness": 0.35,
    "background_color": { "r": 0.90, "g": 0.95, "b": 1.00 },
    "light_intensity": 2.6,
    "light_radius": 0.10,
    "forward_decay": 220.0,
    "volume_scatter_gain": 3.0,
    "volume_step_scale": 1.0,
    "secondary_diffuse_samples_3d": 8,
    "transmission_samples_3d": 4,
    "object_audit_enabled": true,
    "object_audit_max_dimension": 160,
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

The optional `resources` object controls native `3D` tile scheduler concurrency
for headless renders:

- `cpu_percent`: `1..100`, caps worker threads to an approximate percentage of
  detected host CPUs. `50` on a 2-vCPU VPS resolves to one render worker.
- `max_workers`: caps render workers directly, after CPU-percent and
  scheduler-global limits.
- `reserve_cpu_count`: subtracts host CPUs before resolving the worker cap, so
  colocated services keep explicit headroom.

When `resources` is omitted, direct local CLI runs keep legacy behavior. The
packaged Linux worker wrapper sets `CODEWORK_RAY_TRACING_DEFAULT_CPU_PERCENT=50`
unless the environment already provides another value, so older worker jobs get
a conservative VPS-safe default after the worker package is refreshed. Explicit
request JSON takes precedence over package environment defaults.

Supported `volume.source_kind` values:
- `auto`
- `manifest`
- `scene_bundle`

`volume.visible=false` keeps the configured volume source available for
sidecar-driven imports such as `scene_bundle.json.water_source`, but skips
attaching the visible VF3D volume body to the native `3D` render scene. This is
the preferred WTR water-surface review mode when the water heightfield should
render as a native mesh without the surrounding volumetric slab/box.
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
- `disney_v2`

Optional `render.denoise_enabled` overrides the runtime animation setting for
that detached request. It is intended for apples-to-apples denoise ablations,
for example rendering the same `disney_v2` scene with `temporal_frames=12`
once with denoise off and once with denoise on.

Relative paths resolve from the request file directory.

## Visual Matrix Runner

Use the visual matrix runner when a proof needs multiple comparable headless
cells, copied summaries, PNG frames, contact sheets, and amplified difference
metrics:

```bash
make -C /Users/calebsv/Desktop/CodeWork/ray_tracing \
  build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless

/Users/calebsv/Desktop/CodeWork/ray_tracing/tests/integration/run_ray_tracing_visual_matrix.py \
  --manifest /Users/calebsv/Desktop/CodeWork/ray_tracing/tests/fixtures/disney_v2_visual_matrix/transparent_interior_stack/matrix_manifest.json \
  --group disney_v2_denoise_ablation
```

The first Disney v2 D2.18b canonical matrix fixtures live under:

```text
ray_tracing/tests/fixtures/disney_v2_visual_matrix/
  primitive_glass_corridor/
  transparent_interior_stack/
  mirror_glossy_preservation/
  high_noise_emitter/
```

Each matrix writes a private review package by default under:

```text
_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/<matrix_id>/matrix_review/
```

Generated packages include:

- `matrix_report.json`
- `index.md`
- `matrix_contact_sheet.png`
- `frames/<cell>.png`
- `requests/*.json`
- `summaries/summary_<cell>.json`
- `comparisons/<comparison_id>/diff_metrics.json`
- `comparisons/<comparison_id>/side_by_side_*.png`

The D2.18b fixture manifests use `render.denoise_enabled=false` and `true`
with the same `temporal_frames=12` budget so the resulting diff is the
Disney-v2 edge-safe final resolve only, not a temporal-sample-count change.

For caustic-readiness work, use the glass-sphere probe:

```bash
make -C ray_tracing test-ray-tracing-caustic-probe-matrix
```

The fixture lives under
`ray_tracing/tests/fixtures/caustic_probe_glass_sphere/`. Its Disney v2 request
uses the high-fidelity `asset_sphere_256x128` glass mesh and runs the Disney v2
caustic policy with `inspection.caustic_mode = "analytic"`. The summary
readback records the request mode, sidecar strength, sidecar sample count,
contributing sample count, and sidecar radiance totals. Direct-light and
emission-transparency requests remain baseline measurements and do not enable
the Disney v2 caustic path.

Optional `inspection` fields are headless-only tuning overrides. They do not
change the persisted runtime scene:

- `camera_zoom`
- `camera_position`
- `camera_look_at`
- `environment_light_mode`
- `ambient_strength`
- `environment_preset`
- `background_brightness`
- `background_color`
- `top_fill_strength`
- `environment_brightness`
- `light_intensity`
- `light_radius`
- `forward_decay`
- `volume_density_scale` (`density_scale` alias)
- `volume_density_gamma` (`density_gamma` alias)
- `volume_scatter_gain`
- `volume_absorption_gain` (`absorption_gain` alias)
- `volume_opacity_clamp` (`opacity_clamp` alias)
- `volume_step_scale`
- `preset`
- `secondary_diffuse_samples_3d`
- `transmission_samples_3d`
- `object_audit_enabled`
- `object_audit_max_dimension`
- `volume_tint`
- `volume_albedo` / `volume_albedo_tint`

Preferred environment-light override contract:
- `environment_light_mode = "off"` disables the extra environment fill lane
- `environment_light_mode = "top_fill"` uses `top_fill_strength`
- `environment_light_mode = "ambient"` uses `ambient_strength`
- `environment_preset = "sky" | "warm_sky" | "neutral"` selects the resolved
  background gradient before `background_color` tinting
- `background_brightness` (`0.0..4.0`) explicitly controls background/miss
  radiance. When omitted, it is derived from `ambient_strength` for
  compatibility with existing ambient requests.
- `background_color` accepts `{ "r": ..., "g": ..., "b": ... }` or
  `[r, g, b]` with channels clamped to `0.0..1.0`
- `environment_brightness` still maps directly to the persisted
  `0..255` environment brightness value and should be treated as a lower-level
  compatibility override rather than the first-choice authored request field

The headless summary writes a resolved `environment_lighting` object with:
- `mode`
- `preset`
- `ambient_strength`
- `background_brightness`
- `background_brightness_source`
- `background_color`
- `background_top_color`
- `background_bottom_color`
- `ambient_surface_fill_contributes`
- `background_miss_contributes`
- `top_fill_contributes`

## Current Behavior

`--preflight`:

1. parses the request schema
2. applies the runtime scene through the existing bridge
3. optionally validates and attaches the configured VF3D volume source and
   `scene_bundle.json.water_source` water-surface sidecar
4. resolves the native 3D route
5. prepares a native 3D frame in memory
6. writes `ray_tracing_headless_summary_v1`

`--render` runs the same readiness path and then writes BMP frames to:

```text
<output.root>/frames/frame_%04d.bmp
```

Frame filenames use the requested absolute render frame index. For example, a
single-frame request with `render.start_frame = 17` writes
`frames/frame_0017.bmp`.

When `progress.progress_path` is set, the render CLI now also writes a
`ray_tracing_render_progress_v1` JSON file with stage transitions such as
`loading_settings`, `applying_runtime_scene`, `runtime_scene_applied`,
`attaching_volume`, `resolving_native_route`, `preparing_native_frame`,
`bvh_ready`, `object_audit`, `object_audit_ready`, `preflight_ready`,
`rendering_frame`, `writing_frame`, and `completed`.

Headless object-audit summaries are enabled by default but capped by
`inspection.object_audit_max_dimension`, which defaults to `160` pixels on the
longest side. The audit still records per-object primary hit counts, but uses a
downsampled primary-ray pass and scales hit counts by the audit stride so large
worker renders do not perform a full-resolution diagnostic ray pass before the
first frame. Set `inspection.object_audit_enabled` to `false` to skip this
diagnostic pass entirely for production-only runs.

The frame index starts at `render.start_frame` and writes `render.frame_count`
frames. The summary reports:

- `rendered_frames`
- `frames_rendered`
- `outputs.frame_dir`
- `outputs.first_frame_path`
- `outputs.last_frame_path`
- `render.has_denoise_enabled_override`
- `render.denoise_enabled`
- `denoise.has_request_override`
- `denoise.enabled`
- `denoise.applied`
- `volume_visible`
- `volume_summary_built`
- `volume_summary.density_non_zero_cell_count`
- `water_surface_source_found`
- `water_surface_loaded`
- `water_surface_mesh_attached`
- `water_surface.frame_selection_dynamic`
- `water_surface.selected_first_frame_path`
- `water_surface.selected_last_frame_path`
- `water_surface.triangle_count`
- `water_surface.grid_w`
- `water_surface.grid_d`
- `water_surface.wet_columns`
- `water_surface.surface_min_y`
- `water_surface.surface_max_y`
- `water_surface.material.ior`
- `water_surface.material.absorption_distance_m`
- `water_surface.material.absorption_rgb`
- `water_surface.payload.applied`
- `water_surface.payload.ior`
- `water_surface.payload.absorption_distance_m`
- `water_surface.payload.transparency`
- `water_surface.payload.tint_rgb`
- `render_stats.hit_pixels`
- `render_stats.visible_pixels`
- `render_stats.nonzero_pixels`
- `render_stats.max_radiance`
- `render_stats.max_rgb`
- `render_stats.emissive_area_candidate_count`
- `render_stats.emissive_area_selected_candidate_count`
- `render_stats.emissive_area_visibility_ray_count`
- `render_stats.emissive_area_primary_sample_count`
- `render_stats.emissive_area_recursive_sample_count`
- `render_stats.emissive_area_full_scan_fallback_count`
- `render_stats.mirror_dominant_pixels`
- `render_stats.mirror_base_attenuated_pixels`
- `render_stats.mirror_reflection_hit_pixels`
- `render_stats.mirror_emitter_reflection_pixels`
- `render_stats.mirror_geometry_reflection_pixels`
- `render_stats.max_mirror_dominance`
- `render_stats.max_mirror_specular_reflection_radiance`
- `render_stats.max_mirror_base_radiance_before_attenuation`
- `render_stats.max_mirror_base_radiance_after_attenuation`
- `render_stats.total_mirror_specular_reflection_radiance`
- `render_stats.total_mirror_base_radiance_before_attenuation`
- `render_stats.total_mirror_base_radiance_after_attenuation`
- `render_stats.denoise_temporal_frame_count`
- `render_stats.denoise_raw_pixel_count`
- `render_stats.denoise_reconstructed_pixel_count`
- `render_stats.denoise_stable_interior_sample_count`
- `render_stats.denoise_rejected_edge_sample_count`
- `render_stats.denoise_preserved_transparent_pixel_count`
- `render_stats.denoise_preserved_mirror_glossy_pixel_count`
- `render_stats.denoise_skipped_unstable_temporal_pixel_count`
- `render_stats.denoise_skipped_invalid_surface_pixel_count`
- `render_stats.denoise_raw_radiance_luma_total`
- `render_stats.denoise_reconstructed_radiance_luma_total`
- `render_stats.denoise_radiance_luma_delta`
- `object_audit_summary.enabled`
- `object_audit_summary.width`
- `object_audit_summary.height`
- `object_audit_summary.sample_count`
- `object_audit_summary.full_resolution_pixel_count`
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
- `inspection.caustic_mode` (`disney_v2` only; `analytic` default, `off`,
  `transport` reserved)
- `inspection.caustic_sidecar_enabled` (`disney_v2` legacy alias)
- `inspection.caustic_sidecar_strength` (`disney_v2` only)
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

The summary also emits diagnostic `timing_breakdown` fields for headless
capacity proofs. Top-level fields separate runtime-scene application, duplicate
scene preflight, native `3D` frame preparation, optional object audit, render
trace, frame analysis/write, optional video encode, and total run time. Nested
`mesh_asset_loader` fields cover runtime-scene read/parse, mesh runtime
document load, cached document copy, cache hit/miss counts, loaded asset bytes,
vertices, and triangles. Nested `scene_builder` fields cover primitive seeding,
mesh append reserve/expansion, BVH rebuild wall time, mesh instance counts, and
expected/appended triangle counts. Treat these fields as diagnostic timing
telemetry for local/private performance proofing, not as promotion gates.

For static file-backed runtime mesh startup proofing, prepared acceleration
summaries also expose persistent BLAS/BVH cache counters:
`blas_persistent_cache_hits`, misses, writes, invalidations, refreshes, read
milliseconds, and write milliseconds. The startup timing matrix mirrors those
as `mesh_local_blas_persistent_cache_read` and
`mesh_local_blas_persistent_cache_write` stages.

The `bvh_summary` object includes diagnostic build microtiming fields for
high-triangle capacity work: allocation, centroid build, inclusive tree build,
range-bounds scans, sort/partition work, node append, final stats, unaccounted
build overhead, range/sort/node call counts, and maximum range sizes. These are
for local performance diagnosis and should not be used as route promotion
criteria.

## Render-Cost and Temporal-Pruning Diagnostics

Use these environment gates when a headless run is meant to explain ray count,
per-ray cost, or temporal-subpass work. Keep baseline beauty renders free of
diagnostic envs unless the comparison explicitly calls for them.

Current default-on render-cost policies:

- pure `tlas_blas` native `3D` renders skip the legacy flattened-BVH build by
  default. Use `RAY_TRACING_NATIVE3D_DISABLE_DEFAULT_TLAS_BVH_SKIP=1` only for
  rollback/parity checks.
- Disney v2 reflected-transmission first-subpass no-hit reuse is default-on.
  Use `RAY_TRACING_DISNEY_V2_REFLECTED_FIRST_SUBPASS_NO_HIT_REUSE_PROBE=0` to
  force old full-trace behavior for regression checks.

Current opt-in diagnostics and candidates:

- `RAY_TRACING_RENDER_TRACE_COST_LEDGER=1` writes the render trace cost ledger
  into the headless summary. Use it for ray-class, material-family,
  direct-light visibility, transmission, and Disney v2 path-cost attribution.
- `RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER=1` writes frame-dataflow and
  ownership diagnostics into the headless summary. Use it for startup,
  scheduler, scratch, TLAS/BLAS, and frame-lifetime accounting.
- `RAY_TRACING_NATIVE_3D_TEMPORAL_RISK_EARLY_STOP=1` enables the R4 per-pixel
  temporal risk early-stop candidate. It is not a default launcher or desktop
  package setting. The representative textured-glass operator-scene proof did
  not justify promotion, so keep this as an operator/debug option unless a
  future narrower policy earns a new proof.
- `RAY_TRACING_NATIVE_3D_TEMPORAL_BUDGET_HEATMAP=1` emits the temporal budget
  heatmap for threshold review. Pair it with the risk early-stop env when the
  goal is to see which pixels were pruned, held by transparent/material risk,
  held by light/edge risk, or kept as high-budget glass/receiver regions.
- `RAY_TRACING_DIRECT_LIGHT_CLEAR_VISIBLE_DECISION_SAMPLE_PROBE=1` enables the
  stricter all-clear direct-light visibility decision probe. It is useful for
  attribution and ray-count study, but it is not a timing win on the restored
  transparent stack and should stay off for default beauty runs.
- `RAY_TRACING_DISNEY_V2_REFLECTED_TRANSMISSION_SAMPLE_CAP=<n>` is an
  experimental reflected-transmission cap override. The current proven safe
  reflected-transmission default is the guarded first-subpass no-hit reuse
  above, not a blind lower cap.

For tiled A/B reviews, run the exact same request and tile renderer settings
with only the candidate env changed. The promotion review should include:

- rendered pixel-subpass delta
- total ray and transmission/direct-light ray deltas
- max channel delta and count of pixels above small thresholds
- internal tile-line and 1-pixel tile-band changed-pixel density
- side-by-side and amplified-diff images
- the headless summary paths and candidate envs used

The current R4 status is intentionally conservative: high-quality synthetic
reviews showed useful subpass reduction and structurally clean tile borders,
but the representative textured-glass operator-scene proof had nontrivial drift
and no timing win. `RAY_TRACING_NATIVE_3D_TEMPORAL_RISK_EARLY_STOP=1` remains
opt-in only.

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
attachment, nonzero VF3D density ingestion, selected first/last volume frame
paths, loaded first/last VF3D frame indices, and whether the final frame export
produced visible pixels. Manifest-backed and scene-bundle-backed rendering now
resolves VF3D or pack sources by requested render frame.

For PhysicsSim water-surface handoff, the validation flow runs
`physics_sim_headless --water-mode --save-volume-frames`, points RayTracing at
the generated `scene_bundle.json`, imports `water_source`, and appends the
selected PhysicsSim Y-up heightfield frame as native `3D` triangle geometry.
The render mesh maps PhysicsSim height `y` into RayTracing scene-up `z`, so the
water surface displays as a horizontal basin plane while preserving the
producer-side `surface_axis: "y"` contract. The current backend path is
headless only; editor controls and LineDrawing liquid-region authoring remain
follow-up work.

```text
ray_tracing/build/agent_runs/physics_trio/water_surface_handoff_image_export/ray_tracing/frames/
```

For single-frame water optics review, the WTR-5.3 validation flow runs
`physics_sim_headless --water-mode --water-review-ripples`, warms the basin for
18 frames, selects the final `water_surface_000017.json` sidecar through
`scene_bundle.json.water_source`, and renders one `emission_transparency` frame
with contrast geometry behind the imported water surface:

```text
ray_tracing/build/agent_runs/physics_trio/water_optics_review_single_frame/ray_tracing/frames/frame_0017.bmp
```

The test asserts that the review sidecar applied ripples, the selected
heightfield has non-flat finite heights/normals, the water material payload is
active, the camera visibly hits the generated `water_surface` object, and the
render traces through transparent water.

For larger-surface visual review, use
`make -C ray_tracing test-ray-tracing-render-headless-water-basin-surface-review`.
This fixture keeps the same PhysicsSim -> `scene_bundle.json.water_source` ->
RayTracing path, but the standalone Water Basin now resolves to a square X/Z
footprint so the imported surface frame is broad instead of a narrow strip. The
review run uses a lighter `32 x 32` water surface, deterministic review
ripples, a plain basin/floor/wall scene, and a closer camera to produce:

```text
ray_tracing/build/agent_runs/physics_trio/water_basin_surface_review_single_frame/ray_tracing/frames/frame_0011.bmp
```

This is still a headless backend optics/review fixture. The current producer
contract remains the PhysicsSim-authored Y-up heightfield, while the RayTracing
builder now remaps that imported heightfield into a Z-up rendered water sheet.

For moving-light multi-frame water optics review, use
`make -C ray_tracing test-ray-tracing-render-headless-water-moving-light-review`.
This WTR-5.4 fixture warms a standalone PhysicsSim Water Basin for `16` frames,
renders four consecutive RayTracing frames (`0008..0011`) through the generated
`scene_bundle.json.water_source`, and samples an authored
`extensions.ray_tracing.authoring.light_path` across normalized render time:

```text
ray_tracing/build/agent_runs/physics_trio/water_moving_light_review/ray_tracing/frames/frame_0008.bmp
ray_tracing/build/agent_runs/physics_trio/water_moving_light_review/ray_tracing/frames/frame_0011.bmp
```

The test asserts that the selected first/last water surface sidecars are both
loaded, the heightfield evolves between those frames, transparent-water
secondary hits remain active, and the rendered BMP sequence has measurable
frame-to-frame pixel deltas from the moving light over the rippled water
surface.

For long-motion sparse-frame review before object displacement work, use
`make -C ray_tracing test-ray-tracing-render-headless-water-long-motion-review`.
This WTR-5.5 fixture runs PhysicsSim Water mode for `201` frames with `4`
simulation steps per exported frame, samples frames `40, 80, 120, 160, 200`,
and renders full RayTracing basin BMP frames from `scene_bundle.json.water_source`:

```text
ray_tracing/build/agent_runs/physics_trio/water_long_motion_review/ray_tracing/frames/frame_0040.bmp
ray_tracing/build/agent_runs/physics_trio/water_long_motion_review/ray_tracing/frames/frame_0200.bmp
ray_tracing/build/agent_runs/physics_trio/water_long_motion_review/ray_tracing/frames/water_long_motion_contact_sheet.bmp
```

This target is a sparse full-RayTracing basin proof for large time separation:
it shades the actual simulated water surfaces with a moving light inside the
same basin-style render composition and asserts height/image deltas across
sparse frames. Full-length video/output throughput remains a later optimization.

For WTR-6 object-water review, use
`make -C ray_tracing test-ray-tracing-render-headless-water-object-coupling-review`.
This fixture runs PhysicsSim Water mode with `--water-object-fixture`, then
renders full RayTracing basin frames `0008`, `0018`, and `0027` through
`scene_bundle.json.water_source` with a visible block object in the basin:

```text
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_review/ray_tracing/frames/frame_0008.bmp
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_review/ray_tracing/frames/frame_0027.bmp
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_review/ray_tracing/frames/water_object_coupling_review.mp4
```

The BMP sequence is the deterministic acceptance artifact. When local `ffmpeg`
is available, the test also encodes the MP4 review helper without replacing the
full RayTracing basin render path.

For WTR-6.4 longer object-water output, use
`make -C ray_tracing test-ray-tracing-render-headless-water-object-coupling-long-review`.
The target is profile-driven:

```bash
# Small validation profile; this is what the make target runs by default.
make -C ray_tracing test-ray-tracing-render-headless-water-object-coupling-long-review

# Expensive review profile requested for long visual inspection.
WTR6_LONG_PROFILE=full \
  make -C ray_tracing test-ray-tracing-render-headless-water-object-coupling-long-review
```

The default `smoke` profile warms up `8` PhysicsSim frames, renders `4`
RayTracing frames at stride `5`, and writes:

```text
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_long_review_smoke/
```

The `full` profile warms up `200` PhysicsSim frames, selects `100` output
frames at stride `5` from the evolved Water Basin (`200..695`), uses a looped
overhead light path, and writes the full BMP/MP4 review under:

```text
ray_tracing/build/agent_runs/physics_trio/water_object_coupling_long_review/
```

Useful overrides:

- `WTR6_LONG_PROFILE=smoke|review|full`
- `WTR6_LONG_WARMUP_FRAMES=<n>`
- `WTR6_LONG_OUTPUT_FRAMES=<n>`
- `WTR6_LONG_FRAME_STRIDE=<n>`
- `WTR6_LONG_SIM_STEPS_PER_FRAME=<n>`
- `WTR6_LONG_GRID=<WxHxD>`
- `WTR6_LONG_WIDTH=<px>`
- `WTR6_LONG_HEIGHT=<px>`
- `WTR6_LONG_TEMPORAL_FRAMES=<n>`
- `WTR6_LONG_INTEGRATOR_3D=<emission_transparency|direct_light|disney_v2>`
- `WTR6_LONG_FPS=<n>`
- `WTR6_LONG_RIPPLE_AMPLITUDE=<meters>`

For WTR-6.6 cache-first preview-matrix planning, use the dry-run planner before
launching more PhysicsSim or RayTracing work:

```bash
python3 ray_tracing/tools/wtr66_preview_matrix_planner.py --overwrite
make -C ray_tracing test-ray-tracing-wtr66-preview-matrix-planner-dry-run
```

The default dry-run writes:

```text
ray_tracing/build/agent_runs/physics_trio/wtr66_preview_matrix_dry_run/
```

It creates `matrix_request.json`, `matrix_summary.json`, one planned
`cache_manifest.json`, named selected-frame lists such as `contact_short` and
`every_10`, plus direct-light and Disney-v2 temporal-2 render request sets over
the same planned PhysicsSim cache. It does not run PhysicsSim or RayTracing.

To execute the first local cache/job-runner validation boundary:

```bash
make -C ray_tracing test-ray-tracing-wtr66-preview-matrix-local-job-runner
```

That explicit gate writes:

```text
ray_tracing/build/agent_runs/physics_trio/wtr66_preview_matrix_local_job_runner/
```

It runs one local PhysicsSim water/object cache and submits direct-light plus
Disney-v2 temporal-2 one-frame jobs over the same selected frame set through
`ray_tracing_job_runner`, then validates job status, render summaries, water
mesh attachment, block visibility, secondary hits, and BMP outputs. The harness
prefers the current clang toolchain job-runner binary before the legacy
`build/<arch>` path to avoid stale local binaries.

The water summary reports whether a water source was found, whether the frame
loaded, whether mesh triangles were attached, requested/loaded first and last
frame indices, selected frame paths, grid dimensions, wet/dry/solid column
counts, surface height statistics, finite-normal status, material IOR and
absorption metadata, the resolved RayTracing water payload, and appended
triangle count. The source `material.absorption_rgb` values are absorption
coefficients; the resolved `payload.tint_rgb` values are the Beer-Lambert
transmittance tint used by the native `3D` material path. Per-render-frame
native `3D` preparation reloads the water heightfield for the requested
absolute render frame, so animated PhysicsSim sidecars can move with the
existing camera/light path sampling.

For WTR-6.7C worker-backed external-cache probes, nonzero render start frames
must keep their absolute frame identity. The repaired worker path publishes
`frame_0120.bmp` for `render.start_frame=120`, and RayTracing imports VF3D and
water-sidecar manifests by exact `frame_index` first. The Linux PC proof
`ray-tracing--wtr67c-cache-direct-probe--20260624T173200Z--wtr67crt05`
verified that frame `120` selected `frame_000120.vf3d` and
`water_surface_000120.json`. Treat this as a plumbing/readiness proof, not a
water-quality proof: before longer direct-light or Disney-v2 reviews, inspect
the PhysicsSim water sidecars and require meaningful temporal height deltas.

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

`volume.debug_overlay=true` renders direct in-scene density contribution for
diagnosis. It uses the request camera, volume source, surface clipping, density
remap, step scale, and albedo/tint controls, but bypasses light-dependent
single scattering so sparse volume structure is visible even when production
lighting washes it out. Summaries read back this request state as
`volume_summary.debug_overlay_enabled`.

The volume material remap controls are request/readback-only inspection
overrides for reinterpreting the same VF3D scalar cache without regenerating
the simulation:

- `volume_density_scale` multiplies sampled scalar density before scatter.
- `volume_density_gamma` applies the post-scale density curve; values below
  `1.0` lift low-density smoke while values above `1.0` concentrate it.
- `volume_scatter_gain` scales single-scatter brightness.
- `volume_absorption_gain` scales extinction independently from scatter.
- `volume_opacity_clamp` caps the remapped density before scatter/extinction.
- `volume_albedo` / `volume_albedo_tint` accepts `{ "r": ..., "g": ..., "b": ... }`
  or `[r, g, b]` and takes precedence over legacy `volume_tint` when both are
  present.

Defaults preserve the previous direct-density behavior: density scale `1.0`,
density gamma `1.0`, scatter gain `1.0`, absorption gain `1.0`, and an
effectively unbounded opacity clamp.

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
  - `completed_tiles_in_subpass`
  - `total_tiles_in_subpass`
  - `elapsed_seconds`
  - `estimated_remaining_seconds`
  - `progress_ratio`
- long single-frame renders can now show real temporal-subpass movement while
  still in `rendering_frame`, instead of looking frozen until the frame
  finishes
- active temporal subpasses can now also expose tile-level movement inside the
  current subpass, so status readers can distinguish "slow but moving" from a
  truly stale worker
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
- current long single-frame limitation: tile-level progress is now available,
  but there is still no bounce-level or per-ray convergence signal beyond the
  active subpass/tile counters
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

Detached path policy:

- `--jobs-root` overrides must be absolute existing directories. Relative,
  missing, or traversal-containing override paths are rejected.
- `job_id` values from generated jobs and outer bundles are treated as one safe
  path segment before any job path is built.
- direct request `output.root` remains an explicit caller-selected artifact
  root, but detached submit requires it to resolve to an absolute non-root path
  before rendering starts.
- outer bundle jobs continue to default artifacts to
  `<job_root>/output/artifacts`.

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
make -C ray_tracing test-ray-tracing-render-headless-water-surface-handoff
make -C ray_tracing test-ray-tracing-job-runner-smoke
make -C ray_tracing test-ray-tracing-job-runner-policy
```
