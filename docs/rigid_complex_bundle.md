# Complex rigid-result bundle development

The optional complex bundle reader recognizes `ball_rigid_result_bundle_v2` under
the existing `ray_rigid_result_binding_v1` scene reference. The caller explicitly
selects the expected immutable result revision. A manifest that merely hashes its
own contents is not independent physics or source-geometry qualification.

This is a development integration checkpoint. A native producer/replay-qualified
29-object Aquarium v2 recording now passes normal/SAN admission across all720
ticks, exact repeated source seeking, mesh identity and coordinate checks, sampled
affine verification and strict BVH/TLAS surface checks. A separate native timing
check verifies an explicitly source-clocked water cache through Hold and Resume.
A69-frame native collision-only preview also passes full sequence receipt,
constant-rate encoding and decode checks. This remains draft visual evidence;
final aquarium materials, synchronized-water video and visual acceptance are separate. Small BCR parser controls are separate compatibility evidence.

## Envelope and integrity

V2 permits 32 distinct assets, 256 artifacts, 256 MiB aggregate, 64 MiB per file
and a 256 KiB manifest. Pose JSON retains 64 MiB/four-million-node bounds. Body32,
contact496 and tick14400 limits remain. V2 permits 64 convex children; v1 retains
its eight assets, 31 artifacts, 64 MiB aggregate, 1 MiB ordinary-file reads and
12-child physical profile. Paths have exact roots `asset_0` through `asset_31`,
with no leading zeros. Duplicate/unlisted paths, symlinks, altered bytes and
noncanonical manifest identities refuse before current playback replacement.

Each v2 root includes a `ball_rigid_asset_certificate_v1`. Its engine, native
metadata, explicit collision-model declaration/null and file inventory are pinned
by the selected global revision. BCR metadata additionally undergoes the existing
independent artifact checks. Accepted VHACD, exact static-surface and explicit
approximate models cross-check their native manifests and source/visible/model
hashes against the certificate. The producer owns native geometry/source validation;
the renderer does not link a solver or claim a second geometry qualification.
Static metadata reference inertia does not change static zero physical mass/inertia.

The optional `integration_policy` in both the scene rigid contract and physical
input accepts only `bounded_subsegments_v1`. Absence retains the legacy policy and
projection bytes. This declaration is part of physical identity; a mismatched
scene/result refuses. Camera edits remain independent presentation changes.

## Current source-bound build

The default vendored core_scene and shared-input pin now select the reviewed
1.12.0 profile. Ordinary worker builds require no development path overrides.
The explicit source-bound development command remains available for diagnostics:

```sh
make BUILD_DIR=<fresh-owned-output> \
  CORE_SCENE_DIR=<workspace>/shared/core/core_scene \
  RIGID_SCENE_CORE_DIR=<workspace>/shared/core/core_scene \
  RIGID_RESULT_INPUTS=tools/rigid_result_complex_shared_inputs.json \
  rigid-result-consumer-probe rigid-result-profile-probe ray-tracing-render-headless
```

The module-only managed import is source-checkpointed. Worker package preparation,
installed-runtime readback and publication remain separate. Legacy bundle_v1
admission retains its original limits despite the broader shared result profile.

`tests/test_rigid_bundle_v2.py` consumes a caller-selected qualified BCR v1
fixture and exercises v1/v2 pose equivalence plus certificate/metadata/engine/
inventory/tamper and failed-replacement controls. Its synthesized v2 revisions
are data-only controls; they are never numerical acceptance artifacts. Use the
real producer's accepted v2 recording for the full typed-asset acceptance.

For explicitly attested approximate models, original display-source identity is
authenticated separately from the derived physical collision source. Strict
models retain shared source identity. V2 current-mesh verification has the same
64MiB per-file bound as bundle admission; v1 retains1MiB. Exact coordinate and
triangle verification uses linear linked-list traversal, without dropping entries.
Optical assembly must choose the accepted mesh carrier and remove redundant
primitive descriptions, which otherwise can generate competing geometry.

## Near-vertical recorded orientations

The consumer extracts roll coupled to yaw through stable matrix entries near
vertical pitch, avoiding amplified disagreement between nearly-zero rows. The
full purposeful29body recording passes normal/SAN independent vertex checks
(maximum2.665e-15m at the tested poses) with the unchanged2e-12m bound. Its
legacy/v2 attestation and rollback controls pass in both builds. The rollback
probe compares semantic object fields rather than unspecified C struct padding.
These remain source-bound development checks; PC package delivery is separate.
