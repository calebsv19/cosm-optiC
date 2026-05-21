# optiC Docs Index

Start here for public repository documentation.

Public identity:
- packaged desktop product: `optiC`
- repository/program key: `ray_tracing`

Last audited: 2026-05-04.

Current public focus:
- shipped native `3D` runtime ladder through `Disney`
- Phase 4.1/4.2 headless-agent render request preflight CLI
- deep-render start/resume and export workflow
- menu/editor control-surface truth for native `3D` scene and atmosphere selection
- narrower native `3D` verification lanes for config persistence, prepared-render parity/scatter, and geometry contracts
- post-`I6` renderer-lane selection before VF3D / `physics_sim` ingestion

## Current State
- `docs/current_truth.md`: current runtime contract and active boundaries.
- `docs/future_intent.md`: near-term renderer and workflow direction.
- `docs/headless_agent_render_cli.md`: `ray_tracing_agent_render_request_v1`
  request schema and preflight CLI command.
- `docs/render_review_sets/README.md`: local repo-doc review sets from detached
  renders. These are not the live visualizer website lane.
- `docs/desktop_packaging.md`: `.app` packaging commands, launcher diagnostics, and release-readiness workflow.

Live visualizer website publication is a separate pipeline:
- stage a `visualizer-run/v1` drop under
  `_private_workspace_artifacts/codework_visualizer_runs/<drop_id>/`
- upload/import it through `skills/codework-visualizer-drop/`
- or use `tools/publish_render_outputs.sh --mode visualizer|both` as the
  higher-level entrypoint
- or use `tools/publish_latest_render_run.sh` when the latest completed run and
  its last frame are the intended publish target

Current verification contract:
- `make -C ray_tracing clean && make -C ray_tracing`
- `make -C ray_tracing test-stable`
- `make -C ray_tracing run-headless-smoke`
  - currently routes through `test-stable` rather than a separate runtime-only lane
- `make -C ray_tracing visual-harness`
  - build-only readiness gate, not an unattended execution surface
- `make -C ray_tracing test-legacy`
- `make -C ray_tracing package-desktop-self-test`
- `make -C ray_tracing package-desktop-refresh`
- `make -C ray_tracing release-bundle-audit`

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview and build/run flow.
- `docs/KEYBINDS.md`: current runtime keybind reference.

Public product-facing docs should treat `optiC` as the primary app name and
use `ray_tracing` where repo/runtime identifiers need to stay exact.

## Review Sets
- `docs/render_review_sets/grime_screen_motion_review_v2/index.md`: latest published
  detached Disney motion review for the grime-screen scene.

## Private Planning Docs
- Private scaffold plans and internal execution docs are in the workspace private docs bucket:
  - `../../docs/private_program_docs/ray_tracing/`
