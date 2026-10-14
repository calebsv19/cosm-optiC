# Shared preview controls and scene catalog provenance

Preview has a caller-owned shared control surface for Play/Pause, Loop, Bounce
and Close Preview. The native Vulkan painter uses the existing shared button
style and app shared text/theme adapter; rectangular geometry remains intentional.
Matching release activates the existing transport command. F6 enters control
traversal; Tab/Shift-Tab and Enter/Space use shared focus and repeat semantics.
Outside traversal, Space, L/B, arrow stepping, Q quality and Escape retain their
application shortcuts. Window identity filtering, resize, focus/visibility and
fullscreen transitions cancel stale control and scrub ownership. A canceled
scrub retains its last inspected sample and restores its previous playing state;
its later release cannot seek again. Embedded exit retains the host restore policy.

Normal windows retain the horizontal transport arrangement. Compact windows
place the slider and frame readout below the buttons. Controls qualify at
320x240, 480x360, 639/640x360, 800x600 and 1024x768 logical sizes. Route diagnostics
reserve Close Preview's bounds. Selected-marker readouts take priority over
secondary route diagnostics at compact sizes; readouts and labels are clipped to
their allocated rectangles. This changes presentation, not camera evaluation,
timeline clock/range, marker projection, quality routing or scientific pixels.

Scene catalog rows retain their original name and have a separate directory line.
The directory is the shortest parent-path suffix that distinguishes scanned
locations, computed from copied catalog strings without filesystem work on the
UI thread. Multiple files in one directory share provenance. Existing resolved-path
alias deduplication is retained. Distinct scene copies are not collapsed by name,
content or directory label. Paths, row ordering, filters, selected source, document
loading and save identity remain unchanged. Scene row height follows the actual
font line height, and hit testing uses the painted height. Volume rows keep their
previous layout. Names and provenance use the shared native text presentation.

Verification commands (fresh contained source and output):

```sh
make BUILD_TOOLCHAIN=clang all scene-editor-workspace-visual-test
make BUILD_TOOLCHAIN=clang test-preview-shared-controls-contract test-scene-catalog-labels
make BUILD_TOOLCHAIN=clang test-menu-catalog-discovery test-menu-shared-controls-contract
make BUILD_TOOLCHAIN=clang test-shared-theme-font-adapter test-ray-tracing-window-lifecycle-contract
make BUILD_TOOLCHAIN=clang test
```

The aggregate suite includes preview transport, compact workspace/canceled-scrub,
timeline inspection and menu contracts. The native workspace harness mode
`--preview-shared-ui` qualifies normal/compact embedded preview, an interrupted
Close gesture, exact frame stepping, unchanged editor document revision/path and
host restoration. Run it with cloned scene/runtime data and Vulkan validation.
Existing `--shared-ui` and `--shared-fields-hud` modes cover surrounding editor
control/text/save parity. Development packaging and installed visual review are
separate proof layers. These app adapters require no shared module, app VERSION
or WORKER_VERSION change. Operator and other-platform acceptance remain separate.
