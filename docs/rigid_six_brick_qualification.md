# Six-brick resting structure and heavy impact

The local development rigid-scene workflow qualifies two crossed layers of three
fully dynamic bricks. It proves initial stability, finite-mass impact response,
accepted mesh separation and ordinary optiC playback. The structure shifts and
rotates slightly but remains largely stacked; this is not a toppling/fracture proof.

## Fixed fixture and acceptance

The eight-body profile contains a static 12 × 8 × 0.4 m floor, six 2 kg bricks
(0.9 × 0.28 × 0.3 m), and an 8 kg cube projectile. Lower bricks run along X;
upper bricks run along Y. Adjacent centers are 0.285 m apart, with 5 mm gaps.
Centers of mass start at Z = 0.15 and 0.45 m. Every brick has positive mass,
zero initial velocity and simulation ownership from tick zero. There is no
sleeping, freezing, scripted activation or static-to-dynamic conversion.

The primary projectile has 0.8 m sides and starts at (-2, 0.12, 0.42) m. The
rest control has zero launch velocity; the impact control launches at 4 m/s
along +X. Both run 1,200 ticks (five seconds) at 240 Hz under gravity
(0, 0, -9.81) m/s². Restitution is 0.05; floor/brick friction is 0.6 and
projectile friction is 0.15, using the existing geometric mixing rule.

`tests/rigid_six_brick_limits.json` declares the numerical limits before
measurement: 2 mm full source-mesh overlap, 5 mm individual rest drift,
2 mm final-window drift, bounded linear/angular speed, orientation and kinetic
energy, unchanged static state, unit quaternions and no more than 0.5 J energy
growth. Impact additionally requires an initial resting interval, an impulse
path through the brick contacts, at least 2 cm movement of every brick and a
finite projectile velocity response. Geometry uses independent transformed
source vertices and convex SAT, with AABB rejection only as a conservative filter.

A smaller 0.4 m cube is retained as a narrow offset diagnostic, selected with
`--projectile-side .4`. After repair it passes overlap/response checks, but the
far brick moves only about 4 mm, below the same 2 cm per-brick movement gate.
That diagnostic is explicitly not a passing whole-structure qualification.
The broader strike passes the original numerical limits; none were relaxed.

## Repairs exposed by the qualification

1. **Coupled full-solid recovery.** Finishing one dynamic pair could leave a
   neighboring pair overlapping by 11.405 mm. The scene adapter now revisits
   all pairs with a dynamic body together using full convex penetration queries
   and existing conservative broadphase bounds. Position correction is weighted
   by inverse mass. The fixed budget is 64 global sweeps plus a validation-only
   sweep, targeting the existing 0.5 mm slop plus 1 µm query tolerance. Failure
   refuses the staged tick. Velocities, impulses, orientations and static-body
   bytes are preserved. The previous static-only helper is replaced by
   `src/interop/rigid_scene_contact_recovery.*` in Ball Bounce.
2. **Pair-specific cached normals.** The compound world previously considered
   the first active cached normal regardless of its body identities. It now
   requires the exact unordered body pair before using that normal. The broad
   strike exposed the old defect as displacement recovery without the correct
   impact impulse. An unrelated-cache poison regression fails against the old
   source and passes after repair. P5b's separate cold solver is unchanged.
3. **Shared-edge render remapping.** A local BLAS hit on a triangle diagonal
   could select a triangle rejected after world transformation, while its
   coplanar neighbor accepted the world ray. Advancing distance skipped the
   neighbor and lost the surface. In-range remapping failures now recheck that
   instance's authoritative world triangles. Existing out-of-range/self-hit
   retries remain. Tick 3 of the broad projectile reproduces the former miss.
4. **Local workflow budget.** The producer now accepts the same explicit maximum
   300-second transaction budget as optiC preparation. The default stays 60
   seconds; deadlines, immutable results and head compare-and-swap remain intact.
   Values outside (0, 300], including NaN/infinity, refuse without changing head.

These are app-owned numerical, tracing and workflow changes. Shared schemas,
shared versions, body capacity, materials and the renderer's optical laws are
unchanged. Standalone geometry-test recipes now include the existing reflection
normal dependency and explicitly stub the unbound rigid adapter in geometry-only
fixtures; the real consumer is exercised by the separate playback qualification.

## Measured result

All 68 primary checks pass normally and under ASan/UBSan. All 2,402 snapshots
(19,216 body poses, including contacts) match exactly between those builds.
The largest rest displacement is 0.995 mm, with final two-second drift below
2.3e-13 m. Before impact, every brick remains within that same settling bound.

The first projectile impulse occurs at tick 78 (0.325 s). Its forward speed
changes from 3.11586 to 1.61037 m/s in that tick; all six bricks move roughly
0.136–0.186 m and exhibit rotational response. Maximum accepted source-mesh
overlap across the two cases is 0.501 mm, within the unchanged 2 mm gate.
Total mechanical energy never exceeds its initial value in either case.

Both immutable results pass producer replay and portable optiC preparation.
All 2,402 ordinary native evaluation samples preserve exact positions,
quaternions and ticks. All 691,776 triangle-vertex samples agree with independent
source transforms within 4.45e-16 m. Sixteen selected views across two cameras
pass 1,769,472 explicit flattened/TLAS comparisons and 165,888 independent
camera/source-ray and material-pixel checks; worst reference depth error is
below 1.2e-14 m. The second camera avoids projectile occlusion for image review.
These checks establish source/native/render behavior; human acceptance is separate.
Qualification receipts remain `ok: false` during partial work and only become
passing after every requested case finishes; forced-backend failures verify this
for both physics and playback tools.

Existing native runtime/rollback, immutable-result, contact/cache/friction/world,
previous resting/head-on/offset and BR6 pillar regressions pass. The immutable
result suite now has 49 checks, including the explicit budget boundary. Existing
headless preflight/image-export and three surrounding geometry suites pass.

## Repeat locally

Use the fresh-build instructions in [the local workflow](rigid_scene_workflow.md).
The qualification does not choose an installed backend or store implicitly.

```sh
python3 ray_tracing/tests/qualify_rigid_six_brick_scene.py \
  --backend <build>/rigid_scene_native \
  --parent <retained-pillar-scene>/scene.json \
  --out <fresh-physics-proof> --projectile-side .8
```

The parent supplies only the scene envelope/camera/light context; the test
prepares its own assets and physical bindings. Use an explicit optimized native
build for the eight-body case. The qualified builds used Clang `-O2 -g`, with
`-fsanitize=address,undefined -fno-omit-frame-pointer` for the sanitizer variant.
`--wall-seconds 600` allows a bounded per-call sanitizer diagnostic budget;
this does not change the producer transaction's 300-second maximum.

`tests/test_rigid_six_brick_playback.py` accepts explicit renderer, consumer,
pixel-probe, pack-reader, portable-scene parent and fresh output paths. Its
`--views-only --camera-position X Y Z` mode adds a camera review without claiming
another full tick/vertex qualification. Review colors use existing typed material
edits and do not modify the physical input.

## Remaining boundary

This is a five-second, two-layer, eight-body, one-convex-child, discrete-step
qualification. It does not establish general piles, tall towers, spherical shots,
CCD, fracture, sleeping/waking, fluid interaction or a new body-capacity limit.
A narrower/taller six-brick arrangement or a spherical projectile is a separate
next qualification. Canonical adoption, production workers and release remain
separate from this local development result.

A later [three-layer toppling qualification](rigid_toppling_qualification.md) proves
upper-layer overturning and falling; its post-impact quiet-position gate remains open.
