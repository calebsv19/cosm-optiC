# Contact placement and impulse consistency repair

October 22, 2026: local Main Edit native producer/consumer qualification. Free
collision groups conserve total spin-plus-orbital angular momentum through the
selected impacts, while existing anchored resting behavior is retained. This is
source/tool evidence; installed apps, workers, release packages and the earlier
movie are unchanged.

## Physical boundary and implementation

The previous scene adapter solved impulses at predicted centers, then changed
those centers through linear-drift reconciliation and overlap recovery. Moving a
center by delta-r while retaining momentum p changes orbital angular momentum by
`delta-r × p`. This caused the saved closed-pair failures despite conserved energy
and linear momentum.

The repaired native owner separates contact groups using conservative source
AABBs expanded by the existing per-tick travel budget. A group whose envelope
connects to authored static geometry retains the qualified support-stabilization
path. Other dynamic groups use pose-consistent contact advancement. Classification
uses geometry and static roles, independent of scene names and shape families.
Resting blocks retain finite mass and dynamic roles; they can still react to hits.

Free-group impulses act at a retained pose. The world solver is configured with
zero position correction for that solve, and a pose-identity guard refuses any
hidden center or orientation relocation. The bodies then advance under the
solved velocities and existing gyroscopic rotation operator. A bounded local
contact bracket finds a shallow contact pose before further advancement. No
energy or angular-momentum projection, sleep or new damping is introduced.

The following implementation paths are in the companion Ball Bounce producer:

| Module | Responsibility |
| --- | --- |
| `src/interop/rigid_scene_runtime.c` | Own the staged tick, validate and atomically commit or refuse. |
| `src/interop/rigid_scene_contact_partition.c` | Build conservative support/free groups, check their envelopes and merge results. |
| `src/interop/rigid_scene_contact_advance.c` | Advance free groups and solve impulses at retained poses. |
| `src/interop/rigid_scene_support_advance.c` | Retain existing anchored support stepping and map body IDs to owner data. |

Persistent support caches retain output-tick age. Free-contact caches are local
to the internal event loop, so their epochs cannot age or suppress another
group's warm start. Internal solves are bounded by 64 events per output tick and
40 bracket iterations per event; zero-advance residual solves consume the same
event budget. Per-solve iteration, capacity, material and overlap limits remain
unchanged. Free-group total swept travel is bounded by the existing budget;
a support displacement escaping its grouping envelope refuses atomically.

`core_scene` contracts, `core_sim`'s exact 240 Hz output clock, result format,
32-body/496-pair capacity and renderer ownership are unchanged. Ball owns the
numerical result; optiC consumes sealed poses and verified source geometry.

## Fixed-gate qualification

Three two-second off-center tumbling pairs use brick/hexagonal prism,
hexagonal/triangular prism and triangular prism/brick. A mixed scene adds a
separate finite-mass resting block and static floor, and an analytical sphere
pair verifies the isotropic case without artificial spin changes.

- Original closed-pair angular-momentum errors of 0.0159723, 0.0124621 and
  0.0230246 kg m²/s fall to at most 8.76e-14 kg m²/s. The fixed gate is 1e-8.
- Across all five controls, relative energy error is at most 1.85e-13 and linear-
  momentum error 5.89e-16 kg m/s. The energy gate remains 0.001 relative and
  linear-momentum gate 1e-8 kg m/s.
- All 2,405 normal/repeat/sanitizer collision snapshots agree exactly. The mixed
  scene's resting body remains byte-identical. Sphere spin changes stay below
  the negative-control gate; non-cubic impacts produce the required spin change.
- Independent every-tick source geometry passes with maximum overlap
  0.323544 mm, below the unchanged 2 mm gate.
- Single/stack rest and impact, the finite-mass restitution oracle, top-heavy
  six-brick rest and tilted/spinning floor impacts pass their original energy,
  geometry and quiet-state limits. The final normal/SAN producer reproduces all
  11,172 snapshots across these qualified controls exactly.
- The retained 32-body sphere/cube crossing scene reproduces all 2,401 complete
  frames exactly. The direct gyroscopic contract passes 72,000 free steps.
- Ten owner checks (five normal and five sanitized) verify successful contact
  ticks, exact static bytes, and full body/cache/diagnostic/clock rollback for
  initial overlap, envelope escape and later motion-budget refusal.
- Native runtime regression passes 38 checks; immutable-result regression 49.
- Fresh portable free-collision and mixed-support bundles pass native replay,
  optiC preflight and every-tick playback: 962 ticks / 109,668 source vertices,
  maximum coordinate error 6.67e-16 m against the unchanged 2e-12 m gate.

## Remaining qualification boundaries

The original 15-body obstacle stress profile still refuses tick 128 after 127
accepted ticks at the unchanged 0.05 m motion budget. Its exact normal/SAN prefix
and atomic refusal are retained. Ten-second obstacle coverage and settling are
not qualified by that prefix; the budget was not raised.

The existing top-heavy impact fixture settles all six bricks within its original
quiet gates and passes energy, overlap, mass and impact-response checks. Its
scene-specific requirement that both upper bricks overturn and fall now fails:
one of two meets the unchanged criterion. That complete scene remains failed,
including in the retained normal/SAN replay. The input, threshold and historical
result are preserved; no force or material was retuned to make it pass.

A closed group here means a group separated from static geometry by its
conservative travel envelope. Anchored contacts retain the existing approximate
support-stabilization model; this adds no claim of exact static-reaction torque
balance. General CCD, concave/compound expansion, simultaneous many-body tumbling
and larger capacity remain separate qualifications.

## Reproduction

Use fresh task-owned build roots with Ball's `rigid-scene-native-build`,
`rigid-scene-cache-io-build`, `rigid-scene-contact-advance-contract-build` and
`rigid-gyroscopic-rotation-contract` targets. Build normal and ASan/UBSan tools
explicitly; do not reuse stale objects or clean another build root.

Run `tests/qualify_rigid_tumbling_impacts.py` with explicit `--backend`,
`--sanitizer`, `--parent`, companion optiC `--ray-tests`, fresh `--out` and
`--cases pair_0 pair_1 pair_2 mixed_pair_0 sphere_pair_0`. The parent supplies
verified source assets; the qualifier prepares each producer's own assets.

`tests/audit_rigid_larger_room.py` accepts these named cache cases for independent
source-geometry audit. Existing optiC `qualify_rigid_resting_scene.py` and
`qualify_rigid_topheavy_scene.py` retain their physical gates.
`tests/test_rigid_contact_advance_replay.py` verifies fresh normal/SAN trajectories
against measured references; its optional failed-reference mode explicitly
preserves physical qualification false. `tests/audit_rigid_contact_rest.py`
independently checks full-inertia energy and a declared quiet window.

The old single-drift COM-shift decomposition remains diagnostic history. It is
not a valid predictor for a tick now split at contact times. Total momentum,
energy, geometry and resting gates above remain the acceptance criteria.
