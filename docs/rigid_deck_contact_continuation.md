# Deck-contact continuation for complex rigid assets

The seven-body Orbital Repair Bay now records and independently replays **7,200
ticks / 30 simulated seconds**, with 7,201 exact states at 240 Hz. Normal and
ASan/UBSan producer trajectories agree exactly. This supersedes the retained
13.34-second result that refused tick 3,203; those older results remain valid
historical evidence.

The physical profile retains its measured child-pair collision manifold when
resolving static support. Previously a gravity support-band reduction could
replace the lowest contact with a raised corner or interior witness. Resolving
that point could leave the actual lowest feature closing, exhausting the
continuation budget. The legacy gravity-room support heuristic remains available
when `bound_static_support_to_contact_depth` is false. The field and wire schemas
are unchanged; its physical-mode meaning is documented in the world header.

No contact budget, solver tolerance, source geometry, gravity, friction,
restitution, or energy/overlap gate was relaxed. The same source-pinned bracket,
arch, seven-child wrench, twelve-child U-channel, probes and finite deck are used.

Qualification covers:

- fresh normal and ASan/UBSan native builds, six support-witness cases each,
  surrounding compound-world and support-patch contracts;
- all 7,201 states: exact native replay and normal/sanitized equality, independent
  source and collision overlap below 0.499 mm (2 mm gate), normalized rotations,
  and maximum per-tick travel bound 7.59 mm (25 mm gate);
- cumulative positive numerical energy 0.0452% of initial kinetic energy
  (0.1% gate), free-flight gravity oracles, actual applied-impulse momentum
  residuals below 5e-12 (1e-8 gate), and Coulomb admissibility;
- forced late failures at ticks 3,203, 4,800 and 7,200: exact rollback of bodies,
  world, caches, diagnostics, gravity continuation and clock, plus sticky fault;
- 13 retained wrench/channel controls, 38 native runtime checks in each build,
  49 immutable-store checks, and both optiC consumers checking 4,061,364 vertices
  each with maximum playback error 3.56e-15 m and replacement rollback.

The arch and both small probes satisfy speed below 0.02 m/s and spin below
0.05 rad/s throughout the final two seconds. The bracket and U-channel leave the
finite deck and continue falling. This is qualified continuation of this scene,
not a claim that every body settles or that the visible bay has physical walls.
The next presentation step can consume this exact 30-second result with a view
covering its full travel; containment or a longer physical run needs its own
qualification.

The [60-second result envelope](rigid_result_duration.md) and immutable storage
remain separate contracts. This is retained development-source qualification;
canonical adoption, installed apps, package versions, publication and new raster
render acceptance are separate.
