# Menu controls

The main menu uses the same numeric controls in Effective Render and in the
Render workspace tabs. This is local development behavior; see
`main_edit_worktree.md` for the separate installed-app and acceptance boundary.

## Editing values

- Drag a slider for a quick adjustment, or use its small up/down buttons to
  adjust relative to the current value.
- Double-click the value to edit it. The initial value is selected. Left/Right,
  Home/End, Shift selection, Backspace/Delete, and Ctrl/Cmd+A/C/X/V are supported.
- A draft may be completely empty. It remains empty with a blinking caret;
  editing never changes the live setting until commit.
- Enter, Tab, or clicking another control commits a valid draft. Escape or
  losing window focus cancels it. Invalid input retains focus with a red outline.
- Empty input means zero, then the setting's minimum and maximum apply. Thus
  Frame Limit accepts zero, while FPS and image dimensions retain valid minima.
  Zero Frame Limit is an idle state: Start requests a positive value for a deep
  render. A new project render request is not silently synthesized with one frame
  from a zero limit; existing explicit project requests keep their own ranges.

## Saving and closing

Changes are saved as they are applied. **Close** leaves applied changes in place
and cancels an unfinished numeric draft; it is not a rollback button. **Save**
commits the active valid field and retries writing the current configuration.
A failed save reports that changes remain in memory, with a prompt to retry.
Scene and animation files use the adopted `core_io_write_all_atomic` helper,
so each file is replaced only after its temporary write succeeds. The two files
are separate saves, not a combined transaction; Save reports failure if either
required write fails. Existing void save APIs remain compatible wrappers.

Path and Start Frame editing capture keyboard shortcuts until Enter/Escape or a
click moves focus. Start Frame uses checked whole-number parsing and allows an
empty draft to commit zero. These legacy fields retain their existing text-entry
presentation; the slider fields provide full cursor and selection editing.

## Adjustment rules

| Setting | Drag and arrow step | Typed values |
| --- | --- | --- |
| Frame width / height | 2 pixels | Even pixels; odd input rounds upward, then clamps to the supported range |
| Tile Size | 4 | Multiples of four, matching the existing tile constraint |
| Large integer ranges (maximum at least 1000) | 5 | Exact integers within the supported range |
| Other integer settings | 1 | Exact integers within the supported range |
| Decimal settings | One stored increment | Displayed units, rounded to the setting's storage precision |

Frame dimensions stay even for video encoding. A typed Frame Limit of `1237`
stays `1237`; clicking up gives `1242`, while dragging snaps to the five-unit
grid. A roulette threshold of `0.005` maps to its correct internal units.
Render Scale still displays `HiDPI` for zero and `Nx` otherwise; its text editor
uses the corresponding numeric value. Existing setting-specific rules, including
tile-size normalization and render-state invalidation, still apply.

## Scrolling and space

Scene, volume, Effective Render, and Render-tab slider lists share six-pixel
scrollbar geometry and rendering. Mouse/trackpad wheel input, track paging,
thumb capture, left-button outside release, focus loss, and content-size clamping use one
menu adapter. The scrollbar's mouse target is wider than its visible track.

Render Info starts with the configured/runtime summary; its heading toggles all
five details. Layout and drawing both use logical text metrics. Scene-list height
reserves room for the root, mesh, and volume controls. A different mouse button does not release a left-button drag. Focus loss also
releases pane splitters. Slider row spacing includes the numeric field height,
and scene-list sizing does not override the reserved controls with an old minimum.
Menu buttons use the shared
rounded-rectangle primitive with a small radius and hover/pressed feedback.

## Implementation and reuse

- `reuse-adopted`: existing `kit_ui 0.11.2` SDL scrollbar geometry/rendering and
  rounded surfaces. The app now links `kit_ui_sdl.c`; no shared API/version changes.
- `reuse-adopted`: existing `core_io` atomic file replacement; checked app-level
  save APIs preserve the existing compatibility wrappers.
- `reuse-deferred`: a common text-field library is not exposed by the currently
  adopted kit. The bounded numeric draft follows BehaviorSim's cursor/anchor
  editing model; numeric parsing, limits, focus, and settings commits remain in
  `menu_numeric_model.c` and `menu_numeric_controls.c`. A cross-app extraction is
  a separate adoption task, rather than copying an entire IDE rename workflow.
- Shared adoption catalogs should record this consumer at canonical adoption.
  Main Edit preparation does not supersede concurrent changes in those catalogs.

## Verification

The `ui_menu_contracts` C test group checks draft editing, cursor replacement,
selection, invalid input, zero/default behavior, even dimensions, typed precision,
nudges, decimal conversion, double-click entry, long-draft caret hit testing,
legacy-field shortcut capture, Close draft cancellation, checked save failure,
real-font layout at 960x640, 1080x760 and 1200x900, scroll drag and
release, content shrinkage, and panel layout. Run it through the normal test
runner with `TEST_RUNNER_GROUP=ui_menu_contracts`.

The isolated Main Edit package self-test checks assembly, source identity,
runtime namespace, launcher behavior, and signing. A live visual review at normal
and Retina scaling remains separate from these automated checks.
