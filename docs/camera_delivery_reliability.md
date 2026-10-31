# Portable event plans and verified native sequence delivery

These changes ship in desktop 0.22.0 / worker 0.16.0, release `v0.22.0`.
The older 0.21.0 / worker 0.15.0 packages retain their original behavior.
[Release and installed-runtime identities](release_versions.md) remain distinct
from source labels. Python delivery tools use the checked-out source workflow;
the worker package provides the native renderer, job runner and worker runtime.

## Portable retained event plans

`ray_event_geometry_validation_v1` accepts existing saved schema-v1 event plans
without rewriting them or generating a platform-local replacement proposal.
Validation has three separate checks:

1. Saved source/camera/sequence dependencies and the retained proposal's canonical
   content hash remain exact. The native re-derivation's hash is not substituted
   for the saved plan's hash.
2. Only named derived quantities allow bounded numeric variation. World-metric
   vectors (`focus`, `pivot`, bounds, event/lead positions) and lead distance use
   `min(1e-9, 1e-12 + 32*DBL_EPSILON*max(abs(a),abs(b)))`, in metres. Derived
   yaw/pitch and lead duration use absolute `1e-12`, in radians/seconds. All values
   must be finite. Authored FOV, candidate index, counts, visibility flags and
   other metadata remain exact; missing/extra fields and wrong types refuse.
3. The reviewed saved seed/focus/pivot are retained. The existing native owners
   rebuild their motion schedule and recheck feasibility, framing against current
   source bounds, entry visibility and sampled occlusion metadata. Unsupported or
   meaningfully changed geometry still refuses.

Event frame receipts identify `event_validation_policy`. This is a validation
policy version; the saved scene schema and canonical hash format are unchanged.
The contract permits numerical equivalence, not bit-identical rendered pixels
across different platforms. Large-coordinate scenes may conservatively refuse
because the metric allowance is capped; the cap is not expanded with translation.

## Encoder admission before rendering

Delivery profiles are explicit: `libx264` (H.264, the default) or `mpeg4`
(MPEG-4 Part 2). There is no automatic fallback. Readiness performs a real
16-by-16 two-frame encode, verifies the actual codec/dimensions, exact timestamps
and duration, then fully decodes the result. Temporary probe files are retired.

```sh
python3 tools/sequence_video.py --preflight --encoder libx264
python3 tools/sequence_video.py --preflight --encoder mpeg4
```

A missing executable, encoder, muxer, decoder or invalid timing fails readiness.
The multiview tool performs admission before creating delivery output or invoking
any renderer/job submission. Its views and matrix use the same selected profile.
A required H.264 delivery still requires a host with qualified H.264 support;
choosing MPEG-4 is an explicit output-format decision.

For existing complete native frames:

```sh
python3 tools/sequence_video.py --scene <scene.json> --frames <frames> \
  --output <new-video.mp4> --encoder mpeg4
```

Readiness and video sidecars record the selected codec, tool paths/digests,
exact frame timing, input hashes and successful decode. Technical validation
remains separate from human review of composition, occlusion and motion.

## Queued delivery through the existing native owner

Build `ray-tracing-render-headless`, `ray-tracing-job-runner` and
`ray-tracing-worker-runtime` together into a fresh output root. Then select the
native job runner instead of direct CLI rendering:

```sh
python3 tools/sequence_multiview.py --request <request.json> \
  --camera camera/event_example --runner <ray_tracing_job_runner> \
  --encoder mpeg4 --wait-timeout 300 --output <new-delivery-directory>
```

The request resolves its scene relative to the request file. Staged native jobs
preserve `scene.camera_id`, so an independent view is not replaced by the edited
shot view. Each view retains `native-job.json`, its native job ID, job directory,
request, native result and exact continuation argument vector. The shared
`codework_job` envelope and existing worker protocol remain the queue owner.

## Completion and recovery

`submit` confirms admission. `status` is an observation. Use `wait` to obtain
verified completion:

```sh
<runner> wait --job-id <id> --jobs-root <jobs> --timeout-seconds 300
```

| Exit | Meaning | Action |
| --- | --- | --- |
| `0` | `completion_verified=true`: native worker exit-zero completion event, matching immutable request/summary and complete durable images/sequence receipts | Consume the result |
| `1` | Native failure/interruption or reported completion lacks valid evidence | Inspect the same job and its diagnostics; reconcile before recovery |
| `124` | Bounded wait expired; the job may still run | Repeat `wait` with the same job ID; do not submit a replacement |
| `2` | Invalid wait arguments | Correct the command |

The bounded wait refreshes only its selected job; unrelated malformed or dead
history cannot block collection. It scans event filenames and parses the latest
durable event, avoiding repeated reads of a long job's entire progress history.

For an explicitly cancelled native protocol job, wait for its worker interruption
receipt before resuming. An approved `submit --resume` creates a new native job ID
under the existing recovery owner; retain the prior attempt. This differs from
continuing to wait on an active or uncertain attempt.

Local manual recovery requires the unchanged original request and the same qualified
build. The receipt checks do not constitute a general render-settings or binary
identity fence; production fleet recovery retains its existing stronger fences.

Before skipping sequence frames, recovery checks their BMP validity and image
hashes, scene identity, exact source mapping/revision/ticks, camera/shot identity,
selection and motion clock. Missing, malformed, corrupted or wrong-view receipts
refuse without modifying retained output. A hard interruption between image and
receipt commits may require explicit reconciliation; incomplete pairs are not
silently accepted or overwritten.

The retained acceptance fixture has 102 frames at 24 FPS (17/4 seconds), a moving
lead, cuts, Hold/orbit and Resume. A cooperative interruption after 45 frames and
57-frame continuation match uninterrupted physical states, camera states and
pixels. Independent-view and video timing/decode checks are separate gates.

This native queue/protocol contract does not establish production fleet payload
admission, coordinator rollout or fleet recovery authority. Fleet-managed resumes
must keep their existing coordinator-issued fences. No solver or source-coverage
extension is implied.

## Focused verification

Use a new `BUILD_DIR_BASE`; never clean another build root:

```sh
make BUILD_TOOLCHAIN=clang CLANG_CC=clang BUILD_DIR_BASE=<fresh-build> \
  test-camera-framework test-camera-framework-sanitize test-sequence-encoder
python3 -B tests/integration/check_event_camera_portability.py \
  --fixture <compact-portable-fixture> --out <new-proof> --probe <event_camera_probe>
make BUILD_TOOLCHAIN=clang CLANG_CC=clang BUILD_DIR_BASE=<fresh-build> test-sequence-job-recovery \
  SEQUENCE_RECOVERY_FIXTURE=<compact-portable-fixture> \
  SEQUENCE_RECOVERY_OUTPUT=<new-recovery-proof>
```

`check_sequence_encoder.py --require-encoder libx264` additionally requires a
real H.264 encode/decode on that host. The default requires MPEG-4 and always
checks that missing-encoder admission fails before output/rendering, without
silently selecting another profile.

The standalone strict gates declare POSIX.1-2008, matching the application build
for interfaces such as `strnlen`. Select the actual Clang executable for these
`-Werror` gates. If the native host has conflicting compiler support installations,
qualify and explicitly select a compatible support directory; preserve compiler
diagnostics instead of disabling warnings to obtain a pass.

## Linux leak checking

Run the sanitizer suite from an ordinary untraced Linux process. A traced agent
shell can make LeakSanitizer abort before it checks product memory. Check the
process's `TracerPid` in `/proc/self/status`; a compatible execution reports zero.
Keep `detect_leaks=1`; do not use a suppression or disable leak checking to turn an
incompatible environment into a pass.

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
LSAN_OPTIONS=exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
make BUILD_TOOLCHAIN=clang CLANG_CC=clang BUILD_DIR_BASE=<fresh-build> \
  test-camera-framework-sanitize
```

The October 31 follow-up qualifies exact release source `805c1c9` with seven
sanitizer targets: sequence clock, clock JSON, camera sequence, inspection orbit,
event geometry, event lead and event validation. Its runner first verifies both a
clean allocation/free control and a deliberately leaking control (123 bytes,
nonzero exit with an explicit leak report), then runs the gates. All seven pass
with address/undefined-behavior checks and Linux leak detection enabled. No product
code changed. This bounds the result to the exercised camera tests.

For the internal CodeWork cross-host workflow, the maintained
`<workspace>/bin/linux_pc_optic_camera_sanitizer_request.py` accepts a source
archive, source commit, thread ID and local report-inbox root. It stages a
hash-bound fixed helper through the existing PC report-inbox lane. The outer
runner executes that helper without a traced agent shell. No caller-selected
command, test target or sanitizer override is accepted. The helper writes its
result and logs beneath the thread's `vps/camera_sanitizers/`, preserves failed
reproducers, and retires successful scratch only after a process-consumer check.
An uncertain submission must be inspected under its existing thread identity.
The fixed `retire-current-thread-request` action can retire an earlier successful
scratch tree only with that result's exact digest and a passing consumer check.

Preserve the original ptrace failure as historical evidence; its successor is a
verified Linux pass, not a rewritten old test receipt. This does not qualify
whole-renderer memory use, distributed recovery or abrupt power-loss behavior.
