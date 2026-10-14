# Editor frame rendering through MCP

The Render workspace's **Render frame** panel and MCP share one command owner,
using the existing native async desktop renderer, progress listener, cancellation
and durable BMP completion path. A render captures the current working scene,
including unsaved edits, without invoking File Save or adding an Undo entry.

| Tool | Behavior |
| --- | --- |
| `render_read` | Read the panel's render settings, active state and latest job ID. |
| `render_start` | Start one frame with `request_id`, `width`, `height`, `integrator`, `temporal_frames`, `frame`, and the current `expected_revision`. |
| `render_status` | Read progress, diagnostics and terminal state with `job_id`. |
| `render_cancel` | Request cancellation of the current active `job_id` with the current `expected_revision`. |
| `render_results` | Read the receipt and freshly check BMP dimensions and SHA-256 with `job_id`. `artifact_integrity` states whether the published output still matches. |

The source catalog has 40 named commands and two fixed tools, 42 total.
Cached clients with `editor_catalog` / `editor_command` can discover and invoke
these commands on their existing connection; see [catalog compatibility](mcp_catalog_updates.md).

## Supported settings and identity

This slice supports native 3D, caustics off, and one absolute frame per job.
`integrator` is `direct_light` or `disney_v2`; dimensions are 16–2048 in each
axis. `temporal_frames` is 1–32 for Disney V2 and exactly 1 for Direct Light.
The frame must be a nonnegative integer no greater than 1,000,000,000; the native
scene evaluator/session still validates its supported range. Requested dimensions
are exact output pixels, with render scale 1. Other renderer quality settings
retain their existing UI semantics and are restored after the job.

The panel exposes width, height, passes, frame and integrator selection, plus
Start, Cancel and Results. Results verifies the output through the same query
as MCP. Active job settings and progress also appear when MCP starts a render.
Render settings are session controls; they do not change retained scene fields.

Use a unique request ID consisting of 1–63 letters, digits, hyphens or underscores.
Each job owns `<scene-directory>/.optic-renders/<request_id>/`, containing:

- `job.json`: source revision/hash, requested settings, progress, terminal state,
  prepared geometry/light/volume counts and verified image identity
- `source_snapshot.json`: the working document used for rendering
- `frame.bmp`: published only after successful rendering and durable validation

The source snapshot is provenance, not a portable scene export: dependencies keep
their original scene-directory context. The render preserves other scene files,
attachments, metadata and authoring history. Existing IDs refuse further starts;
after an uncertain outcome, inspect that same ID with status/results instead of
creating another request. No output is overwritten or automatically resumed.

## Lifecycle and result interpretation

The native renderer requires one exclusive context. While active, scene mutation,
Save, timeline movement, conflicting UI input and additional render starts are
refused. Readback and cancellation remain available. Inspector drafts and active
editor gestures must finish before starting. Cancel requests are asynchronous;
inspect status until terminal. Canceling or closing the editor joins/releases its
job and publishes no final image. Save/publication failures become failed jobs.

Successful output reports `state: completed`, committed passes,
`output_verified: true` and an image hash. `render_results` independently checks
that current file and reports `artifact_integrity`; missing, changed or malformed
output cannot qualify as a valid result. Historical results remain readable after
reopening the same scene by their saved ID. A nonterminal receipt without a job
owned by the current editor reports `interrupted`, with no automatic replay.

Animated/unowned volume or water inputs and unsupported native async routes
retain the existing renderer's refusal behavior. Legacy scenes are never migrated
implicitly. Batch rendering, resume, remote jobs, video assembly, output-root
selection and additional renderer/preset controls remain separate slices.

## Native acceptance

```sh
make BUILD_DIR_BASE=/absolute/path/to/new/task-owned/build scene-editor-workspace-visual-test
python3 -B tests/integration/test_render_mcp.py \
  --binary /absolute/path/to/new/task-owned/build/toolchains/clang/<arch>/tests/scene_editor_workspace_visual_test \
  --output-root /absolute/path/to/new/task-owned/acceptance
```

Acceptance covers actual UI settings/start/Results/Cancel, real stdio named and
compatibility dispatch, exact UI/MCP Direct Light BMP parity, Disney V2 committed
passes, prepared static physical-atmosphere attachment, unsaved edit/render/Undo,
identity/type/revision refusals, exclusive-context guards, cancellation/shutdown,
publication failure, changed-artifact detection, queued UI guards, unsaved-scene
close confirmation and fresh-process result reopening.

Desktop prepared-light provenance comes from the prepared frame's light set,
including its enabled count. See [complete scene workflow](mcp_scene_workflow.md)
for full authoring/readback and intensity-change render checks.
