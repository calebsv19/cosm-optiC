## October 31 — Linux camera leak checking completed

The exact released source `805c1c9` (`v0.22.0`, worker `0.16.0`) now passes all
seven camera framework sanitizer gates on native Linux with ASan/UBSan and leak
detection enabled. The compatible runner reports `TracerPid: 0`. A clean control
exits zero; a deliberate 123-byte leak is detected and exits nonzero before the
product tests run. No leak or sanitizer error was observed in these seven gates.
This closes the earlier ptrace execution blocker for this bounded suite; it is
not whole-application or long-render leak qualification. Installed packages,
activation and service state were not changed. Distributed recovery and abrupt
power-loss qualification remain outside this follow-up.
[Repeatable verification and environment requirements](camera_delivery_reliability.md#linux-leak-checking).

## October 31 — reliability release: desktop 0.22.0 / worker 0.16.0

Checkpoint `06d4e8a` is included in release `v0.22.0`, source `805c1c9`.
Portable retained-plan validation, explicit encoder admission and verified native
queued delivery/recovery are now shipped. Exact signed/notarized desktop
installation/startup, worker publication, Registry/snapshot and Linux PC long
activation/admission/service-canary readbacks pass. Source qualification includes
Mac strict ASan/UBSan and Linux strict normal/functional checks; Linux leak
checking remains blocked by the execution environment's ptrace restriction.
The unchanged Mac event plan passes Linux; MPEG-4 passes, while unavailable
H.264 fails preflight before work. Native queue recovery is distinct from
production fleet chunk recovery and hard power-loss pair reconciliation.
[Usage, numeric policy and continuation](camera_delivery_reliability.md).
[Exact release identities and boundaries](release_versions.md).

# Camera framework qualification and render handoff

The camera framework ships as an opt-in feature in desktop 0.21.0 and worker 0.15.0. Existing scenes without a
camera rig retain the legacy camera path. See [named cameras and clocks](camera_sequence.md),
[event planning](event_camera_planning.md), and [recorded sources and delivery](sequence_sources_and_multiview.md).

## Supported contract

Every output frame has a declared presentation sample, simulation mapping,
selected camera, camera-motion time and evaluated pose/lens. A cut selects a
view without changing video duration or simulation progression. Named cameras
can use continuous, selected-time or explicit clip clocks. Event plans instead
use presentation samples independently of shot selection.

Play/Hold/Resume consumes qualified recorded states. Hold repeats the exact
recorded state while the camera can move. Supported sequence source adapters
are authenticated rigid results and complete pinned raw VF3D v1 volumes. The
rigid event planner adds focus/pivot references, reviewed event placement,
stationary or radial dolly lead, aimed Hold orbit and explicit Resume policy.
Volume targeting, particle sources and automatic contact discovery are separate
adapters; they are not implied by generic camera movement.

Native rendered sequence qualification is for integer presentation samples and
one temporal sample per output frame. Fractional camera evaluation is supported,
but rigid mesh render preparation currently refuses fractional presentation
samples, including within Hold. Do not enable shutter interpolation or motion
blur on the strength of camera-only subframe tests. M5 rest-to-rest motion has
piecewise acceleration, not a jerk-limited blend.

## Maintained gates

Use a fresh output root instead of cleaning an existing build:

```sh
make BUILD_TOOLCHAIN=clang BUILD_DIR_BASE=<fresh-build-root> \
  all ray-tracing-render-headless ray-tracing-job-runner \
  event-camera-probe camera-rig-probe rigid-result-consumer-probe \
  volume-sequence-probe scene-editor-workspace-visual-test
make BUILD_TOOLCHAIN=clang BUILD_DIR_BASE=<fresh-build-root> \
  test-camera-framework test-camera-framework-sanitize
```

`test-camera-framework` is part of `test-stable`. It covers exact sequence
clocks, strict saved-clock parsing, camera clock policies, inspection motion,
event geometry, moving leads and the MCP catalog. The sanitizer companion
checks the same C owners with ASan/UBSan.

Recorded-source acceptance needs a compact caller-supplied qualified fixture;
it does not rerun the solver. Run
`tests/integration/check_event_camera_lead_runtime.py --help` for the native
stationary/moving-event qualification inputs. Complete volume qualification uses
`tests/integration/check_volume_sequence.py`; `--skip-delivery` restricts it to
adapter checks. The retained native UI harness exercises Events, stdio MCP,
history, Save and fresh reopen at normal and compact sizes. Source hashes must
match the reviewed UI build before reusing its visual evidence.

Run the aggregate C test runner after the combined source build, along with
worker export tests and a relocated scene snapshot/native job-runner canary.
Do not interpret a local macOS job-runner result as Linux installed-worker proof.

## Release and Linux render handoff

1. Checkpoint and review the complete camera source scope in the development
   branch. Record the source commit and test evidence; do not use a version label
   as an executable identity.
2. Prepare desktop and headless worker through the existing release owner.
   Keep their versions, package digests, protocol/checkpoint compatibility and
   selected worker capabilities distinct. New saved behavior requires a worker
   containing these evaluators.
3. Verify the portable job contains its complete saved camera/sequence intent,
   meshes and immutable recorded results or pinned volume data. Do not fall back
   to the old camera if a declaration is unsupported. Preserve relative assets
   and source/dependency hashes; avoid hand-editing a resolved event plan.
4. On the selected Linux worker, run a short canary with a moving lead, Hold,
   Resume, a deliberate cut and a detached view. Compare exact frame/tick/source
   identities and pose/lens values under declared numerical tolerances. Qualify
   chunking or recovery separately if the long job uses it.
5. Review composition stills and a short preview at the intended aspect ratio.
   Set explicit FPS, frame count, source coverage, sampling and output storage
   before queueing the expensive job. A longer Hold can extend presentation;
   later physical motion requires additional qualified source states.

Uniform PTS, frame count and provenance are technical acceptance. Occlusion,
readability, cut purpose, camera speed/roll and image noise remain visual
acceptance. Keep that review distinct from a successful encoder or solver.

## October 31 release qualification

Camera checkpoint `5e1f27a` is included in release `v0.21.0` (`8be3d53`).
Fresh normal/sanitized framework tests, contained desktop/headless release gates,
Linux package self-test and the activated Linux PC long service canary pass.

The first camera-specific installed Linux preflight refused the retained
Mac-authored event proposal with `resolved pose/proposal does not match
authenticated event geometry`. The original scene and refusal are preserved.
Exact serialized proposal portability across these native platforms remains
unqualified; the runtime guard must remain enabled. Source contracts and
service-canary success do not establish portable event-plan execution, long-job
packaging, chunk recovery or human visual acceptance.

A Linux-native successor, authored by the existing proposal/select/attach owner
with the same event intent, differs in three bounds coordinates by at most
`4.440892098500626e-16` (comparison tolerance `1e-9`). That successor passes the
installed 0.15.0 renderer's unchanged exact guard and renders the edited sequence.
This demonstrates native re-authoring, while the original serialized Mac plan
remains refused. The host FFmpeg lacks `libx264`; the maintained H.264 assembly
command therefore fails there. An explicitly labelled canary-only MPEG-4 delivery
copy retains the frame/hash/timing/decode checks; it does not qualify H.264
installation or change the shipped tool.

The final installed Linux foreground canary passes both the edited and independent
views: 102 frames each at 24 FPS, exactly 17/4 seconds, exact PTS and full decode.
All 102 physical-state and 96 non-cut camera pose/lens comparisons pass, including
cuts, moving lead, exact frozen Hold bodies, Hold orbit and Resume. Maximum consumed
source tick is 290; sealed inputs and all installed package hashes remain unchanged.
This proof uses the native-authored successor and canary MPEG-4 delivery described
above. It is separate from the scheduled service canary; production queue packaging
and chunk recovery remain unqualified for this recorded-source fixture.
