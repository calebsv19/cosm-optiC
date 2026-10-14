# Shared UI render-window foundation

The October 13 Main Edit source checkpoint imports the selected UI/render modules
from committed shared pin `1bf262dc8acac5354d95eb0c57bdc9ce0af39197` using the managed
archive importer per module prefix. This is a partial dependency refresh, not a
whole-subtree replacement or a canonical/released adoption claim.

| Module | Imported version |
| --- | --- |
| kit_ui | 0.18.0 |
| kit_render | 0.15.0 |
| kit_pane | 0.5.0 |
| core_layout | 0.2.1 |
| core_pane | 0.3.1 |
| core_theme | 2.0.1 |
| core_font | 1.0.2 |
| vk_renderer | 1.7.0 |
| vk_runtime | 0.6.0 |

The app retains its window, event loop, optical simulation, render jobs and export
policy. `ray_tracing_window_lifecycle` uses the shared SDL observation adapter to
scope input to the render window, discard hidden/unfocused input, cancel held
keys and stale drags across transitions, and handle F11 without repeated toggles.
Owner-window close enters the existing quit/cancellation route. Existing logical
and drawable dimensions remain distinct; existing rectangular UI geometry remains.

Ordinary hidden/minimized frames do not acquire or present a GPU frame. Asynchronous
deep-render polling, completion, cancellation and output processing occur before
presentation acquisition. Synchronous frame export explicitly permits a positive-size
hidden surface for GPU readback. Frame counters still advance only after verified
output. Zero-size drawable surfaces wait for recovery.

Fullscreen growth revealed an existing 2D CPU frame-buffer overrun. The buffer
module now tracks capacity and reallocates the pixel, preview, energy, direct energy and reflection scratch
buffers before larger frames; rendering equations are unchanged.

Baseline fixtures now respect current contracts: a single VF3D file cannot be
animated, so the test verifies that rejection and uses a manifest for the animated
case. The material graph persistence test opens its owning document before editing.
Existing behavioral assertions remain.

## Verification

Use a fresh, task-owned `BUILD_DIR_BASE`; never use a clean-based verification
recipe against retained outputs. The qualified checkpoint passes fresh Clang
application build, nine focused targets, and the aggregate `make test` suite.
The new target is `test-ray-tracing-window-lifecycle-contract`, covering event
scope, hidden input, key release, drag cancellation, F11 repeat and buffer growth.
Native production-object checks pass all nine resize/visibility/fullscreen stages
in both 2D and 3D, clean shutdown, owner-window close, hidden synchronous export and
hidden asynchronous deep-render completion with validation enabled and no diagnostics.
Native checks use isolated runtime/output paths and a diagnostic-only validation
configuration; they do not attach to a user's open renderer.

The subsequent menu/settings, editor and field/panel/HUD slices complete those
shared integrations. Local macOS package and installed Main Edit verification pass.
See [current rollout scope and remaining preview work](ui_fields_panels_runtime_hud.md).
