# optiC version status

Verified October 18, 2026. Version labels identify separate surfaces; a source
checkout, desktop package and worker package need not share a version number.

| Surface | Verified version | Meaning |
| --- | --- | --- |
| Canonical app `VERSION` | `0.19.0` | Released macOS arm64 desktop; Decision Two complete |
| Main Edit app baseline | `0.18.0` | Preserved development checkout label |
| Canonical `WORKER_VERSION` | `0.13.0` | Released independent Linux x86_64 worker package |
| Main Edit worker baseline | `0.12.1` | Preserved independent development baseline |
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
