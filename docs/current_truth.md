# Ray Tracing Current Truth

Last updated: 2026-04-27

## Program Identity
- Repository directory: `ray_tracing/`
- Public product name: `RayTracing Project`
- Primary runtime entry:
  - `src/app/animation.c` (`main()` delegates through `ray_tracing_app_main(...)`)
  - wrapper shell: `include/ray_tracing/ray_tracing_app_main.h`, `src/app/ray_tracing_app_main.c`

## Current Shipped State
- Editor/preview stabilization lane is complete and archived.
- Active implementation boundary is menu/export support with runtime `3D` proof validation paused behind it.
- Native `3D` route foundation is active (`R3-S1` complete):
  - retained primitive bridge seeds build runtime triangles
  - native camera/light samples route through native scene structures
  - first-hit + direct-light + blocker visibility path exists on native geometry
  - bounded tests cover visible/shadowed/light-motion response
- Export/video workflow state:
  - `frameDir` remains frame export root
  - `videoOutputRoot` is persisted runtime config state
  - menu now exposes grouped Data I/O + batch actions (`Render Frames Root`, `Video Output Root`, `Clear Frames`, `Make Video`)

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
- Complete menu/export usability and batch flow hardening first.
- Then resume runtime proof-fixture validation for bounded native `3D` behavior.
- After that, proceed to VF3D / `physics_sim` ingestion expansion.

## History and Deep Lane References
- Full phase-by-phase details and archived slices are in private docs:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/ray_tracing/`
- This file is the compressed public current-state contract.
