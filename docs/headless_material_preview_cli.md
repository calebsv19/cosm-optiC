# Headless Material Preview CLI

Last updated: 2026-07-05

`ray_tracing_material_preview_headless` is the non-raytraced material swatch
lane for authored `ray_tracing` scenes. It evaluates the same object-level
material and procedural texture stack used by the runtime/editor material path,
but writes deterministic preview artifacts without running the native `3D`
integrators.

## Build

```bash
make -C ray_tracing ray-tracing-material-preview-headless
```

## Request Schema

Schema id:

```json
"ray_tracing_material_preview_request_v1"
```

Required fields:

- `runtime_scene_path`
- `object_id` or `scene_object_index`
- `output_path`

Optional fields:

- `summary_path`
- `width` / `height`: per-cell preview size, defaults `256 x 256`
- `columns`: contact-sheet columns when variants are present, default `5`
- `background_color`: optional solid `0xRRGGBB` backdrop for the whole sheet;
  when omitted, the tool uses the checkerboard backdrop
- `variants`: bounded array of preview overrides

Supported variant override fields:

- `alpha` / `transparency`
- `material_id`
- `object_color`
- `reflectivity`
- `roughness`
- `emissive_strength`
- `texture_id`
- `texture_strength`
- `texture_scale`
- `texture_offset_u`
- `texture_offset_v`
- `texture_seed`
- `texture_pattern_mode`
- `texture_coverage`
- `texture_grain`
- `texture_edge_softness`
- `texture_contrast`
- `texture_flow`
- `texture_color_depth`
- `texture_surface_damage`
- `preview_overlay`: optional synthetic overlay block layered only inside the
  preview tool; this is the fast way to sweep grime/oil looks without rewriting
  the authored scene stack

Supported `preview_overlay` fields:

- `kind`
- `opacity`
- `scale`
- `strength`
- `roughness_influence`
- `reflectivity_influence`
- `specular_influence`
- `diffuse_influence`
- `transparency_influence`
- `offset_u`
- `offset_v`
- `pattern_mode`
- `coverage`
- `grain`
- `edge_softness`
- `contrast`
- `flow`
- `color_depth`
- `surface_damage`
- `seed`

## Example

```json
{
  "schema": "ray_tracing_material_preview_request_v1",
  "runtime_scene_path": "/tmp/lab_probe/scene_runtime.json",
  "object_id": "left_screen",
  "width": 192,
  "height": 192,
  "columns": 3,
  "background_color": 14139328,
  "output_path": "/tmp/material_probe.bmp",
  "summary_path": "/tmp/material_probe_summary.json",
  "variants": [
    { "label": "base" },
    {
      "label": "grime_mid",
      "preview_overlay": {
        "kind": "grime",
        "opacity": 0.52,
        "roughness_influence": 0.74,
        "reflectivity_influence": -0.25,
        "coverage": 0.46,
        "grain": 0.44,
        "contrast": 0.61,
        "surface_damage": 0.34
      }
    }
  ]
}
```

Run:

```bash
ray_tracing/build/arm64/tools/cli/ray_tracing_material_preview_headless \
  --request /tmp/material_probe_request.json
```

## Outputs

- one BMP swatch or contact sheet at `output_path`
- one JSON summary at `summary_path` when requested

The summary reports the effective material/texture values per variant so Codex
or later automation can compare candidate settings without re-parsing the scene.

## Publishing a Website-Facing Set

Use the helper script to copy one generated set into
`ray_tracing/docs/material_preview_sets/<set_id>/`:

```bash
sh ray_tracing/tools/publish_material_preview_set.sh \
  --request /tmp/material_probe_request.json \
  --set-id grime_overlay_sweep_v1 \
  --title "Grime Overlay Sweep v1"
```

The `set_id` is validated as one path segment before repo-doc publication. Use
letters, numbers, dot, underscore, or dash; slashes, traversal segments, and
leading dash values are rejected.

That lane writes:

- redacted `request.json`
- `preview.bmp`
- optional `preview.png`
- redacted `summary.json`
- `index.md` with left-to-right, top-to-bottom variant order

The full request/summary artifacts remain at their source paths. Repo-doc
copies redact local/private paths before publication.

## Material Family Preview Grid

Use the family-grid integration helper to compare Glass, Mirror, and Rough Metal
through the same headless preview CLI:

```bash
make -C ray_tracing test-ray-tracing-material-family-preview-grid
```

The helper writes generated scenes, per-family preview requests, summaries,
family contact sheets, and one stitched grid under:

```text
ray_tracing/build/agent_runs/ray_tracing/material_family_preview_grid/
```

Rows are ordered Glass, Mirror, Rough Metal. Each row contains four variants:
clear/rough/default, a stronger roughness or polish comparison, and one or more
overlay cases. The generated `index.md` and
`material_family_preview_grid_summary.json` are the readback surfaces for later
proof review.

## Layer Control Preview Grid

Use the M12 layer-control helper to compare opacity, placement strength, and
signed response influence changes through the same headless preview CLI:

```bash
make -C ray_tracing test-ray-tracing-material-layer-control-preview-grid
```

The helper writes generated scenes, per-row preview requests, summaries, row
contact sheets, one stitched grid under
`ray_tracing/build/agent_runs/ray_tracing/layer_control_preview_grid/`, and a
redacted promoted doc set under
`ray_tracing/docs/material_preview_sets/m12_s5_layer_control_preview_grid/`.
