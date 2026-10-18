## October 18 — current version reconciliation

Canonical and Main Edit source labels are app **0.18.0** and worker **0.12.1**,
matching their respective production records. Main Edit's water checkpoint
`60a1670` inherits the exact source commit/tree recorded for worker 0.12.1.
Proposed desktop **0.19.0** and worker **0.13.0** remain unapplied Decision-1
intent. The [version status](release_versions.md) supersedes older dated version
summaries for current attribution. Historical entries remain unchanged.

## October 18 — water transport source checkpoint

The retained clear-water, rectangular-emitter, water-normal/reflection-footprint,
pixel-area sampling, medium-aware photon and explicit wet-contact changes are
checkpointed together on `codex/ray-tracing-main-edit` at the operator's request.
Their contracts and ownership maps are linked from `docs/current_truth.md`.

The latest fresh headless/test build passed; 33 of 34 surrounding test groups
passed. `runtime_emission_transparency_adaptive_center_preserved` remains a
previously reproduced failure. The wet-contact native comparison accepted
131,071 of 131,072 photon paths; the remaining near-boundary origin case is
retained for diagnosis. Still-image acceptance is provisional, and motion review
remains open. These are source qualification results, not package release gates.

Canonical adoption and release preparation must bind this new checkpoint before
building packages. The read-only release preflight still observes canonical
`ee9e7348a9e6e01c8a64f43207e0a7475fd27b6f`, identifies a worker-adapter source
binding mismatch, and requires refreshed selected-host evidence. Its JSON output
works; the human formatter currently fails on missing `release_progress`.
A minor app release is recommended, with worker version intent selected separately.
No VERSION, installed app, worker, publication or Registry state changes are part
of this checkpoint.

## October 16 — rough-glass local source freeze

The operator accepted the 512×512 fixed-roughness still quality and authorized a
local Main Edit source checkpoint of the unified glass, deeper-path and compensated
termination work. All retained functional source matches the qualified installed
renderer input. At 32 passes and 32 transmission samples, 16/16/32 versus 32/32/64
changes selected interior/rim raw linear means by at most 0.112%. Use 16/16/32 as
the working orb preset; animation temporal acceptance remains open. Existing
videos, scenes, UI, MCP and installed app bytes are preserved. See
[qualification and remaining limits](rough_glass_qualification.md).

This is a source freeze in the persistent Main Edit branch. App 0.18.0 and worker
0.11.0 are unchanged. Installed build metadata retains its original dirty-source
build identity; checkpointing does not rebuild or relabel that package. Canonical
adoption and public release are separate. The next proposed slice is polished
mirror detail and controlled one/two/three-reflection qualification, beginning
with existing mirror fidelity fixtures. No mirror estimator change is included.

## October 14 — preview and catalog UI follow-ups

Preview Play/Pause, Loop/Bounce and Close use a dedicated shared control surface,
matching-release activation, F6 traversal and interrupted-gesture cancellation.
Native shared text/theme presentation covers preview diagnostics, frame and selected
marker readouts; normal/compact transport geometry stays inside the qualified
viewport. Timeline, scrubbing, camera evaluation and rendering remain application-owned.
Scene catalog rows expose distinguishing directory provenance with font-aware row
height and matching hit testing; paths, ordering, selection and save identity stay
unchanged. Existing resolved-path alias deduplication is preserved.

Fresh Clang application/native builds, six focused gates, aggregate regression and
three validation-enabled native acceptance modes pass, including preview interrupted
Close/step/document/host parity and surrounding editor controls/fields/save behavior.
See [preview and catalog contract](ui_preview_catalog.md). Operator and other-platform
acceptance remain separate. Versions stay app 0.18.0 and worker 0.11.0.

## October 14 — combined optical, atmosphere, MCP and shared UI checkpoint

The operator approved one coherent source checkpoint containing the retained
optical repairs, physical atmosphere transport, document and 80-tool MCP work,
and the shared UI rollout. The nine-prefix shared import at pin `1bf262d` is
checkpointed separately from application integration. Fresh Clang application,
headless and native builds, 11 focused gates including atmosphere sanitizers,
aggregate regression, 18 validation-enabled native groups, managed STL import,
MCP catalog reload and strict smoke-request checks pass. Optical snapshot isolation,
exact legacy pixels, UI/MCP history, File Save and fresh reopen retain their contracts.

The verified checkpoint is adopted into local canonical `main`; persistent Main
Edit stays available as the clean development lane. App VERSION 0.18.0 and worker
VERSION 0.11.0 remain unchanged. Committed-source development packaging is a distinct
verification gate; no public release or worker deployment is implied.

The main menu/settings, editor controls and fields, specialized panels and runtime
progress/timer HUD use the shared integration. Embedded timeline preview transport
buttons and diagnostic readouts still use local painting helpers; a small shared
presentation/control pass remains there. Canvas tools, scientific geometry and
simulation ownership remain intentional application responsibilities. Operator and
other-platform acceptance remain separate from local macOS qualification.

## October 13 — explicit glass receiver and null-interface repair

Transmitted opaque receivers in Disney V2 now use the arriving ray's view direction and the resolved material payload. Legacy receiver shading retains its previous estimator. Smooth, fully transmitting explicit IOR 1 glass with white interface tint and absorption is treated as a neutral boundary on Disney V2 camera and reflected/emitter queries when no volume is active. The trace keeps the original ray and differentials, preserving the receiver's filtering and stochastic inputs. Legacy glass, non-unit IOR, frost, partial transmission, tint, absorption, emission and authored normal response remain geometric hits.

The fixed-seed no-sphere/index-matched comparison is exact at 96x96, including a second seed with 16 temporal passes. This bounded result does not certify all glass, caustic or nested-medium transport. Saved comparison matrices retain their original renderer receipts and images; repaired proof is separate. Program and worker versions remain unchanged; Main Edit is uncommitted and canonical adoption/public release remain separate boundaries.

## October 12 — bounded glass performance repair

Main Edit reuses per-call deterministic explicit-glass transmission queries and reads one primitive seed for face metrics instead of copying the full fixed-capacity scene seed table at every hit. Legacy and rough/normal/atmosphere/caustic transport remain on the existing path. Sample accumulation retains its order. A paused matrix can explicitly qualify a changed renderer by full-settings exact completed-cell replay before resume; default renderer-drift refusal remains. Source/package/live parity and resumed-run completion are recorded separately in the private glass performance checkpoint. No analytic sphere scene primitive was added; the existing analytic sphere-lens intersection provides the geometry-cost control.

## October 11 — prepared optical snapshots and Disney V2 slice 2

The existing Main Edit now captures complete volume material/scattering tuning by
value in each prepared frame. Direct Light and Disney V2 queries read that
snapshot, including cell layout and sky budget. Static saved physical atmospheres
can use responsive asynchronous rendering; animated caches retain synchronous
selection. Disney V2 integrates existing extinction and single scattering along
camera, reflection, diffuse continuation, transmission and environment paths.
Physical profiles require caustics off and refuse unsupported integrators.
Omitted metadata and explicit legacy profiles preserve existing appearance.

The uncommitted source checkpoint remains on base `2a7a29f`; app `0.18.0` and
worker `0.11.0` are unchanged. Nine C groups, isolated native snapshot/global-edit
and concurrent-frame checks, exact legacy lookup pixels for both integrators,
analytic reflection/transmission path checks, final native UI/MCP/history and
save/fresh-reopen parity, typed request validation, headless
preflight and two-frame export pass. Saved/inspection physical-policy parity and
coefficient effects pass for both integrators. Direct Light BMP bytes exactly
match slice 1. Portable inputs are sealed and reverified after relocation/render.
See [physical smoke settings](physical_smoke_volume.md) for the supported contract.

Stop at this verified local source checkpoint. Canonical adoption, committing,
installed-app refresh, packages, workers and publication remain separate lanes.
Combined caustics, multiple scattering, measured calibration and production visual
acceptance remain open. This entry supersedes slice 1's synchronous/Direct-only
qualification boundary; the historical slice 1 evidence below remains retained.

## October 10 — saved physical atmosphere slice 1

Main Edit adds optional retained physical-smoke coefficients and sampling policy,
with Environment controls and the same MCP atmosphere commands, revision checks,
Undo/Redo and File Save. Missing optical metadata preserves legacy behavior;
explicit legacy selection keeps dormant coefficients. Field replacement preserves
placement and optics. See [saved physical settings](physical_smoke_volume.md).

Contained app/headless/native builds, codec plus ASan/UBSan, nine relevant C groups,
strict smoke-request validation, preflight and two-frame export passed. Isolated
native UI/real MCP/fresh reopen acceptance proves shared state and active-edit,
stale and typed refusals. Native Direct Light pixels match an independent prepared
frame exactly; headless saved settings match the existing inspection kernel and
legacy profiles match absent metadata exactly. Rendered coefficient effects,
unsupported/conflicting requests and sealed portable relocation are covered.

This is a verified **uncommitted Main Edit source checkpoint** on base `2a7a29f`.
VERSION remains 0.18.0; current source WORKER_VERSION remains 0.11.0. Canonical,
installed desktop and worker packages are separate. Physical saved atmospheres
use synchronous selection and require Direct Light with caustics off. Immutable
prepared optical snapshots, Disney V2 transport and visual/calibration acceptance
remain future work. A minor public release is recommended; approval is required
before any version bump or package/release action.

## October 10 — denoise and dielectric transport source checkpoint

Implementation checkpoint `ee9ace0` preserves qualified denoise uncertainty
handling, bounded pixel/transport diagnostics, camera boundary/medium response,
TIR and radiance transmission repairs, and photon receiver/estimator accounting.
The receiver observer uses a fixed 8208-byte table rather than image-sized storage.
See [transport qualification](transport_weight_audit.md),
[photon mapping](photon_mapping.md), and
[denoise guidance](denoise_material_guidance.md) for behavior and limits.

Fresh contained Clang builds, 23 receiver/estimator checks, 39 camera dielectric
checks, 15 affected regression groups, preparation-capture integration and matched
native images passed before checkpointing. The commit pass checked retained source
identities, compiled the transport diagnostic and passed staged whitespace checks.
No implementation changed during checkpointing; the unrelated full GUI suite was
not rerun. The local 1280x720, 32-pass still runs from its retained qualified
executable; completion and high-resolution visual acceptance remain separate.

VERSION 0.18.0 and Main Edit WORKER_VERSION 0.9.0 remain unchanged. Canonical and
its release-owned changes are preserved. This local source checkpoint leaves
canonical adoption, package/release qualification and worker delivery as separate
boundaries. A minor release is recommended for the accumulated runtime work;
approval is required before a version bump.

## October 9 — verified Main Edit reconciliation

The local source checkpoint preserves and commits physical-smoke Direct Light
optics, Disney V2 material-guided denoise, content-addressed mesh caching and
portable snapshot storage, plus bounded photon transport/diagnostic repairs.
Mesh-loader fixtures and retained-scene staging no longer place a 54 MiB asset
set on the platform stack. The standalone atmosphere-optics math remains a
numerical reference; no renderer or editor consumes that module yet.

Fresh contained app/headless/test builds, 23 affected C groups, 12 surrounding
regression groups, mesh-pack contracts, atmosphere math with ASan/UBSan, 10
workflow and three storage tests, strict smoke-request gates, photon preparation
with 15 negative cases, and ordinary native preflight/two-frame export pass.
The full unrelated suite and new GUI/Linux qualification were not run for this
source reconciliation. The Aquarium lane's ongoing four-sample visual comparison
remains separate from source acceptance.

VERSION remains 0.18.0 and Main Edit WORKER_VERSION remains 0.9.0. Released worker
0.10.2 is a separately qualified package; its denoise source matches this lane,
but that does not qualify the newer smoke/photon/storage changes remotely.
Canonical source and installed app are unchanged. A minor public release is
recommended for the accumulated features; approval is required before a bump.

Next renderer focus: connect the headless physical-optics contract to saved,
editable atmosphere settings and qualify Disney V2 transport; preserve legacy
scene appearance; verify sky/direct-light step invariance and visual presets;
then prove native simulation-cache playback/export. Physical smoke currently
requires Direct Light and caustics off. Multiple scattering, calibrated soot and
combined smoke/photon transport remain separate work.

# RayTracing Main Edit Worktree

## October 4 — atmosphere authoring usability repair

Environment again defaults to geometric editing. Atmosphere resources expose
loaded cache/frame/size, selectable bounds and a density guide; Inspector/gizmos
edit centre, rotation and size through retained commands. Initial attachment fits
visible scene geometry; replacement preserves placement and appearance. Native
Direct Light preview stays manual/off by default. See [atmosphere editing](atmosphere_attachment.md).

Complex-room native creation/selection, XYZ transforms, cancellation, shared
Undo/Redo, File Save and fresh-process reopen pass, alongside existing cache,
MCP, water, timeline and headless export checks. Local package delivery is a
separate checkpoint recorded in private work status. Preserve the running user
scene; do not silently replace or close its unsaved session.


## October 4 — retained atmosphere development checkpoint

Main Edit merged the released 0.18.0/worker 0.9.0 source baseline at ca73660,
preserving the newer scene-workflow commits. Retained atmosphere UI/live MCP,
static presets/import, actual cache-ID playback and existing water sidecars now
have native/headless acceptance. See [atmosphere attachments](atmosphere_attachment.md).
File Save/fresh reopen, identity refusal, raw/pack metadata, portable sealed
exports and exact relocated images pass. UI/MCP share history and validation.
PhysicsSim source and solvers remain untouched.

Environment now has a progressive native scene-camera preview and the shared
timeline. Seek, density edit/Undo, exact native pixels and playback acceptance pass.
Placed bootstrap/lens-transport volume caches now pass numerical/sanitizer and
native render checks; photon-volume placement remains explicitly refused pending
its separate beam-normalization work. Local package identity and human
complex-scene usability remain separate evidence. No new VERSION, canonical adoption or public
release is implied. The current installed build identity is recorded in private
work status after local refresh; older dated identities below are historical.


## September 30 — stable orientation and shared playhead acceptance

Five bounded slices now provide deterministic transported route frames, the same
evaluated object pose across all five editor workspaces, editable start-up/end-roll
handles with undo and persistence, and explicit camera route/stable-focus modes
with full forward/up propagation into preview and final rendering. See
[movement paths](motion_paths.md) for controls, compatibility and limits.

Clang full rebuild, numerical/ASan/UBSan, native handle/undo/File Save/reopen,
combined three-object/camera/light seeking across five workspaces, camera projector
parity and headless target/roll image checks pass. Existing targeted timeline,
preview and picking regressions pass. The unrelated full renderer/material/worker
suite was not rerun. Human aircraft-scene acceptance remains separate. Automatic
banking, intermediate roll markers and closed-loop seam correction are deferred.
VERSION and WORKER_VERSION remain unchanged; no canonical adoption or release.

## September 30 path usability checkpoint

The six path UI slices and bounded native/render regression audit pass. See
[movement paths](motion_paths.md) for the compact library, contextual follower
inspectors, XYZ gizmo, smoothing and selected-target planning workflow.
Source and installed identity are recorded in private work status after delivery.
Human usability acceptance remains separate; no VERSION or canonical change.

## M5.4–M5.6 source and behavior acceptance

M5 now has explicit retained planning for object, camera and light route followers.
Curved/corner limits, waypoint holds/fixed arrivals, Apply/Restore, dependency
revalidation, persistence, arbitrary seeking and combined rendering are integrated.
See [planned route timing](motion_plans.md) for use and boundaries.

Clang build, numerical/sanitizer gates, native prepare/reopen and unit/clock/cache
audit pass. Seven native/headless poses match exact independent baked renders;
six frozen legacy images remain unchanged. M2 restoration and M4 combined/trail
checks, timeline contracts and headless preflight/image export are retained gates.
The final Main Edit refresh/installed identity is recorded in private work status.
VERSION 0.17.0 and WORKER_VERSION 0.8.0 are unchanged; no canonical adoption or
publication is implied. Earlier dated checkpoints below are historical.


## M5.4 detached curved-route checkpoint

The curved-route adapter now verifies length/curvature bounds, explicit corner
stops, singular endpoint rest bounds, and conservative total acceleration limits.
XYZ sampling and numerical/sanitizer tests pass; see
[curved-route planning](motion_route_planning.md). M5.5 retained UI/persistence
integration and M5.6 final live acceptance remain active work. Installed Main
Edit remains M4 f2491a1 until that integration is verified.


## September 29 M5.3 detached scheduling checkpoint

Arrival/hold/departure scheduling now supports fixed-time anchors, preserved
waypoint speeds, zero-speed reversals, feasible slack distribution and explicit
conflict readback. Fixed-time profiles and deterministic seeking pass numerical
and sanitizer checks. See [scheduling contract](motion_timing_schedule.md).
This remains detached: existing saved scenes, runtime evaluation and installed
M4 app are unchanged. M5.4 curved/corner route bounds are the next separate slice;
M5 is not complete.


## September 29 M5.1–M5.2 detached foundation checkpoint

Main Edit extends clean M4 f2491a1 with a detached straight-route timing planner
and explicit compatibility/limit contract. Numerical, sanitizer, native/headless
build and existing timeline contract checks pass. No live evaluator or UI consumes
the planner yet; the installed app remains M4 f2491a1, intentionally unrefreshed.
See `motion_timing_planner.md` and private work status for the source checkpoint.
M5.3 arrival/hold/departure scheduling is next under separate authorization.


## September 29 M4 source completion checkpoint

M4 spatial policies, temporal smoothing, movement feedback and derived editable
XYZ trails are implemented and pass native/headless, compact UI, persistence,
combined follower/focus and M1–M3 regression acceptance. See `motion_paths.md`
for compatibility and limits. The final audit preserves dormant corner handles
when splitting straight segments and labels raw scalar distance rates accurately.

Source checkpoints: `7962edd`, `47a3260`, `8335e22`, `4752090`, followed by the
completion audit commit containing this entry. Final commit and Desktop identity
are recorded in the private work status after refresh. Canonical stays at
`286f866`; VERSION 0.17.0 and WORKER_VERSION 0.8.0 remain unchanged. No release,
canonical adoption or remote operation. M5 is the next separately authorized
implementation boundary; human hands-on acceptance remains separate.


## September 29 M3 completion audit

M3 implementation and automated acceptance are complete in Main Edit. Explicit
camera/light legacy conversion copies cubic or degree-elevated quadratic XYZ
geometry, fits independent route timing, preserves scene/source identities and
refuses unsupported fits atomically. Camera focus is an explicit saved binding
option applied after final translation; default-off preserves old output.
Combined mesh/camera/light followers retain independent timing, FOV/intensity,
source restoration, undo/redo and fresh-process persistence.

Build: Clang native/headless plus foundation/entity/view/navigation contracts pass.
Targeted: cubic scale-2 and quadratic scale-0.5 conversion, capacity/stale-source/
subframe-easing refusals, combined followers, focus edge cases and compact-window
controls pass. Render: four converted frames meet pose/pixel tolerance and four
combined frames match independent baked references exactly for each curve mode.
Six frozen legacy and eight prior camera/light attachment images are unchanged.
Registry/snapshot/evaluated-preview/light-motion/light-persistence groups and
fresh M2 restoration/edit-targeting prepare/reopen pass.
The unrelated full renderer/material suite was not rerun for this bounded slice.
Evidence: `build/dm3-accepted`, `build/dm3-completion-quadratic`,
`build/dm3-legacy-final`, and `build/dm3-*.log` in Main Edit.

Conversion limits: 2–32 points, 4096 frames, 128 integer-frame keys; positional
bound `1e-4 * max(1, route length)` in authored units, tested at integer/eighth
frames, not a continuous analytic bound. Existing route timing is never replaced.
One main camera and the existing animated-light slot are supported. Focus uses
an existing authored scene target. Human hands-on usability acceptance remains
separate. M4 smoothness is next; M5 and original E/F remain later. No version,
canonical adoption, publication, or remote operation is part of this closure.
A minor release is recommended; VERSION remains 0.17.0.

## September 29 M3 animated-light attachment checkpoint

The clean camera checkpoint `7877cc8` is extended in the registered Main Edit
lane with explicit attachment for the existing animated light. Route timing is
independent of intensity; rebind preserves the predecessor, and detach restores
it exactly. No legacy path conversion or additional animated-light slots.

Clang/native/headless builds and foundation/entity/view/navigation contracts
pass. Native attach, animated intensity, hold, rebind, invalid target/source and
ownership refusal, detach, undo/redo and fresh reopen pass. Four light frames
match independent baked references exactly. Camera reopen/render parity, M2
restoration/edit-targeting, and registry/snapshot/evaluated-preview groups pass.
Evidence: `build/dm3-light-accepted` and `build/dm3-light-*.log`. The full suite was
not rerun for this bounded slice. Commit and Desktop identity are recorded in
the private work status after refresh. Canonical stays at the reconciled M1/M2
baseline; VERSION and WORKER_VERSION are unchanged. Next M3 boundary: explicit
legacy conversion and combined-follower acceptance; human acceptance is separate.

## September 29 first M3 camera attachment checkpoint

Canonical and Main Edit were clean and aligned at `286f866438bf` before M3
implementation. This first slice adds explicit main-camera attachment to reusable
XYZ routes with independent route timing. Saved position drivers are restored on
detach; their orientation evaluation and authored yaw, pitch and FOV remain
independent. Legacy scenes retain their prior evaluation path.

Build and foundation/entity/timeline/navigation contracts pass. Native camera
attach, timing, detach, undo/redo and fresh-process reopen pass. Four headless
frames exactly match baked camera poses; six legacy camera/light frame hashes
remain unchanged. Object M2 attachment and restoration regressions pass. Evidence
is retained in `build/dm3-camera-final`, `build/dm3-legacy-render`, and the
`build/dm3-*.log` files. The full repository suite was not rerun for this bounded
slice. Light attachment, explicit legacy conversion, combined followers and
focus-target acceptance remain later M3 work. Main Edit retains this development
checkpoint; canonical remains the accepted M1/M2 baseline. A committed-source
Desktop Main Edit refresh follows, with identity recorded in private status docs.

## September 29 movement checkpoint and M3 entry

The user authorized reconciliation of the accumulated D1/M1/M2 development work,
a clean source checkpoint and continuation into M3. The M1/M2 checkpoint includes
reusable object paths, guarded gestures, exact prior-XYZ restoration and field
edit targeting repairs. Evidence is in docs/motion_paths.md and the retained
build/dm2-repairs logs. Focused build/contracts, native repair/reopen/gesture and
headless pixel parity pass; the full repository suite was not rerun. Hands-on
acceptance is not inferred from authorization to continue development.

Local canonical adopts this verified Main Edit baseline by fast-forward before
M3 starts. VERSION stays 0.17.0 and WORKER_VERSION stays 0.8.0. The installed
Main Edit package still records its precommit dirty-source identity until the
next development refresh. No release, Registry promotion, remote push or
production app replacement is included. M3 remains in the same registered lane;
first converge camera position through explicit bindings while preserving legacy
sampling and orientation/FOV. Light convergence and explicit conversion proof
follow in bounded slices; M4/M5 remain later work.


## September 27 D1 workflow repair

The retained Main Edit lane repairs the operator-reported object visibility and
setup/save workflow gaps. XYZ editing is grouped and explicit; Render meshes
remain visible in light-path mode, object tracks synchronize selection, and
Frame selected uses the evaluated translation. Canonical adoption remains at
c601453. This local development refresh changes no VERSION, Registry or release.
Verification details and the packaged-reopen limitation are in current_truth.md.

## September 27 timeline adoption and D1 continuation

The operator accepted the first camera/light timeline UI after testing playback
and scrubbing. Canonical main fast-forwarded from `cc158c1` to verified `c601453`;
both lanes were clean at adoption, with VERSION 0.17.0 unchanged. The registered
Main Edit lane now owns D1 object position animation. D1 is a separate development
checkpoint, not included in that adoption. No release, Registry promotion, remote
push or production app replacement is implied.

## September 26 scene-authoring adoption checkpoint

The bounded U2.4 Add/import/placement and Inspector-delete baseline is committed
in registered Main Edit at `041a4aa` before documentation closeout. Canonical
`main` was a clean ancestor at `f3346f6`, with nine Main Edit-only commits.
Canonical then fast-forwarded to the documentation closeout tip; both Git lanes
are aligned at this source baseline.
Foundation A, pane-host, runtime-bridge, navigation and mesh-pick contracts pass;
the copied skull-scene native workflow and committed Main Edit package self-test
pass. The Desktop Main Edit app identifies `main-edit-0.17.0-041a4aa82b6a` and
the operator reports improved import placement. This adoption records a usable
source baseline, not completion of library placement or Foundation B. Retain
the registered Main Edit worktree for later feature work. Canonical source
adoption does not replace the production app or advance VERSION, release, or
Registry state.

## September 24 local proof-history archival

The ignored `build/editor_ui_recovery/` proof tree is no longer retained in this
worktree. Its 5,980 files were archived as batch
`ray-main-edit-editor-ui-recovery-20260924a` on the Linux PC cold archive under
`generated-runs`; export item
`20260925T001215Z--ray-main-edit-editor-ui-recovery-20260924a` contains the
independently restorable `tar.zst`, source manifest, checksums, and restore notes.
The archive payload SHA-256 is
`48ca9fcf71c04816e63a5beb998c96b78e7584ee88c9c49f44dc439fa82d363c`.
Historical `build/editor_ui_recovery/...` paths below refer to paths inside that
archive, not current laptop-local files.

The remainder of ignored `build/` was subsequently archived as batch
`ray-main-edit-remaining-build-products-20260924a` in the same cold-archive
`generated-runs` lane. Its 3,276-file restore archive has SHA-256
`2767b786a3c5750a021994a7cca4f6b6e4c21f42b8cc6f6d0de477b578813384`.
Accordingly, every historical `build/...` evidence path in this document is now
archive-relative until regenerated by a current verification run.

## September 24 canonical evidence archival note

Canonical surface-material adoption receipts previously retained under ignored
`build/` paths are now in cold-archive batch
`ray-canonical-surface-material-adoption-evidence-20260924a`. Historical release
and authenticated-release build trees are in
`ray-canonical-historical-release-builds-20260924a`. Historical `build/...`
references below are archive-relative unless a fresh local run regenerates them.

## September 22 registered-lane reconciliation (local source only)

This entry supersedes earlier material-lane usage of “Main Edit” where the
actual command target was canonical `ray_tracing`. The three checkout identities
are distinct, even when their commits match:

| Lane | Local path | Branch | Verified HEAD |
| --- | --- | --- | --- |
| Canonical | `<workspace>/ray_tracing` | `main` | `97f0064a23773dbe512dd7f857fee8c37f10327a` |
| Registered Main Edit | `<workspace>/_worktrees/ray_tracing_main_edit` | `codex/ray-tracing-main-edit` | `97f0064a23773dbe512dd7f857fee8c37f10327a` |
| Material specialist | `/Users/calebsv/.codex/worktrees/d786/ray_tracing` | `codex/surface-material-m0` | `97f0064a23773dbe512dd7f857fee8c37f10327a` |

The material adoption commands and receipts through T4 and the adaptive fixture
cleanup targeted canonical, not the registered Main Edit worktree. Their build
and test evidence remains valid for the source identities and paths actually
recorded; it is not evidence that the registered checkout or installed app had
already been refreshed. Historical “Main Edit adoption” wording in those material
closeouts must be read with this correction.

Under the explicit bounded node-B authorization, registered Main Edit now
fast-forwarded from `c8f64772d0953f82d56374ee399e47d18ea44d0e` to the accepted
canonical `97f0064a23773dbe512dd7f857fee8c37f10327a`. Before mutation, all three
checkouts were clean; registered Main Edit was 23 behind and zero ahead, with
ancestry verified and no unique commits. Process command/cwd and open-write-handle
checks found no RayTracing writer, build or render conflict. The observed `make`
process belonged to the unrelated DAW checkout. Retained tool-host processes in
the specialist cwd were not RayTracing builds or writers; the UI coordinator task
was not running. No process was stopped. These are point-in-time checks and must
be repeated before later mutation.

App `VERSION` remains `0.16.0`; `WORKER_VERSION` remains `0.7.1` in every lane.
No commit was created by this reconciliation. Canonical and specialist branch
heads did not move. This runbook correction is the only intentional uncommitted
change, in canonical `docs/main_edit_worktree.md`; the registered and specialist
checkouts remain clean at the accepted commit. It is not yet committed or copied
into those other worktree files. Preserve this owned documentation change during
subsequent release planning; do not misclassify it as source implementation drift.

Source-bound evidence remains in canonical:

- `build/surface_material_t4_cleanup_adoption/closeout.json`: cleanup source
  `1f49771`, documentation `332abfc`, fixed memory gate and parallel diagnostics.
- `build/adaptive_regression_cleanup/closeout.json`: `97f0064`, test-only closure
  of all 14 stale convergence assertions, affected and adjacent suites passing.
- [T4 cleanup contract](surface_material_t4_contract.md#t4-cleanup-and-stop-before-t5)
  and [adaptive fixture closeout](surface_material_t4_contract.md#adaptive-regression-fixture-closeout).

No tests were rerun for this exact fast-forward, and no registered-worktree build
or installed-package acceptance is inferred. Ignored evidence was retained.
Human material/interaction acceptance and acceptance of the chosen installed
package remain unresolved. No version decision, release build, app launch,
replacement, publication, Registry mutation or remote work occurred. T5 stays
paused. The coordinator owns shared handoff and subsequent release preparation.


## September 20 UI source closeout

The current source includes the compact document bar and pane headers, shared
viewport display selector, geometry-based Scene/Material picking, compact object
list, themed panel hierarchy, material disclosure inspector, and undoable object
rename. Material display adds a cached procedural/studio approximation; it is
not final-render mapping parity. The eight-case diagnostic exposes physical
coordinate/seed differences and missing viewport face overrides. See
[Editor workspace](editor_workspace.md), [parity evidence](material_viewport_parity.md)
and [next surface-mapping lane](surface_material_mapping_plan.md).

The operator authorized committing this work and fast-forwarding main without a
version update. VERSION remains 0.16.0; WORKER_VERSION remains 0.7.1. This is source
closeout, not a package release or a claim that all E2–E7 functionality is done.
Earlier dated checkpoint/adoption statements below are historical.

Closeout validation: Clang application/test build; foundation/document and managed
mesh tests; pane, runtime bridge, navigation, viewport bridge, pick/scroll,
outline, shading, primitive and startup-discovery gates all pass. Native acceptance
at `build/editor_ui_recovery/ui-closeout-20260920/acceptance.json` passes, including
rename, workspace/picking, transforms, persistence and fresh headless render.
Reference render SHA-256 remains
`c46886820bca87c35c59892929d4f7db59549dc3903af89a3ff2a2eb0c7d793d`.
The full stable suite was not rerun; older broad-suite failures remain historical
unresolved evidence, not a current all-green claim. The material-parity diagnostic
is intentionally retained as a failing-fidelity baseline. Ignored build outputs
are retained. No installed app was closed or rebuilt for this source closeout.



## Document bar and pane header reorganization

Main Edit replaces three global toolbar rows with File/Edit/View and document
identity above a pane-local workspace selector. Scene transform tools and
Material In scene/Object controls live in the center header; Add lives with the
Scene list. Pane separators are visible and feedback moves to the bottom.
See [Editor workspace](editor_workspace.md) for commands, limits and source
verification under `build/editor_ui_recovery/menu-reorg-final/`. No package refresh
or canonical adoption is included in this source pass.

## U2.3 selection, Scene list and Inspector checkpoint

The Scene tab now lists retained document objects, including hidden objects,
with readable fallback names, type labels and separate View/Lock controls.
Assets remains the secondary tab. Search accepts names, stable IDs and types;
indices are no longer user-facing identities. Rows retain clipped hitboxes and
visible-row rendering with the existing kit_ui scrolling helpers. New row and
sticky-identity text storage remains valid throughout the render frame.

A sticky Inspector header shows the name, readable type and hidden/locked state.
Details exposes the stable ID and the single-selection limit. Transform,
Material, Geometry / Surface and Visibility sections appear where applicable;
there are no inert Rendering/Advanced property groups. Hidden geometry must be
shown before geometry/transform inspection; its identity, flags and stable-ID
rename remain available through document readback/commands. Multi-selection,
multi-edit, temporary isolation and new Rendering properties are not implemented.

Selection in a retained runtime document is stored by object_id. Runtime indices
are resolved adapters; legacy non-document scenes retain their existing tracker.
Outliner and viewport selection converge on that identity, which survives
filtering, document rehydration and workspace changes. Hiding an object retains
selection with runtime_index=-1; deleting it clears selection on readback.
Document close resets selection. Material focus cannot fall back to another
runtime slot while a hidden object is selected.

Existing flags.visible and flags.locked are persisted without a schema migration.
Absent flags default to visible/unlocked. A changed flag is one retained command;
identical values are no-ops. Unknown fields, including unknown flags, survive.
Visibility excludes objects consistently from primitive, mesh and curve imports,
so it affects viewport picking and final rendering, not opacity alone. Hidden
objects remain in the Scene list and can be shown again. This corrects the older
primitive bridge behavior that merely reduced opacity.

Locked objects remain selectable and inspectable. Retained transform, rename,
material assignment, duplicate/remove and shading mutations reject them; the
object/material editing facades and selected-object motion controls also enforce
locks. Visibility requires unlocking first; unlock, Undo and Redo remain allowed.
Locks are editor protections, not file permissions or protection against another
program rewriting the scene. Unrelated Add/import remains available.

`scene_editor_document_objects.c` owns document object readback and flag/name
commands through the existing private transaction seam. The focused
`scene_editor_object_commands.c` exposes revision-checked select/rename/visibility/
lock execution and semantic selection/revision/dirty/history readback. No public
CLI/MCP transport or second history system is added. Shared kit_ui/font/render
contracts are reused; no shared API, adoption minimum or VERSION changes.

Evidence is retained under ignored `build/editor_ui_recovery/u23-closeout/`.
Acceptance includes actual row selection/lock clicks, stable-ID selection parity,
rename, lock rejection, primitive-hide index remapping, hidden selection across
Material/Scene, no-op/stale commands, Undo, flag Save/fresh reopen, existing
transform/material/Add/import behavior, window-size captures and headless render
comparison for a hidden mesh. Focused editor, mesh-loader and curve material
regression gates supplement native acceptance. Screenshots are source GUI proof;
operator visual acceptance and installed-package acceptance remain separate.

This checkpoint stops before U2.4. The existing U2.2 Local-axis mathematics and
numeric label-dragging gaps remain. No packaging, Desktop refresh, canonical main
adoption, release or Registry operation is part of U2.3.

## September 19 U2.2 transform ergonomics checkpoint

The Scene tool row now exposes Move, Rotate and Scale alongside World/Local
orientation state and an explicit Snap toggle. Keyboard access is mnemonic for
this editor: `Q` Select, `W` Move, `R` Rotate, `E` Scale and `F` Frame selected.
The complete binding and focus rules are recorded in `docs/KEYBINDS.md`.

Snapping is opt-in and quantizes Move to 0.1 scene units, Rotate to 15 degrees
and Scale to 0.1 factors. Scale adds an `All` handle that previews and commits
all three components in one document command. Active gestures show a compact
measurement beside the grabbed handle while retaining the bottom status line.
World/Local is currently explicit presentation/tool state; local-axis basis math
is reserved for the next transform extension and is not claimed by this checkpoint.

Inspector fields retain click-to-type exact entry and now add copy/paste plus
double-click reset (0 for position/rotation, 1 for scale). Each completed drag or
reset is one undoable command; Escape and existing context-change cancellation
remain exact. Label-drag numeric scrubbing is still open and is not claimed here.
The native copied-scene acceptance covers shortcut selection, visible transform
state, snapped rotation, uniform-scale preview/commit/undo, existing XYZ gestures,
Save/fresh reopen and final headless rendering.

## September 19 U1.1/U1.2 ownership and stop

The coordinating task explicitly transferred sole source-writer ownership of the
bounded selection/live-move slice to the retained UI-overhaul task. Start state
was clean `bf9811dc693e083a39f10a32f63ef2f3d555914f`, six ahead of canonical
`752d12b`, with both versions at `0.16.0`. The source slice stops at one focused
commit after the forced rebuild and copied-scene acceptance described in
`docs/editor_workspace.md`. The implementation writer stops after reporting the
checkpoint back; U1.3 and later work need their own sequential handoff. The installed
Desktop package remains at the prior head and has not been refreshed or operated.

## September 16 editor checkpoints

Main Edit committed the September 12 lifecycle/Add/material corrections as
`e6ff6bb`. The following selected-object move-gizmo source slice is limited to
the Scene workspace and copied-scene verification; operator visual acceptance
and canonical adoption remain open. Read Git state before using this historical
summary for a later handoff.


## September 12 usability review boundary

Lifecycle and selected-object workflow corrections are implemented in retained
Main Edit. The new pinned copied-scene development review is under
`build/editor_ui_recovery/usability-review-final/`; it has not been launched for operator
review. Source and native acceptance evidence are in `docs/editor_workspace.md`.
The installed app/render and canonical main were not operated or replaced. E1
remains open for visual acceptance.

## Historical: September 11 UI recovery review boundary

The initial E1 layout was rejected and the phase reopened. The compact recovery
is ready for another operator visual test; details and proof are in
`docs/editor_workspace.md`. The separate source review app is under ignored
`build/editor_ui_recovery/live-review/`. Existing installed apps and canonical
source have not been replaced. E1 remains open until operator acceptance.

## Historical: September 11 E0/E1 first review boundary

E0/E1 source implementation and automated acceptance are complete in Main Edit;
operator visual acceptance is next. The isolated acceptance driver and interactive
review mode are documented in `docs/editor_workspace.md`. The review runs a copied
scene with its own configuration and a pinned local binary. It does not replace
or close the installed Main Edit app. Canonical main remains at `752d12b`; source
adoption, Desktop refresh and release work are separate decisions.


## September 11 source alignment and E0/E1 entry

The operator requested checkpointing existing Main Edit work and fast-forwarding
local main before starting the editor overhaul. Foundation A was committed as
`752d12b065c430b72d18a4182cb6d7a4d2ce1b9f`; both source lanes were verified clean
and aligned at that commit before the new slice. Existing ignored outputs were
retained. This source adoption does not establish package or full GUI acceptance.

The subsequent bounded Scene workspace slice belongs in Main Edit; canonical
remains at the aligned foundation baseline until separately adopted. See
`docs/editor_workspace.md` for tests and open acceptance. Read fresh Git state
before resuming; historical snapshots below are not current lane identity.

Last verified: 2026-09-04

This is the RayTracing pilot runbook for the CodeWork Persistent Main-Edit
Worktree Contract (`MEW1`). Public clones can use this document without the
private CodeWork scaffold reference.

## Lane Roles

| Lane | Convention | Purpose |
| --- | --- | --- |
| Canonical source | `<workspace>/ray_tracing`, catalog branch `main` | Accepted source and source-level `VERSION` |
| Main Edit | `<workspace>/_worktrees/ray_tracing_main_edit`, branch `codex/ray-tracing-main-edit` | Persistent functional-development integration |
| Canonical app | `~/Desktop/optiC.app` | Canonical local package/release review |
| Main Edit app | `~/Desktop/optiC Main Edit.app` | Isolated local visual review of edit-lane source |

Other specialist RayTracing worktrees may coexist. Do not repurpose or remove
them to satisfy this runbook.

## Current Pilot Capability

RayTracing implements:

- separate `optiC Main Edit.app` bundle and display identity
- bundle identifier `com.cosm.optic.main-edit`
- separate `RayTracing-Main-Edit` runtime and log namespaces
- development distribution root `dist/dev/main-edit`
- embedded `Contents/Resources/build_identity.json`
- branch, commit, dirty flag, tracked/untracked source fingerprint,
  architecture, toolchain, binary digest, and build-time readback
- package failure if source changes during assembly
- isolated fake-home package self-test
- a refresh target that cannot overwrite `optiC.app`

The development app is local visual-test evidence. It is not a public package,
release candidate, Registry record, deployment, or activation.

## Start Or Resume Readback

Run from the CodeWork workspace:

```bash
git -C ray_tracing status --short --branch
git -C ray_tracing rev-parse HEAD
tr -d '\n' < ray_tracing/VERSION; printf '\n'
git -C ray_tracing worktree list --porcelain
git -C _worktrees/ray_tracing_main_edit status --short --branch
git -C _worktrees/ray_tracing_main_edit rev-parse HEAD
git -C ray_tracing rev-list --left-right --count \
  main...codex/ray-tracing-main-edit
```

Record canonical identity, Main Edit drift, ahead/behind counts, and worktree
ownership before editing. Stop on an unexpected branch, missing worktree,
ownership conflict, or unknown dirty path. Never reset or clean first and
investigate later.

## Continue Functional Work

1. Work in `<workspace>/_worktrees/ray_tracing_main_edit`.
2. Keep unrelated dirty and untracked files unchanged.
3. Implement one bounded behavior or proof boundary.
4. Run `git diff --check` and focused affected tests.
5. Commit an owned checkpoint when the boundary is coherent.
6. Run broader renderer or package gates in proportion to risk.
7. Use the isolated app only when visual review is needed.

For packaged visual behavior, use:

```bash
make -C _worktrees/ray_tracing_main_edit \
  package-desktop-main-edit-self-test
make -C _worktrees/ray_tracing_main_edit \
  package-desktop-main-edit-refresh
```

Opening the GUI is a separate explicit boundary:

```bash
make -C _worktrees/ray_tracing_main_edit \
  package-desktop-main-edit-open
```

Do not silently close or replace a running app.

## When Canonical Main Moves

Canonical `main` may receive narrow source-local release-control, packaging,
version, or documentation commits while Main Edit remains active.

Before merging those commits into Main Edit:

1. checkpoint or otherwise protect the owned Main Edit change set
2. inspect both commit ranges
3. verify main-only commits belong to the expected canonical lane
4. merge `main` into `codex/ray-tracing-main-edit`
5. resolve in Main Edit and rerun affected tests

Unexpected renderer or product-behavior work on canonical is not a routine
executive merge. Stop and establish ownership before combining it.

## Adopt Main Edit Into Canonical

Adoption begins only after the selected Main Edit scope is committed and its
required source, package, and visual gates pass.

1. confirm both lane identities and ahead/behind counts
2. merge current canonical into Main Edit if canonical moved
3. run final focused and broad gates in Main Edit
4. adopt the verified Main Edit tip into canonical
5. independently read back canonical commit, version, and cleanliness
6. make the version decision separately
7. use Release Control and Production Registry only through their own
   authorized workflows

Fast-forward is preferred when canonical is an ancestor of Main Edit. A
reviewed integration merge is valid when legitimate canonical drift exists.

## Retain Or Recycle Main Edit

Normally retain the named Main Edit lane after adoption. Recycle it only when
there is a concrete topology or operator reason.

Before recycling, prove:

- the worktree is clean
- no untracked user-owned bytes would be lost
- all retained commits are reachable from canonical or a retained ref
- ignored outputs have a retention or rebuildability decision
- no process owns the checkout or development app
- the old branch tip and adoption result are recorded

Never force-remove, clean, or destructively reset an active dirty Main Edit
worktree to reclaim the name.

## 2026-08-27 Status Boundary

At the last verification, canonical `main` was clean at source version
`0.15.0`. The Main Edit lane was ahead of canonical and had active uncommitted
mirror-transport source/test work. Therefore continuing bounded work in Main
Edit was valid, while recycling it was not. Any later integration decision
requires a fresh live readback.

## 2026-09-04 Retained-Lane Readiness

The readiness pass reconciles the committed portable fisiCs units change
(`a8386f0`) with canonical's Disney-v2 documentation correction (`e843cab`).
The integration checkpoint is `ec17542`; both lane versions remain application
`0.16.0` and worker `0.7.1`. The existing named worktree is retained for the next
functional-development cycle, with canonical adoption and final cleanliness
read back separately after verification.

Fresh validation of the integration checkpoint passed:

- clean Clang application, headless-render, and material-preview builds
- all 13 app-local fisiCs semantic-dump targets, each with zero semantic errors
- 74 of 80 registered C test groups, executed individually
- headless preflight, image export, mesh-asset spheres, material preview, and
  source first-frame visual proof

The broad C suite is not fully passing. Six groups fail identically in a fresh
source snapshot of the preceding canonical commit and in the integration:

- `runtime_scene_3d_geometry`, `runtime_mesh_asset_loader`, and
  `runtime_preview_editor` terminate in the macOS stack-check path
- `runtime_emission_transparency` reports 17 assertion failures
- `runtime_native_3d_render` and `runtime_native_3d_render_live` each report the
  same two assertions concerning visible emitter and environment brightness

These are inherited baseline exceptions, not successful tests. Keep their
repair as a separate bounded follow-up; do not describe this readiness pass as
a full `test-stable` pass. Run the isolated package self-test for the exact
final committed source identity before canonical adoption.

Git cleanliness and artifact cleanliness are separate. The readiness pass
preserves old render proofs, packages, and fresh validation outputs in an
ignored archive outside Main Edit before leaving its generated roots empty.
`make clean` removes compiler output but leaves `build/agent_runs/`, package
self-test output, and `dist/`; an artifact-free checkout therefore requires a
path-specific retention/archive decision. Future builds recreate those roots.

The installed Desktop Main Edit app has its own embedded identity. Source
integration and worktree cleanup do not refresh or close that app; read its
identity before using it as visual evidence for a new source checkpoint.

### U1 startup-discovery acceptance correction (2026-09-19)

The `2475269` editor candidate exposed a normal macOS app-launch blocker that
package self-test did not exercise. A disposable package launched with `open -n`
and an isolated home reproduced `opendir` stalling at the saved input root,
`/Users/calebsv/Desktop/Simulations/scenes/`. The same binary launched from the
terminal completed discovery. After removing that main-thread scan, sampling
found a second startup scan in `menu_batch_panel_refresh`, counting frames under
the saved external `frameDir`. The launch-context difference is observed;
macOS privacy/filesystem mediation is a hypothesis, not a proven OS diagnosis.

Menu initialization now requests asynchronous scene/volume discovery and frame
summaries. Each root has independent copied inputs; the main thread polls completed
snapshots. Forty process-lifetime slots bound outstanding work, repeated refresh
reuses a blocked matching scan, obsolete results are ignored, and shutdown does
not join filesystem calls. Completed slots are reclaimed on refresh. Frame counts
show `scanning...` while pending; they are not reported as zero. Scan begin/end
logs identify a stalled root. Opening a dropdown freezes its rows until it closes.

Shared reuse review: `core_jobs` runs callbacks inline, while `core_workers`
shutdown joins in-flight workers. Neither provides cancellable filesystem calls.
Reuse is deferred for this small app-specific adapter; existing scene/volume
collectors and render-export summary logic remain the owners of discovery rules.
No shared API/version or program VERSION change is required.

Regression target: `make BUILD_TOOLCHAIN=clang test-menu-catalog-discovery`.
The test holds a root and frame-summary call indefinitely, proves independent
healthy discovery, bounds duplicate work on refresh, rejects stale completion,
and exits with a blocked call outstanding. `ui_menu_contracts` also exercises
normal `menu_state_init` followed by asynchronous healthy-library readback.

Acceptance boundary: normal packaged startup and tab interaction were observed
with the copied failing configuration while external scans stayed pending.
Explicitly opening that external scene subsequently stalled in
`SceneEditorSessionBegin -> runtime_scene_bridge_apply_file_with_options ->
core_io_read_all -> fopen`. This separate synchronous scene-load issue remains;
startup recovery does not establish external-scene load acceptance. Do not promote
this result as full user acceptance or broaden it into U1.3. Desktop refresh,
canonical adoption, public release, and Registry changes remain separate actions.

Final local acceptance also opened the bundled `optic_studio_starter_v1` scene
through the normal packaged menu, showed all five scene objects and viewport
geometry, and exited with launcher status 0. The U1.1/U1.2 native acceptance
passed again with mesh/primitive live previews, committed-move reopen, cancelled
preview isolation, and preserved unknown fields/source assets. Retained evidence
is under `build/editor_ui_recovery/u11-u12-startup-final/` and the sibling
`startup-blocker-evidence/` directory. These are development acceptance artifacts,
not installed Desktop or external-scene acceptance.

## September 19 U1.3 transform usability checkpoint

The Scene header keeps an explicit `Gizmo: Move | Rotate | Scale` control and a
separate persistent `Op` readout. During a drag it reports the axis and signed
scene-space distance, signed degrees, or scale factor; idle text explains the
active tool. Inspector groups use Position (scene space, meters), Rotation
(degrees), and Scale (unitless factor). During preview the inspector explicitly
shows the transform before the drag; release updates the absolute values.
Selected shaded geometry and its outline remain primary against softer wires.
The Transform handles control now controls these gizmos as well.

Move, Rotate and Scale share the retained transaction boundary: preview changes
only copied presentation geometry, release commits one command, and Escape,
focus loss, mode/workspace changes, resize and stale document context cancel.
Undo/Redo and Save/fresh-process reopen use the existing document path. Undo's
conservative dirty-state behavior is unchanged. Rotation edits Euler XYZ
components; nearly edge-on rings use linear drag. Scale is per-axis with a
positive minimum of 1e-6. Uniform scaling and snapping are not included.
The importer currently admits meters; authored movement and its guide respect
world_scale. This does not add other unit schemas.

Existing kit projection, shading, mesh/primitive math and document commands are
reused. Focused app-local handles, copied preview and feedback modules separate
presentation from gesture state. No shared API or version changes are needed.
The Gizmo/Op separation follows Sculpts' established interaction semantics.

Native evidence is retained in ignored
`build/editor_ui_recovery/u13-final-verified/`: dense idle/active captures,
committed inspector captures, mesh and primitive preview checks, existing Move
regressions, XYZ Rotate/Scale transactions, pretransformed mesh checks,
non-default world scale, and fresh-process committed/cancelled snapshot reopen.
Focused foundation, pane, navigation, viewport bridge, pick/scroll, outline,
shading and startup discovery gates also pass. Full test-stable and hands-on
operator acceptance are separate. Development packaging is checked separately;
this pass does not refresh Desktop, adopt canonical main, or enter U2.

## September 19 U2.1 shell hierarchy checkpoint

U2.1 is implemented in this retained Main Edit lane. The editor uses stable
document, workspace and viewport-tool rows plus one normalized task-status line.
Workspace selection is direct and persistent; Environment replaces the visible
`Atmos / Water` name while the compatible internal profile remains unchanged.
The presentation change does not add document mutations.

The forced Clang build, pane-host contract and isolated native workspace
acceptance pass. The native pass covers Add/import, transform live preview and
cancellation, all five direct workspace segments, view-only revision invariants,
pane expand/restore, Save/fresh reopen and headless render. U2.2 must begin as a
new bounded transform-ergonomics slice from this committed checkpoint; canonical
adoption, VERSION changes and release remain separate.

## October 11 — retained lighting MCP slice

Three lighting tools share the retained native light inspector, brightness/radius
sliders and environment menu commands. Static existing primary point/sphere
position, intensity and radius use world-unit conversion and one document command;
environment patches preserve omitted settings. Reads do not synthesize legacy
fields. Static edits refuse animated paths/timelines/motion bindings and
unsupported/ambiguous primary sources. Save writeback preserves unknown environment
and light-settings properties. Other lights, cameras, objects and volume attachments
remain intact. See [lighting MCP](lighting_mcp.md). Catalog is now 32 named commands
plus two fixed discovery/dispatch tools, 34 total. Normal Direct Light prepared
values/pixel effects, exact legacy Undo restoration, UI/real-MCP shared history and
fresh save/reopen are qualified. App 0.18.0/worker 0.11.0 remain unchanged.
Stop at this bounded lighting checkpoint; render job/result control and broader
light inventory/animation authoring remain separate.

## October 11 — editor render control and results MCP slice

Five render tools share the new Render frame panel command and reuse the native
async desktop renderer/listener/cancellation/durable BMP completion path. Jobs
capture the current working scene without Save or history mutation, serialize an
exclusive render context, and preserve legacy scenes, metadata and attachments.
Unique contained request IDs cannot overwrite or resubmit. Status/result reads
include source/settings, committed passes, prepared-scene provenance and independently
verified BMP hashes. Direct Light and Disney V2 single-frame workflows, unsaved
edit/render/Undo, UI/MCP output parity, cancellation/shutdown, publication failure
and fresh-process results pass. Existing camera/object/material/lighting native
acceptance and twelve focused editor/async render C groups pass. See
[render MCP](render_mcp.md). Catalog now has 37 named commands plus two fixed tools
(39 total). App 0.18.0/worker 0.11.0 unchanged; batch/resume/remote rendering and
additional render controls remain separate. Stop at this bounded checkpoint.

## October 11 — shared scene lifecycle MCP slice

Open, Create and Save As use one shared File-menu/native command owner. Open
validates before switching; Open/Create refuse dirty scenes. New-file publication
never overwrites; Save As retains working edits and Undo/Redo in the same project
directory, preserving relative dependencies and the original saved scene. New
empty scenes include a static camera and primary point light. Revisions remain
monotonic across transitions, with native path guards for updated bridges. The
control socket and attached bridge processes survive switches; aliases support
older running bridge code. Catalog is 40 named commands plus two fixed tools,
42 total. Actual UI/real MCP, legacy metadata/physical-volume preservation,
creation/edit/save/reopen and both supported render workflows are qualified.
See [scene lifecycle MCP](scene_lifecycle_mcp.md). App 0.18.0/worker 0.11.0 remain
unchanged. Stop at this bounded checkpoint; project relocation/export, document
close/delete, batch/resume/remote rendering and wider light authoring are separate.

## October 11 — cross-directory Save As checkpoint

The shared File/MCP Save As owner now copies contained scene dependencies into
another existing directory. Working/Undo/Redo roots retain relative layouts and
legacy declarations. Explicit resources, implicit native mesh runtime/preview
files, pinned PNGs and physical atmosphere manifest/frame dependencies are
verified and copied independently; identical destination assets may be reused.
No scene or conflicting asset is overwritten. Copy/publication/native-application
failures preserve the original document and attachment and roll back only
proven new assets. Changed external files retain a recovery receipt. Native actual
UI/MCP, history, fresh destination reopen with source assets offline and exact
Direct Light output parity pass. See [scene lifecycle MCP](scene_lifecycle_mcp.md)
for copy budgets and unsupported-reference boundaries. Catalog stays42 tools;
app0.18.0/worker0.11.0 unchanged. Stop at this bounded checkpoint; whole simulation
project export and unsupported asset producer lanes remain separate.

## October 12 — repeatable optical comparison matrices

The local MCP runner creates portable comparison projects with a sealed baseline,
saved cells, fixed-sequence native renders, verified hashes and a self-contained
labeled contact sheet. IOR, transmission, absorption distance and uniform
graph-backed frosted roughness use existing shared authoring commands. The
original clean scene and supported attachments are preserved and restored;
partial runs retain completed cells and refuse overwrites. Saved-cell verification
reopens and repeats renders with exact same-build image hashes. Legacy rendering
keeps automatic sampling. See [optical comparisons](optical_comparison_mcp.md).
The catalog adds `render_start_seeded`: 78 named commands plus two fixed tools,
80 total. App 0.18.0 and worker 0.11.0 remain unchanged. Resume of partial matrices,
measured optical calibration and production image-quality acceptance remain
separate. Stop at the bounded verified Main Edit checkpoint.

## October 12 — optical matrix partial-run recovery

New matrix runs use a v2 cell journal with sealed recipe, baseline, copied
resources and native executable identity. `--resume` uses existing MCP commands
to adopt completed unjournaled results, wait for an owned active render or retry
terminal attempts under fresh IDs. Completed cells and uncertain saved scenes
are preserved; source/build drift, unrelated active work and concurrent runners
refuse. Original scene/attachment restoration and explicit saved-cell verification
use the existing owner. Old v1 matrices remain verifiable. See
[optical comparisons](optical_comparison_mcp.md) for the sealed-baseline boundary.
No new native command/schema, renderer kernel or catalog change: 80 tools remain.
App/worker versions remain 0.18.0/0.11.0. Stop at the bounded recovery checkpoint;
partial sample accumulation, remote resume and measured calibration are separate.

## October 12 — dynamic MCP scene attachments

Scene endpoint bindings now use dynamically owned storage instead of a fixed
32-alias array. The listener and previously attached clients survive repeated
scene switches; failed reservations roll back only their new endpoint. Existing
scene formats and the 80-tool catalog are unchanged. This bounded local repair
uses the existing Main Edit lane and leaves release/canonical adoption separate.

## October 12 — explicit glass optical response repair

Native explicit glass transmission consumes authored optical roughness separately
from GGX regularization and uses IOR for its interface reflection floor. Eligible
rough-cone samples carry coherent camera texture footprints; unsupported cases
keep their conservative fallback. Legacy material paths and scene formats stay
unchanged. The glass-orb scene and full detailed matrix proof are retained in the
private scene delivery lane; no canonical or public release action is implied.
