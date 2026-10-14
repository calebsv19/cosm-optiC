# Lighting editing through MCP

The live editor exposes three lighting commands through the same retained
commands as its light XYZ inspector, brightness/radius sliders and environment
menu. Read the current scene revision before each mutation. Commands use shared
Undo/Redo and File Save; neither editing nor inspection starts a render job.

| Tool | Arguments and behavior |
| --- | --- |
| `lighting_read` | No arguments. Returns the primary light's ID, world position/intensity/radius, static editability/refusal reason, environment settings and retained source. Reads never synthesize omitted legacy fields. |
| `lighting_set_primary` | `position_m` (three finite world metres), `intensity` (0–20, existing UI scale), `radius_m` (0–25 world metres), and `expected_revision`. Sets the existing first authored point/sphere light as one retained command. |
| `lighting_set_environment` | A nonempty `settings` patch and `expected_revision`. Omitted settings and unknown producer properties remain preserved. |

Environment patch fields follow the existing UI:

| Setting | Supported values |
| --- | --- |
| `light_mode` | 0 Off, 1 Top Fill, 2 Ambient |
| `environment_preset` | 0 Neutral, 1 Sky, 2 Warm Sky |
| `ambient_strength` | 0–1; UI ambient brightness is this value multiplied by 255 |
| `top_fill_strength` | 0–20 |
| `background_brightness_auto` | Boolean; automatic background derives brightness from ambient |
| `background_brightness` | 0–4; applies when automatic brightness is false |
| `background_color` | Three RGB values in 0–1 |

Use explicit
`background_brightness_auto: false` with a custom background brightness;
`lighting_read` reports effective brightness when automatic mode is active.
Position and the authored light radius convert through the scene's `world_scale`.
The legacy `light_settings.radius` carrier retains its existing runtime-unit
contract. Other light records, shape/color/emission data and producer metadata
remain untouched. File Save may normalize supported runtime authoring carriers;
effective lighting and unknown environment/light-settings properties survive.

Static primary edits refuse multi-point paths, legacy light timelines, keyed
primary-light channels, enabled primary motion bindings, disabled/moving or
ambiguous primary lights, and unsupported area/emissive shapes or physical
radiometry. Existing animated UI commands remain available. Environment edits
can coexist with animation. Inspector drafts, light/camera gestures and active
brightness/radius slider input block MCP mutations while inspection remains
available. Creating/removing/selecting arbitrary lights, emitter shape/color,
physical radiometry and animated-light authoring remain separate.
[Local frame rendering](render_mcp.md) is available.

The source catalog has 37 named commands and two fixed tools (39 total).
Cached clients can discover and dispatch all three commands through
`editor_catalog` / `editor_command` without reconnecting or changing the attached
scene. See [catalog compatibility](mcp_catalog_updates.md) for the requirement
that the initial connection already provides those fixed tools.

Native UI/MCP, Direct Light and fresh-process persistence acceptance:

```sh
make BUILD_DIR_BASE=/absolute/path/to/new/task-owned/build scene-editor-workspace-visual-test
python3 -B tests/integration/test_lighting_mcp.py \
  --binary /absolute/path/to/new/task-owned/build/toolchains/clang/<arch>/tests/scene_editor_workspace_visual_test \
  --output-root /absolute/path/to/new/task-owned/acceptance
```
