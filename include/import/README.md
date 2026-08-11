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
- The app-local compound-scene handoff reader, renderer binding manifest, and
  detached evaluated-scene exact-tick adapter. This boundary imports
  provenance-bound rigid transforms only. The separate render-owned detached
  geometry adapter consumes borrowed source positions; source mesh resolution
  and all render policy remain optiC-owned.
- The opt-in S9-G assembly archive contract: canonical multi-tick metadata,
  external runtime-mesh references, exact replay, and explicit separation of
  renderer set dressing from producer-proven simulation collision surfaces.
  Mesh bytes and render policy are excluded.
- The S9-H2 static-room import and rigid basis contract. It strictly consumes
  Ball Bounce's six-surface sidecar, joins it to the frozen transform packet,
  and maps body and surface frames from producer Y-up coordinates into optiC
  Z-up coordinates. It does not create rendered walls or visibility policy.
- The S9-I1 typed ingestion descriptor and resolver. It is local-only and
  derives a basis-correct runtime result from existing object/mesh bindings;
  it does not parse a render request or mutate a saved/base scene.

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
- Compound-scene packets must not pull collision hulls, solver code, V-HACD,
  materials, cameras, lights, sampling, or final-image policy across the
  import boundary.
