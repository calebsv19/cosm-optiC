# Crossing and off-center impact qualification in the 32-body profile

This is a local development qualification of the existing 32-body contract,
not an installed-app or production release claim. Ball produces numerical motion;
optiC reads immutable poses and transforms the verified source geometry.

## Fixture and unchanged physical gates

The 10 × 8 × 3 m room contains 18 finite-mass dynamic cubes, four analytical
spheres, six fixed room boundaries, three fixed pillars and one fixed convex
wedge. All 22 dynamic bodies begin on the floor. Two 8 kg spheres approach a
shared crossing region along perpendicular axes at 0.8 m/s. Two other 8 kg
spheres strike separate cube chains at 3.5 m/s with a 0.14 m lateral offset
and an initial 0.5 m/s upward component. The crossing pair has no upward launch.
The cubes have isotropic inertia; general anisotropic rotating bodies are outside
this particular energy oracle.

Rest, impact and the entire arrangement rotated 180 degrees each record 2,400
ticks at 240 Hz. Restitution remains 0.45, friction 0.5 and gravity 9.81 m/s².
Floor/sphere rolling and torsional resistance lengths remain 2 mm. No solver,
contact law, sleep, damping, capacity, recording horizon or acceptance-tolerance
change is included. The earlier parallel 32-body fixture remains available.

All 22 dynamics must pass the existing quiet checks throughout seconds 8–10:
speed ≤0.01 m/s, spin ≤0.02 rad/s, kinetic energy ≤0.002 J, drift ≤0.002 m
and orientation change ≤0.01 rad. Mechanical energy may not grow by more than
0.5 J, and independent source overlap must remain below 2 mm.

Directed coverage requires a loaded sphere/sphere crossing collision, each
sphere changing velocity by more than 0.05 m/s at that contact, and direct
projectile contact with each designated offset block followed by more than
0.05 rad/s of vertical-axis spin within 24 ticks. Floor rolling alone cannot
satisfy that spin check. The room also requires at least four moved cubes,
five distinct loaded dynamic pairs, two distinct nonfloor obstacles struck,
two simultaneous loaded dynamic pairs and 22 simultaneous constraints.
These coverage gates supplement the unchanged physical tolerances.

## Observed results

| Arrangement | Crossing contact, seconds | Smaller projectile velocity change, m/s | Smaller offset-block peak yaw spin, rad/s | Peak loaded dynamic pairs | Peak constraints |
| --- | ---: | ---: | ---: | ---: | ---: |
| impact | 0.566667 | 0.545625 | 5.706339 | 6 | 28 |
| reverse | 0.566667 | 0.545625 | 5.624596 | 6 | 29 |

Both impacts move all six upper-chain cubes, interact across eight dynamic
pairs and strike the upper pillar and wedge. The two lower obstacles are present
but are not coverage targets for this scene. This extends the earlier four-obstacle
parallel qualification; it does not replace it with a claim that every obstacle
was hit here. Undisturbed dynamic bodies remain stable.

All bodies remain below speed/spin limits from tick 1413 (5.8875 s) onward;
all quiet measures pass throughout the original 8–10 s window in both impacts.
The resting control also passes. Mechanical energy never exceeds its initial
value in either impact. Maximum independently measured overlap is
0.500999286 mm. Static poses remain exact and quaternions remain normalized.

Normal repeat and ASan/UBSan trajectories agree exactly across all 7,203 scene
snapshots. The immutable portable workflow reproduces those same frames.
Native optiC playback checks all 7,203 ticks and 44,514,540 source vertices;
maximum coordinate error is 6.88338275268e-15 m, below the unchanged 2e-12 m limit.
All source packs remain unchanged after playback. The 38-check native regression
suite and seven directed-impact observer checks per impact arrangement pass.

## Reproduction and authoring guard

Ball `tests/qualify_rigid_larger_room.py --profile 32-crossing` selects
`tests/rigid_crossing_room_fixture.py`. Supply explicit `--backend`, `--sanitizer`,
`--parent` (a prior portable room containing verified source assets), `--ray-tests`
and a fresh `--out`. Existing `--profile 16` and `--profile 32` remain available.
`tests/audit_rigid_larger_room.py` performs independent every-tick source overlap
checks; `--initial-only` exposes the same authored-placement preflight that now
runs before each long qualification record. A preliminary cube/rotated-pillar
overlap was rejected at tick zero, its placement corrected, and all final initial
layouts have zero overlap. This was a fixture repair, not a numerical repair.

`tests/test_rigid_crossing_coverage.py --cache CACHE --out JSON` checks the actual
recording and rejects removed crossing/offset contacts, suppressed offset spin,
and suppressed crossing velocity response. optiC `tools/rigid_scene_workflow.py`
prepares immutable portable scenes; `tests/test_rigid_elastic_playback.py` checks
every tick and source vertex. Build outputs should use fresh contained roots.

The broader package/UI/release suites are not repeated for this fixture/test-only
change. This proves the local native producer/consumer path. Installed apps,
workers, visual/video acceptance, arbitrary chaotic rooms, CCD, concave/compound
import and anisotropic-body dynamics need separate qualification. A useful next
physics slice is rotating non-cubic convex bodies with unequal principal inertia.
No release version change is recommended for these fixture/observer changes.
