# Ray Tracing Docs Index

Start here for public repository documentation.

Last audited: 2026-05-04.

Current public focus:
- shipped native `3D` runtime ladder through `Disney`
- deep-render start/resume and export workflow
- menu/editor control-surface truth for native `3D` scene and atmosphere selection
- narrower native `3D` verification lanes for config persistence, prepared-render parity/scatter, and geometry contracts
- post-`I6` renderer-lane selection before VF3D / `physics_sim` ingestion

## Current State
- `docs/current_truth.md`: current runtime contract and active boundaries.
- `docs/future_intent.md`: near-term renderer and workflow direction.
- `docs/desktop_packaging.md`: `.app` packaging commands, launcher diagnostics, and release-readiness workflow.

Current verification contract:
- `make -C ray_tracing clean && make -C ray_tracing`
- `make -C ray_tracing run-headless-smoke`
- `make -C ray_tracing visual-harness`
- `make -C ray_tracing test-stable`
- `make -C ray_tracing test-legacy`
- `make -C ray_tracing package-desktop-self-test`
- `make -C ray_tracing package-desktop-refresh`
- `make -C ray_tracing release-bundle-audit`

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview and build/run flow.
- `docs/KEYBINDS.md`: current runtime keybind reference.

## Private Planning Docs
- Private scaffold plans and internal execution docs are in the workspace private docs bucket:
  - `../../docs/private_program_docs/ray_tracing/`
