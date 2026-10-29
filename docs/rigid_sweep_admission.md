> Subsequent Main Edit source update: [retained swept response](rigid_swept_response.md) now handles the bounded isolated zero-gravity face profile. The safe-refusal checkpoint below remains historical.

# Staged classified-sweep admission

October 22, 2026 — bounded Main Edit source integration. Ball now consults the
classified compound cast before accepting endpoint-clear translational mesh
motion. A detected intervening impact refuses the staged tick; this slice does
not add a swept impact response solver.

The new `src/interop/rigid_scene_sweep_admission.c` module runs before anchored
and free contact partition advancement, using the same gravity-predicted velocity
and unchanged displacement budget. The free event loop also checks each remaining
segment after its ordinary impulse solve. Query inputs are copies; the preflight
never moves bodies, applies impulses, advances clocks or changes caches. The
existing staged owner discards a rejected tick in full.

## Bounded policy

The admission profile is translation-only mesh compounds with at most three
children, 256 vertices and 512 faces per child. Analytical spheres, spinning
bodies and larger representations retain their existing paths and are outside
this new sweep qualification. This does not certify general rotating CCD.

Existing initial touching/overlap treatment and endpoint contact response remain
with their current owners. A cast stops at its configured distance tolerance;
when the selected endpoint pair is independently touching/overlapping or within
that same tolerance, and the remaining path after the first impact is at most
two cast distance tolerances, the existing endpoint solver retains ownership. This avoids
refusing ordinary exact-endpoint landings. A failed endpoint query refuses.
No tolerance, travel envelope, contact capacity or event budget is increased.

| Classified endpoint-clear motion | Adapter result |
| --- | --- |
| Clear sweep | Continue through existing advancement |
| Future impact with certified face normal | `swept_response_required`, atomic refusal |
| Future impact with unresolved normal | `swept_response_normal_unresolved`, atomic refusal |
| Query failure | `swept_query_refused`, atomic refusal |
| Invalid/unhandled decision | `swept_classification_refused`, atomic refusal |
| Initial touch/overlap | Existing initial-contact policy |

A valid normal is necessary for a future response implementation; it is not
permission to use an endpoint impulse or accept a crossing. The adapter never
substitutes a witness normal or interprets query failure as a clear path.

## Retained failures and controls

The original diagonal plates at 5 m/s, 50 m/s and near the existing travel limit
now refuse their first tick with `swept_response_required`. The prior failed
trajectories remain historical evidence. All other 19 fast/thin controls preserve
their exact frame histories, classifications and refusal reasons. Normal,
repeat and ASan/UBSan results agree exactly.

Added controls verify the same crossing with two dynamic bodies, an unresolved
edge crossing, clear free motion, initial separating touch, initial overlap and an endpoint only 50 nm beyond the
far side of the plate. That last crossing also refuses; near-exit proximity
never becomes landing permission.
Both new crossing refusals preserve complete world, body and clock bytes; a
faulted owner cannot step again. Static body bytes remain unchanged on success.
The 38 existing runtime checks pass, including the flat-face restitution landing
that caught the cast-tolerance boundary during implementation.


## Legacy physics and immutable playback

All 15 retained bracket/arch bridge and compound-pair scenes reproduce their
original 6,735 complete frame histories exactly in normal and ASan/UBSan builds.
The independent physical gates were rerun without relaxation; final histories
also equal that audit exactly. This preserves their existing overlap, momentum,
angular momentum, energy, restitution and settling evidence without claiming
new swept-response physics. Current-engine immutable stores are sealed and
all 15 copied portable bundles verify independently. The three original refused
crossings publish no head or result revision and leave no running staging.

## Reproduction and next boundary

Build `rigid-scene-native-build`, `rigid-scene-cache-io-build` and
`rigid-fast-motion-observer-build` into fresh normal and sanitizer roots. Run
`tests/diagnose_rigid_fast_thin.py` into a fresh diagnostic root, then
`tests/test_rigid_sweep_admission.py` against that root and the retained original
fast/thin baseline. Add `--pack-io` to verify that all three rejected stores
publish neither a head nor a revision. The latter compares original accepted histories and verifies
rollback through the actual staged owner.

This is safe-refusal integration, not swept-response qualification. The next
bounded implementation is a response owner at a retained swept impact pose,
with physical impulse/conservation and overlap checks before continuing the
remaining time. It must preserve the same initial-contact policies, guards,
atomic rollback and optiC result playback. Spinning and larger-mesh sweep
coverage remain separate.

Ball owns the solver; shared core_scene/core_pack contracts and optiC numerical
ownership are unchanged. Source remains uncommitted Main Edit development.
No canonical/installed adoption, VERSION edit, package or release is included.
Patch release recommended for this safety fix; approval required before bump.

Every sealed legacy tick and 863,016 source-vertex samples per build pass the
normal and sanitized optiC rigid consumer, within the unchanged 2e-12 m gate.
The pose packs remain byte-identical during playback. This is geometry/playback
qualification; no new rendered-image or installed-app acceptance is claimed.
