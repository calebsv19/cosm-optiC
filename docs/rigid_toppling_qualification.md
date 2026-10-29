# Original six-brick upper-layer toppling qualification

The [October21 support-creep repair](rigid_settling_qualification.md) supersedes
the open numerical-creep boundary below. It retains the original3–5s offset
failure explicitly and separately qualifies4–6s settling. This page records
the October20 baseline.

A bounded local development case now demonstrates genuine loss of support and
upper-layer toppling through the ordinary Ball-to-optiC immutable-result flow.
The tower stays upright without a strike. An upper-layer strike makes both upper
bricks tilt more than 60° and lose more than 0.35 m of center-of-mass height.
This extends the [earlier broad strike](rigid_six_brick_qualification.md), which
largely moved a low structure without overturning it.

**October20 baseline: post-impact quiet-position settling was open.** Two supports move up to
2.303 mm during seconds 3–5; a small offset control reaches 3.655 mm. Both exceed
the existing 2 mm quiet-position criterion despite negligible velocities. The
passing toppling result does not establish that the resulting pile has fully
settled. No solver, shader, shared contract, capacity or installed app changed
in this slice; the new work is qualification fixtures, observers and documentation.

## Frozen scene and observable criteria

Three crossed layers of two 2 kg bricks use independently prepared closed box
STLs, each 0.65 × 0.22 × 0.4 m. Adjacent centers are 0.225 m apart, leaving a
5 mm gap. Layer centers start at Z = 0.2, 0.6 and 1.0 m, alternating the long
axis between X and Y. A static 12 × 8 × 0.4 m floor and a finite 8 kg, 0.4 m cube
complete the existing eight-body limit. Every brick is dynamic from tick zero.

The projectile starts at (-1.7, 0, 0.95) m. The rest control has zero initial
velocity; the impact case starts at (4, 0, 1) m/s. Gravity determines its flight;
there is no authored object path, trigger, pinning, sleeping or velocity clamp.
Gravity, materials, contact policy and stepping match the preceding qualification:
240 Hz for 1,200 ticks, gravity (0, 0, -9.81) m/s², restitution 0.05, brick/floor
friction 0.6 and projectile friction 0.15.

`tests/rigid_toppling_limits.json` retains the previous 2 mm source-mesh overlap,
5 mm rest drift, bounded energy and quaternion gates. Rest includes all six
per-brick stability checks. Impact requires the same stable initial interval,
delayed contact and finite projectile response, plus **both upper bricks**
overturning at least 60° while each loses at least 0.35 m height at the same tick.
Tilt uses the world direction of source +Z through the principal mass frame;
yaw alone cannot pass. Lower supports may remain in place. The previous broad
strike's “every brick moves 2 cm” criterion is inappropriate for this different
observable and remains unchanged in its own fixture.

The separate motion observer reports contact-free ballistic intervals, energy,
external momentum balance and the existing final two-second settling gates.
Its ballistic, impulse and parity tolerances are explicit in the limits file.
It does not turn a failed quiet-position result into a toppling failure or hide
that failure behind a combined green status.

## Measured physical behavior

The undisturbed tower's largest displacement is 1.457 mm; final two-second drift
is below 0.018 mm. First impact occurs at tick 71 (0.295833 s). The finite shot's
X velocity changes from 4 to 2.54112 m/s in that tick. Upper bricks first satisfy
the simultaneous tilt/height criteria at ticks 212 and 218 (0.8833/0.9083 s),
losing 0.595/0.612 m height. Contact-free ballistic intervals last 24 and 25 ticks
(0.100/0.104 s), with velocity increments matching gravity within 6.6e-16 m/s.
This establishes actual loss of support in addition to an orientation change.

Full independent source-mesh SAT over every accepted frame finds maximum overlap
0.501 mm, below the unchanged 2 mm gate. All 62 primary physical checks pass
normally and under ASan/UBSan, including exact replay. All 2,402 snapshots,
19,216 body poses and contact arrays match between those builds and the portable
results. Leak detection was disabled; this is not an LSan qualification.

The launched projectile matches the independent semi-implicit ballistic position
reference before impact within 2.45e-15 m. Total mechanical energy starts at
213.188 J and ends at 73.9581 J, never exceeding its initial value. Individual
steps can increase energy by up to 0.01230 J; these observations do not establish
exact energy conservation or an error-free physical model.

The test-only native momentum observer reads final accumulated contact impulses,
including warm starts and signed tangent vectors. It does not modify the solver.
Maximum per-step linear momentum residual after gravity and fixed-floor reaction
is 8.3e-11 kg·m/s, with accepted-frame momentum agreement within 3.6e-15 kg·m/s.
Over five seconds, gravity contributes (0, 0, -981) kg·m/s and the floor contributes
approximately (-32, 0, 973) kg·m/s to the moving bodies. The external world holds
the floor fixed; total moving-body momentum alone is not conserved. Exported
`normal_impulse_kg_mps` and `tangent_impulse_kg_mps` are solver iteration-activity
scalars, not signed net impulse vectors. Summing them cannot close this budget.
Angular momentum and a complete energy/work ledger are outside this proof.

## Bounded sensitivity and rendering

One normal-build sensitivity control shifts only the initial launch Y by +0.02 m.
It passes the same physical gates and exact replay: first hit remains tick 71;
both upper bricks overturn/fall by ticks 195–196. Its trajectory differs and
quiet-position drift reaches 3.655 mm. This supports repeatability of the intended
toppling effect under this small input change, not quantitative timestep
convergence, broad robustness or a sanitizer matrix for the offset control.

Ordinary optiC playback checks all 2,402 primary tick poses and 691,776 source
triangle-vertex samples; maximum vertex error is 7.73e-15 m against 2e-12 m.
Eighteen primary selected views across two cameras plus three offset-control
views pass 2,322,432 flattened/TLAS comparisons and 217,728 independent source
camera rays and material pixels. Worst reference depth error is 2.85e-14 m.
The offset views prove selected frames only. Review sheets use unaltered source
images with labels; rendering success remains separate from human acceptance.

## Reproduce and inspect

Use explicit compatible development checkouts, a fresh task-owned output root
and optimized Ball tools. See the preceding qualification for normal/sanitizer
native builds and immutable scene preparation. The new physical entry point is:

```sh
python3 <ray>/tests/qualify_rigid_toppling_scene.py \
  --backend <ball-native> --parent <portable-parent-scene.json> --out <fresh>/normal
python3 <ray>/tests/qualify_rigid_toppling_scene.py \
  --backend <ball-native> --parent <portable-parent-scene.json> \
  --out <fresh>/offset --cases impact --projectile-offset-y .02
python3 <ray>/tests/test_rigid_toppling_contract.py
```

Nine positive/negative controls reject yaw-only motion, sliding, tilt without
fall, a single overturned upper brick, pre-impact/no-impact falls, and wrong
interpretations of nonidentity principal frames. Both qualification tools keep
`ok: false` until all requested work finishes.

The momentum probe links the existing Ball runtime through a supplemental test
makefile, without modifying Ball build rules or numerical identity:

```sh
make -C <ball> -f Makefile -f <absolute-ray>/tests/rigid_toppling_probe.mk \
  CONTRACT_BUILD_DIR=<fresh>/ball \
  COMMON_CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -O2 -g' \
  rigid-toppling-momentum-probe
<fresh>/ball/toppling_momentum_probe <input.json> <asset_set.json> > <fresh>/momentum.jsonl
python3 <ray>/tests/rigid_toppling_diagnostics.py \
  --cache <cache.json> --metrics <metrics.json> \
  --limits <ray>/tests/rigid_toppling_limits.json \
  --momentum <fresh>/momentum.jsonl --out <fresh>/diagnostics.json
```

Playback's existing `test_rigid_six_brick_playback.py` accepts explicit
`--camera-position`, `--camera-look-at`, `--impact-ticks`, `--cases` and
`--views-only`; defaults preserve its original case. Use ticks 0, 71, 140, 218,
360 and 1200 for this tower. Controlled material edits change presentation only.

Saved-scene walkthrough: select the exact accepted revision using
`tools/rigid_scene_workflow.py select`, relocate the resulting folder as a unit,
then open/evaluate its scene. This case was actually selected without numerical
resimulation, relocated and reopened with exact poses and source geometry at
ticks 0, 71, 218 and 1200. Wrong-revision selection refused atomically and original
scene/result hashes stayed unchanged. Retain the full `result/` folder beside the
scene; the result owns its engine-bound assets and poses.

The next proposed checkpoint is a bounded investigation of post-impact positional
creep in the remaining support contacts. General piles, larger towers, spheres,
CCD, concavity, sleeping, fracture, production adoption and remote jobs remain
separate. No new physical feature or installed-app refresh is part of this case.
