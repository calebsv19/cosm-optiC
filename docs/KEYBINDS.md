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

## Notes

- Fluid controls apply when a fluid scene/frame is active.
- The `U` toggle affects the native `3D` reconstruction/present lane only; it is separate from the legacy blur toggle.
- Velocity arrows currently use stride `4` sampling with capped, log-scaled lengths to avoid extreme spikes.
