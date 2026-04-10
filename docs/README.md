# Ray Tracing Docs Index

Start here for public repository documentation.

## Scaffold State
- `docs/current_truth.md`: current scaffold/runtime structure and verification snapshot.
- `docs/future_intent.md`: planned scaffold convergence path and pending migration slices.

Current verification contract:
- `make -C ray_tracing clean && make -C ray_tracing`
- `make -C ray_tracing run-headless-smoke`
- `make -C ray_tracing visual-harness`
- `make -C ray_tracing test-stable`
- `make -C ray_tracing test-legacy`

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview and build/run flow.
- `docs/KEYBINDS.md`: current runtime keybind reference.

## Private Planning Docs
- Private scaffold plans and internal execution docs are in the workspace private docs bucket:
  - `../../docs/private_program_docs/ray_tracing/`
