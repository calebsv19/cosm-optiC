# Full-capacity larger-room qualification

The retained development profile is qualified at 16 bodies: six boundaries,
three fixed obstacles (two rotated pillars and a convex wedge), five initially
resting dynamic cubes and two analytical spheres. The room interior is 7×6×3 m;
cube masses are 1, 2, 3, 1.5 and 2.5 kg, sphere masses 8 and 5 kg. There are
seven distinct verified assets. No authored object paths drive the motion.

Rest, two-projectile impact and the arrangement rotated by 180 degrees each run
2,400 ticks at 240 Hz. Restitution is 0.45, friction 0.5 and gravity 9.81 m/s².
The floor and spheres declare 2 mm rolling/torsional resistance lengths; the
existing maximum pair rule avoids doubling equal values. These are demonstration
parameters, not measured material calibration.

Both impact cases move four blocks and exercise four dynamic-body contact pairs
plus a block/pillar pair. All seven dynamic bodies pass the existing speed,
spin, kinetic energy, orientation and drift tolerances throughout seconds 8–10.
This longer window was declared for this larger-room fixture; the older 4–6 s
results retain their original meaning. Undisturbed total drift is below 5 mm.
Independent every-tick source geometry stays below 0.501 mm overlap versus the
unchanged 2 mm limit. Mechanical-energy growth stays below the unchanged 0.5 J
bound. Static poses are byte-exact and all 7,203 scene frames repeat exactly and
match ASan/UBSan production, using independently verified build-bound assets.

The longer duration exposed a result export bug: cJSON sometimes printed tick
1924 / 240 as 8.01666666666667, while exact-time validation expects the binary64
value 8.016666666666667. The producer now emits result timestamps with 17-digit
round-trip numeric precision. It changes neither simulation timing nor admission
tolerances. All 2,401 times pass Python/native normal/sanitized admission, and a
one-ULP timestamp alteration still refuses. The declared ten-second recording
limit remains in force; a twelve-second attempt correctly refuses.

Ball tests `tests/qualify_rigid_larger_room.py`,
`tests/audit_rigid_larger_room.py` and `tests/test_rigid_result_time_precision.py`
provide the producer, independent geometry and timestamp checks. The qualifier
requires explicit normal/sanitized backends, a parent room bundle with the
existing source assets, the companion optiC tests directory and a new output
root. optiC `tools/rigid_scene_workflow.py run` creates portable accepted results;
`tests/test_rigid_elastic_playback.py --case NAME` can select individual room
bundles for pose/source-vertex checks. Without --case it still checks all bundles.

This is bounded source/tool qualification at the existing capacity, not a
capacity increase. CCD, general concave or compound collision, complex stacks,
material calibration, installed apps and production adoption remain separate.
The wedge is imported and geometry-audited here; this fixture does not establish
a dynamic impact on every obstacle. No sleep or global damping is added.
