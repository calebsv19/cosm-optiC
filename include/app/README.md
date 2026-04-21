# include › app

Exposes the public interface for the application runtime.

- `animation.h` – Core runtime/app API and shared runtime globals.
- `animation_input_helpers.h` – Input helper contract for fluid overlay toggles and text-zoom shortcuts.
- `animation_output.h` – Output helper contract for optional render-metrics dataset export.
- `data_paths.h` – Canonical path/root resolution helpers for config/import/export manifest lanes.
- `runtime_time.h` – Runtime timing helper contract.
- `scene_loop_policy.h` – Mode-aware wait-timeout policy contract for menu/editor idle behavior.
- `scene_loop_diag.h` – Schema-locked loop diagnostics sink contract for blocked-vs-active calibration.
