# Collision-material resistance in saved rigid motion

The Ball/optiC Main Edit workflow supports optional rolling_resistance_length_m
and torsional_resistance_length_m on extensions.rigid_scene.bindings. These are
collision-material lengths in metres, separate from appearance materials. Zero
or omission preserves the previous model. A contact uses the maximum length
from its two colliding bodies, independently for the two channels.

```json
"rolling_resistance_length_m": 0.002,
"torsional_resistance_length_m": 0.002
```

Ball computes contact-load-bounded, energy-dissipating angular response in a
staged native world. Coulomb friction couples rolling spin to translation on
subsequent ticks. Free flight is unaffected. Source core_scene1.7.0 validates
these optional values and binds them into the physical projection/result match;
recompile consumers after the public binding struct change. optiC only reads
accepted immutable poses. Changing either length requires a new Ball result;
failed admission preserves the previously loaded scene/result.

Thirteen profiles match all18,733 normal/sanitizer snapshots. Analytical rolling
and torsion, energy/momentum, mass scaling, channel isolation, no-contact and
zero-law controls pass. In the new13-body room with both sphere lengths2mm, all
five dynamic bodies pass the unchanged4–6s quiet gates; overlap<0.501mm and no
mechanical energy growth. The original no-resistance room retains its exact
trajectory and historical failure. Its earlier diagnosis remains valid.

Portable room, rolling and torsion results pass all4,323 ordinary native playback
ticks and6,328,872 source vertices with error<4.5e-16m. Both parameter-change
refusal/rollback controls and selected consumed-module sanitizer samples pass.
See the companion Ball Bounce docs/rigid_contact_resistance.md for the law,
operator split, units, analytical predictions and repeatable tests.

The2mm values are demonstration parameters, not measured material calibration.
This is Main Edit source/tool qualification. Existing discrete-step/geometry
limits remain; installed app, canonical and production adoption are separate.
