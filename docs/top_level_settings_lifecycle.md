# Top-Level Settings Lifecycle

The main menu owns operator-authored render preferences. A completed menu edit
must update the effective runtime recipe, invalidate prepared render state when
the value changes rendering, and persist the animation configuration before the
next launch.

## Commit boundary

`menu_settings_lifecycle_commit()` is the common boundary for top-level menu
actions. Buttons commit immediately. Sliders commit when the pointer is
released, so dragging remains responsive while the final value is durable.
Ambient/background proxy sliders already invalidate prepared scene state while
they are dragged; the lifecycle helper avoids issuing a duplicate invalidation
when those sliders are released.

The boundary currently covers the top-level render recipe, caustic controls,
space/editor modes, mesh normal policy, render modes, pane widths, and numeric
or keyboard edits routed through the main menu.

## Runtime-owned values

Window width, window height, and the native 3D ray budget are persisted in the
animation configuration. `ApplyAnimationWindowSizeOverride()` copies these
operator values into `sceneSettings` for non-fluid scenes before rendering.
This keeps runtime-scene JSON loading from being the only durable owner of the
ray budget.

The dimension and ray contracts use the named defaults and limits in
`config_manager.h`; persistence normalization and menu slider construction use
the same values. Width and height have separate maximum clamps so a stale or
manually edited config cannot bypass the UI limits.

Starting a runtime scene may import scene-authored lighting defaults. The Start
flow therefore preserves and reapplies the current menu-owned environment and
direct-light recipe after the scene source is restored. The protected recipe
includes environment mode, background color and brightness, top fill, direct
light intensity/radius/height, falloff distance/mode, and falloff softness.

## Validation

The focused configuration suite verifies slider-release persistence, caustic
recipe synchronization, and native 3D ray-budget round trips. The UI contract
suite verifies that Start reapplies both environment and direct-light menu
values. The runtime scene bridge suite guards the source-loading boundary.

## Applied-state readback

The Render Recipe panel compares the persisted animation-config values with
the live `sceneSettings` dimensions and ray budget, then checks the prepared
scene generation. It reports:

- `edit pending` when the live values differ from the current config values
- `render refresh pending` when values match but the prepared scene is stale
- `applied` when values match and the prepared scene generation is current

This readback uses the same runtime cache generation as the ambient-light
readback; it does not infer application from a button click or successful
serialization alone.

## Responsive right-pane presentation

The shared pane host owns the outer health/right-pane rectangle. The optiC menu
layout subdivides that rectangle into three non-overlapping app-owned regions:
Effective Render sliders, Render Info, and Runtime Route. Each region inherits
the current pane x-position and width, so dragging a pane splitter or resizing
the menu cannot leave the slider panel attached to legacy screen coordinates.

Render Info uses the measured font line height to reserve five compact rows for
integrator/caustic mode, sampling cost, configured output, effective runtime
output, and route/scene digest. Runtime Route uses a geometry helper to place
two paired action rows above a full-width Start action; its content is
bottom-aligned and its row height follows the active menu font within bounded
limits.

UI geometry tests cover narrow, standard, and wide health panes plus short,
standard, and tall menu windows. They require full-width ownership, contained
slider tracks, non-overlapping subregions, a two-column route grid, and a
bottom-aligned Start action.

## Remaining lifecycle work

- Extend the applied-state snapshot beyond dimensions, ray budget, and prepared
  scene generation as additional runtime consumers expose trustworthy readback.
- Continue moving isolated direct `SaveAnimationConfig()` calls behind the
  lifecycle boundary as their ownership is audited.
- Add operator-facing ownership labels where a scene-authored value and a
  menu-authored override can both exist.
- Expand visual smoke coverage for the remaining top-level tabs after their
  individual runtime consumers are confirmed.
