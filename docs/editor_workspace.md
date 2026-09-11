# Scene workspace shell

The September 11 source slice places the existing Scene, Materials, Camera and
Paths mode selectors and scene actions in a window-wide header. These are the
existing editor modes, not the future Surface or Atmosphere workspace profiles.
Document/feedback status stays beneath the actions. Object-mode retained transform,
name, undo/redo and managed STL controls remain in the right inspector.

`Expand view` hides both side panes and any open light timeline. `Show panes`
restores their widths and timeline visibility. `Reset layout` restores the default
pane widths, shows the side panes and collapses the timeline. These operations
change presentation only; they do not dirty the document or change selection.
The existing pane graph and splitter kit still own sizing and drag behavior;
`scene_editor_workspace_layout.c` owns only header button geometry.

Expanded view supports navigation and the dedicated native 3D and material canvas
routes. Legacy pane pointer handlers are suppressed there because they also own
sidebar hit rectangles; show the panes to use legacy 2D tools. Hidden inspector
controls cannot receive pointer edits. Workspace state is session-local.

## Verification

```sh
make BUILD_TOOLCHAIN=clang all test-scene-editor-foundation-a \
  test-scene-editor-pane-host-contract test-runtime-scene-bridge-contract \
  test-ray-tracing-render-headless-preflight \
  test-ray-tracing-render-headless-image-export
make BUILD_TOOLCHAIN=clang scene-editor-workspace-visual-test
```

The second target builds an opt-in native GUI test. Run
`build/toolchains/clang/<arch>/tests/scene_editor_workspace_visual_test` with a
task-owned working directory and an absolute path to a **copied** runtime fixture.
The test edits/saves that copy, creates its own SDL window and writes PPM captures
in the supplied working directory. Provide isolated `data/runtime/animation_config.json`
and `scene_config.json` there; set `RAY_TRACING_PROGRAM_ROOT` to the checkout so
fonts and read-only input assets resolve. Do not pass a scene or working directory
you want preserved unchanged. A GUI desktop session is required.

The pane contract checks toolbar reachability, expand/restore, splitters and layout
at 1024x640, 1280x800, 1440x900 and 2560x1600, plus increased header text height.
The native source test checks 1280x800 and 1024x640 (2x backing pixels on the
validation host), selection/revision preservation, an inspector position edit,
Undo, Redo, Save and document reopen. Reopen uses the document API in the same
process; it is not an installed-app restart test.

## Open acceptance

This is the first bounded Scene shell slice. The legacy left-pane asset/material
stack still overflows at smaller sizes; scrolling/group separation needs its own
follow-up. Full text-scale, keyboard/focus, all mode controls, native STL picker
and material assignment acceptance remain open. Dedicated Surface, Atmosphere &
Water and Render profiles, durable workspace persistence and graph expansion are
future work. Broad `test-stable` was non-green before this slice (495 reported
failures); focused passes do not establish broad green or package acceptance.
