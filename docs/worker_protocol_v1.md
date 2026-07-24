# RayTracing Worker Protocol v1

Status: implemented locally
Protocol version: `1`
Worker runtime version: canonical `WORKER_VERSION` (currently `0.5.0`)
Checkpoint schema range: `2..2`

## Purpose

The RayTracing worker protocol separates durable render-job ownership from a
particular process. The detached job runner is the first client; it negotiates
with a spawned `ray_tracing_worker_runtime`, submits an immutable request
message, and consumes durable event files. The existing
`ray_tracing_render_headless` executable remains the renderer behind the
runtime.

This protocol is RayTracing-local. `core_headless_job` remains the outer
job-envelope/report boundary, while a shared cross-program worker protocol is
deferred until another simulation proves the same contract.

## Build And Commands

```sh
make -C ray_tracing ray-tracing-worker-runtime ray-tracing-job-runner

ray_tracing/build/toolchains/clang/arm64/tools/cli/ray_tracing_worker_runtime \
  capabilities \
  --render-cli /absolute/path/to/ray_tracing_render_headless \
  --output /absolute/path/to/worker_capabilities.json

ray_tracing/build/toolchains/clang/arm64/tools/cli/ray_tracing_worker_runtime \
  run --message /absolute/path/to/worker_request.json
```

The exact build directory may vary by host toolchain.

## Negotiation And Identity

Before spawn, the client requests a durable `capabilities` message and checks:

- the schema is `ray_tracing_worker_protocol`
- protocol `1` lies inside `protocol_min..protocol_max`
- every required capability bit is present
- the worker checkpoint schema range overlaps the desktop-supported `2..2`
  range
- `renderer_build_sha256` matches the renderer executable selected by the
  client

The request binds both:

- `request_sha256`: SHA-256 of the canonical staged render request
- `renderer_build_sha256`: SHA-256 of the selected headless renderer

The worker recomputes both digests before launching the renderer. A mismatch,
unsupported version, missing capability, malformed message, or changed binary
fails closed. A present but incompatible worker never silently drops to the
legacy path.

## Message Set

All messages are UTF-8 JSON. Required message types are:

- `capabilities`
- `request`
- `progress`
- `dirty_region`
- `cancellation`
- `checkpoint_reference`
- `completion`
- `interruption`

Capabilities declare protocol and checkpoint ranges, a capability bitmask,
worker-runtime version, and renderer digest.

The worker-runtime version is generated from root `WORKER_VERSION`; it is not
derived from the desktop/source `VERSION`.

Requests declare protocol version, required capabilities, sequence, job id,
canonical request and renderer paths plus their digests, output/status/event
paths, cancellation path, dimensions, frame window, and declared temporal
subpass count. Lease-managed recovery requests additionally carry a recovery
descriptor path, coordinator-issued authority path, worker identity, and
one-use receipt path. Those fields are absent for ordinary local runs.

Events share protocol version, monotonic sequence, job id, state, diagnostics,
exit code, frame/subpass progress, optional dirty rectangle, optional durable
reference path/digest, and optional completion-summary digest. Fields not
applicable to an event retain neutral values.

## Durable Files

A protocol-mode job root adds:

```text
worker_capabilities.json
worker_request.json
worker_client_state.json
worker_cancel.json              # only after cancellation is requested
ray_tracing_recovery_descriptor.json # after interrupted-job reconciliation
resume_authority.receipt.json   # only after an authorized resume consumes its token
worker_events/
  00000000_progress.json
  ...
```

Protocol control files and events use the RayTracing durable-publication
boundary: same-directory temporary output, file synchronization, atomic
rename, and parent-directory synchronization. Event filenames bind sequence
and message type and are create-only, preventing a later event from rewriting
earlier evidence.

`checkpoint_reference` can point to either a fully committed Tier A frame or a
schema-2 tile-batch generation. References include the frame,
completed-subpass count, immutable generation path, and generation SHA-256.
The schema-2 payload and fallback rules are documented in
`docs/temporal_subpass_checkpoints.md`.

## Cancellation And Interruption

The client durably writes a `cancellation` message before signaling the spawned
worker. The worker also polls that cancellation file and forwards termination
to its renderer child. It then emits a cancellation event when it can complete
the cooperative path.

If the worker process is killed before it can emit a terminal event, the
existing restart-visible job reconciliation classifies the job as
`interrupted`, then as `resumable` or `recovery_required` from durable
completed-frame evidence. Observation does not grant automatic restart or
fleet lease authority.

## Phase E Recovery Authority

Every job-runner invocation performs a non-executing scan of the persistent
jobs root. A dead active job is refreshed to `interrupted`; its durable evidence
is projected into `ray_tracing_recovery_descriptor.json` as either `resumable` or
`recovery_required`. The descriptor binds the original request SHA-256 and the
exact completed-frame, tile-batch, or empty-prefix recovery anchor SHA-256.

Fleet resume remains manual. When `RAY_TRACING_FLEET_JOB=1`, `--resume` fails
closed unless the launch also provides:

```text
RAY_TRACING_RECOVERY_DESCRIPTOR_PATH
RAY_TRACING_RESUME_AUTHORITY_PATH
RAY_TRACING_RECOVERY_WORKER_ID
```

The coordinator authority binds one token to the source job, worker, lease,
lease generation, request digest, checkpoint digest, output generation, fence
path, durable receipt path, and expiry. The worker verifies every binding,
checks the current fence, then creates the receipt with create-only semantics
before spawning the renderer. Token replay therefore fails even through a
different resumed job root.

For an authorized recovery, every durable RayTracing rename checks the active
output fence immediately before publication. The worker supervisor also checks
the fence while the renderer is alive and terminates the child if the lease,
worker, token, or output generation is superseded. Coordinator ingestion still
performs its own lease and generation validation; local fencing does not
replace coordinator authority.

## Compatibility And Fallback

The detached job runner uses protocol mode by default when the sibling worker
runtime exists. `RAY_TRACING_WORKER_PROTOCOL_MODE=direct` explicitly selects
the legacy direct-spawn fallback. A missing worker executable also permits that
fallback; an executable that answers incompatibly or fails digest validation
does not.

Synchronous and desktop in-process rendering remain available. The desktop is
not switched to the protocol by default in Phase B; its eventual adoption must
reuse the thin client and retain its current synchronous fallback.

## v1 Proof Contract

`make -C ray_tracing test-ray-tracing-worker-protocol` covers serialization,
validation, SHA-256 identity, negotiation, future-version rejection, missing
capabilities, every event type, and invalid checkpoint references.

`make -C ray_tracing test-ray-tracing-worker-protocol-phase-b` proves:

- exact BMP parity between protocol and direct fallback
- matching completion/diagnostic and timing schema
- durable capability, request, progress, dirty-region, checkpoint, and
  completion evidence
- explicit direct fallback
- request-digest tamper rejection
- durable cancellation request and event
- process-death recovery through restart-visible interrupted status

`make -C ray_tracing test-ray-tracing-temporal-checkpoint-phase-c` additionally
proves injected post-commit termination, corrupt-newest fallback, durable
temporal checkpoint references, and byte-identical resumed output.

`make -C ray_tracing test-ray-tracing-fleet-recovery-phase-e` additionally
proves boot reconciliation without execution, exact descriptor digest
handoff, manual authority activation, one-use receipt creation, fleet resume
rejection without authority, stale-holder durable-write rejection,
duplicate-host rejection, and monotonically advancing reassignment
generations.

The Linux worker package source now includes the runtime entrypoint and
`ray-tracing-worker-protocol-v1` and `ray-tracing-recovery-fence-v1`
capabilities. Its artifact and manifest identity use `WORKER_VERSION`, while
`source_program_version` records the source app line without coupling the two
release pointers. See `docs/independent_worker_release.md`. No package build,
distribution, installation, Registry promotion, or independent worker release
is implied by this source contract.
