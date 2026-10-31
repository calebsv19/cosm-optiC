# optiC version status

Release and runtime records verified October 31, 2026.

| Surface | Verified version | Meaning |
| --- | --- | --- |
| Canonical and reconciled Main Edit source | `0.21.0` / `0.15.0` | Camera checkpoint adopted and released |
| Public desktop | `0.21.0` | Signed/notarized; publication, metadata, Registry and signed snapshot verified |
| Registered headless worker | `0.15.0` | Independently authenticated Linux x86_64 package |
| Versioned worker installation | `0.15.0` | Exact package trees verified on home server and Linux PC |
| Selected active runtime | `0.15.0` | Linux PC long: installed identity, coordinator admission and service canary pass |
| Local desktop installation | `0.21.0` | Exact release installed and launched; one confirmed prior-app rollback retained |

Release source `8be3d538de20a836b871ce8b84a3f160aaa033fb`, tag `v0.21.0`,
contains camera checkpoint `5e1f27a69dce5844696bd8b8a8126d870c35cc6f`.
Desktop ZIP SHA-256:
`5a82b95661044b65de4f503971e5d36f6b1a8b0363813cc4c05e608ad5b58513`.
Worker archive SHA-256:
`9bebc202b5d857a2b89062e4e386569e2667dbb975af7efc74ac803efbdd04be`.
See the [GitHub release](https://github.com/calebsv19/cosm-optiC/releases/tag/v0.21.0).

Independent release readback is terminal, valid and `current_exact`, with
`runtime_ready=true` and no blockers for the selected Linux PC long role.
Versioned presence on the home server does not establish active runtime there.
The independent fast-worker hold is preserved. The camera qualification and
cross-platform saved-proposal limits are tracked in
[camera framework qualification](camera_framework_qualification.md).

## Historical status verified October 30

Release and runtime records verified October 30, 2026. Desktop, worker, local
installation and selected runtime identities are separate surfaces.

| Surface | Verified version | Meaning |
| --- | --- | --- |
| Canonical and Main Edit app `VERSION` | `0.20.0` | Released desktop baseline; clean source reconciliation |
| Canonical and Main Edit `WORKER_VERSION` | `0.14.0` | Independently versioned authenticated worker |
| Registered public desktop | `0.20.0` | Publication, Registry and signed snapshot independently verified |
| Registered headless worker | `0.14.0` | Current `ray_tracing_headless_worker` production record |
| Versioned worker installation | `0.14.0` | Package contents verified on the home server and Linux PC |
| Selected active runtime | `0.14.0` | Linux PC long role: current pointer, native identity, coordinator admission and representative canary verified |
| Local desktop installation | `0.20.0` | Exact signed/notarized release installed and launched; confirmed rollback retained |

Release source commit `2886958f96c86e49e53095fa4af6754b349c62c2` is published as
`v0.20.0`. Desktop ZIP SHA-256:
`9c458498284804775c34e06e838c6a340c0b787d046cf9c143e90a55a1fd9b9d`.
Worker archive SHA-256:
`997fe4c3f29375fbe5cd08b788ba03c21790b476521545ae42cd371cb63301cb`.
See the [GitHub release](https://github.com/calebsv19/cosm-optiC/releases/tag/v0.20.0).

Independent release verification is terminal, valid and `current_exact`. Fresh
selected-runtime readback reports ready with no blockers. This qualification is
for `linuxpc-long`; the independent fast-worker hold remains unchanged. Versioned
installation on another host does not establish its active/default runtime.
The local app's installed bytes and normal process/window identity are verified;
image capture was unavailable, so no new screenshot-based acceptance is claimed.
The physical and visual limits in [current truth](current_truth.md) still apply.

## Historical status verified October 18 and reconciled October 29

Release records verified October 18, 2026; development reconciliation verified October 29, 2026. Version labels identify separate surfaces; a source
checkout, desktop package and worker package need not share a version number.

| Surface | Verified version | Meaning |
| --- | --- | --- |
| Canonical app `VERSION` | `0.19.0` | Released macOS arm64 desktop; Decision Two complete |
| Main Edit app baseline | `0.19.0` | Reconciled existing release baseline; later engineering checkpoint is not yet packaged |
| Canonical `WORKER_VERSION` | `0.13.0` | Released independent Linux x86_64 worker package |
| Main Edit worker baseline | `0.13.0` | Reconciled independent existing release baseline; no new worker artifact selected |
| Registered public desktop release | `0.19.0` | Publication and production-current readback passed |
| Registered headless worker release | `0.13.0` | Current `ray_tracing_headless_worker` production record |
| Selected-host installation | `0.13.0` | Versioned package trees independently verified on the home server and Linux PC |

Release source commit `f2cf7472789775cbb2d0f9eb1499577c954c2317` is published as
`v0.19.0`. It includes the water, emitter and camera checkpoint
`60a1670dfacf16ff89b6b518a6875bfd6ac9f540`, adopted through
`060bc5b394fa9dbe738570ef87bf189fbff2398c` before packaging.

Desktop package SHA-256:
`230f28844a0670b5ceb189456b2d449ebaf65bc25bcb0ccc5becb33dccbcf9ff`.
Worker archive SHA-256:
`cc847d493febab3e1b0d618707b12331f1ddc6c375ba088c0326c72e669ffba8`.
The desktop package is signed and notarized. Independent publication readback,
Registry promotion, signed snapshot and terminal verification all passed. See the
[GitHub release](https://github.com/calebsv19/cosm-optiC/releases/tag/v0.19.0).

Selected-host versioned installation and the coordinator's exact loaded
code/catalog/package binding have their own passing receipts. Those results do
not establish a worker current-pointer rollout, workload dispatch readiness,
installation of the desktop app, or motion/visual acceptance. Those surfaces
require their respective verification.

Development packages additionally carry their own source/binary build identity.
Their displayed package-version baseline does not prove inclusion of a later
source checkpoint. Installed and loaded worker identities require host readback;
the registered current version alone does not establish either.

The dated entries in [current truth](current_truth.md) and the
[Main Edit runbook](main_edit_worktree.md) retain their original checkpoint
versions. In particular, their October 14–16 references to worker `0.11.0`
describe that earlier state. This page supersedes those entries for present
version status without rewriting history.

## Version checks in every release

Before Decision 1, reconcile both version files, the exact candidate source,
the desktop production record and the independent worker production record.
Record current and proposed versions separately in the release plan. In CodeWork,
ordinary Release Control resolves the active operational Registry separately from
its source repository; historical source-tree records are not current authority.

Decision 1 binds the selected transitions and the validation, packaging and
authentication work. Before presenting Decision 2, verify the final source
versions against app bundle metadata, artifact names, checksums, manifests and
source/binary identities. A package self-test must not stand in for this check.

After Decision 2, independently read back each selected publication and Registry
surface, then update this status page and the owning release docs in the same
closeout. Record worker installation and runtime qualification only when their
own receipts exist. Keep unsuccessful attempts and immutable releases intact.

## Package discovery

Use the [Ecosystem optiC manifest](https://ecosystem.calebsv.tech/agents/programs/optic.json)
for live public download URLs, architectures, checksums and sizes. The table above
is a dated reconciliation of source and Registry records, not a live public-byte
or host-runtime readback. See [desktop packaging](desktop_packaging.md) for the
package identity checks.
