# optiC version status

Verified October 18, 2026. Version labels identify separate surfaces; a source
checkout, desktop package and worker package need not share a version number.

| Surface | Verified version | Meaning |
| --- | --- | --- |
| Canonical candidate app `VERSION` | `0.19.0` | Applied by Decision 1; not yet released |
| Main Edit app baseline | `0.18.0` | Development checkout at the accepted source checkpoint |
| Canonical candidate `WORKER_VERSION` | `0.13.0` | Applied by Decision 1; not yet released |
| Main Edit worker baseline | `0.12.1` | Independent development baseline |
| Registered public desktop release | `0.18.0` | Current desktop production record |
| Registered headless worker release | `0.12.1` | Current `ray_tracing_headless_worker` production record |
| Desktop preparation target | `0.19.0` | Authorized Decision 1 in progress |
| Worker preparation target | `0.13.0` | Authorized independent Decision 1 in progress |

The worker `0.12.1` record names source commit
`ee9e7348a9e6e01c8a64f43207e0a7475fd27b6f` and tree
`f1c2b7b3901a5828ea71524b36971f7e1d2921a4`. These exactly match the
canonical baseline inherited by Main Edit. The water, emitter and camera work
is checkpointed at `60a1670dfacf16ff89b6b518a6875bfd6ac9f540`; it has not
been included in a released package. Canonical adopted the qualified Main Edit
source and documentation through `060bc5b394fa9dbe738570ef87bf189fbff2398c`
before Decision 1 applied the candidate version transitions.

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
