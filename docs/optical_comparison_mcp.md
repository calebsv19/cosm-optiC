# Repeatable optical comparison matrices

The local `tools/optical_comparison_mcp.py` runner uses the live editor's MCP
catalog bridge and shared material, graph, Save As, Open and render commands.
It creates a separate portable project with a sealed baseline and one saved
scene per cell, renders each cell, and writes a labeled self-contained SVG
contact sheet plus `matrix.json` with source, response and image hashes.
The original saved working scene is restored after success or a handled failure.
Run against an idle editor with a saved, clean document. The refreshed Main Edit
must expose `render_start_seeded`; older editors are refused before mutation.

```sh
python3 -B tools/optical_comparison_mcp.py \
  --scene /absolute/path/scene_runtime.json \
  --request /absolute/path/comparison.json --plan
python3 -B tools/optical_comparison_mcp.py \
  --scene /absolute/path/scene_runtime.json \
  --request /absolute/path/comparison.json \
  --output-root /absolute/path/new-comparison
python3 -B tools/optical_comparison_mcp.py \
  --scene /absolute/path/scene_runtime.json \
  --verify /absolute/path/new-comparison
python3 -B tools/optical_comparison_mcp.py \
  --scene /absolute/path/scene_runtime.json \
  --resume /absolute/path/interrupted-comparison
```

The output directory must be new and its parent must exist. Relative scene assets
are copied through shared cross-directory Save As, preserving the supported
managed mesh, texture and atmosphere dependencies. The original scene bytes and
legacy declarations are preserved. Each cell opens the same baseline rather than
accumulating mutations from a preceding cell. Avoid editing the shared editor
while a matrix is running; the runner owns its scene transitions during the run.
If the editor leaves its comparison project, automatic restoration is refused.

An example request, with horizontal IOR and vertical frosted roughness:

```json
{
  "schema": "optic_optical_comparison_v1",
  "comparison_id": "ior_frost",
  "object_id": "glass_object",
  "axes": [
    {"parameter": "ior", "values": [1.1, 1.5, 1.8]},
    {"parameter": "roughness", "values": [0.02, 0.3, 0.65]}
  ],
  "settings": {
    "transmission": 0.95, "absorption_distance": 2,
    "thin_walled": false, "interface_tint": 16777215,
    "absorption_color": 12312046
  },
  "render": {
    "width": 320, "height": 200, "temporal_frames": 16, "frame": 0,
    "integrator": "disney_v2", "sample_sequence": 321
  }
}
```

For a transmission/absorption matrix, replace `axes` with:

```json
[
  {"parameter": "transmission", "values": [0.2, 0.6, 0.95]},
  {"parameter": "absorption_distance", "values": [0.25, 1, 4]}
]
```

Hold IOR, roughness, tint and thin-wall settings in `settings` as appropriate.
Absorption distance uses scene/world material units in the existing optical
response implementation. Colored absorption and a closed solid with thickness
make absorption differences visible; white absorption or a thin sheet can make
this axis visually inactive. Camera, geometry and a contrasting background must
place the glass in the render. A successful workflow is not proof that every
choice of scene or parameter produces a visible difference.

Limits are two distinct axes, 1–8 distinct values per axis, at most 16 cells,
16–512 pixels per dimension and 1–32 temporal passes. IOR is 1–2.5,
transmission/roughness 0–1, and absorption distance 0.25–8. The runner selects the
glass preset in the copied baseline. A roughness axis replaces the default typed
surface graph roughness output and existing region roughness outputs with a
new constant scalar. Other graph data remains intact; the untouched original
graph remains in the source scene. Graphs need a spare node. Ambiguous mapped or
layered legacy roughness without a typed graph is refused instead of claiming a
uniform sweep. This represents authored roughness, not measured frost geometry.

`render_start_seeded` takes the existing render-start arguments plus an integer
`sample_sequence` from 1 to 4294967295. It calls the same native render command
as the UI. All cells use the same sampling sequence, frame, settings and baseline
so stochastic variation does not obscure parameter changes. Verification reopens
saved cells, compares response readback and repeats the render with a new job ID;
image bytes must match. Exact hashes qualify the same native build and inputs,
not cross-platform or cross-version floating-point equivalence. Existing
`render_start` and ordinary UI rendering retain automatic sampling by default.
Direct Light remains available with one pass; Disney V2 is the comparison default
for stochastic glass/frost response.

Failures preserve completed cells, the incremental manifest and MCP transcript.
The runner cancels only its known active render and attempts to restore the
original scene. Dirty interrupted comparison state is saved into the owned
project first. Existing output roots and render IDs are never overwritten.
The bundled runner ships beside the MCP bridge in Main Edit Resources/bin.

## Partial-run resume

New runs write a `optic_optical_comparison_result_v2` journal. `--resume` reads the
original recipe from that journal; it cannot accept replacement render settings or
axes. It checks the original scene hash, sealed baseline, copied dependencies,
completed-cell scenes/images and native executable digest before editing. The
original scene must be saved and unchanged, and the editor must show that scene
or an owned comparison scene. Dirty state outside the project and unrelated
active renders are refused. A process-held file lock prevents two runners from
writing the same matrix. The lock is released automatically when a runner dies.

Saved-cell and render-request intent is recorded before the corresponding render
command. Resume skips completed cells and leaves their source/image files and job
identities intact. A pending completed native render is independently checked and
adopted into the journal; a still-active owned render is awaited. Canceled, failed,
interrupted or incomplete reserved attempts are preserved, and retries use a new
job ID. A saved scene whose seal was not committed is preserved as an orphan and
regenerated from the immutable baseline under a fresh scene filename. An unsealed
contact sheet is also preserved while a new sealed sheet is published.

Recovery works after runner death, after reconnecting, and after restarting the
same native executable following an editor interruption. Successful completion
restores the original saved scene and attachment. Completed `--resume` validates
and returns the existing result without rerendering or rewriting the journal;
`--verify` remains the explicit saved-cell replay operation. New results preserve
all legacy scene metadata through existing native authoring/save commands.

Resume begins after the baseline is sealed. An interruption during baseline
preparation must be retained and restarted in a fresh output root. Old v1 results
remain readable and verifiable, but have insufficient recovery intent and seals
for automatic resume. A changed native executable, recipe, original scene,
baseline, dependency or completed cell is refused. Finish partial matrices before
refreshing to a different native build. This is recovery at cell/render-request
boundaries; partially accumulated samples inside a canceled render are not resumed.
It does not establish power-loss durability, remote batch resume or measured glass
calibration. Avoid concurrent manual scene edits during the matrix operation.

The editor retains scene attachment endpoints dynamically for the lifetime of its
native session. Repeated matrices and recovery no longer encounter a fixed
32-scene alias ceiling. Earlier scene bindings continue to resolve to the same
editor socket; known scenes reuse their existing endpoint. Allocation or endpoint
publication failures refuse before switching scenes, and a failed lifecycle command
removes only its newly reserved endpoint. Closing the editor releases its owned
endpoints and memory. Available memory and filesystem permissions still apply.

## Explicit glass and secondary textures

Native explicit glass transport preserves authored zero optical roughness for
transmission while the common/principled GGX numerical floor remains unchanged.
Its interface reflectivity floor follows IOR instead of an inherited opaque
material reflectivity. Omitted glass transport retains the legacy paths.

For the existing bounded rough-direction cone, eligible explicit glass samples
transport the camera footprint with the same coherent rotation as the selected
ray. Constant-normal, finite, same-hemisphere and ideal-interface guards remain
required. Unsupported normals, grazing/TIR ambiguity and invalid differentials
retain the conservative source-mean fallback. This permits a graph checker to
remain readable through polished glass and soften progressively through the
existing frosted model. It does not establish a calibrated microfacet glass model.

Dense managed STL runtime meshes can use up to 64 MiB of JSON during cross-directory Save As, provided their schema is `codework_geometry` / `mesh_asset_runtime_v1` version 1. Other JSON dependencies retain the 16 MiB limit. The aggregate 512 MiB / 2048-file budget, containment, hashes, and no-clobber transaction remain enforced.

## Explicit renderer qualification for a paused matrix

Ordinary resume still refuses a changed renderer executable. A deliberately updated renderer can opt into an explicit transition only when the v2 matrix is paused/failed, its original scene has been restored cleanly, and at least one completed cell is sealed. Run the packaged client with `--scene <original-scene> --qualify-renderer <matrix-root> --timeout <seconds>`. This rerenders every completed cell at its original full settings and seed, checks response/source/settings and exact image hashes, and restores the original scene.

Only after all cells match does it retain the previous journal plus a sealed transition proof and change the current renderer seal. Completed cells and their original jobs remain unchanged; canceled attempts keep their IDs and are retried with new IDs on `--resume`. Resume checks the proof chain and its source/images. Failed parity or drift retains the prior journal. This explicit path does not automatically qualify arbitrary renderer changes or certify optical quality.

Explicit polished glass now reuses an identical per-call transmission trace while retaining the original sample count, accumulation order and rounding. No cache crosses pixels, hits, frames, scenes or threads. Rough interfaces, legacy materials, normal response, physical atmosphere and active caustic configurations keep their previous tracing path.
