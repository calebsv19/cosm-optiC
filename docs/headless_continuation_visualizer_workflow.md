# RayTracing Headless Continuation And Visualizer Workflow

Last updated: 2026-06-06

Use this note when the goal is to run a cheap seed render first, continue later
frame ranges against the same PhysicsSim output, and keep the resulting runs
coherent on the live visualizer website.

Current worker-host truth:

- trio submission now prefers the `home-server` worker label by default
- fallback remains enabled
- tiny proof runs should therefore be expected to:
  - claim on `homeserver` when it is free
  - fall back to the VPS local dispatcher when the homeserver slot is busy

## Intended Use

This workflow is for the current proven trio worker lane:

1. seed one low-cost frame with the full trio worker
2. continue later frame ranges with `start_stage = ray_tracing`
3. preserve one shared animation sampling window when chunks should form a
   continuous camera/light motion
4. publish or backfill any missing visualizer drops
5. inspect the grouped sequence under the `ray-tracing` program bucket

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

### 2A. Shared Sampling Window Chunks

Use a shared RayTracing sampling window when separate worker jobs should render
one continuous animation timeline. Without it, each chunk samples normalized
time locally across its own `frame_count`, so multiple jobs can replay the same
light/camera path instead of covering different parts of the full motion.

Request shape:

- `render.start_frame`: first output frame for this job
- `render.frame_count`: number of frames this job renders
- `sampling.frame_offset`: offset into the shared animation timeline
- `sampling.frame_count`: total animation frame count for the whole sequence

Mac submit helper flags:

- `--ray-start-frame`
- `--ray-frame-count`
- `--ray-sampling-frame-offset`
- `--ray-sampling-frame-count`

Plan a 32-frame render as eight 4-frame jobs:

```bash
python3 bin/plan_ray_tracing_frame_chunks.py \
  --total-frame-count 32 \
  --chunk-size 4 \
  --format table
```

Plan a preview-first flow: frame `0`, then frames `1..3`, then 4-frame chunks:

```bash
python3 bin/plan_ray_tracing_frame_chunks.py \
  --total-frame-count 32 \
  --batch-pattern 1,3,4 \
  --repeat-last \
  --format table
```

For a planned chunk with `ray_start_frame = 8`, `ray_frame_count = 4`,
`ray_sampling_frame_offset = 8`, and `ray_sampling_frame_count = 32`, the
RayTracing-only continuation submit should carry:

```bash
python3 bin/run_ray_tracing_worker_continuation_flow.py continue \
  --seed-run-id <seed_run_id> \
  --ray-start-frame 8 \
  --ray-frame-count 4 \
  --ray-sampling-frame-offset 8 \
  --ray-sampling-frame-count 32
```

The same flags are also accepted by:

- `bin/build_codework_worker_submit_payload.py`
- `bin/submit_codework_worker_job.py`
- `bin/submit_codework_worker_job_and_exchange.py`
- `bin/prepare_codework_worker_stage_requests.py`

The helpers reject impossible windows where
`ray_sampling_frame_offset + ray_frame_count` exceeds
`ray_sampling_frame_count`.

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

### 3A. Request Pause Or Cancel

For live worker-backed control actions, use:

- `bin/control_codework_worker_job.py --job-id <run_id> --action pause`
- `bin/control_codework_worker_job.py --job-id <run_id> --action cancel`

Current proven control-path truth:

- the VPS API accepts the control request
- active RayTracing cancellation now stops the running stage instead of waiting
  only for the next outer checkpoint
- worker-exchange readback surfaces `review.cancel_requested = true`
- terminal worker state becomes `canceled`
- `visualizer_publish` does not run for the interrupted job
- `publication_state` remains `none` and `published_frames` remain empty for a
  pre-publication interruption proof

### 4. Publish Missing Drops When Needed

If a run finished correctly but does not appear in live visualizer artifacts,
publish the staged drop instead of rerunning the render.

Preferred operator entrypoint:

- `bin/run_ray_tracing_worker_continuation_flow.py backfill --run-id <run_id> --wait`

That helper wraps the bounded VPS publish lane:

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
- use `bin/plan_ray_tracing_frame_chunks.py` before long multi-job RayTracing
  sequences so chunk boundaries and sampling offsets are explicit
- use `--ray-sampling-frame-offset` plus `--ray-sampling-frame-count` whenever
  chunked jobs should cover one continuous camera/light path
- when the goal is only worker-fleet validation, prefer a tiny
  `bin/submit_codework_worker_job.py` `live_debug` run instead of a longer
  continuation proof so render duration does not become the dominant variable
- prefer `bin/run_ray_tracing_worker_continuation_flow.py backfill` when the
  render completed correctly but visualizer artifact coverage needs repair
- use `bin/control_codework_worker_job.py` when the goal is worker control,
  not render submission
- Use a tiny seed run first when testing a new scene or new worker contract.
- Prefer continuation over rerunning PhysicsSim when only later RayTracing
  frames are needed.
- Keep seed and continuation quality intentionally cheap until the workflow is
  proven for that scene.
- Treat visualizer artifact publication as a separate step from render
  correctness.

## Current Gaps

- full trio resume beyond RayTracing-only continuation remains a later contract
  because PhysicsSim resume semantics are still narrower
