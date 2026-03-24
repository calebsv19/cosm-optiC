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
- `Esc`: Exit current run loop (returns to menu flow).

## Runtime Interaction

- `Mouse move` / `Mouse click`: Move light position in interactive mode.

## Notes

- Fluid controls apply when a fluid scene/frame is active.
- Velocity arrows currently use stride `4` sampling with capped, log-scaled lengths to avoid extreme spikes.
