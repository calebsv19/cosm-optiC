# Gyroscopic rotation qualification

October 22, 2026: local Main Edit native scene-producer qualification. Installed
applications, canonical adoption, worker delivery and a new rendered video are
separate. The earlier Disney V2 movie retains its original pre-repair physics.

## Numerical behavior and ownership

Ball owns numerical evolution; optiC consumes immutable exact-tick poses. An
unequal-inertia body now evolves body-frame angular momentum through the implicit
midpoint Euler equations, paired with a Cayley quaternion attitude increment.
This preserves spatial angular momentum and rotational kinetic energy to solve
and floating-point tolerance, while allowing world angular velocity to change
as the body tumbles. It adds no damping, energy projection or speed clamp.

The implementation is isolated in Ball's
`src/physics/rigid3d/rigid_gyroscopic_rotation_3d.{c,h}`. The native scene owner
uses a drift-kick split for unequal inertia: preserve the predicted attitude and
contact-solved spin together, so orientation reconciliation cannot change world
angular momentum after an impulse. Isotropic bodies retain their existing exact
orientation update and contact reconciliation. The older kinematic orientation
API and other Ball runtime owners are unchanged. Existing `core_scene` and
`core_sim` contracts are reused; no shared API, result-format or capacity change.

A staged update refuses atomically on invalid input, failed convergence or more
than 256 internal substeps. Each substep allows at most 16 Newton iterations.
This is a bounded rotational solve, not continuous collision detection.

## Qualification

Fresh normal and ASan/UBSan builds pass:

- Thirty free-rotation controls / 72,000 steps: maximum scaled angular-momentum
  error 9.95e-13, relative energy error 2.00e-12 and quaternion-norm error 6.67e-16.
- Independent RK4 reference: four-second attitude error 0.000331 rad at 240 Hz;
  halving the timestep reduces error by approximately four, as expected for
  second-order accuracy. An analytic contact-impulse check verifies delta-L=r×J.
- Ten-second nonprincipal brick, hexagonal-prism and triangular-prism cases:
  maximum absolute angular-momentum error 1.65e-13 kg m²/s and relative energy
  error 9.57e-13. World spin changes rather than remaining artificially fixed.
- The original two-second failure improves from 0.159713689 kg m²/s angular-
  momentum drift to 5.31e-14 with the same initial state and 480 ticks.
- Ten seconds forward plus ten seconds with reversed velocity returns within
  8.70e-12 quaternion distance and 1.51e-11 rad/s world-spin error.
- Three tilted, spinning prisms strike the floor and settle within the existing
  8–10 s quiet gates. Maximum independent source overlap is 0.500977 mm.
- The previous six isolated controls and two 32-body gallery rooms pass again.
  Fifteen props react across 33 dynamic pairs; impact energy never exceeds its
  initial value. Rest-case growth is 7.12e-7 J, inside the unchanged energy gate.
  Maximum source overlap is 0.501000 mm, below the unchanged 2 mm gate.
- Across these 13 cases, all 19,693 normal/sanitizer scene snapshots agree exactly.
  The retained sphere/cube crossing scene reproduces all 2,401 frames exactly.
  Native regression passes 38 checks and immutable-result regression 49 checks;
  existing resistance, world and contact-cache contracts also pass.

Fresh portable free-brick, tumbling-floor and gallery-impact bundles pass native
replay and optiC preflight. Full exact-tick source-vertex playback is verified
separately; see the retained qualification report for measured errors.

## Reproduction and limits

Build Ball's `rigid-gyroscopic-rotation-contract` target in a fresh output root.
`tests/test_rigid_gyroscopic_reference.py` checks an independent RK4 reference;
`tests/qualify_rigid_gyroscopic_scene.py` checks native nonprincipal controls,
reversal and floor impact with explicit normal/sanitized producers and existing
verified assets. Existing `tests/qualify_rigid_convex_gallery.py` reruns the
32-body matrix. optiC `tests/test_rigid_elastic_playback.py` checks portable poses
and original source vertices at every tick. Each script's `--help` lists the
required source, producer, asset and output arguments.

This closes the selected convex scene owner's free-tumbling defect. General
concave/compound scenes, CCD, arbitrary stacks and arbitrary extreme inertia/spin
remain unqualified. The next useful qualification is initially tumbling,
off-center object/object impact with several static obstacles inside the existing
32-body profile. A more complex scene or new movie is a subsequent step.

All three portable bundles now pass full optiC playback: 7,203 exact ticks and
15,587,292 source vertices. Maximum coordinate error is 7.96e-13 m, below the
unchanged 2e-12 m gate. Source result packs remain byte-identical after readback.

## Subsequent tumbling contact diagnostic

The subsequent [object/object qualification](rigid_tumbling_impact_diagnostic.md)
finds a separate accepted-contact-placement defect in total angular momentum.
This limits contact-scene qualification; it does not invalidate the isolated
free-rotation conservation, convergence and reversal results above.

## Contact-placement successor

The subsequent [contact-placement repair](rigid_contact_placement_repair.md)
closes the saved free-pair angular-momentum defect while preserving the original
resting limits. Anchored stabilization, the motion-budget refusal and the changed
top-heavy toppling expectation retain the explicit boundaries described there.
Historical measurements and movies above are unchanged.
