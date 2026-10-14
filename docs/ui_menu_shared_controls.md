# Shared menu and settings controls

The October 13 Main Edit checkpoint adopts shared `kit_ui_surface`, interaction,
SDL event translation and button-style resolution against the scoped shared pin
`1bf262dc8acac5354d95eb0c57bdc9ce0af39197`. No additional vendor update is required.
The app owns the menu layouts, action meanings, persistence and render settings.

`menu_shared_controls` publishes only painted button bounds, clipped to the active
viewport. Named layout actions have stable semantic keys, including the three
workspace tabs, renderer tabs, mode/root controls, Output + Batch, Run + Resume,
Save, Restore Defaults, Preview, Scene Editor, Close and Start. The painter resolves
shared selected/hover/pressed/focus/disabled state and draws directly through the
native Vulkan renderer. Start retains its rectangular geometry; other adopted
buttons retain the existing four-pixel compact corners. Labels are measured,
width-fitted and clipped using the existing shared theme/font adapter.

Buttons activate once on matching pointer release inside their current bounds,
or on Enter/Space release after Tab/Shift-Tab focus. Repeats do not reactivate.
Leaving the control, scrolling while armed, window/layout/scope changes and modal
ownership changes cancel outstanding presses. Foreign-window and hidden input
cannot reach the menu. The menu uses the shared window observer and F11 policy.
Current automatic 3D responsive rendering is read-only; deep-render-only controls
follow the existing action prerequisites rather than offering inactive clicks.

Activation enters the existing menu action handler. Save, failed-save status,
retry, effective render recipe, cache invalidation, root edits, batch operations,
Preview/Start and defaults keep their established implementations. Editable text
and numeric drafts retain Enter/Tab ownership; slider drags, splitters, scrolling
and scene/volume selection rows retain their current owners. Folder pickers and
the workspace authoring host cancel background button gestures. Editor canvas,
MCP/document ownership, optical equations, exports and running jobs are unchanged.

## Verification

Use a fresh task-owned build root. Fresh Clang app build, seven focused targets
(including `test-menu-shared-controls-contract`) and aggregate `make test` pass.
The contract covers release cancellation, moved geometry, scope changes, repeats,
disabled/modal controls, scroll cancellation and draft keyboard ownership.

The native probe links actual app objects and exposes existing menu entry/event
functions for observation; its only runtime configuration difference enables
Vulkan validation. Isolated runtime/config/output paths qualify the main menu,
keyboard Save with persisted readback, forced save failure and successful retry,
Output + Batch, Run + Resume, renderer tabs, 3D read-only behavior, compact sizing,
hide/restore, fullscreen/return and owner close. Twelve captures plus clean close
pass with zero validation warnings/errors. Native frame captures are not installed
or packaged acceptance evidence.

The subsequent editor and field/panel/HUD slices complete those integrations.
Local macOS package/installed Main Edit verification passes, and the combined
source checkpoint is adopted into canonical. See [current rollout limits](ui_fields_panels_runtime_hud.md).
