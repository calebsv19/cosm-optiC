# Retained swept-impact response and continuation

Later October23: the retained centered zero-restitution refusal is now covered by
[bounded resting continuation](rigid_resting_continuation.md). The 35-control
checkpoint below remains historical; the current response matrix has 36 controls.

October 23, 2026 — bounded Main Edit source/headless qualification. The three
retained fast/thin plate failures now produce physical impacts and continue
through the remaining tick. This extends staged safe refusal for an isolated,
zero-gravity face-impact profile; unsupported motion remains a safe refusal.

## Runtime boundary

Ball owns the new `src/interop/rigid_scene_sweep_response.c` module. Shared
core_scene/core_pack contracts and optiC result ownership are unchanged.
The classified donor selects a future endpoint-clear impact with a certified
face normal. Initial touching/overlap and ordinary endpoint contact retain
their existing solver paths. Existing displacement admission runs first.

Response requires exactly two bodies, zero gravity, translation-only supported
mesh compounds and an unambiguous positive-area face patch. The selected
normal must support a separating plane for the whole compounds, including all
children. Larger coupled groups, gravity-driven swept responses, unsupported
features and unresolved normals refuse before publication.

The cast stops within its distance tolerance. The response owner advances by
actual elapsed time to that point plus measured support gap divided by closing
speed. The resulting impact time must lie within the current tick. The retained
pose must have coincident support planes within the existing 1e-12 m coordinate
roundoff check. Bodies are never projected onto a contact plane.

At that pose, the existing manifold solver consumes the clipped face patch,
unchanged iteration budget, min restitution, geometric friction and existing
velocity threshold. Position/angular correction and warm start are disabled
for this cold impact. Assertions verify that the impulse solve changes no COM
or orientation. Existing angular resistance remains with its original owner.
The impact contact and impulse remain visible in the exact-tick result even
when the bodies finish the tick separated.

## Validated remaining time

Let `n` point from A to B. Continuation requires

```text
(v_B - v_A) dot n >= R_A |omega_A| sqrt(I_A,max/I_A,min)
                       + R_B |omega_B| sqrt(I_B,max/I_B,min)
```

Static bodies contribute zero rotational travel. With a whole-compound
separating plane at impact, this conservative rotational surface-speed bound
certifies a nondecreasing gap throughout the remaining free segment. The
existing free-rotation integrator advances the actual orientation. There is
no endpoint-only fallback when this certificate fails and no manufactured
elapsed time or post-impulse center correction.

Cumulative pre/post-impact translation and rotational travel must fit the same
per-body displacement budget; final collider bounds must remain inside the
original partition envelope. Energy and momentum diagnostics must be finite.
Elastic energy uses the unchanged 0.1% gate; closed two-dynamic-body momentum
and angular momentum use 1e-8 absolute gates. Source overlap retains the 2 mm
qualification limit. Static bodies remain byte-identical. Unsupported or
failed response/continuation discards the staged world, body/cache state and
clock together. The existing event/contact capacities are unchanged; this
owner handles one impact followed by certified separation.

## Qualification

Thirty-five retained/new controls pass normally, on repeat and under ASan/UBSan:

- The original 5 m/s, 50 m/s and near-budget plate misses now complete all four
  ticks with a reflected trajectory. Independent impact-position, velocity,
  energy and transformed source-mesh SAT checks pass.
- Equal/unequal finite masses, half restitution, tangential motion, a common
  translation, reversed pair order and the near-exit case pass. Ten current
  motion stores seal and independently verify/replay.
- Unresolved edge geometry, a third body, gravity outside this profile and a
  selected zero-restitution continuation fail safely. The last refusal occurs
  after impulse computation, proving rollback of partial response work.
- Each accepted impact also passes an injected later-tick travel refusal with
  byte-exact rollback of the previously accepted state. All four natural
  refused stores publish neither head nor result revision.
- The other 19 original diagnostic histories and two initial-contact controls
  remain exact. Existing 38 runtime and 49 result checks pass.

The unchanged physical gates are 0.1% elastic energy error, 1e-8 closed
momentum/angular-momentum error and 2 mm source overlap. The donor 0.5 µm spatial
gate is reused for trajectory position and supplemented by a 1e-6 m/s analytic
velocity gate. Independent output geometry and analytic
trajectories supplement the runtime continuation certificate.

## Development reproduction and limits

Build `rigid-scene-native-build`, `rigid-scene-cache-io-build` and
`rigid-fast-motion-observer-build` into fresh normal and sanitizer roots.
Run `tests/diagnose_rigid_fast_thin.py` with both builds, then
`tests/qualify_rigid_swept_response.py` with that diagnostic root and the
retained original fast/thin baseline. This is a development qualification
workflow and requires the retained baseline. Exact legacy replay and optiC
pose/source-vertex comparison remain separate gates.

This does not qualify general rotating CCD, many-body swept response,
gravity-driven swept impacts, repeated contacts without a separating-plane
certificate or general concave motion. Existing supported legacy behavior is
preserved. The next bounded extension is a resting-continuation certificate for the
retained zero-restitution refusal. Gravity/work-energy and coupled response
remain later work.

Source remains uncommitted Main Edit development. Program/shared versions,
canonical source, installed apps, packages and releases are unchanged. Patch
release recommended for the collision repair; approval required before bump.

## Legacy and playback closure

All 15 original bracket/arch physical histories reproduce 6,735 complete frames
exactly in final normal and sanitizer builds, preserving the previously qualified
physical gates. The 25 old/new immutable playback scenes pass 6,785 ticks and
866,616 source-vertex checks per build. Maximum vertex error is 1.333e-15 m against
the unchanged 2e-12 m gate; source pose packs remain unchanged. Existing legacy
bundles remain at their original identities; ten new response stores carry the
current engine identity and independently verify after relocation.

Qualified impact controls use frictionless faces. Authored friction and angular
resistance still use their existing owners; a broader swept-friction response
matrix remains separate.
