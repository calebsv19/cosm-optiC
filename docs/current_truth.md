# Ray Tracing Current Truth

Last updated: 2026-04-29

## Program Identity
- Repository directory: `ray_tracing/`
- Public product name: `RayTracing Project`
- Primary runtime entry:
  - `src/app/animation.c` (`main()` delegates through `ray_tracing_app_main(...)`)
  - wrapper shell: `include/ray_tracing/ray_tracing_app_main.h`, `src/app/ray_tracing_app_main.c`

## Current Shipped State
- Legacy `2D` rendering and editor flows remain present.
- Native `3D` runtime ladder is shipped through:
  - `Direct Light`
  - `Diffuse Bounce`
  - `Material`
  - `Emission / Transparency`
  - `Disney`
- Native `3D` output is RGB-aware through the full shipped ladder.
- Native `3D` support layers now include:
  - tile preview
  - dirty-rect preview updates
  - tile occupancy culling
  - temporal accumulation upgrades
  - stratified + blue-noise sampling support
  - Disney-only denoise
  - optional top-fill lighting
- Deep-render export now supports:
  - absolute start-frame selection
  - resume from highest existing saved frame
  - shared absolute-frame truth across output numbering and path sampling
- Export/video workflow state:
  - `frameDir` remains frame export root
  - `videoOutputRoot` remains persisted runtime config state
  - menu exposes grouped Data I/O + batch actions

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Support lanes: `config/`, `assets/`, `data/`, `tmp/`
- Active source subsystems:
  - `app`, `camera`, `config`, `editor`, `engine`, `export`, `geo`, `import`, `material`, `path`, `render`, `scene`, `tools`, `ui`

## Verification Contract
- Build:
  - `make -C ray_tracing clean && make -C ray_tracing`
- Smoke/harness:
  - `make -C ray_tracing run-headless-smoke`
  - `make -C ray_tracing visual-harness`
- Stable tests:
  - `make -C ray_tracing test-stable`
- Legacy lane:
  - `make -C ray_tracing test-legacy`

## Release and Packaging Snapshot
- Release-readiness and desktop packaging lanes are active and maintained.
- Standard package flow is available through `package-desktop*` targets.
- Release flow includes contract/audit/sign/notary/staple/verify/distribute gates.

## Runtime Data Policy
- Tracked defaults remain under `config/`.
- Runtime/generated state remains under `data/runtime` and other ignored runtime lanes.
- Export/video roots are runtime-config driven and menu-editable.

## Current Boundary
- Do not reopen closed `I5`/`I6` slices.
- Choose the next post-`I6` renderer lane cleanly.
- Keep deep-render start/resume behavior stable while adjacent runtime-scene buckets settle.
- Defer VF3D / `physics_sim` ingestion expansion until the next internal renderer boundary is chosen.

## History and Deep Lane References
- Full phase-by-phase details and archived slices are in private docs:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/ray_tracing/`
- This file is the compressed public current-state contract.
