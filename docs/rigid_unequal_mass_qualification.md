# Unequal-mass six-brick qualification

The local Ball-to-optiC workflow qualifies one six-brick mass distribution with
unchanged geometry, contact law, timing and settling tolerances. No numerical
runtime repair was needed. This builds on the [support-creep repair](rigid_settling_qualification.md).

From the base upward, brick masses are **3, 2.5, 2, 1.75, 1.5 and 1.25 kg**.
The tower still totals 12 kg and the projectile remains 8 kg. All six bricks are
dynamic from the start; the fixed floor is the only static object. Uniform-density
box inertia scales with each mass. A native observer checks actual admitted
masses and principal inertias against an independent analytic reference, rather
than checking total mass or labels alone. Uniform substitutions, swapped masses
and incorrect inertia are rejected by three negative controls.

| Case | Result | Maximum drift during seconds 4–6 |
| --- | --- | ---: |
| Undisturbed tower | Stable and settled | 2.70e-13 m |
| Original strike | Upper bricks topple and settle | 1.43e-13 m |
| Strike shifted laterally 2 cm | Upper bricks topple and settle | 1.97e-13 m |

The six-second duration and seconds-4-to-6 observation window were declared
before measurement. Gates remain 2 mm drift, 0.01 m/s linear speed, 0.02 rad/s
angular speed, 0.002 J kinetic energy and 0.01 rad orientation change. No gate
was weakened. The older equal-mass offset's seconds-3-to-5 failure remains a
historical failure; this new experiment does not change it.

Both strikes first contact at tick 71. Upper bricks pass the simultaneous 60°
source-up tilt / 0.35 m COM-drop criterion at ticks 207/208 for the original
strike and 209/211 for the offset. Both also have independently verified
contact-free ballistic intervals. Full source-mesh overlap stays below 0.501 mm
against the existing 2 mm gate. No prescribed object paths, sleeping, pose pinning
or friction tuning were introduced.

All **194 physical checks per build** pass normally and under ASan/UBSan;
leak detection is disabled. Normal, sanitizer, repeat replay and portable results
match across 4,323 snapshots / 34,584 body poses and contact arrays. Signed linear
momentum closes after gravity and fixed-floor reaction within 1.26e-10 kg·m/s per
step. Total mechanical energy remains below its initial value; individual steps
can gain up to 0.03131 J, so this is not exact energy/work conservation or an
angular-momentum budget.

Ordinary native optiC playback checks all 4,323 ticks and 1,245,024 source
triangle-vertex samples. Thirteen selected renders pass 1,437,696 paired
flattened/TLAS rays and 134,784 independent source-ray/material pixels. An
immutable result selects without resimulation, relocates and reopens with exact
geometry; a wrong revision refuses atomically. Materials change only the review
copy, with the physical contract unchanged.

## Reproduce the bounded experiment

Use the fresh native Ball backend and a compatible parent scene with the normal
local tools described in the [six-brick workflow](rigid_six_brick_qualification.md):

```sh
python3 tests/qualify_rigid_unequal_mass_scene.py \
  --backend <ball-native-backend> --parent <parent-scene.json> \
  --out <fresh-output>
python3 tests/qualify_rigid_unequal_mass_scene.py \
  --backend <ball-native-backend> --parent <parent-scene.json> \
  --out <fresh-offset-output> --cases impact --projectile-offset-y .02
python3 tests/test_rigid_unequal_mass_contract.py
```

The declared mass table is `tests/rigid_unequal_brick_masses.json`; limits reuse
`tests/rigid_settling_limits.json`. The optional test observer links through
`tests/rigid_unequal_mass_probe.mk` after Ball's Makefile. It observes actual
runtime state without modifying the native engine.

This is local source/native/saved-scene proof for one distribution at 240 Hz,
eight bodies including floor and projectile, one convex child per body. It does
not qualify arbitrary mass ratios, reversed top-heavy towers, larger structures,
fracture, CCD or general piles. Installed apps, canonical adoption and releases
remain separate. Next proposed checkpoint: reverse this mass ordering in the
same six-brick geometry under the same gates before considering more bodies.

The subsequent [reversed-mass checkpoint](rigid_topheavy_qualification.md) repairs a recovery-budget refusal but retains a failed offset settling gate.
