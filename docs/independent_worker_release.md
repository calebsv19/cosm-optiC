# Independent RayTracing Worker Releases

Status: implementation complete; first independent release is in the
decision-two rollout lane.

## Version Ownership

RayTracing has two release identities:

- `VERSION` owns the optiC desktop/source package line.
- `WORKER_VERSION` owns `ray_tracing_headless_worker`.

The worker package records both values, but its package name, manifest
`version`, runtime capability identity, package catalog pointer, installed
runtime receipts, worker-submission Registry projection, and Production
Registry worker-current pointer all use `WORKER_VERSION`.

The current `0.10.0` worker package is preserved as legacy app-coupled history.
It must not be relabeled as worker runtime `0.4.0`. The first independent
worker release is a new immutable artifact. Historical app-coupled package
roots already used `0.5.0` on part of the fleet, so the collision-free first
independent package is `0.5.1`; immutable historical roots are never replaced.

## Compatibility

Desktop/worker compatibility is negotiated from behavior, not app/worker
version equality:

- worker protocol range `1..1`
- checkpoint schema overlap `2..2`
- required `ray-tracing-worker-protocol-v1`
- required `ray-tracing-recovery-fence-v1`
- exact renderer executable SHA-256

The client fails closed on a protocol or checkpoint range mismatch, a missing
capability, or renderer digest drift. A compatible worker patch can therefore
ship without a desktop package release. A desktop release is needed only when
desktop code or its accepted compatibility contract changes.

## Read-Only Status And Plan

```sh
python3 production_registry/scripts/versionctl.py \
  worker-status --program ray_tracing

python3 production_registry/scripts/versionctl.py \
  worker-plan \
  --program ray_tracing \
  --bump minor \
  --observed-at <UTC> \
  --evaluated-at <UTC>
```

These commands do not edit either version file, build a package, contact a
host, or mutate Registry state.

The program-local contract is
`config/worker_release_contract.json`. It binds the version files, package
target, compatibility ranges, all-compatible-host policy, required receipt
classes, and authority boundaries.

## Two-Decision Release Sequence

Decision one may grant only:

1. `WORKER_VERSION` transition
2. exact source commit
3. fixed local gates
4. worker package build
5. worker artifact authentication

The resulting artifact must be immutable and identify the exact source commit,
worker version, source app version, platform, protocol/checkpoint ranges,
capabilities, and checksum.

Decision two may grant only:

1. worker package catalog publication
2. distribution to every compatible host selected from canonical inventory
3. installed-runtime readback from every selected host
4. worker-submission Registry projection
5. Production Registry worker-current compare-and-swap
6. independent selected-surface completion readback

Desktop version transition, desktop packaging/publication, and cleanup remain
separate authority domains. No worker decision implies any of them.

## Completion Gate

An independent worker release is complete only when all of these agree on the
same worker version and artifact:

- package manifest and checksum
- package catalog
- each selected host distribution receipt
- each selected host installed-runtime receipt
- coordinator-facing worker-submission Registry projection
- Production Registry worker-current pointer

Any missing, stale, incompatible, rejected, interrupted, or compare-and-swap
drifted receipt blocks completion. Exact replay may resume retained work, but
must not duplicate a completed mutation.
