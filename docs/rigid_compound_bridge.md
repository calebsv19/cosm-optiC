# Exact bracket and arch compound-motion bridge

October 22, 2026 — qualified local Main Edit development capability.
Ball owns simulation; optiC reads sealed results and the original visible mesh.

The native adapter admits the source-pinned `l_bracket_v1` (two children) and
closed collision-source `arch_v1` (three children) from exact-reference preparation.
The ordinary result bundle needs only `source.stl`, `visible.json` and
`collision.bcr3d` per asset. Ball captures and reconstructs those bytes from the
pinned source before admission. Arbitrary concave intake and hull substitution
are outside this profile.

Grouping and static envelopes include every child. Penetration/recovery checks
all child pairs; compound contact identity retains child-pair masks and normal
agreement within the existing bounded contact storage. Unresolved searches,
initial overlap, exhausted capacity and motion limits refuse transactionally.
The profile remains 32 bodies, eight assets, 496 candidate child pairs and the
existing contact/time/travel limits. More children can exhaust the pair budget
before the body budget. No numerical solver law, timestep, damping or tolerance
was relaxed.

optiC reads actual child counts and source COM/principal metadata from the
collision artifact, authenticates them against the sealed physical input, and
transforms the original visible triangles once. Its parser is data-only and does
not link Ball physics. Shared `core_scene` development 1.9.0 extends result-profile
matching to three children; this is direct-source development use, not a shipped
or vendored adoption. Existing one-child results retain their format and behavior.

## Bounded qualification

Nine cases pass: free tumble; finite cube passage through the arch; separate
left/right/top arch impacts; isolated bracket/cube and arch/cube collisions;
bracket seam contact; and dissipative bracket settling on a floor. All 3,849
normal/replay/sanitizer snapshots agree exactly. Independent source-box overlap,
static-pose preservation and closed-system momentum/energy checks pass. Maximum
source overlap is 0.422487 mm in the bracket collision, within its unchanged gate.
The settling control uses the existing restitution 0.05, friction 0.6 and default
0.5 m/s restitution cutoff; final-window speed is 2.31e-12 m/s, spin 2.02e-12 rad/s
and position drift 4.31e-14 m.

Native child witnesses exercise all three arch children and both bracket seam
children. Full world/body/cache/clock rollback passes after loaded contact;
initial overlap and child-pair overflow refuse. Seven owner checks pass normally
and under ASan/UBSan. Eight metadata parser cases and seven result-integrity
checks pass; the latter also pass under sanitizer instrumentation. optiC checks
every accepted tick and 447,432 source triangle vertices, with maximum position
error 1.12e-15 m (gate 2e-12 m). Its sanitizer reader subset covers 962 ticks;
surrounding renderer objects are reused, so this is not whole-app instrumentation.

The original restitution-one, zero-cutoff bracket support case **fails** quietness:
0.858375 m/s speed and 39.6751 mm drift versus 0.01 m/s and 2 mm gates. Its accepted
producer bundle remains a failed physical qualification. The dissipative control
is a different input, not a retroactive pass. The previous room still refuses at
tick 128 (accepted prefix through 127) at its unchanged travel budget; the retained
free-collision control remains exact. Prior runtime/result regressions pass 38/49.

## Reproduction and limits

Ball tests `qualify_rigid_compound_bridge.py`, `test_rigid_compound_owner.py` and
`rigid_scene_compound_contract.c` own physics and transactional proof. optiC tests
`test_rigid_elastic_playback.py`, `test_rigid_compound_result_guards.py` and
`rigid_compound_metadata_contract.c` own result/geometry proof. Use fresh contained
build and output roots and each Python command's `--help` for explicit paths.

These finite fixtures do not qualify general dynamic compound/compound impacts,
arbitrary meshes, fast/thin-feature CCD, arbitrary stacks or long-term accuracy.
Native image/video appearance is not newly qualified here. Program versions,
installed apps, canonical source and releases are unchanged. The next physical
boundary is bounded dynamic compound/compound collision, followed separately by
fast/thin-feature motion work.

## Subsequent dynamic compound-pair qualification

Six aligned/spinning bracket/bracket, arch/arch and mixed pairs now pass the
existing gates without a runtime change. See [qualification](rigid_compound_pairs.md).
General compound meshes, many-body piles and CCD remain outside this finite proof.
