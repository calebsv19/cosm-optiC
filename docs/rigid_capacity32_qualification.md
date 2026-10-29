# 32-body mixed-room qualification

The retained development profile supports **32 bodies**, **496 unordered pairs
and constraints**, up to **8 distinct verified assets**, and one convex child
per body. The optional shared result API is `core_scene 1.8.0`. Its larger
embedded binding array requires consumer recompilation. Existing source callers
and result formats remain supported; compiled struct layout is not a stable ABI.
This qualification does not establish installed or production adoption.

## Fixture and fixed gates

A 10 × 8 × 3 m room contains six boundaries, three pillars, one convex wedge,
eighteen initially resting dynamic cubes, and four 8 kg analytical spheres.
Twelve cubes form four impact chains, with 1, 2 and 1.5 kg masses per chain.
Six dynamic sentinels sit outside those chains. The projectiles drive the chains
into four distinct fixed obstacles. This is a controlled capacity and contact
qualification; arbitrary chaotic scenes and general stacking remain separate.

Undisturbed rest, impact and the arrangement rotated 180 degrees each run for
2,400 ticks at 240 Hz. Gravity is 9.81 m/s², restitution is 0.45 and friction is
0.5. The floor and spheres use 2 mm rolling and torsional resistance lengths,
combined by the existing maximum-pair rule. No prescribed object paths, sleep,
global damping or numerical contact-law changes are added.

Before measurement, the following gates were fixed:

- All 22 dynamic bodies must satisfy the existing speed (0.01 m/s), spin
  (0.02 rad/s), kinetic energy (0.002 J), orientation change (0.01 rad) and
  drift (0.002 m) bounds throughout seconds 8–10.
- Source overlap must stay below 2 mm and mechanical-energy growth below 0.5 J.
- At least ten cubes must move, all four obstacles must be struck, at least
  eight dynamic-body pairs must interact, at least four loaded dynamic contacts
  must occur simultaneously, and one tick must solve at least 24 constraints.

Both impacts move all twelve chain cubes, strike all four obstacles and reach
12 simultaneous loaded dynamic contacts. Their peak constraint counts are 37
and 36. All 22 dynamic bodies settle within the original physical tolerances.
Normal repeats and ASan/UBSan trajectories match exactly across all 7,203 scene
snapshots. Independent geometry checks every tick and stays below 0.501 mm
penetration. The six undisturbed sentinels remain dynamic and stable.

The retained 16-body impact replays all 2,401 frames exactly after its verified
assets are rebuilt under the new engine identity. Native, Python and shared
readers accept 16 and 32 bodies and refuse 33. Normal and sanitized optiC
replacement tests preserve a loaded 32-body result after rejecting 33 bodies.
All ten seconds retain exact tick timestamps; a one-ULP alteration still refuses.

## Result storage and ownership

The larger impact result is about 40 MiB. Its 2.3 million value nodes exceed the
old two-million-node guard, as well as the old 32 MiB file ceiling. Shared result
admission, native pack I/O, Python execution and optiC now support a 64 MiB
result ceiling and a four-million-value-node guard. Depth 32, smaller authored
input/asset limits and the existing 64 MiB total bundle budget remain enforced.
The 2,400-tick, ten-second recording horizon is unchanged. Not every possible
contact/time combination is promised to fit: over-budget results must refuse
without truncation.

The data-only shared contract owns result meaning and strict readback. Ball owns
collision capacity and response, verified assets and immutable result production.
optiC owns result admission, evaluated poses, appearance materials and rendering.
Both Main Edit source manifests pin the reviewed shared 1.8.0 files. Other
shared inputs and vendored subtrees are preserved. Canonical source adoption,
program versions, installed apps, workers and releases remain separate.

## Reproduction

Ball `tests/qualify_rigid_larger_room.py --profile 32` uses
`tests/rigid_capacity_room_fixture.py`; its default 16-body profile is retained.
Supply explicit normal and sanitized producers, a prior portable room containing
verified cube, sphere, pillar and wedge source assets, the companion optiC test
directory, and a fresh output root. `tests/audit_rigid_larger_room.py` checks
source geometry independently. `tests/test_rigid_capacity_legacy_replay.py`
verifies old trajectories, and `tests/test_rigid_result_time_precision.py` keeps
the exact-time regression.

Normal/sanitized world, contact-cache and resistance contracts pass, alongside
38 native, 49 result-store and 45 consumer regression checks. Contact-cache tests
exercise the enlarged 496-entry storage and deterministic full-cache behavior.
The fixture's actual peak of 37 constraints is distinct from that storage bound.

optiC `tools/rigid_scene_workflow.py run` prepares portable results.
`tests/test_rigid_result_capacity.py` checks 16/32/33 admission agreement;
`tests/test_rigid_capacity_rollback.py` verifies that loaded 32 survives rejected
33. `tests/test_rigid_elastic_playback.py --case NAME --batch-ticks 120` checks
every recorded pose and source vertex. Batch size changes observer-process reuse,
not coverage. Selected consumed-module sanitizer samples also check transformed
source geometry.

The subsequent [crossing and off-center qualification](rigid_crossing_qualification.md)
uses this same 32-body profile. General concave/compound import, CCD, arbitrary
stacks, measured material calibration and growth beyond 32 remain separate
resource, physics and playback qualifications.
