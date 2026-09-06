# Managed STL assets and per-object shading

This is a backend workflow in Main Edit. It reuses `scene_runtime_v1`,
`mesh_asset_runtime_v1`, the shared mesh compiler, and the existing runtime mesh
path adapter. It does not require a new editor panel or change legacy scenes.

## Ownership and contract

The directory containing the runtime scene owns managed files. For project
use, keep the active runtime scene at the project root. Import copies source
bytes; the original external path is not a loading dependency. Retain the whole
project directory when moving or backing up a project.

`extensions.ray_tracing.managed_mesh_assets` contains:

- `schema`: `optic_managed_mesh_assets_v1`;
- `smoothing_enabled`: boolean, initially true;
- `assets`: map from stable asset IDs to retained source records.

Each asset records `source` (scene-relative STL path), `source_sha256`,
`original_name` (informational basename), `default_shading`, and `import`.
The import recipe records positive `source_to_asset_scale` (source units to
meters), `weld_tolerance` in converted coordinates, and `weld_vertices`.
Defaults are scale 1, welding enabled, tolerance 0.000001, and flat shading.
No topology repair or manifold/closed-volume claim is made at intake.

Each managed mesh instance stores `extensions.ray_tracing.managed_mesh`:

- `asset_id`: stable reference into the catalog;
- `shading`: `"inherit"` or `{ "mode": "flat|smooth|crease_aware",
  "crease_angle_degrees": 60 }`;
- `compiled`: generated runtime/authoring paths and hashes, runtime ID,
  normal provenance, and complete recipe including compiler executable digest.

Angles must be finite and greater than zero through 180 degrees. Inherit uses
an asset's saved default. Scene smoothing OFF resolves every managed object to
flat without erasing its choice. Unmanaged objects are unaffected. The backend
writes the resolved derivative into `geometry_ref` and the existing
`extensions.line_drawing.runtime_mesh_path` adapter. The renderer therefore
consumes actual generated normals, not a flag it does not implement.

The stable logical asset ID is separate from each compiled runtime ID. Two
instances may share a retained STL and use different normal variants. Position,
rotation, material, visibility, other extensions and unrelated objects remain
unchanged. The original STL remains immutable; no silhouette refinement occurs.

## Callable API and CLI

Build the compiler with `make BUILD_TOOLCHAIN=clang smooth-mesh-runtime-compile-tool`.
The compiler binary is under `build/toolchains/clang/<arch>/tools/smooth_mesh_reflection/`.
Its existing fixture-oriented name is retained; it accepts ordinary STL authoring
recipes through the shared production mesh compiler.

Import into an existing mesh instance:

```sh
python3 tools/managed_mesh_assets.py apply \
  --scene /path/to/project/scene_runtime.json \
  --compiler build/toolchains/clang/arm64/tools/smooth_mesh_reflection/compile_runtime_fixture \
  --source /path/to/dragon.stl --asset-id dragon --object-id dragon_object \
  --default-mode smooth
```

For a wrench, select `--default-mode flat`. For mixed curved/hard surfaces,
select `--default-mode crease_aware --crease-angle 60` and validate the result.
Use `--scale 0.001` for millimeter coordinates. The backend does not guess units.

Change one instance by omitting `--source`, providing `--object-id`, and selecting
`--shading inherit|flat|smooth|crease_aware`. Bind another existing mesh object to
an asset using `--asset-id`. Toggle only managed scene objects using
`--smoothing on|off`. Every apply compiles/validates the effective variants before
publishing the scene; it does not modify an in-memory running renderer.

Python callers import `update` and `status` from `tools/managed_mesh_assets.py`.
`update(..., spawn=<complete mesh_asset_instance dict>, asset_id=...)` creates an
instance from a caller-authored template. The template supplies transforms,
material references and normal scene fields; duplicate object IDs are rejected.
A source intake requires a binding or spawn template so compilation is exercised.

```sh
python3 tools/managed_mesh_assets.py status --scene /path/to/project/scene_runtime.json
# Rebuild missing derivatives from retained sources:
python3 tools/managed_mesh_assets.py apply \
  --scene /path/to/project/scene_runtime.json \
  --compiler build/toolchains/clang/arm64/tools/smooth_mesh_reflection/compile_runtime_fixture
```

Status reports `legacy_unmanaged`, `ready`, or `rebuild_required`, with effective
per-instance settings. Pass `--compiler` to include compiler drift in this check.
A changed/missing retained source is an error; a missing derivative requires
rebuild. Corrupt existing immutable derivative files are not silently overwritten.
Preserve/quarantine a corrupt file explicitly before rebuilding it.

## Persistence, portability, and compatibility

Dependencies are written first into content-derived paths; the scene JSON is
atomically replaced last. Compiler or validation failure preserves the previous
scene. Interrupted imports may leave unreferenced immutable files; there is no
automatic cleanup. Backend writers share a file lock and detect scene changes
before replacement. Do not concurrently save this same scene in an editor:
external writers do not participate in that lock.

Normal editor overlay writeback preserves the managed catalog. New backend
changes become visible when the scene is reopened/reloaded. Existing prepared
render state is not hot-swapped. Direct manual changes to stored policies require
backend apply before rendering; the C renderer consumes the resolved sidecar.

The existing project validator includes managed retained sources and active
variants in its content manifest. Legacy explicit scenes remain compatible.
This is an additive runtime authoring extension, not a rewrite of the LineDrawing
source scene. If an upstream authoring tool recompiles the scene from scratch,
it must carry these extensions/instance bindings forward or reapply the managed
operation; that upstream editor integration is not implemented here.

Moving the whole project preserves these assets. Existing render-only worker
exports are not automatically rebuildable authoring archives: they can carry
compiled meshes without source STLs. Keep the complete project for rebuilds.
Other external textures, volumes and simulation dependencies remain governed by
their existing project contracts; managed STL intake does not collect them.

## Verification and boundaries

`make BUILD_TOOLCHAIN=clang test-managed-mesh-assets` proves two instance modes,
scene toggle restoration, failed-compile atomicity, invalid angle rejection,
removal of an external source, project relocation, deletion of derived meshes,
byte-identical rebuilding, and a real headless render with route readback.
`runtime_scene_bridge_writeback` covers editor catalog preservation.
Artifacts are under `build/managed_mesh_proof/`.

Shared reuse decision: reuse-adopted for existing mesh asset/compiler and scene
extension contracts; project intake policy and CLI remain app-owned. No shared
API or module version changes. Complex-topology smoothing corrections and new
editor controls remain separate work.
