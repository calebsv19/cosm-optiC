# RayTracing Headless Continuation And Visualizer Workflow

Last updated: 2026-05-24

Use this note when the goal is to run a cheap seed render first, continue later
frame ranges against the same PhysicsSim output, and keep the resulting runs
coherent on the live visualizer website.

## Intended Use

This workflow is for the current proven trio worker lane:

1. seed one low-cost frame with the full trio worker
2. continue later frame ranges with `start_stage = ray_tracing`
3. publish or backfill any missing visualizer drops
4. inspect the grouped sequence under the `ray-tracing` program bucket

It is not the full-physics continuation lane. `physics_sim` resume semantics
are still a later slice; the proven continuation path today is RayTracing-only
reuse of an earlier PhysicsSim `scene_bundle.json`.

## Proven Reference Sequence

The reference proof sequence is:

- `ctest01`: seed frame `0`
- `ctest02`: continuation frames `1..4`
- `ctest03`: continuation frames `5..8`
- `ctest04`: continuation frames `9..10`

The live visualizer grouping root for that sequence is `ctest01`.

## Operator Flow

### 1. Seed Run

Run one short full-trio job first so PhysicsSim and RayTracing both materialize
normal worker state, visualizer metadata, and the reusable PhysicsSim bundle.

Use:

- `bin/run_ray_tracing_worker_continuation_flow.py seed`
- `bin/build_codework_worker_submit_payload.py`
- `bin/submit_codework_worker_job.py`
- `bin/submit_codework_worker_job_and_exchange.py`

Recommended low-cost seed shape:

- `start_stage = physics_sim`
- `start_frame = 0`
- `frame_count = 1`
- low resolution
- low `temporal_frames`
- low-cost integrator such as `direct_light`

What to verify after the seed run:

- `publication_state` is populated
- `published_frames` contains frame `0`
- `preview_url` exists
- the job has a usable PhysicsSim `scene_bundle.json`

### 2. RayTracing-Only Continuation

For later frame batches against the same underlying sim output:

- prefer `bin/run_ray_tracing_worker_continuation_flow.py continue`
- set `start_stage = ray_tracing`
- set the next `start_frame`
- set the new `frame_count`
- carry forward the PhysicsSim bundle as the RayTracing resume source

Expected continuation behavior:

- `physics_sim = skipped`
- `resume_mode = ray_tracing_only`
- `requested_frame_range` and `effective_frame_range` reflect the requested
  continuation batch
- `published_frames` reflect only the new continuation frames from that run

### 3. Read Back Progress And Completion

During the run, use the VPS status and worker-exchange summary as the main
operator surface. For long renders, watch:

- `stage_progress`
- `stage_history`
- `completed_frames`
- `remaining_frames`

After the run, confirm:

- `publication_state`
- `published_frames`
- `published_frame_relpaths`
- `resume_source`

### 4. Publish Missing Drops When Needed

If a run finished correctly but does not appear in live visualizer artifacts,
publish the staged drop instead of rerunning the render.

The bounded VPS publish lane now exists specifically for that:

- `vps_publish_visualizer_drop`

That lane was used to backfill `ctest01` and `ctest02` so the full
`ctest01..ctest04` lineage appeared in live grouped site data.

### 5. Inspect Grouped Visualizer Results

Live grouped runs should appear under the `ray-tracing` program bucket, not a
different simulation program.

For grouped continuation sequences:

- the logical root is `sequence_root_run_id`
- later runs share the same `sequence_key`
- grouped UI lanes should flatten outputs across the related sequence runs

If only the newest continuation frames show up, treat that as a site-data or UI
grouping issue, not as proof that the render itself failed.

## Current Practical Rules

- prefer `bin/run_ray_tracing_worker_continuation_flow.py proof` when checking
  that the whole seed/continue path still works after tooling changes
- Use a tiny seed run first when testing a new scene or new worker contract.
- Prefer continuation over rerunning PhysicsSim when only later RayTracing
  frames are needed.
- Keep seed and continuation quality intentionally cheap until the workflow is
  proven for that scene.
- Treat visualizer artifact publication as a separate step from render
  correctness.

## Current Gaps

- the helper still leaves visualizer backfill as a separate bounded VPS step
  instead of folding that publish repair into one local command
- interruption/failure-path continuation behavior still needs one dedicated
  proof pass
- full trio resume beyond RayTracing-only continuation remains a later contract
  because PhysicsSim resume semantics are still narrower
