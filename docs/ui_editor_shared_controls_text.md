# Editor shared controls and text

The retained Main Edit editor adopts the existing immutable shared UI pin
`1bf262dc8acac5354d95eb0c57bdc9ce0af39197` through an app-native Vulkan adapter.
`kit_ui_surface` and interaction own painted chrome, popup-menu, dirty-close and
rename action activation. Existing document, tool, workspace, lifecycle and save
handlers remain authoritative. Controls retain rectangular geometry and their
current density; this integration adds no blanket corner rounding.

Paint snapshots register stable semantic keys and clipped bounds. Matching pointer
release activates once. Leaving a control, a window/geometry/document/modal change,
or an interrupted gesture cancels activation. Foreign-window and hidden input is
filtered. A fresh paint is required after ownership changes, so a queued click
cannot use a stale document's chrome. F6 enters chrome keyboard traversal;
Tab/Shift-Tab then moves focus and Enter/Space release activates. Ordinary Tab
retains workspace cycling until F6 is used. Numeric/text drafts and canvas gestures
retain their keyboard and pointer owners. Session teardown clears shared state.

The common editor typography helpers use shared text presentation with the existing
font/DPI/text-scale adapter, native UTF-8 measurement and drawing, width fitting,
wrapping and clip restoration. Queued Vulkan text is produced through the existing
text renderer; transient presentation buffers are consumed synchronously.

Object rename and Open/Create/Save As paths use shared UTF-8 draft editing and
presentation, including selection, clipboard editing, caret reveal and IME preedit.
Rename retains its 96-byte limit, single-line/control-character validation, custom
word navigation, blank-name refusal and revision-guarded commit/undo behavior.
IME composition is draft-only; Enter does not commit preedit. Cmd/Ctrl+A selects
the path. Cancel and failed path operations preserve document ownership and edits.
Document writes and Save As dependency containment remain in the existing command
owners, shared with MCP; no persistence, optical or MCP protocol change is included.

Qualification uses fresh source/build output and task-owned scene copies: native
chrome release/focus/window checks, Unicode/IME/overflow, rename undo/redo, canvas
navigation, failed path edits, themes and compact sizing, plus the existing native
scene lifecycle and MCP acceptance. Local macOS package/installed Main Edit
acceptance is verified separately.
The subsequent [field/panel/HUD slice](ui_fields_panels_runtime_hud.md) integrates
those active owners. The combined verified source checkpoint is adopted into local
canonical main; operator and other-platform acceptance remain separate.
