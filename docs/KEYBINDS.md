# RayTracing Keybinds

## Runtime Overlay Controls

- `F`: Toggle fluid overlay on/off.
- `V`: Cycle fluid overlay mode:
  - `density`
  - `density + velocity arrows`
  - `velocity heatmap + velocity arrows`
- `[` : Step to previous fluid frame (clamped at 0).
- `]` : Step to next fluid frame.

## Runtime Rendering Controls

- `B`: Cycle blur mode (`None` -> `Light` -> `Heavy`).
- `U`: Cycle native `3D` upscale mode (`OFF` -> `Nearest` -> `Bilinear`).
- `Esc`: Exit current run loop (returns to menu flow).

## Menu Data Root Controls

- `Cmd/Ctrl+B`: Open native folder chooser and set input root.
- `Cmd/Ctrl+Shift+B`: Open native folder chooser and set output root.
- `Cmd/Ctrl+Shift+I`: Start typed input-root edit.
- `Cmd/Ctrl+Shift+O`: Start typed output-root edit.
- `Enter`: Apply active typed root edit.
- `Esc`: Cancel active typed root edit.

## Runtime Interaction

- `Mouse move` / `Mouse click`: Move light position in interactive mode.

## Scene Editor

These bindings apply while the Scene workspace is active and no text field or
modal operation owns keyboard input:

- `Q`: Select tool.
- `W`: Move tool.
- `R`: Rotate tool.
- `E`: Scale tool.
- `F`: Frame the selected object; frame the scene when nothing is selected.
- `Esc`: Cancel the active transform or placement operation and return to Select.
- `Cmd/Ctrl+Z`: Undo the last committed scene edit.
- `Cmd/Ctrl+Shift+Z` or `Cmd/Ctrl+Y`: Redo.

Transform controls:

- `World` / `Local`: choose the visible transform orientation state.
- `Snap off` / `Snap on`: toggle move `0.1` scene-unit, rotate `15°`, and scale
  `0.1` factor increments.
- Drag the Scale `All` handle to scale X, Y, and Z together.
- Click a numeric transform field to type an exact value. `Enter` applies and
  `Esc` cancels the draft.
- `Cmd/Ctrl+C` and `Cmd/Ctrl+V` copy or paste the active numeric/name field.
- Double-click a transform field to reset position/rotation to `0` or scale to
  `1`; the reset is one undoable edit.

## Notes

- Fluid controls apply when a fluid scene/frame is active.
- The `U` toggle affects the native `3D` reconstruction/present lane only; it is separate from the legacy blur toggle.
- Velocity arrows currently use stride `4` sampling with capped, log-scaled lengths to avoid extreme spikes.
- Scene Editor shortcuts use mnemonic labels chosen for this editor: `R` means
  Rotate and `E` means Scale. They intentionally differ from applications that
  assign `E` to Rotate and `R` to Scale.
