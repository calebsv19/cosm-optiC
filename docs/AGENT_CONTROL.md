# optiC Agent Control

This is the supported public agent-control guide for local `optiC`
(`ray_tracing`) workflows. It is intentionally smaller than the full renderer,
release, and worker system.

## Supported Local Surface

The live editor MCP also provides revision-guarded materials and presets through
the shared Materials inspector commands. See [materials MCP](materials_mcp.md)
for stable-ID inspection, typed edits, presets, shared history and persistence.
The bridge supports [live catalog updates](mcp_catalog_updates.md) that preserve
its connection and attached scene. Clients that cache tool definitions can use
`editor_catalog` and `editor_command` to discover and invoke current commands
through the same connection. See that guide for the initial-connection requirement.

Object inventory, base transforms, names, flags, box/plane creation, duplication
and removal use the same retained UI commands. See [object MCP](objects_mcp.md)
for revision guards, legacy preservation and save/reopen verification.

Static render-camera XYZ/yaw/pitch edits use the camera Inspector command. See
[camera MCP](camera_mcp.md) for world-unit conversion, animation refusals and
shared persistence.

Primary-light XYZ/intensity/radius and environment lighting/background edits
share the light Inspector, sliders and menu commands. See [lighting MCP](lighting_mcp.md)
for static/animation boundaries and legacy/attachment preservation.

The Render workspace and MCP also share local single-frame start, progress,
cancel and verified result controls. See [render MCP](render_mcp.md) for exclusive
context, job identity, unsaved-scene capture and result integrity semantics.

The File menu and MCP share guarded Open, Create and Save As commands. See
[scene lifecycle MCP](scene_lifecycle_mcp.md) for dirty-scene protection, new-file
publication, attachment continuity and the contained cross-directory dependency-copy boundaries.

Animation setup, camera/light and object channels, scalar keys, retiming,
interpolation, tangents and handles share the retained UI timeline commands. See
[animation MCP](animation_mcp.md) for units, guards and render-frame sampling.

For the end-to-end create/edit/render/Save As/reopen sequence, see
[complete MCP scene workflow](mcp_scene_workflow.md).

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

Run commands from a scratch parent directory that contains the checkout. To use
the commands below unchanged, clone the public GitHub repo into a `ray_tracing/`
directory:

```bash
git clone https://github.com/calebsv19/cosm-optiC ray_tracing
```

If you use GitHub's default `cosm-optiC/` directory name instead, replace
`make -C ray_tracing ...` and `ray_tracing/...` paths with
`make -C cosm-optiC ...` and `cosm-optiC/...`.

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
remote submit. The export command is repo-local. The queue validation command
uses the broader CodeWork workspace helper `bin/vps_worker_job_queue.py`; skip
that validation step in a standalone public clone unless that helper is present
beside the checkout.

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

## Managed STL scene authoring

For project-owned STL intake, per-instance shading, safe rebuilds and relocation
proof, use [Managed mesh assets](managed_mesh_assets.md). Keep the complete
project for source rebuildability; a runtime-only render export is a different
guarantee. Existing loose scenes keep their current behavior.

Spatial object/camera/primary-light route authoring is documented in
[Spatial paths MCP](spatial_paths_mcp.md), including authored units, source restoration
and cached catalog dispatch.

[Motion plans and follower orientation MCP](motion_orientation_mcp.md) describes
shared movement intent, preview/restore, direction/axis/focus and up/roll controls.

Managed STL intake and inspection: [MCP workflow](managed_stl_mcp.md).

Optical material transport editing: [shared UI/MCP workflow](optical_response_mcp.md).

## October 12 — repeatable optical comparison matrices

The local MCP runner creates portable comparison projects with a sealed baseline,
saved cells, fixed-sequence native renders, verified hashes and a self-contained
labeled contact sheet. IOR, transmission, absorption distance and uniform
graph-backed frosted roughness use existing shared authoring commands. The
original clean scene and supported attachments are preserved and restored;
partial runs retain completed cells and refuse overwrites. Saved-cell verification
reopens and repeats renders with exact same-build image hashes. Legacy rendering
keeps automatic sampling. See [optical comparisons](optical_comparison_mcp.md).
The catalog adds `render_start_seeded`: 78 named commands plus two fixed tools,
80 total. App 0.18.0 and worker 0.11.0 remain unchanged. Resume of partial matrices,
measured optical calibration and production image-quality acceptance remain
separate. Stop at the bounded verified Main Edit checkpoint.

## October 12 — optical matrix partial-run recovery

New matrix runs use a v2 cell journal with sealed recipe, baseline, copied
resources and native executable identity. `--resume` uses existing MCP commands
to adopt completed unjournaled results, wait for an owned active render or retry
terminal attempts under fresh IDs. Completed cells and uncertain saved scenes
are preserved; source/build drift, unrelated active work and concurrent runners
refuse. Original scene/attachment restoration and explicit saved-cell verification
use the existing owner. Old v1 matrices remain verifiable. See
[optical comparisons](optical_comparison_mcp.md) for the sealed-baseline boundary.
No new native command/schema, renderer kernel or catalog change: 80 tools remain.
App/worker versions remain 0.18.0/0.11.0. Stop at the bounded recovery checkpoint;
partial sample accumulation, remote resume and measured calibration are separate.
