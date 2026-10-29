# Tumbling object/object qualification diagnostic

October 22, 2026: the new tumbling impact matrix **does not pass physical
qualification**. The previous gyroscopic free-rotation repair remains valid;
accepted contact placement has a separate angular-momentum defect. Do not use
repeatability, zero accepted overlap or a successful render to imply that this
matrix is physically qualified.

## Fixed controls and observed boundary

Three isolated pairs use brick/hexagonal prism, hexagonal/triangular prism and
triangular prism/brick, with unequal masses, nonprincipal initial spins, different
orientations and off-center strikes. They run for 480 ticks / two seconds at
240 Hz, with no gravity, friction, resistance or external bodies and restitution
one. Both bodies visibly change world spin during free flight; their first
loaded impact occurs at ticks 114, 105 and 109, respectively. Each impact also
changes both bodies' spin by more than 3 rad/s.

All three full trajectories reproduce exactly across normal repeats and
ASan/UBSan builds. Independent full-inertia energy and linear momentum pass:
relative energy error is at most 1.74e-13 and linear-momentum error at most
5.42e-16 kg m/s. Noncontact-step total-angular-momentum change is at most
2.96e-15 kg m²/s. But **spin plus orbital angular momentum** over the complete
closed-system trajectory fails its 1e-8 kg m²/s gate:

| Pair | Maximum total angular-momentum error (kg m²/s) |
| --- | ---: |
| Brick / hexagonal prism | 0.0159723201 |
| Hexagonal / triangular prism | 0.0124620768 |
| Triangular prism / brick | 0.0230245946 |

The independent every-tick convex source-mesh SAT observer finds no accepted
solid overlap in the three controls, within the unchanged 2 mm gate. This does
not establish conservation or continuous collision detection.

## Accounted contact mechanism

The contact solver applies impulses at predicted body centers. Afterwards the
native scene owner reconciles linear drift with the new velocity, and the world
and scene recovery paths move centers to remove overlap. Those position changes
leave the velocities and spin angular momenta unchanged. Moving a center by
delta-r changes orbital angular momentum by `delta-r × (mass × velocity)`.

An independent observer computes every pair contact's accepted-center shift and
its orbital contribution. It accounts for the entire total-angular-momentum jump
with unexplained residual below 9.15e-16 kg m²/s. Both linear reconciliation and
position recovery contribute. Disabling only the former cannot close the defect.
The impulse law itself balances at its predicted centers; the new free-tumbling
integrator continues to conserve between contacts. No solver change, damping or
momentum projection was introduced to turn this diagnostic into a pass.

## Obstacle room and safe refusal

The ten-second room fixture adds six tumbling non-cubic dynamics, four fixed
posts, four room walls and a floor: 15 bodies inside the existing 32-body profile.
It refuses tick 128 after 127 accepted ticks (0.52917 s). A triangular prism's
next-step swept-distance estimate is 0.0523448 m, exceeding the unchanged 0.05 m
budget; its accepted spin is 19.5855 rad/s. Normal repeat and sanitizer runs agree
on the entire 128-snapshot prefix and refusal.

Independent prefix geometry shows no accepted solid overlap; mechanical energy
decreases from 197.421 to 162.698 J. The prefix is diagnostic only. It does not
qualify ten-second settling or the intended loaded post contacts. Static bodies
and initial placement are separately checked by the retained observer evidence.
No guard, material or initial condition was relaxed to call this scene a pass.

## Reproduction and next repair

Ball `tests/qualify_rigid_tumbling_impacts.py` prepares verified source assets,
runs normal/repeat/sanitized pairs and the obstacle profile, and writes all failed
gates before returning nonzero. Supply explicit `--backend`, `--sanitizer`,
`--parent`, `--ray-tests` and a fresh `--out`. The parent is an existing portable
non-cubic scene with verified source assets. No GUI or installed app is needed.

`tests/diagnose_rigid_tumbling_room.py` retains the refused run, independently
accounts for the fixed motion budget and writes the accepted-prefix cache;
its successful observer status explicitly retains physical qualification false.
`tests/audit_rigid_larger_room.py --cases pair_0 pair_1 pair_2` now accepts named
cache cases and checks every recorded source-geometry state.

The next numerical repair must make the contact pose, impulse lever arms and
accepted trajectory consistent without later unaccounted COM relocation. It must
preserve resting/support behavior and the earlier sphere/cube trajectory. Bounded
internal contact advancement/substeps may also be needed for the obstacle scene;
that must preserve exact 240 Hz output times and refuse exhausted budgets. Keep
the closed-pair energy, linear/angular momentum, overlap and rollback gates fixed,
then rerun the original room and earlier resting/tower controls. General CCD,
concave/compound expansion, larger capacity and new renders are separate.

This is a local Main Edit test/diagnostic addition. Numerical source, installed
applications, versions and production are unchanged. Full optiC playback and new
renders stop at the failed physical gate.

## Contact-placement successor

The subsequent [contact-placement repair](rigid_contact_placement_repair.md)
closes the saved free-pair angular-momentum defect while preserving the original
resting limits. Anchored stabilization, the motion-budget refusal and the changed
top-heavy toppling expectation retain the explicit boundaries described there.
Historical measurements and movies above are unchanged.
