# RayTracing Main Edit Worktree

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
