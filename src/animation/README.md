# RayTracing Timeline Foundation

This directory owns authored frame meaning, detached property-track evaluation,
and immutable evaluated-frame snapshots. It exposes a transactional copied-scene
application seam, but does not mutate live global scene state, invalidate
renderer caches, persist data, own editor state/playback, step simulations, or
schedule wall-clock work.

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

## TAF3 evaluated-frame snapshot seam

`timeline_frame_snapshot.*` freezes one canonical evaluation context, copied
property results, provenance, and the aggregate invalidation-domain mask. The
snapshot has no mutator API and is consumed through `const` pointers.

Application stays outside the property registry. A caller supplies a typed
adapter plus reusable scratch scene storage; the seam copies the authored base,
applies every property in deterministic track order, validates the candidate,
and commits to caller output only after the complete application succeeds.
Static documents produce an empty snapshot, copy the base scene exactly, and
report no invalidation domains. Failed target resolution, snapshot validation,
property application, or scene validation leaves the authored base, committed
output, and application report unchanged.

## Next boundary: TAF3 live integration

Define stable live target identity and the first explicit `SceneConfig`/runtime
scene adapter, then map the descriptive camera/light/material/rigid-transform
domains onto existing scene preparation, accumulation, and TLAS/BLAS cache
owners. Object motion can become a compatibility consumer only after that
identity and invalidation routing are proven. UI and persistence remain later.
