# Temporal-Subpass Checkpoints

Status: implemented locally
Checkpoint schema: `2`
Worker runtime version: `0.3.0`

## Recovery Boundary

Schema 2 recovers a partially rendered native `3D` frame at a quiescent tile
batch inside a temporal subpass. The scheduler submits a bounded batch, waits
for every worker in that batch, and only then snapshots accumulation state.
The checkpoint codec therefore never reads buffers while workers mutate them.
Per-tile committed counts act as the completed-tile map while the global
completed-subpass boundary has not advanced.

This is an opt-in headless boundary. Detached jobs with more than one temporal
frame enable it automatically under `<output.root>/checkpoints`; `--resume`
requests restoration. The renderer request form is:

```json
"checkpoint": {
  "enabled": true,
  "resume": true,
  "root": "/absolute/job/output/checkpoints",
  "tile_batch_size": 8,
  "max_tile_batch_size": 64,
  "max_interval_ms": 2000
}
```

`resume` requires `enabled`, and an enabled checkpoint requires an absolute
root after normal request path resolution.

## On-Disk Shape

Each committed generation is immutable:

```text
checkpoints/
  frame_0000/
    current.json
    generation_00000000000000000041.rtck
    generation_00000000000000000042.rtck
```

The binary generation is written through a same-directory temporary file,
file sync, atomic rename, and parent-directory sync. Only then is
`current.json` durably replaced with relative newest and previous generation
references plus their SHA-256 digests. After the pointer is durable, retention
removes generations older than the newest two. Resume validates immutable
files newest to oldest; `.tmp` files are never candidates. A crash after
rename but before pointer replacement may select either the new complete
generation or the previous complete generation. The pointer is an operator
hint, not restore authority.

If a completed frame's BMP is intentionally removed but its final-subpass
checkpoint remains and no longer matches the freshly reconstructed tile
layout, resume performs a clean deterministic rerender of that frame. A
partially completed incompatible checkpoint still fails closed.

## Identity And Payload

A generation binds:

- normalized render-request, scene, resolved-asset, worker-runtime, and
  renderer-binary SHA-256 identities
- sampling identity, frame index, dimensions, tile size and tile layout
- integrator, temporal-frame count, schema version, and renderer ABI sizes
- per-tile committed subpass counts
- radiance accumulation, activity, and sample-count buffers
- adaptive-sampling stable, active, scratch, and tile masks
- adaptive per-pixel states and summary state

Prepared frames, BVHs, feature buffers, and other acceleration structures are
rebuilt from the immutable inputs after restart. They are not serialized.

Schema 2 is deliberately renderer-build-bound. Its raw floating-point and
adaptive-state payload is accepted only on the same little-endian float32
format and exact renderer/runtime identities. This prevents a nominal schema
match from silently restoring state across incompatible code.

## Worker Evidence

Worker protocol v1 runtime `0.3.0` advertises checkpoint schema range `2..2`.
Its request
includes the declared temporal-subpass count, and each durable generation can
produce a `checkpoint_reference` event with path, SHA-256, frame index, and
completed-subpass count. If the renderer exits immediately after committing a
generation, the worker scans immutable generation names and emits the newest
readable reference before its interruption event.

Checkpoint presence never grants automatic execution or fleet lease
authority. Local recovery still requires an explicit `--resume`; fleet
reassignment remains a later lease-fenced phase.

## Proof

```sh
make -C ray_tracing test-ray-tracing-temporal-checkpoint-phase-c
make -C ray_tracing test-ray-tracing-tile-batch-checkpoint-phase-d
```

The integration test renders an uninterrupted reference, terminates a worker
immediately after subpass 2 has durably committed, corrupts that newest
generation, resumes from the surviving subpass-1 generation, and requires the
completed BMP to match the uninterrupted reference byte-for-byte. It also
checks interruption/resumability status and durable checkpoint-reference
events. The Phase D matrix separately exits before write, during temporary
write, after file sync, after rename, and before directory sync. Every case
resumes from a previous or new complete generation and produces a
byte-identical BMP. Summary evidence reports generation counts and
total/last/maximum write cost; cadence grows an undersized tile budget when
checkpoint overhead dominates and contracts it when a render batch exceeds
the elapsed-time bound.
