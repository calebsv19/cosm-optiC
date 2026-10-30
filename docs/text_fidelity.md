# Native text fidelity

optiC uses the existing shared font roles, SDL_ttf rasterization and Vulkan text
cache. UI labels, text fields, menu and HUD helpers opt into kit_render 0.16.0's
`kit_render_external_text_register_font_source_native_pixels` policy. The managed
module import is from shared commit `683068a`; other vendored modules are unchanged.

The policy measures the actual high-density raster and encloses it in logical
bounds for layout/hit presentation. Float geometry maps the glyph surface's exact
width/height to drawable pixels and snaps its origin to the pixel grid. An odd
raster dimension is not rounded into whole logical pixels and then stretched back
across the drawable. Wrapped lines use the actual drawable scale. Explicit
non-natural destination resizing remains supported; normal UI labels use their
measured natural size. Font family, hinting, user zoom, palette and image sampling
are unchanged. Ordinary shared font registration keeps its prior metrics policy.

The app font cache keys both logical size and loaded raster size. Logical13 at2x
and logical 26 at 1x may both load a 26-point face, but retain separate source/scale
identity. This avoids reusing a font registered for a different logical size.

The shared `external-text-pixel-harness` takes `<font.ttf> <output-directory>`.
Its real Vulkan captures compare RGB against the CPU glyph raster (rounding
tolerance 1) at 1x, 1.25x, 1.5x and 2x, cold/cached/wrapped and explicit-resize-then-natural
reuse. Default metric compatibility remains covered. Native 1x/2x runs verify
421,444 glyph pixels with validation enabled and no warnings/errors.

The app's `scene-editor-workspace-visual-test` modes `--rigid-ui` and
`--shared-fields-hud` run with isolated copied scenes/config and real shared
fonts/shaders. They cover font-cache logical-size identity, control input,
Save/reopen, evaluated poses, composition/search and clipped HUD presentation.
No numerical renderer/solver behavior or physical input is changed. Actual OS
monitor movement, IME/glyph coverage and other platforms retain separate scope.
