# optiC Agent Control

This is the supported public agent-control guide for local `optiC`
(`ray_tracing`) workflows. It is intentionally smaller than the full renderer,
release, and worker system.

## Supported Local Surface

The initial supported agent surface is:

- build the headless render CLI
- validate a render request with `--preflight`
- render one BMP frame from a repo fixture
- optionally run the material-preview CLI for fast surface-treatment checks
- read JSON summaries and generated image outputs from ignored local roots

The local agent surface does not require a packaged `.app`, website access,
production registry writes, or remote worker jobs.

For the canonical demo pack selected by RT-R2, see `docs/AGENT_DEMO_PACK.md`.

## Setup Assumptions

Run commands from the workspace parent that contains `ray_tracing/`.

Required local dependencies are the same as the repo build:

- C compiler toolchain
- SDL2
- SDL2_ttf
- json-c
- FFmpeg only when video assembly is required

Generated outputs are ignored by git under roots such as `build/` and
`visual_artifacts/`.

## First Render Workflow

Build the headless render CLI:

```bash
make -C ray_tracing ray-tracing-render-headless
```

Validate the smallest request fixture without writing frames:

```bash
ray_tracing/build/arm64/tools/cli/ray_tracing_render_headless \
  --request ray_tracing/tests/fixtures/agent_render_preflight_request.json \
  --preflight
```

On non-Darwin builds, the binary may be:

```bash
ray_tracing/build/tools/cli/ray_tracing_render_headless
```

Render one BMP frame:

```bash
ray_tracing/build/arm64/tools/cli/ray_tracing_render_headless \
  --request ray_tracing/tests/fixtures/agent_render_image_export_request.json \
  --render
```

Expected result:

- a `ray_tracing_headless_summary_v1` JSON summary
- BMP frame output under the request's local output root
- nonzero diagnostics unless the fixture intentionally exercises a negative path

## Make Targets

Use these routine local gates before broader tests:

```bash
make -C ray_tracing test-ray-tracing-render-headless-preflight
make -C ray_tracing test-ray-tracing-render-headless-image-export
```

For the baseline unattended visual proof:

```bash
make -C ray_tracing visual-artifact
```

That target renders and validates:

```text
ray_tracing/visual_artifacts/source_first_frame/frames/frame_0000.bmp
```

and writes:

```text
ray_tracing/visual_artifacts/source_first_frame/artifact_validation.json
```

## Material Preview Workflow

For material/readback questions where full lighting transport is unnecessary:

```bash
make -C ray_tracing ray-tracing-material-preview-headless
make -C ray_tracing test-ray-tracing-material-preview-headless
```

See `docs/headless_material_preview_cli.md` for the request schema and generated
preview-set publication helper.

## Canonical Demo Pack

Run the RT-R2 demo pack when the agent needs to prove and explain a complete
local demo:

```bash
make -C ray_tracing test-ray-tracing-render-headless-image-export
make -C ray_tracing test-ray-tracing-render-headless-mesh-asset-spheres
```

The selected canonical scene is
`ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/`. It renders three
mesh-asset sphere instances and records route, BVH, object-audit, and BMP output
readback. `docs/AGENT_DEMO_PACK.md` lists the full pack, expected outputs, and
the optional local queue-bundle dry run.

## Worker Bundle Dry Run

The first local worker-package workflow is a dry local queue fixture, not a live
remote submit:

```bash
python3 ray_tracing/tools/export_worker_queue_fixture.py \
  --fixture \
  --output-root ray_tracing/visual_artifacts/worker_queue_exports \
  --item-name ray-tracing-agent-local-fixture
python3 bin/vps_worker_job_queue.py \
  --queue-root ray_tracing/visual_artifacts/worker_queue_exports \
  validate \
  --item-name ray-tracing-agent-local-fixture
```

This validates local bundle shape only. Live upload, remote submit, worker
package refresh, and visualizer publication are separate operator-controlled
lanes.

## Failure Triage

- Missing CLI binary: rerun the matching `make -C ray_tracing ...` build target.
- Request parse failure: check `schema_version`, relative paths, and JSON syntax.
- Missing scene or mesh sidecar: resolve paths relative to the request file.
- Empty or invalid BMP: rerun the focused fixture target before broad tests.
- Existing output collision: use the request's documented overwrite behavior or
  clean only the generated output root you own.
- Long-running detached job confusion: trust `job_status.json`,
  `render_progress.json`, and `result_summary.json` over raw stdout text.

## Do Not Do In The First Agent Workflow

- edit renderer source
- edit `VERSION`
- build release artifacts
- sign, notarize, upload, deploy, or publish
- mutate `production_registry`
- edit website release metadata or `/agents/` manifests
- submit remote worker jobs
- rely on maintainer-local absolute paths as public instructions

Those actions belong to later release-readiness passes after the public local
contract is stable.
