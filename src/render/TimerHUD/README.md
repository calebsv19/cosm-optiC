# TimerHUD – Real-Time Timing Overlay & Config Visualizer

**TimerHUD** is a real-time profiling and HUD system designed to track code block durations, visualize performance, and edit 
configuration settings interactively.

## Directory Summary

| Path | Purpose |
| --- | --- |
| `src/` | Core implementation split into submodules (timers, HUD renderer, config, logging, etc.). Each subdirectory has its own README with file-level details. |
| `external/` | Third-party dependencies bundled with the HUD (e.g. JSON parser). |
| `assets/`, `TextRender/`, `SDLApp/`, etc. | Supporting assets and helper modules documented inline below. |


It is part of the broader **TimeScope system**, and is built for maximum modularity — allowing you to inject it into any C/SDL-based 
project with minimal integration effort.

---


##  Current Features

- Named Timer System
  - Track time for custom code blocks using ts_start_timer() / ts_stop_timer()
  - Measures min, max, avg, and stddev for each timer

- HUD Overlay
  - Renders real-time timing info using SDL2 + SDL_ttf
  - Dynamically scales and formats timer display
  - Controlled by toggleable hud_enabled flag

- Settings-Driven Architecture
  - Runtime-configurable via settings.json
  - Supports toggles for:
    - hud_enabled
    - log_enabled
    - event_tagging_enabled
    - render_mode: "always" or "throttled"
    - render_threshold: frame delay in seconds
  - Configurable timer buffer size and logging format

- Main Executable as Configurator Tool
  - Includes a UI for toggling settings and live-saving settings.json
  - Lets you preview HUD behavior before integrating into another app

---


##  Project Structure Overview

TimerHUD/
├── include/              # Public headers (timescope.h)
├── src/
│   ├── core/             # Timer engine and frame logic
│   ├── hud/              # HUD overlay renderer
│   ├── config/           # JSON settings loader/saver
│   ├── logging/          # CSV/JSON logger
│   ├── events/           # Event tagging system
│   └── history/          # Summary compression (WIP)
├── TextRender/           # SDL_ttf wrapper
├── SDLApp/               # Modular SDL main loop
├── assets/               # Fonts and future themes
├── main.c                # Configuration + live HUD executable
├- externals/            # json import for config settings
├── settings.json      # Runtime settings file
├── Makefile              # Build script with dependency tracking


---


##  Example Integration

#include "timescope.h"

void gameLoop() {
    ts_frame_start();

    ts_start_timer("physics");
    simulate_physics();
    ts_stop_timer("physics");

    ts_start_timer("render");
    render_everything();
    ts_stop_timer("render");

    ts_frame_end();
    ts_render(renderer);
}

---


##  Future Goals

- Separate executable modes:
  - TimerHUD runtime overlay
  - TimeScope Configurator (settings + analysis)

- HUD Features:
  - Per-frame graph strips
  - Color alerts for spikes
  - Moveable overlay

- Analyzer Tool (Phase 6):
  - GUI viewer for logs and event tags
  - Live editing of settings.json
  - Visual tag correlation and performance graphs

---


## ️ Build & Run

Dependencies:
- SDL2
- SDL2_ttf
- cJSON

Build:
    make
    ./bin/timescope_demo

---


##  Example settings.json

{
  "hud_enabled": true,
  "log_enabled": true,
  "event_tagging_enabled": false,
  "timer_buffer_size": 128,
  "log_filepath": "timing.csv",
  "log_format": "csv",
  "render_mode": "throttled",
  "render_threshold": 0.033
}

---


##  Notes

- Ideal for SDL-based apps, game loops, or simulation tools
- Helps visualize and debug performance in real time
- Drop-in modular — works in any C project

---


##  Status

Fully functional for dev use. Config editor and JSON-saving logic are complete.  
Ready for integration into other projects.

---


##  Feedback & Contributing

This is a solo project — but ideas, bug reports, and contributions are welcome.

---


## License

MIT-compatible license planned. Currently private, but moving toward public dev tool release.
