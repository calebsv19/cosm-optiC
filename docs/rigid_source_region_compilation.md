# Audited source-region compilation and elastic jaw response

October 27, 2026 Main Edit source qualification. The explicit `compile-source-regions` route reuses the audited, digest-pinned source partitions for the U-channel and open-end wrench. It merges two regions only when the convex hull preserves their combined volume within a relative source-volume tolerance of 1e-12. The wrench reduces from 18 exact source regions to seven convex children, within the unchanged 12-child native capacity. Measured relative volume error is 8.381926343142675e-16. Source STL, visible geometry, center of mass, principal frame and source-solid inertia remain unchanged.

This route requires an already verified prepared asset with an audited exact source partition. It is an explicit curated-source compiler, not an automatic decomposition backend for arbitrary STL files. An unsupported source, occupied output, failed hull, excess child budget or readback/publication failure is refused. Publication uses a private temporary directory and a non-replacing link. The output is a BCR3D artifact for the existing source/hash/basis-checked `prepare-decomposition` workflow:

```sh
rigid_scene_native compile-source-regions <prepared-asset-directory> wrench <new-output.bcr3d>
rigid_scene_native prepare-decomposition <request.json> <new-prepared-directory>
```

The request uses `artifact_basis: "right_handed_z_up"`, the original source SHA-256 and the newly compiled artifact SHA-256, with meter units and scale 1. Preserve the legacy artifacts and scenes as separate inputs; replacing a scene's collision digest is an explicit new physical input and result identity.

## Bounded response

The source-exact seven-child jaw reaches its first contact at tick 313. Geometry alone still failed the elastic energy gate with the sequential contact solver. A native opt-in now solves a qualified pair of elastic contact patches together through their 2x2 effective-mass system. It applies positive normal impulses at the patch centroids, with the equivalent equal per-point impulse distribution. Every original point must independently satisfy its restitution target and separation; otherwise the candidate bodies are restored exactly and the ordinary solver remains responsible. Singular systems, friction, unqualified point distributions, rotating/off-center patches that fail those conditions, or other manifold counts do not gain an approximation through this route. The default numerical world configuration leaves the opt-in disabled.

The opt-in is enabled only on the native anchored support path for a bound imported decomposition satisfying the existing zero-gravity, restitution-one, zero-friction and zero-resistance energy profile. Accepted direct results report one block solve. The ordinary sequential iteration budget is unchanged. The staged source-overlap and original-reference energy gates remain active, with the same 2 mm and 0.1% limits and atomic rollback.

The centered 360-tick deep-jaw run now completes with two loaded contact clusters, maximum independently audited source overlap 4.690692279041286e-15 m and maximum relative kinetic-energy error 1.2143064331837648e-15. Normal and ASan/UBSan histories agree exactly. Its immutable result replays. Speed, mass, transform and nearby response controls are recorded in the retained qualification receipt; only passing cases are admitted as physical results.

## Remaining limits and reproduction

The older twelve-child approximation still refuses tick 316 at the source-overlap gate, retaining tick 315. Its historical later energy failure is preserved. Source preparation and a successful centered two-patch response do not qualify arbitrary concave CCD, asymmetric multi-contact impact, settling, frictional jaws, or unrestricted STL decomposition. optiC consumes immutable Ball results and visible mesh transforms; it does not acquire a collision solver.

A portable regression runner accepts the output of the existing legacy intake qualification:

```sh
python3 tests/qualify_rigid_source_regions.py --bin-root <normal-and-san-build-root> \
  --legacy <qualify_rigid_decomposition-output> --out <fresh-proof-root>
```

The legacy qualification must have been run with `--cases wrench_jaw`. The source-region runner verifies artifact compilation/readback, non-replacement and unsupported-source refusals, unchanged source/visible bytes, normal/sanitizer trajectories, independent source SAT and energy, post-impact rollback, immutable result sealing and replay. A diagnostic off-center case retains its actual outcome rather than assuming qualification.

No app/shared version change, canonical adoption, installed refresh, raster render, remote job, release or publication is part of this source slice. The historical [jaw refusal](rigid_wrench_jaw_refusal.md) remains evidence for its original collision artifact.

Final receipt: ten accepted cases, 3460 exact frames per producer, maximum source overlap 4.690692279041286e-15 m and relative energy error 3.026745520884333e-12. The15mm off-center control also passes its independent audit and immutable replay. Six historical controls and all38 ordinary runtime checks pass.
