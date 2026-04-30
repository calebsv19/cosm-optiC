# Ray Tracing Future Intent

Last updated: 2026-04-29

## Direction

Keep `ray_tracing` stable as a hybrid editor/runtime while treating the shipped native `3D` RGB ladder as the current product truth, not as a future experiment.

## Near-Term Intent

1. Choose the next post-`I6` renderer lane cleanly.
   - The shipped native `3D` ladder is already closed through `Disney`.
   - New work should extend that ladder or its support systems intentionally, not reopen completed `I5`/`I6` slices.

2. Preserve deep-render export usability.
   - Keep absolute start-frame and resume-from-existing controls stable.
   - Keep frame numbering and path sampling locked to the same absolute-frame contract.

3. Keep menu/editor/runtime truth aligned.
   - Preserve current object color/material separation.
   - Preserve current RGBA object-authoring controls, object-level transparency/emissive multipliers, and the current runtime-scene persistence behavior.

4. Defer VF3D / `physics_sim` ingestion until the next internal renderer boundary is chosen and stabilized.

## Structural Intent

- Keep native `3D` renderer work seam-driven:
  - tier docs remain under the active integrator lane
  - completed sub-slices should archive instead of accumulating under active
- Keep large host files focused on orchestration:
  - `src/app/animation.c`
  - `src/render/pipeline/ray_tracing2.c`
  - `src/ui/menu/sdl_menu*.c`
- Keep support-layer changes shared across the shipped native `3D` ladder when the behavior is truly common:
  - temporal accumulation
  - sampling
  - denoise
  - preview/update behavior

## Non-Goals

- No reopening of closed grayscale-to-RGB migration slices.
- No broad rewrite that merges active renderer work with unrelated editor polish.
- No ingestion-expansion push ahead of the next internal renderer proof boundary.

## Private Planning Reference

Detailed execution history and archived support lanes live in:
- `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/ray_tracing/`
