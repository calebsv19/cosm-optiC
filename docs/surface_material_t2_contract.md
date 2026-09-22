# T2 resource workflow and preparation

T2 is implemented in isolated source. Main Edit adoption remains pending explicit
operator authorization. This is local source/native proof, not an installed or
published release. App 0.16.0, worker 0.7.1 and shared authored texture 0.7.1 remain
unchanged. T3 composition is next.

## Resource ownership

The optiC adapter owns a process-local immutable cache of image pyramids and
procedural chart bakes. It reuses the existing shared surface-pyramid math.
Images are keyed by SHA-256, channel and color/data interpretation, independent
of path names. Bakes are keyed by their effective program, base material, seed
and period. Object transforms and normal/height strength do not rebuild resources.
Validation retains unique candidate images and preparation reuses those pyramids.
File pins are verified even on warm loads; cached bytes never conceal corruption.

The 128 MiB limit counts unique resident pyramid values, including resources held
by active and staged generations. Decode/build scratch storage is additional.
Only unreferenced resources are eligible for least-recently-used eviction. A
budget failure cannot evict a live resource; T0 publication/recovery rules remain
in force. Cache counters report decodes, image/program builds, hits, resident and
live bytes, and evictions. Per-hit evaluation borrows prepared data and performs
no file IO, decoding or allocation. Scene capacity is now 128 objects; its
capacity-based editor/import/material storage was rebuilt together.

Ordinary transform edits still validate and reapply the retained scene. T2 avoids
decoding and baking during those edits; it does not claim constant-time document
editing or eliminate digest checks at preparation boundaries.

## Inspector workflow

For a mapped brick/solid material, open **Sources** and select Base color,
Roughness, Normal or Height. Select/Relink imports a PNG into
`assets/materials/<sha256>.png` beside the retained scene. Base color supports
sRGB/linear; other channels require data encoding. Normal and Height remain
mutually exclusive. Strength, height in meters and integer repeat periods use
transactional retained edits with rejected drafts preserved.

The card shows its path, digest, interpretation and explicit health check.
Checking a resource performs IO only on request. Relinking retains prior channel
metadata, including replacement provenance. Imports stage, sync and verify bytes
before atomic publication. Relinking original bytes can repair a corrupt or
missing pinned file. Symlink destinations are refused. Undo restores the source
declaration; imported immutable files remain available for redo and other objects.
File chooser results are guarded by scene path, revision, object identity and
render activity. Hidden/clipped controls cannot receive input.

M5 combinations remain bounded: PNG dimensions are powers of two up to 1024,
files are at most 16 MiB, and chart periods are integers 1–32. Graph, manifest and
region bindings require their existing workflows; T2 does not silently convert
them into image channels. Mixed image/procedural graphs remain T3.

## Portable projects

M5 paths may be absolute (existing scenes) or relative to the scene file's
parent directory. Relative paths reject parent traversal and symlink escape.
JSON-only callers must set an explicit scene-path context; file APIs scope it
automatically. CWD never supplies an implicit image root.

Create a new portable bundle with:

```sh
python3 tools/surface_material_resources.py bundle \
  --scene <project>/scene_runtime.json --expected-sha256 <scene-sha256> \
  --output <new-bundle-directory> \
  --renderer build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless
python3 tools/surface_material_resources.py verify \
  --bundle <bundle-directory> \
  --renderer build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless
```

The tool preserves unknown retained metadata, deduplicates image bytes, includes
self-contained meshes and generated-UV support, rewrites runtime dependencies to
content-addressed relative paths, validates and publishes create-only. It reuses
shared dependency-manifest v2 vocabulary and existing app publication helpers;
its receipt is app-owned, not a shared compiler export receipt. Unsupported
managed catalogs, curves, procedural-solid and authored-manifest dependencies
are rejected rather than emitted as incomplete portable bundles.

## Generated-UV candidate workflow

```sh
python3 tools/surface_material_candidate.py \
  --scene <project>/scene_runtime.json --expected-scene-sha256 <scene-sha256> \
  --source <source.obj> --expected-source-sha256 <obj-sha256> \
  --object-id <object-id> --output <project>/uv_candidate.json \
  --compiler build/toolchains/clang/arm64/tools/smooth_mesh_reflection/compile_runtime_fixture \
  --renderer build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless \
  --method box --scale-m 1 --uv-set-id generated_uv
```

This composes deterministic projection, compilation, unchanged-geometry checks,
runtime validation and a preview render. It emits a same-directory candidate,
preview and `.receipt.json`; it does not adopt. Inspect the preview, then choose
**Sources → Adopt reviewed UV candidate…** in the saved original scene.
Adoption verifies reviewed state, geometry flag, scene baseline/current bytes,
exact candidate bytes and preview bytes, then uses the existing atomic saved
command with Undo/Redo. Stale or dirty inputs leave the document unchanged.
`--preflight-only` emits a distinct receipt which cannot pass reviewed adoption.
This is bounded planar/box projection of a matching OBJ, not general atlas unwrap.

## Verification

Ignored receipts under `build/surface_material_t2/` record:

- `acceptance-07/acceptance.json`: real cards/numeric controls, stale/locked guards,
  relink, corrupt/missing pin repair, symlink refusal, duplication, Undo and
  fresh-process reopen; normal/narrow native captures, independent cache/budget accounting.
- `candidate-01/acceptance.json`: stale source/candidate/preview rejection,
  receipt type/state guards, saved adoption, Undo and Redo.
- `tools-03/acceptance.json`: nine tool tests and actual projection/compiler/
  preflight/preview; relocated primitive and UV bundles render after source
  relocation, with generated-UV dependency closure.
- `t1-crash-regression-01/acceptance.json`: all seven node kinds and the graph
  workflow from the reported earlier null-input crash, plus fresh-process reopen.
- `m5-01/acceptance.json`: eleven M5 cases, flattened/TLAS equality, negative
  preflights and an 8,192-triangle fixture. The budget fixture uses distinct seeds
  because identical procedural bakes now share memory.
- `curved-01/acceptance.json`: planar, cylindrical and spherical basis/filter
  checks; foundation, pane-host and mesh-preview shading targets also pass.
- `m3-01/acceptance.json` and `legacy-01`: region/manifest authoring, reopen
  and all eight frozen legacy render hashes are unchanged.
- `lifecycle-01/acceptance.json`: T0 early/late failure, restore, history and
  before/after publication guarantees.

The measured 100-object fixture decodes one shared image, builds one image pyramid
and one program bake: cold 147.03 ms, warm 57.18 ms; transform-edit p50 45.59 ms,
p95 49.47 ms; 2,818,032 live bytes, 2,839,872 resident bytes after recovery, and one
recorded eviction. Warm loads, transform edits and viewport frames decode/build
zero new resources. These are local fixture timings, not a general performance
promise. Native OS chooser completion and human hands-on acceptance remain
unverified; native commands, panel interactions and guarded adoption are tested.
