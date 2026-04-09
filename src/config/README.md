# src › config

Configuration persistence.

- `config_manager.c` – Owns animation/scene config schema, normalization/clamping, runtime-first fallback policy (`data/runtime/*.json` -> `config/*.json` -> legacy `Configs/*.json`), and save flows.
- `config_file_io.c` – Shared file/directory/JSON helpers used by config load/save paths.
