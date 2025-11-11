# TimerHUD Text Rendering

Thin wrapper around SDL_ttf used by the HUD to draw crisp text overlays.

| File | Responsibility |
| --- | --- |
| `text_render.h/c` | Loads fonts, caches glyph metrics, and exposes draw helpers tailored for TimerHUD’s layout routines. |

Shared between the HUD renderer and any auxiliary overlays that need text.
