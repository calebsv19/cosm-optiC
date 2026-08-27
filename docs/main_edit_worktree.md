# RayTracing Main Edit Worktree

Last verified: 2026-08-27

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
