# Non-cubic convex collision qualification

The development workflow has a bounded upright-prism qualification within the
existing 32-body / 496-pair profile. This is local native source/tool evidence,
not installed-app, worker, production or human visual acceptance.

## Shapes, controls and physical checks

Eighteen finite-mass bodies use elongated rectangular blocks, asymmetric
hexagonal prisms and triangular prisms. Each shape has unequal principal inertia
and one verified convex child. Four analytical spheres, four fixed pillars and
six room boundaries complete the 32-body room. All motion is produced by Ball;
optiC consumes exact immutable poses and verified source geometry. The energy
observer rotates world angular velocity into each body's principal inertia frame.

Each of the three non-cubic shapes has a two-second principal-axis free-rotation
control and a fully elastic off-center sphere-impact control, recorded at240Hz.
The coupled world requires at least two bodies, so free controls include a distant
static anchor. Pair controls use restitution1, zero friction/resistance and zero
gravity. Energy relative error stays below4.5e-16 and linear-momentum error below
2.5e-32kgm/s. The targets develop2.21–4.00rad/s angular speed. Principal-axis
rotation preserves both energy and world angular momentum within1e-10 gates.

The full room has rest and impact cases, each2400ticks/10seconds. Restitution is
0.94, Coulomb friction0.06, gravity9.81m/s² and floor/sphere rolling and torsional
resistance lengths0.0005m. These are declared scene materials, not a solver change.
Near-elastic normal response does not make the complete frictional room lossless.
Impact initial mechanical energy is208.98264J and final energy109.1537705J;
energy never exceeds the initial value.15 of18 non-cubic props move more than2cm,
and29 dynamic pairs interact. The existing2mm overlap gate is unchanged;
maximum independently measured overlap is0.500967605mm over all2401 room states.

The resting room passes all22-body speed, spin, full-inertia kinetic energy,
orientation-change, quiet drift and total-drift gates over seconds8–10, with the
same tolerances used for the earlier cube scene. The moving near-elastic room
is assessed for bounded energy, contact coverage and geometry; it is allowed to
retain physical motion after10seconds. No artificial stopping is added.

Normal repeats and ASan/UBSan trajectories agree across7688 recorded snapshots
(6×481 isolated controls plus2×2401 room states). Immutable producer publication
and native replay pass. optiC checks every2401 impact tick and15,039,864 source
vertices, with maximum coordinate error below2.94e-15m. Selected optimized-consumer
checks also pass; the rendering build uses ordinary-O2 without fast-math.

## Dated pre-repair rotational boundary

This original qualification covered upright extruded-prism impacts and
principal-axis free rotation. Its separate nonprincipal two-second diagnostic
conserved energy but drifted world angular momentum by about 0.1597 kg m²/s,
against a 1e-10 kg m²/s gate. The original world-spin orientation advection lacked
gyroscopic precession. The earlier movie and measurements retain that model.

The subsequent [gyroscopic repair](rigid_gyroscopic_rotation.md) closes that
selected native scene-owner defect with independent conservation, convergence,
reversal, tumbling floor impacts and repeat gallery qualification. This does not
retroactively alter the original eight cases or video. General concave/compound
bodies and CCD remain separate.

## Reproduction and portable delivery

Ball `tests/qualify_rigid_convex_gallery.py` uses
`tests/rigid_convex_gallery_fixture.py`. Supply explicit normal and sanitized
`--backend`/`--sanitizer`, a prior portable room with sphere source assets as
`--parent`, the companion optiC `--ray-tests` directory and a fresh `--out`.
The qualifier prepares all source meshes itself, verifies their unequal inertia,
records the six controls and two rooms, and uses the existing initial-placement
preflight. `tests/audit_rigid_larger_room.py --cases rest impact` independently
checks every source-geometry state. `tests/test_rigid_elastic_playback.py` compares
all native consumed poses and vertices with the immutable result.

The scene-only snapshot workflow now recognizes the typed immutable rigid-result
directory through `tools/scene_workflow_rigid_assets.py`. It copies only declared
artifacts after size, digest, containment, required-inventory and result-revision
checks. Historical producer paths inside sealed input documents are preserved
as provenance, not followed as current dependencies. Native rigid admission
remains the semantic authority. Snapshot and verify share the same helper;
changed artifacts, symlinks, escaping/absolute/reserved paths and revision drift
refuse. Eight focused custody/refusal checks and ten existing workflow tests pass.
The supported scene_delivery create and captured-r1 path now succeeds with a
self-contained immutable rigid result. It does not advance visual acceptance.

## Disney V2 presentation status

The captured candidate has a polished dark blue floor, a tinted mirror wall,
gold metal blocks, jewel-colored glossy triangular prisms, solid transmissive
hexagonal prisms and mirror spheres. Optics edits do not change physical inputs
or cached motion. Three visual boundary shells are hidden for the cutaway view;
all six physical room boundaries remain present. Two authored point lights and
a dark background keep the collision region readable. The authored16Hz timeline
samples every15th240Hz physics tick without interpolation or rerunning simulation.

The ten-second,160-frame,1280×720 Disney V2 MP4 is verified, rendered at32
passes with denoising. Native time audits verify exact15-tick sampling through
video frame159/source tick2385. Independent ffprobe and full ffmpeg decode verify
160frames,16FPS,10seconds and1280×720. Six selected PNGs and a cover remain;
all160 raw BMPs (589,843,520bytes) were deleted after verification.
Successful task-owned builds, duplicate stores/caches and private-site output
were also retired after a no-consumer check. Original captured source hashes
remain unchanged. Render plus completion took about5.68hours.
MP4 SHA-256: `77b6ff11e0ecccca787735396e8b533e4c496edda1884a2bd882cb2005ef9fe7`. Human visual approval remains separate. General package/UI/release gates
were not repeated for these fixture and Python delivery-tool changes; no native
renderer or solver implementation changed. No version was edited. A patch
release is recommended for the delivery-tool fix when release work is authorized.
