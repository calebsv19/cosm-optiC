# optiC Build Week Showcase

This is the self-contained, photon-free demo scene for the optiC OpenAI Build
Week submission. Load `scene_runtime.json` from the app's scene picker to inspect
and edit three original procedural objects with mirror, glossy, and metal
materials.

The package includes each editable procedural recipe, its generated STL source,
the compiled runtime mesh sidecar, and generation/import summaries. No
third-party model or texture is required.

## Included objects

- `reflection_blob`: 20,480 triangles, mirror material
- `grooved_orb`: 16,128 triangles, glossy material
- `lattice_shell`: 8,064 triangles, metal material

## Demo flow

1. Launch optiC and choose **Load Scene**.
2. Open `samples/optic_build_week_showcase/scene_runtime.json`.
3. Select an object in the scene editor.
4. Change its material preset, color, roughness, or reflectivity.
5. Move the camera and start a deep render to compare the result.

The scene intentionally does not enable or depend on photon mapping.

## Headless proof

From the source repository, build the headless renderer and run:

```sh
build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless \
  --request config/samples/optic_build_week_showcase/render_request.json \
  --render \
  --summary build/agent_runs/ray_tracing/optic_build_week_showcase/render_summary.json
```

The first frame is written to
`build/agent_runs/ray_tracing/optic_build_week_showcase/frames/frame_0000.bmp`.

## Asset provenance

All three meshes were generated during the submission work with the repository's
procedural STL authoring tool. The recipes and generation summaries under this
directory are the reproducibility and ownership record. Runtime JSON meshes were
derived locally from those STL files using the CodeWork imported-mesh pipeline.
