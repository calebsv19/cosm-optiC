# core_scene_view

`core_scene_view` owns the renderer-free scene-view packet vocabulary used by
apps to exchange preview/readback metadata without sharing editor behavior.

Version: `0.1.0`

## Scope

- schema family/variant constants for scene-view packets
- preview quality, degraded reason, display flags, and pick-id vocabulary
- compact JSON readback validation for packet metadata

## Boundary

- Does not own rendering, viewport input, picking policy, material sampling, or
  editor mutation.
- Does not create, delete, move, resize, or rewrite scene objects.
- App producers still own packet construction/serialization until more hosts
  prove the contract.
- App consumers still own local object/face mapping and UI behavior.

## Current Proof

- RayTracing produces `ray_tracing_scene_view_packet_v0` JSON locally.
- LineDrawing consumes the same JSON through a read-only fixture.
