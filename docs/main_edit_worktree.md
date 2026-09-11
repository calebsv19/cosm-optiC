# RayTracing Main Edit Worktree


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
