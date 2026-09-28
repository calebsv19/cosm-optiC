# Agent motion frame sets

`tools/render_motion_frame_set.py` prepares a separate, portable mesh scene and
optionally runs the native headless renderer. It never edits the open editor scene
and refuses an existing output directory. This is a bounded 20-frame recipe,
not a new MCP endpoint or a general scene-authoring language.

From the persistent Main Edit checkout:

```sh
make TOOLCHAIN=clang ray-tracing-render-headless
python3 tools/render_motion_frame_set.py \
  --cli build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless \
  --output /path/to/frame_sets/tests/object_motion_NEW \
  --width 320 --height 180 --temporal-frames 1 --fps 24 \
  --frame-count 20 --integrator direct_light --render
```

Choose a new output name for each run. Omit `--render` to prepare inputs only.
The tool isolates its runtime settings and mesh cache inside the output directory.
Writing outside the agent's sandbox may require the platform's normal approval.

## Scene and timeline

The scene contains a floor and one mesh sphere. A fixed inspection camera at
(0, -6.2, 2.6), looking at (0, 0.3, 1.0), frames the complete motion. Lighting is
steady. The render request supplies the camera override.

| Frame | Sphere position (X, Y, Z) | Behavior to next key |
| --- | --- | --- |
| 0 | -1.4, 0.3, 0.7 | Move upward and across |
| 8 | 0, 0.3, 1.4 | Hold |
| 11 | 0, 0.3, 1.4 | Resume movement |
| 19 | 1.4, 0.3, 0.7 | End |

All three position channels have linear keys. Equal positions at frames 8 and 11
produce a pause. `--start-frame` and `--frame-count` select a subset of frames 0–19.
The recipe writes timeline FPS with `--fps`; set the frame viewer to the same rate.
Twenty frames at 24 FPS make a short, approximately 0.83-second sequence.

## Quality controls

- `--width` and `--height` set actual image pixels. Monitor DPI is unrelated.
- `--temporal-frames` controls the renderer's temporal sampling count, not output
  frame count or animation duration.
- `--integrator direct_light` gives the fast draft used for motion verification.
  `disney` and `disney_v2` select the other supported integrators.
- `--fps` changes the authored rate; frame-indexed key positions stay the same.

For example, use `--width 640 --height 360 --temporal-frames 2 --integrator disney_v2`
with a fresh output directory for a higher-cost pass. More pixels and samples cost
more rendering time. Start with a small frame subset when changing quality.

## Outputs and verification

Open the output's `frames/` directory in sCope and inspect `frame_0000.bmp` through
`frame_0019.bmp` in numerical order. The output retains:

- `scene_runtime.json` and `assets/`: portable scene and mesh inputs.
- `render_request.json`: exact range, image size, sampling and camera settings.
- `recipe.json`: authored rate, recipe description and renderer SHA-256.
- Preflight/render logs, summaries and progress JSON.
- `frame_manifest.json`: frame paths, byte sizes, dimensions and image hashes.

The command checks successful frame count, BMP dimensions and fixed camera position.
A full direct-light run also requires movement to change the images and the hold
endpoints to match. Inspect the frames visually as well: hashes prove change, not
composition quality. Stochastic integrators do not use the exact-image hold check.

For future custom scenes, edit a separate saved scene's `scene_timeline` tracks and
use the native request/CLI described in [Headless Agent Render CLI](headless_agent_render_cli.md).
Keep camera and light static for a motion proof, preflight first, render a short
range to a new destination, then inspect the images before increasing quality.
The recipe intentionally does not expose arbitrary object geometry or keyframe
editing flags; those remain native scene data.
