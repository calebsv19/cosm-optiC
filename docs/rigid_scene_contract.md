# Rigid scene contract inspection

BR4 now adds [verified immutable result playback](rigid_result_playback.md) in
the retained development lane. The inspection tool below remains metadata-only;
unbound generalized scenes still refuse execution.

BR1 adds a development-only read-only tool for the shared core_scene1.3.0 rigid
metadata contract. It does not add general physics playback to the renderer.
The contract source is the explicit shared source checkout, separate from the
renderer’s retained vendored runtime modules:

```sh
make BUILD_DIR=/private/tmp/<fresh-task>/ray ray-tracing-rigid-scene-contract-tool \
  RIGID_SCENE_CORE_DIR=<shared>/core/core_scene
make BUILD_DIR=/private/tmp/<fresh-task>/ray test-ray-tracing-rigid-scene-admission \
  RIGID_SCENE_CORE_DIR=<shared>/core/core_scene \
  RIGID_SCENE_PROOF_ROOT=/private/tmp/<fresh-task>/proof
/private/tmp/<fresh-task>/ray/tools/rigid_scene_inspect <scene.json> <asset_metadata.json>
```

Within CodeWork the tool defaults to the workspace shared/core/core_scene source;
standalone consumers must supply that source explicitly. This is research/tool
linkage, not a managed subtree adoption or a new renderer dependency. The tool
uses the renderer's existing cJSON implementation and the shared typed validator
and optional decoder. See `<shared>/core/core_scene/RIGID_SCENE_V1.md` for the
normative contract and synthetic metadata fixture.

A valid reply has `proof_layer: "contract_metadata_only"` and
`execution_supported: false`. It reports roles, motion owners, IDs, mass frames
and explicit consumer limits. Catalog consistency does not prove mesh bytes or
collision quality. Native production is BR2/BR3; verified optiC result consumption is now
implemented in the optional BR4 development reader.

Ordinary preflight and rendering refuse `extensions.rigid_scene` without an
accepted, compatible BR4 result binding before scene mutation or output frames. No implicit fallback to static authored poses occurs.
The older exact compound-scene bridge remains supported. Camera/light path
support is unchanged. There is no UI, installed app or worker rollout in this
contract checkpoint.

`tests/test_rigid_scene_contract_guards.py` accepts explicit native Ball,
renderer, Ball source root and fresh output arguments to qualify actual consumer
refusals. The shared admission matrix accepts multiple `--tool` paths to compare
native shared/Ball/Ray replies; physics and numerical replay remain separate.
