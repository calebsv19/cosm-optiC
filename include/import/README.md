# include › import

Public import-facing headers for optiC scene, mesh, pack, manifest, and
simulation handoff adapters.

Headers in this directory expose app-local import contracts that other
`ray_tracing` subsystems may call. They should stay focused on stable import
interfaces and avoid leaking renderer implementation details, editor UI state,
or remote worker orchestration policy.

## Ownership

- Runtime scene bridge contracts.
- Mesh asset import and pack/load contracts, including stable asset identity,
  sidecar validity, loaded runtime mesh documents, and mesh asset instance
  handoff records.
- Fluid/VF3D/PhysicsSim pack import contracts.
- Water-surface sidecar import contracts.

## Boundaries

- Prefer narrow structs and functions that describe imported data ownership,
  validation results, and handoff records.
- Keep private parsing helpers in `src/import/` unless a caller genuinely needs
  the interface.
- Keep native `3D` integrator and material policy under `include/render/`.
- Keep future BLAS/TLAS declarations under `include/render/`; import headers
  should expose the mesh data needed by acceleration builders, not acceleration
  ownership itself.
- Keep command-line/headless request contracts under the app/tooling headers
  that already own those entry points.
