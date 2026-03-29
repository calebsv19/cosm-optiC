# src › config

Configuration persistence.

- `config_manager.c` – Loads animation/scene settings with runtime-first fallback (`data/runtime/*.json` -> `config/*.json` -> legacy `Configs/*.json`), and saves mutable state to `data/runtime/*.json`.
