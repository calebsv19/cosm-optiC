# Shared fields, specialized controls and runtime HUD

The retained Main Edit uses the existing immutable shared UI pin through native
Vulkan adapters. Editor search and numeric/inspector drafts share scalar-safe
editing, selection, clipboard operations, caret reveal and IME presentation.
Enter, Escape and Tab retain each owner's commit/cancel policy; preedit and key
repeat cannot submit. Buffers, parsing, numeric ranges/units, validation, document
revisions and history remain application-owned. Search changes filters, not documents.

Covered owners include transform/name, camera/light, atmosphere appearance/placement,
material and surface mapping/resources, graph search, motion path/orientation/plan,
object/timeline keys and clock/channel settings, and render options. Menu numeric
editing adopts shared text operations/presentation while keeping its ASCII draft
syntax, snapping, range normalization and setting-save policy.

Specialized controls publish explicit semantic keys and clipped paint bounds through
the editor surface. Activation calls the existing handler on matching release.
Document/selection, geometry, window, modal and interrupted-gesture changes cancel
capture. F6 traversal yields to active text input. Existing scene/library selection,
sliders, scrubbing, splitters, gizmos, import/folder preparation and purpose-specific
commands retain their domain owners. Rectangular geometry remains intentional.

The runtime progress HUD has a separate painter using shared text presentation and
HUD style. Narrow windows stack the progress details; normal windows use two columns.
The panel, bar and rows remain inside the logical render viewport and restore the
caller clip. Timer diagnostics consume the same shared text presentation while
preserving alignment and timing ownership. Progress, pass/tile counters, ETA,
computation, cancellation, publication and hidden-window work remain unchanged.

Fresh application build, focused contracts and aggregate regression qualify source.
Isolated native field/control and document, camera, object, lighting, timeline,
motion-orientation and material UI/MCP acceptance qualify behavior and saved-state
parity. Actual 2D/3D render windows qualify HUD/window transitions, hidden export and
continued asynchronous computation with Vulkan validation enabled.

The October 14 macOS arm64 Main Edit package passes source/binary identity,
bundled dependencies, managed STL import, isolated runtime storage and strict
code-signature verification. Packaged visual review covers normal startup, menu
numeric drafts, search, inspector cancellation, material controls, File Save and
fresh-process reopen. A bounded native render exercises progress/timer HUD display
and returns to the menu. This local package proof does not establish operator or
other-platform acceptance. The combined optical/atmosphere/MCP/UI source checkpoint
is adopted into local canonical main, retaining persistent Main Edit for development.

## Timeline preview and catalog follow-ups

Timeline preview transport/Close controls now use a separate shared surface and
native shared button painter. Diagnostics, frame and selected-marker readouts use
the same shared text/theme integration. Catalog rows show directory provenance;
scene paths, ordering and selection retain their original owners. See
[preview and catalog integration](ui_preview_catalog.md) for the scope and checks.
Operator and other-platform acceptance remain separate from local qualification.
