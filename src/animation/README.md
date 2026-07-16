# RayTracing Timeline Foundation

This directory owns authored frame meaning and detached property-track
evaluation. It does not own scene mutation, renderer invalidation, persistence,
editor state, playback, simulation stepping, or wall-clock scheduling.

## TAF0 ownership audit

| Concern | Current owner | Foundation relationship |
| --- | --- | --- |
| FPS, authored start, and frame count | RayTracing animation configuration | Represented canonically by `TimelineRate` and `TimelineRange` |
| Absolute render frames and chunk/resume offsets | app and headless render orchestration | Converted to one `TimelineEvaluationContext` without changing orchestration |
| Seconds and normalized compatibility time | preview/headless callers | Derived from authored frame identity; `normalized_t` is never stored as a key address |
| Spatial path and rigid-motion sampling | path and motion modules | Future consumer; unchanged in TAF0-TAF1 |
| Camera, light, object, and material application | scene preparation | Future TAF2+ binding consumer; no mutation here |
| Renderer cache invalidation | scene/render preparation | Future typed-property metadata; no invalidation emitted here |
| Retained simulation-frame selection | scene-project and simulation bridges | Future explicit authored/simulated binding; unchanged here |

The anticipated invalidation domains are camera, lighting, material/shading,
rigid transform, deforming geometry/acceleration, volume, and simulation-cache
selection. TAF0 records those domains but deliberately does not encode them in
track data: TAF2 must introduce typed property descriptors before any track can
claim an application or invalidation policy.

## TAF1 contract

- Stable bounded target, property, and track identifiers.
- Scalar and vector values; rotation is reserved until its interpolation
  semantics are specified.
- Step and linear interpolation; cubic is reserved and refused.
- Sorted, unique authored-frame keys.
- Detached result evaluation with no access to scene or render state.
- Fixed capacities and explicit status results; no hidden allocation.

`core_time` remains the monotonic runtime clock and `core_sim` remains a
simulation orchestration boundary. Neither library owns authored timeline
documents or keyframe interpolation, so no shared module changed for this wave.

## TAF2 property registry

`timeline_property_registry.*` adds bounded copied descriptors for stable
property identity, target kind, value type, units, authoring ownership,
per-component bounds, allowed interpolation, and renderer invalidation domains.
The first registered meanings are object position, light intensity, and
material roughness.

Registry validation refuses unknown properties, mismatched targets/types/units,
non-authorable ownership, unsupported interpolation, out-of-range values, and
duplicate ownership of one target/property pair. Detached document evaluation
copies descriptor provenance and invalidation metadata into results only after
the complete request validates; failed evaluation does not mutate caller output.
The mask is descriptive in TAF2 and does not invalidate renderer state.

## Next boundary: TAF3

Design and implement one immutable evaluated frame snapshot/application seam
over copied scene state, then connect typed invalidation domains to existing
scene preparation and cache ownership. Keep application outside the registry
and prove static-scene behavior remains unchanged.
