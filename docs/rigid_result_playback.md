# Immutable rigid result playback (BR4 development)

optiC can consume an accepted Ball rigid result through its normal evaluated scene
and native geometry paths. Ball owns numerical production; rendering and seeking
never link or invoke Ball physics. This is local Main Edit source capability,
not an installed/public release or a claim of broader physical accuracy.

## Admit an accepted result

Keep the BR3 `ball_rigid_result_bundle_v1` directory intact. Bind its explicit,
previously accepted `result_revision`; discovery of a mutable store head is not
implicit acceptance. The runtime scene retains its original `extensions.rigid_scene`
physical initial conditions and adds:

```json
{
  "extensions": {
    "ray_tracing": {
      "rigid_result": {
        "schema": "ray_rigid_result_binding_v1",
        "bundle_path": "results/<accepted-revision>",
        "result_revision": "<64 lowercase hexadecimal characters>"
      }
    }
  }
}
```

`bundle_path` resolves relative to the current scene file; an explicit absolute
local path is also supported. Use normal mesh instances and their existing
`extensions.line_drawing.runtime_mesh_path` references to the accepted visible
mesh sidecars. Cameras, lights and render materials come from the current scene.
No `phase43`, special room-wall names or two-body slots are required.

Admission captures bounded regular files, checks every inventory hash and the
caller-selected revision, refuses symlinks/unlisted files, reads the one `RPOS`
chunk using core_pack, validates the shared result profile, and matches both the
original and current physical projections. Loaded visible vertex coordinates,
triangle indices and source/mesh identities must agree with the accepted source
mesh. Unhide/reload repeats that validation. Hidden physical objects retain their
recorded poses and collision meaning while omitting visible geometry.

A changed physical role, owner, source/collision identity, initial state, COM,
principal frame, mass/inertia, gravity, physical contact material or time domain
refuses reuse. Competing object tracks/hierarchy/constraints also refuse.
Camera/light/render-material/visibility edits and object/binding order do not
invalidate physics. The binding must be reloaded through the normal file loader;
raw JSON application cannot bypass file-backed admission. A failed candidate
preserves the previously accepted scene and result.

## Exact time and geometry

Without an authored scene timeline, the accepted result supplies a 240/1 clock
and endpoint+1 frames. An authored presentation timeline may use another rational
rate and a nonzero start frame, provided each displayed frame maps to an exact,
covered source tick. For example, 30fps maps local frames0,1,19 to ticks0,8,152.
Local time starts at the presentation range's start. Fractional/uncovered source
ticks refuse; no interpolation, nearest-tick guessing or implicit continuation
runs physics.

Evaluated snapshot schema4 includes exact world COM, principal-to-world quaternion,
body ID, source tick, full result revision and physical input digest. Native frame
preparation rejects snapshots that disagree with the active accepted result.
For a source vertex `v`, the single mapping is:

`world_COM + R(q_body) * transpose(principal_to_source) * (v - source_center_m)`.

The source-to-world rigid affine map is computed directly; COM is not added twice
and source vertices are never rewritten. Existing mesh instance geometry,
normal/tangent transforms and prepared BLAS/TLAS consume that same map. Result
bindings count as executable motion for cache invalidation, so another time
cannot reuse static geometry. Reverse/random seeks always derive from source.
Headless summaries expose exact quaternion/body/tick/result provenance.

## Development dependency and verification

This additive reader uses committed, imported core_scene 1.11.0, the existing core_pack v1
reader and core_scene_compile SHA-256. Ray supplies its existing SHA-256 helper through a callback, so core_scene has
no dependency on the downstream scene compiler. The optional cJSON result adapter adds no
solver, renderer or UI dependency to the shared default archive. The current
native projection serializer matches BR3's cJSON producer; its reviewed source
identity is checked rather than applying a floating-point comparison tolerance.

`make/rules-rigid-result.mk` selects a reviewed imported core_scene when present,
or the CodeWork workspace source for development. Standalone developers can set
`RIGID_SCENE_CORE_DIR=<shared>/core/core_scene`. The checked
`tools/rigid_result_shared_inputs.json` refuses unreviewed source/serializer drift.
There is no manual vendor edit or broad shared subtree rollout. A build without
that API retains a clear unsupported-input stub for rigid scenes and preserves
ordinary rendering.

Use fresh contained build/output roots, preserving existing builds:

```sh
make BUILD_DIR_BASE=<fresh-build> ray-tracing-render-headless \
  rigid-result-consumer-probe rigid-result-profile-probe
make BUILD_DIR_BASE=<fresh-build> rigid-result-consumer-sanitize
python3 tests/test_rigid_result_consumer.py \
  --probe <build>/tests/rigid_result_consumer_probe \
  --profile <build>/tests/rigid_result_profile_probe \
  --renderer <build>/tools/cli/ray_tracing_render_headless \
  --pack-io <data-only-core-pack-helper> \
  --bundle <accepted-BR3-bundle> --out <fresh-proof>
```

Here `<build>` is the architecture/toolchain directory printed by Make. The
pack helper is BR3's data-only `rigid_scene_cache_io`, not a Ball backend. Native
integration checks cover every retained tick, preview/explicit-frame equality,
independent transformed vertices, current moving-body BVH/TLAS hits, immutable
seeks, physical refusal/rollback, presentation reuse and ordinary CLI renders.
The sanitizer target instruments the rigid modules and probe while reusing
surrounding renderer objects; it is not a whole-app sanitizer qualification.

## Remaining boundary

BR5 adds deliberate scene/runtime inspection and workflow controls for selecting
and preparing results. BR6 qualifies physical behavior and useful laptop scenes.
Prescribed/kinematic colliders, concave decomposition/static triangles, CCD,
gyroscopic precession, interpolation, remote trust/signatures and production
activation are outside this slice. The renderer does not certify a newly resealed
forged result: trust comes from the previously accepted expected revision.

See [Local rigid simulation workflow](rigid_scene_workflow.md) for preparation, native result selection, validation, playback controls and save/reopen.
