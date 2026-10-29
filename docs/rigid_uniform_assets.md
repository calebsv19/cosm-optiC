# Curated uniform-scale rigid asset preparation

Retained Main Edit source adds `rigid_scene_native prepare-uniform REQUEST NEW_DIR`.
The supported profiles are exactly bracket0.55 and U-channel0.85/0.52. Original
unit-size preparation remains unchanged. Arbitrary meshes, nonuniform scale,
other scale values and repeated scaling are refused.

```json
{"schema":"ball_rigid_uniform_request_v1","parent":"channel","scale":0.85,"asset_id":"large_channel_asset"}
```

`parent` selects `l_bracket_v1` or `channel`; all four fields are required.
The compiler reconstructs the unit parent through the existing preparation and
readback routes from embedded pinned source/artifact bytes. It bakes source and
child geometry, transforms COM bys, unit-mass inertia bys² and volume bys³,
and independently recomputes the derived source mass properties. The derived
source, visible mesh and collision artifact have new identities. The original
parent source SHA remains in the artifact and both parent source/collision SHAs
are reported in inspection.json. Original parent bytes/results are preserved.

Every native load reconstructs the expected derived payloads and compares all
three source.stl/visible.json/collision.bcr3d files byte for byte. Bundles need no
mutable external parent. The channel retains its three exact source-region
certificates at the new scale, separately from its twelve collision children.
Source-overlap, travel, contact/energy qualification and rollback gates remain.
Runtime objects still use world COM, quaternion orientation and baked scale1.

The normal and ASan/UBSan builds each pass48 affine/property/identity/certificate,
occupied-output, tamper and request-refusal checks,68 original exact-asset checks
and38 native runtime checks.49 immutable-store checks pass. No shared API or
schema extension, app VERSION change, installed-app refresh or release follows.

The six-body Kinetic Machinery Spill preserves the authored arrival layout and
all requested sizes. Its physically qualified prefix ends at tick587 (2.445833s,
588 exact states at240Hz). This includes real airborne pair contact, both large
forms striking the arch and initial landings. Normal/SAN poses, exact native
replay, source/proxy SAT, energy, applied-impulse/Coulomb, rollback and optiC
playback pass for that prefix. The unchanged0.1% cumulative positive numerical
energy gate first fails at588; native continuation later refuses1218. The longer
history is failed qualification evidence. A15-second result or late settled
aftermath is not qualified. Fixing that separate solver boundary is future work.

optiC reads the existing data-only artifact/result format. No renderer source
change or rebuild is required merely to read these derived bundles. Render the
covered prefix only, from its pinned source meshes and exact poses.
