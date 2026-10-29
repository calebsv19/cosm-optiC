> Subsequent source integration: [staged sweep admission](rigid_sweep_admission.md) now closes the retained acceptance gap with atomic safe refusals. The query-only checkpoint below is historical.

# Classified compound-cast initial-state and normal hardening

October 22, 2026 — bounded Main Edit implementation qualified. The new opt-in
`ball_rigid3d_compound_cast_decision_3d` entry point clears the retained donor
contract failures without replacing the legacy cast or enabling runtime CCD.

## Explicit decision contract

The new `rigid_compound_cast_decision_3d.h` carries primary kind, time fraction,
child identities, failure reason and a separate response-normal validity flag.
Future-cast diagnostics are separate from initial-state data:

| Decision | Contract |
| --- | --- |
| Clear | Successful query, `NO_HIT`; no response normal |
| Future impact | `HIT` and selected child/time; response requires `response_normal_valid` |
| Initial touching | Explicit `INITIAL_TOUCHING`, time zero and selected children; existing contact policy owns its treatment |
| Initial overlap | Explicit `INITIAL_OVERLAP`, time zero; no response normal; overlap takes precedence over touching |
| Query failure | False plus a reason; never interpreted as a clear sweep or endpoint fallback |
| Invalid input/config | False, preserving caller output |

The profile supports translation-only principal-frame convex compounds with at
most three children per body, 256 vertices and 512 faces per child. Quaternions
must be unit within 1e-10. Rotation refuses explicitly. This is an additive
entry point: `ball_rigid3d_compound_cast_3d` retains its old semantics for existing
callers. Initial-contact decisions do not automatically create impulses or
zero-time response loops. A successful query alone does not authorize motion.

## Certified face response normals

Queries use pair-local coordinates. A separate small normal module reconstructs
candidate axes from stored collider face normals, verifies opposing support
faces and checks a positive-area finite patch with the existing manifold clipper.
It does not normalize a tiny world-space witness difference into an impulse
normal. Multiple competing contact axes, degenerate/edge patches or unresolved
features leave the response normal invalid; there is no approximate fallback.

For the patch certificate only, a temporary query shape is projected to the
candidate support plane within eight existing cast target tolerances. At the
default 1e-7 m target this is at most 0.8 µm. This never moves a runtime body, changes
the impact time, changes source geometry or relaxes a native motion limit. The
geometry-only manifold helper returns a face patch only; it fabricates no
penetration-query provenance and has no edge/support-point fallback. The current
manifold response entry point and legacy sweep implementation are unchanged.

## Qualification

All 46 cases match exactly under normal repeat and ASan/UBSan, with no blockers.
The original 41 include all eight previously blocked checks, under the same
0.5µm impact-position and1e-6 normal-vector gates. The added controls confirm
compound initial overlap outranks touching within the existing query tolerance,
edge ambiguity remains response-unresolved, rotation refuses, and invalid
motion preserves output; a finite motion outside the numeric envelope returns
an explicit invalid-motion query failure.

Maximum measured response-normal error is 1.93e-16, including the far-origin
translation and rebasing cases that previously failed. Maximum spatial impact
error is 1.001e-7 m, approximately0.1µm. The three original missed plate impacts
remain detectable; near-miss, child selection, order reversal, exhausted-budget
and invalid-configuration controls pass. Initial overlap never advertises a
response normal. All query inputs and retained collider files remain unchanged.

The legacy probe reproduces all 41 archived JSON results exactly, including its
historical limitations. Existing compound-CCD and convex-manifold contracts pass
normally and under ASan/UBSan. This provides compatibility evidence; it does not
expand the old API's qualification. Runtime stepping, source motion guards,
response solver and the original wrong thin-plate trajectories are unchanged.

## Reproduction and next boundary

```sh
make -f tests/rigid_compound_cast_probe.mk BUILD_DIR=<normal-build> \
  rigid-compound-cast-probe-build
make -f tests/rigid_compound_cast_probe.mk BUILD_DIR=<san-build> \
  DEBUG='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  rigid-compound-cast-probe-build
PYTHONDONTWRITEBYTECODE=1 python3 tests/qualify_rigid_compound_cast_donor.py \
  --probe <normal-build>/contracts/rigid_compound_cast_probe \
  --sanitizer-probe <san-build>/contracts/rigid_compound_cast_probe \
  --corpus <retained-fast-thin-diagnostic> --out <new-proof>
```

`classified_query_qualified:true` refers only to this bounded profile. It does
not claim general edge/vertex contacts, rotating CCD, many-body response,
world-space witness-normal reliability or simulation conservation accuracy.
The next slice is staged adapter integration: preserve initial support policy,
travel/event budgets and complete rollback, solve at a retained swept impact
pose and continue only after response validation. Original missed trajectories
must become qualified contacts or explicit safe refusals, with legacy scene
parity and optiC playback verified. An unresolved normal or query failure must
never fall back silently to endpoint-only advancement.

Source remains uncommitted Main Edit development. Program/shared versions,
canonical adoption, installed apps, packages and releases are unchanged. Patch
release recommended for this hardening; approval required before version bump.
The broader pending feature-release recommendation remains separate.
