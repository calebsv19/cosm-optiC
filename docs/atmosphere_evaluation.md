# Atmosphere evaluation scene

This is a local Main Edit diagnostic, using existing VF3D density and appearance
controls with Disney V2. It changes no optical model and runs no simulation.

The scene contains a pale striped backdrop, a blue marker behind the volume and
a red marker in front. Geometry, camera and exposure remain fixed while testing
Light cloud, Soft mist, Smoke, a blue tint and increased transparency. Front,
side and back light layouts each have matched atmosphere-off/on renders. The
transparent case must match the disabled case byte for byte. The field cache
digest must remain unchanged throughout the appearance comparisons.

The default 480 x 320, four-pass images are diagnostic previews. Inspect the
backdrop stripe contrast and rear marker through the volume, while checking that
the foreground marker remains visible. Cloud and mist are currently much fainter
than smoke. This exposes a lighting/optics limit for the next slice rather than
demonstrating final cloud quality or artistic acceptance.

Generate a fresh folder (an existing destination is refused):

```sh
make BUILD_TOOLCHAIN=clang ray-tracing-render-headless test-scene-atmosphere-attachment
python3 tools/atmosphere_evaluation_scene.py \
  --out /absolute/fresh/AtmosphereEvaluation \
  --codec build/toolchains/clang/arm64/tests/scene_atmosphere_attachment_test \
  --cli build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless
```

Use the architecture-specific build path on other machines. The codec exports
the actual application presets; the generator does not maintain duplicate preset
values. The output contains an editable scene and render request, field assets,
eleven variant scenes/requests, native summaries, frames and `verification.json`.
Open `scene_runtime.json` in the editor and choose Environment to adjust the
retained volume. File Save preserves those settings. The density guide shows
field occupancy; use the final renderer to judge its appearance.

The October 6 local delivery is under the CodeWork workspace:
`_private_workspace_artifacts/ray_tracing/Atmosphere Evaluation/`.
Its editable scene is in `Scene Files`, its two labeled comparison sheets are
in `Previews/comparisons`, and its cover and contact sheet remain pending human
review. It has no latest-good or artistic-acceptance promotion.
