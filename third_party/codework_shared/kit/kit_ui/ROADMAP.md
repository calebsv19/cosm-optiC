# kit_ui Roadmap

`kit_ui` is currently a shared immediate-mode control layer on top of `kit_render`.

## Implemented Now

- stack layout helpers
- clip-stack helpers
- label, button, checkbox, slider, scrollbar, and segmented control drawing
- simple input evaluation helpers
- theme-scale style sync
- text measurement and text-fit helpers
- validation harness coverage for live Vulkan-backed checks

## Deferred

1. keyboard focus and navigation helpers
2. retained row/list helpers for higher-level inspectors
3. settings/action binding adapters
4. richer text input or editor controls
5. host-specific event-loop, pane, or persistence behavior

## Hardening Notes

- Version `0.9.1` keeps the `0.9.0` button semantics contract intact while hardening 1px shared button borders against corner overrun artifacts on snapped pixel grids.
- Version `0.9.0` adds additive button spec/state/layout/style helpers plus shared spec-driven button drawing without broadening retained-state ownership.
- Large-file decomposition is now partially applied through the extracted `kit_ui_button.c` lane; broader retained-row/editor decomposition remains deferred.
