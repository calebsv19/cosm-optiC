# optiC Current Truth

Last updated: 2026-07-13

## Program Identity
- Repository directory: `ray_tracing/`
- Public product name: `optiC`
- Internal/repo/runtime identifiers still use `ray_tracing` and `RayTracing`
  in launcher, log, binary, and source-level contracts where required
- The compiler-units rollout now has its first explicit RayTracing customer
  lane:
  - dual-toolchain `clang-build` / `fisics-build`
  - explicit `PACKAGE_TOOLCHAIN` package source selection
  - sema targets on:
    - `src/import/runtime_scene_bridge.c`
    - `src/import/runtime_scene_bridge_authoring.c`
    - `src/app/animation_fluid_scene.c`
    - `src/render/runtime_light_emitter_3d.c`
    - `src/render/runtime_volume_3d_sampling.c`
    - `src/render/runtime_native_3d_sampling.c`
    - `src/render/runtime_volume_3d.c`
    - `src/render/runtime_volume_3d_integrate.c`
    - `src/render/runtime_volume_3d_scatter.c`
    - `src/render/runtime_direct_light_3d.c`
    - `src/render/runtime_visibility_3d.c`
    - `src/render/runtime_camera_3d_rays.c`
    - `src/render/runtime_ray_3d.c`
  - current honest units boundary covers:
    - runtime-scene object positions and primitive dimensions
    - authoring path coordinates, light/camera seed positions, focus target,
      scene bounds, and construction-plane offset
    - fluid-manifest world-fit camera placement, default orbit extents, and
      import placement interpolation into world space
    - emitter radius, falloff distance, and hit-distance attenuation checks
    - volume bounds clip distances and world-position-to-voxel normalization
    - volume grid voxel size, bounds extents, and timestep metadata
    - volume transmittance march-step and segment-length semantics
    - volume single-scatter light-path distance and sample-step semantics
    - direct-light light-path distance and attenuation-falloff semantics
    - visibility segment-distance, light-distance, and target-distance semantics
    - camera near-plane and projected camera-depth semantics
    - ray offset epsilon and first-hit distance semantics
  - support-only sema coverage now also records:
    - native `3D` unitless subpass/jitter sampling in
      `src/render/runtime_native_3d_sampling.c`
  - `extensions.ray_tracing.authoring.light_settings.radius` was checked and
    intentionally left on its existing renderer-local scalar contract
- Primary runtime entry:
  - `src/app/animation.c` (`main()` delegates through `ray_tracing_app_main(...)`)
  - wrapper shell: `include/ray_tracing/ray_tracing_app_main.h`, `src/app/ray_tracing_app_main.c`

## Stable Worker Routing Truth

- the current proven trio worker lane supports preferred-home-server routing
  with fallback still enabled
- a fresh forced-Linux-PC lean trio proof now completes through the normal VPS
  worker status and visualizer publish path after lean result handoff hardening:
  - `ray-tracing--trio-headless-worker--20260604T163741Z--leanpc01`
  - claimed by `linuxpc` at `2026-06-04T16:38:35Z`
  - `start_stage=physics_sim`, one PhysicsSim frame, one RayTracing frame,
    `volume.enabled=false`
  - the submitted package set had to use the VPS registry-pinned
    `ray_tracing_headless_worker@0.1.0` declaration
  - manifest:
    `https://visualizer.calebsv.tech/artifacts/ray-tracing/ray-tracing--trio-headless-worker--20260604T163741Z--leanpc01/manifest.json`
- a fresh tiny trio proof now completes end to end after the VPS remote upload
  ceiling was raised:
  - `ray-tracing--trio-headless-worker--20260526T012119Z--homeserverlimitfixc`
- a fresh mixed tiny queue proof confirms fallback behavior:
  - `ray-tracing--trio-headless-worker--20260526T013518Z--quickmixtrio1`
    completed on the VPS local dispatcher
  - in the same wave, BehaviorSim and BallBounceSim had already consumed the
    single homeserver worker slot
- current operational reading:
  - homeserver preference is active
  - trio should still complete when the homeserver is busy instead of waiting
    indefinitely

## Current Shipped State
- Legacy `2D` rendering and editor flows remain present.
- Linux GUI package status:
  - `desktop_app_linux` is proven as a private Linux desktop package class for
    optiC, separate from the already-published Linux headless worker/CLI
    artifact.
  - The private proof package shape is
    `optiC-<version>-linux-x86_64-desktop-stable.tar.gz` plus `.sha256`
    sidecar.
  - Release-grade Linux GUI proof requires deterministic archive metadata,
    package self-test, clean unpacked launcher self-test, real Linux PC
    desktop-session launch, app-window screenshots, and a runtime action marker.
  - This is not yet a public Linux GUI release; public promotion still requires
    a separate release-control version/export/metadata/readback lane.
- Native `3D` runtime ladder is shipped through:
  - `Direct Light`
  - `Diffuse Bounce`
  - `Material`
  - `Emission / Transparency`
  - `Disney`
  - `Disney v2` as an experimental principled-BSDF scaffold route available
    through the distinct `disney_v2` headless/UI identity
- Experimental `Disney v2` is a separate principled-BSDF route, not a
  replacement for the shipped Disney combiner. It consumes cached native `3D`
  material payloads through `RuntimePrincipledBSDF3D`, records stochastic
  direct/BSDF path state with RGB throughput, PDFs, power-heuristic MIS
  weights, emitter-hit
  diagnostics, secondary material diagnostics, bounded recursive path-loop
  state, and shared `RuntimePathDepthPolicy3D` max-depth/roulette decisions.
  The route now has bounded proof support for transmission/glass
  participation, primary camera-through transparent layer accumulation,
  thin-walled versus solid-glass policy, solid transparent interior-return
  contribution, physical-transmission versus alpha-only classification,
  object-level medium-stack diagnostics, and primary mirror-material geometry
  reflection through the native specular reflection helper. Its bounded
  recursive path loop now re-resolves each secondary material vertex and
  resamples a local diffuse/specular/transmission lobe with local
  throughput/PDF/MIS diagnostics. Reflected mirror/glossy geometry can now
  re-enter that Disney v2 vertex loop with separate reflected geometry,
  emitter, no-hit, rough-sample, contribution, and depth/roulette diagnostics.
  The first estimator-quality pass moved Disney v2 MIS to a focused
  power-heuristic sidecar and added adaptive rough-reflection sample counts:
  tiny canonical proofs can use multiple rough probes, while imported and
  skull-scale scenes cap rough probes to protect local proof cost.
  Disney v2 now also has its own final-resolve denoise/temporal reconstruction
  policy: it only filters stable same-triangle/same-object interiors after
  temporal accumulation, rejects clean visual edges, preserves transparent and
  genuinely low-roughness mirror/glossy pixels, skips high-temporal-activity
  pixels, leaves rough reflective stable interiors eligible for blur, and
  exposes raw-versus-reconstructed/preserved/skipped diagnostics through native
  render stats. Disney v2 also defaults to an analytic caustic sidecar policy
  for focused glass caustic contribution. Headless requests can set
  `inspection.caustic_mode = "off"` for comparison renders; explicit
  `transport` / `physical` requests can populate bounded physical caustic
  volume and/or surface caches when the matching cache flags are enabled.
  Volume-cache transport requires a sampleable VF3D attachment; without one,
  the volume cache is suppressed and reported while surface-cache transport can
  still run. Volume-cache deposits use normalized beam footprints that spread
  each segment deposit over valid neighboring VF3D-aligned cache cells while
  preserving deposited/input energy accounting in summaries. Caustic-volume
  scatter summaries also report sampled cache radiance, density/remap,
  scatter probability, camera transmittance, final visibility term, and
  contributing screen-space concentration for final visibility diagnostics.
  The caustic-cache scatter path treats cached caustic energy as focused
  transport beam energy rather than applying ordinary direct-light isotropic
  phase attenuation a second time; direct volume fog remains on its separate
  scatter-gain path. Caustic-cache volume scatter now samples that derived
  cache with a bounded voxel-scale filter so camera-ray VF3D march samples can
  see nearby narrow beam deposits instead of requiring exact occupied-cell
  point hits.
  The local sphere/mist matrix now uses a review-oriented oblique fixture with
  separate no-caustic, surface-only, volume-only, and combined cells plus
  contract/overlay sheets; current readback proves a separable but still
  narrow volume-caustic column rather than a broad readable mist beam. The
  surface-only review cell has now been calibrated down from the earlier
  proof-scale settings, so volume-only is the primary VF3D caustic visual
  proof while the combined cell remains an integration check. Phase 11J
  calibrated the surface review cells to surface radiance scale `0.5` and
  footprint scale `1.0`; surface-only is no longer classified as overexposed,
  and it no longer dominates the combined cell. Phase 11K then widened
  renderer-side caustic-volume segment footprints without changing direct fog
  gain or VF3D density. The local sphere/mist matrix now writes
  `phase11k_readback` and passes: volume-only rises to `83` positive pixels,
  `263` nonzero cache cells, occupancy `0.008026123`, hit ratio `0.008578998`,
  `451` contributing scatter pixels, average footprint radius `1.828057509`
  voxels, `0.562499986 x 0.562499986 x 1.912499949` world-span, and `21 x 50`
  screen-span; it is no longer classified as a narrow column.
  A follow-up local authored-validation matrix now exercises the same
  volume-caustic-only proof across three authored mesh-sphere variants and
  reports explicit threshold margins for visual area, luma delta, cache
  occupancy, cache hit ratio, scatter/cache conversion, and lateral spread.
  The conservative `256x128`, `128x64`, and raised `64x32` authored variants
  pass locally; this is broader local validation, not remote render,
  latest-good promotion, or coverage for cylinder/bowl/water-like shapes.
  Headless
  `ray_tracing_render_headless` requests can override
  `render.denoise_enabled` per detached run. The D2.18b visual matrix runner
  now renders comparable Disney-v2 proof cells, copies summaries/requests,
  emits PNG frames, contact sheets, amplified diffs, and `diff_metrics.json`
  packages. The first canonical matrices cover the primitive glass corridor,
  transparent/interior preservation, mirror/glossy preservation, and high-noise
  emitter review under
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/`.
  Ordinary emissive material surfaces now participate as bounded Disney v2
  path endpoints for first BSDF secondary hits and recursive-loop hits, with
  emissive-material counters/MIS endpoint classification kept separate from
  finite-radius runtime light sphere hits. Disney v2 also samples ordinary
  emissive material triangles directly from the primary visible vertex and
  from bounded recursive transport vertices as area lights through the shared
  native emissive-direct helper, with separate emissive-area radiance and
  sample/contribution counters. Reflected mirror/glossy recursion preserves
  recursive emissive-area radiance and per-vertex light-branch diagnostics
  when merging reflected geometry back into the primary result. Native
  emissive-direct shading uses derived scene capabilities to skip mesh-emission
  support when a scene proves there are no emissive surfaces. On the normal
  cache-valid Disney v2 path, ordinary emissive material triangles are now
  collected once into a cached `RuntimeEmissiveLightSet3D` during runtime scene
  capability refresh, then sampled as bounded candidates from primary and
  recursive vertices instead of scanning the full triangle mesh per receiver
  hit. Headless summaries and promotion reports expose candidate count,
  selected candidates, visibility rays, primary and recursive area samples, and
  invalid-cache full-scan fallback count.
  The visual-matrix lane now includes a repo-local imported-mesh pressure MRT8
  proof using `asset_sphere_128x64`; its private review package compares
  direct/material/shipped-Disney/Disney-v2 layers, includes a Disney v2
  `temporal_frames=12` denoise on/off diff, and feeds a thresholded promotion
  report with two imported-mesh scenes. The threshold report is a local
  regression gate with separate performance and quality/convergence threshold
  outcomes for elapsed cost, route ratio, visible pixels, secondary rays/hits,
  bounded max radiance, BVH health, and route isolation. It still keeps
  `promotion_ready=false`.
  The skull/high-triangle proof now has a preparer that stages the relocated
  skull runtime sidecar into a private scene package, rewrites stale absolute
  mesh paths to relative scene-local paths, and renders a first direct-light
  versus Disney-v2 smoke matrix at `171,272` BVH triangles with zero trace
  overflows. The D2.22 repeat helper now reruns the D2.20 candidate threshold
  report and D2.21 skull smoke matrix into a private stability report. Current
  policy keeps recurring shipped/local gates on low-to-moderate imported mesh
  fixtures, keeps skull/high-triangle proof private/manual, and excludes
  shipped `disney` from skull-scale comparison for cost. SU4 mirror
  surface-unification proofs now compare the same mirror wall as an authored
  plane, thin rect prism, and runtime mesh asset; all three report the same
  mirror-dominant/reflection-hit/emitter-hit structure in the canonical visual
  matrix.
  This is still an experimental bounded integrator: it does not yet provide
  full production-quality recursive estimator tuning, a settled recursive
  emissive-area sampling policy for large emitter sets, BRDF-evaluated
  direct-light estimator quality for rough reflection/transmission, or
  release-candidate threshold repetitions.
- The shipped native `3D` direct-light tier samples area lights from a
  16-slot hit-seeded stratified finite-radius disk population. Shadow checks
  start with 4 samples and stop there for clearly fully visible or fully blocked
  windows, escalating to 8 only for mixed, penumbra, or partial-transparency
  cases. Samples whose maximum direct contribution is below the radiance epsilon
  skip shadow visibility entirely, so softness tracks authored `light.radius`
  without stable five-point bands or paying the full sample cost everywhere.
- Native `3D` direct lighting now routes through a scene-owned
  `RuntimeLightSet3D` registry instead of relying on only the legacy single
  light mirror. Authored runtime-scene `lights[]` entries import into that
  registry, headless summaries expose `registered_lights`, and finite direct
  lighting iterates enabled registry entries. The first authored/compatibility
  light remains the only Bezier/motion-driven light until a future per-light
  motion contract exists; later registered lights are static by policy.
- Material-authored emitters bridge into that registry without replacing the
  mesh-emissive area-sampling cache. Simple single-candidate material emitters
  become enabled static sphere proxy lights. Flat same-plane material emitters
  become enabled static `rect` proxy lights with `origin=material_emitter`,
  direct-light area samples across the panel axes, and default `one_sided`
  emission profile energy. Complex arbitrary mesh emitters stay disabled
  `mesh_emissive` diagnostics marked `meshAreaSamplerOnly`; their physical-ish
  route remains the cached `RuntimeEmissiveLightSet3D` area sampler used by
  Disney v2 / mesh-emissive paths. The authoring implication is intentional:
  a flush flat panel is directional outward, while visible wall halo or side
  spill should be authored as offset/tilted panel geometry or a thin 3D
  lightbox fixture until a later mesh-light MIS lane exists.
- Native `3D` scenes now derive runtime capabilities from geometry, resolved
  material payloads, and attached volume state; legacy material flags are
  mirrored from the same refresh for compatibility. Hit-to-light visibility uses
  those capabilities for the opaque/no-volume shadow fast path, while primary
  volume scatter can skip camera-ray scatter setup when both capabilities and
  live volume state show no lighting-affecting density. The cached primary-hit
  emission/transparency path also skips mesh-emission support only when
  capabilities are valid, the scene has no emissive surfaces, and the current
  payload is opaque/non-emissive; the same cached entrypoint can bypass
  transparency support only when valid capabilities and the current payload prove
  the scene is opaque/non-transmissive. `ShadeHit`/`ShadePixel` compatibility
  paths and transparent or unresolved scenes stay on the full support path.
- Native `3D` ray hits now use camera-facing shading normals for triangle
  intersections, and generated zero-thickness plane primitives are explicitly
  marked `twoSided` in runtime triangle metadata. This keeps authored room-wall
  planes visible to direct lighting even when the camera/light sees the back
  side of the authored plane, while rect-prism and imported/runtime mesh
  triangles still carry single-sided metadata for future material policy.
- Native `3D` integrator dispatch now shares one primary camera-ray geometry
  trace per pixel across visible-emitter resolution and the selected integrator;
  hit-based entrypoints consume the cached primary hit/transmittance/material
  payload so the renderer no longer runs the emitter resolver, `ShadePixel`
  primary trace, and repeated first-hit material/texture resolution as separate
  scene work for the same pixel.
- Native `3D` output is RGB-aware through the full shipped ladder.
- Runtime UI text measurement now routes through one guarded font contract:
  - only fonts still owned by the live runtime font cache are considered usable
  - HUD/menu measurement and line-height helpers no longer call `SDL_ttf`
    sizing APIs directly on unchecked font pointers
  - when a stale font handle is encountered, text draw/measure now fails closed
    instead of dereferencing the dead handle
- Native `3D` support layers now include:
  - tile preview
  - dirty-rect preview updates
  - shared-frame preview reconstruction
  - explicit `Nearest` / `Bilinear` upscale policy selection
  - tile occupancy culling
  - temporal accumulation upgrades
  - stratified + blue-noise sampling support
  - Disney denoise for the original Disney route plus a separate Disney-v2
    edge-safe final-resolve reconstruction policy; Disney v2 still does not
    inherit shipped Disney temporal pruning
  - explicit environment-light modes:
    - `off`
    - `top_fill`
    - `ambient`
  - separate ambient and top-fill strength controls
  - authored native `3D` environment resolution for ambient-mode scenes:
    - `environment_preset`: `sky`, `warm_sky`, or `neutral`
    - `background_brightness`: explicit miss-pixel/background radiance, or
      ambient-strength-derived compatibility when omitted
    - `background_color`: RGB tint applied to the preset gradient
    - headless summaries expose a resolved `environment_lighting` block that
      says whether ambient surface fill, background miss radiance, or top-fill
      can contribute
- Native `3D` low-resolution presentation no longer has one hardcoded upscale policy:
  - preview dirty-rect redraw and completed-frame resolve both route through
    `runtime_native_3d_preview_reconstruction.*`
  - the persisted/menu/runtime contract now exposes `upscaleMode3D` through a
    renderer-controls button and the runtime `U` key cycle
  - `OFF` preserves a raw non-smoothed low-resolution present path
  - `Nearest` preserves crisp pixel structure at low render scales
  - `Bilinear` keeps the smoother reconstruction path available as an
    explicit mode instead of an accidental default
- Native `3D` material payloads now support opt-in procedural material texture overlays:
  - `textureId=1` rust overlay
  - `textureId=2` fog overlay
  - per-object texture pan/scale/strength fields persisted on scene objects
  - per-object node-ready texture parameters persisted on scene objects: pattern mode, coverage, grain, edge softness, contrast, flow, color depth, surface damage, and seed
  - path-traced/native hit anchoring currently uses triangle barycentric hit coordinates so overlays remain attached to surfaces as view/light state changes
  - generated Material face-group texture overrides now feed the native `3D` material payload when a runtime hit carries a local triangle ordinal, so rust/fog overrides and their parameter blocks can change actual BSDF roughness, reflectivity/specular weight, diffuse response, color, and transparency in the simulation path
- Native `3D Material` is now the first shipped tier that traces bounded opaque
  specular scene reflection:
  - mirror/glossy opaque hits can reflect ordinary non-emissive scene geometry
    and preserve reflected chroma
  - `Direct Light` and `Diffuse Bounce` remain cheaper lower tiers without the
    opaque specular scene ray
  - `Emission / Transparency` and `Disney` inherit the Material-tier specular
    reflection signal in their composed radiance
  - shipped `Disney` now attenuates only its added direct-light specular lobe
    for mirror-dominant payloads, so directly illuminated default Mirror
    objects keep traced reflected geometry dominant instead of washing out into
    the light color
- Native `3D` material payloads also support the first v2 layered material texture stack:
  - bounded ordered stacks carry durable layer ids, base/overlay roles, blend modes, placement, procedural parameters, enabled state, opacity, and material influence fields
  - base layer kinds include solid, brushed metal, wood, brick, concrete, and stone in the stack/evaluator contract
  - overlay layer kinds include rust, fog, grime, and oil in the current Material editor surface, with scratches and edge wear reserved in the enum contract
  - runtime scene authoring save/load persists `material_texture_stack` data additively beside legacy `procedural_texture` compatibility fields
  - the Material preview and native payload path both evaluate through `RuntimeMaterialTextureStackEvaluatePlacedUV(...)`, so focused editor preview and simulation material response share the same stack math
- Material mode now has a dedicated right-pane Active Face Preview for selected generated face groups:
  - the center viewport remains the interaction/selection lane
  - the right-pane preview renders one active face through the shared non-ray-traced material surface evaluator
  - the preview can ignore or honor material alpha for inspection
  - rectangular faces preserve their real aspect ratio, including very skinny prism sides
  - face metrics ground procedural texture sampling to real plane/rect-prism dimensions and orient vertical faces with world `+Z` as preview-up where available
  - selected face addresses now carry primitive-local triangle ordinals so preview dimensions and orientation resolve against the intended primitive/face group instead of a fallback object order
- Native `3D` material payloads now also support object-bound authored bitmap texture sets for the current texture-authoring roundtrip:
  - `drawing_program` exports separate per-face RGBA PNG files plus one JSON manifest
  - `ray_tracing` reads that manifest through app-local authored-texture import code
  - bindings are restored by stable `object_id` under `extensions.ray_tracing.authoring.object_materials[*].authored_texture`
  - the same `extensions.ray_tracing.authoring.object_materials[*]` lane now also
    supports object-wide review-time overrides for:
    - `object_color`
    - `alpha` / `transparency`
    - `reflectivity`
    - `roughness`
    - `emissive_strength`
  - runtime defaults now treat omitted `emissive_strength` as `0.0` rather than
    implicitly emissive
  - authored transparent objects should use `material_id = 5`
  - `material_id = 4` remains the explicit emissive preset lane
    - object-wide procedural texture shorthand:
      - `texture_id`
      - `texture_strength`
      - `texture_scale`
      - `texture_offset_u`
      - `texture_offset_v`
      - `texture_seed`
      - `texture_pattern_mode`
      - `texture_coverage`
      - `texture_grain`
      - `texture_edge_softness`
      - `texture_contrast`
      - `texture_flow`
      - `texture_color_depth`
      - `texture_surface_damage`
  - the first non-raytraced material-preview service is now shipped through
    `ray_tracing_material_preview_headless`:
    - loads the same compiled runtime scene contract as the render CLI
    - resolves one target object by `object_id` or `scene_object_index`
    - evaluates the same object-wide procedural/material stack without running
      the native `3D` integrators
    - writes a deterministic BMP swatch or contact-sheet preview plus an
      optional JSON summary of effective per-variant values
  - runtime-scene authoring persistence now writes authored-texture manifest paths back relative to the runtime-scene file when the manifest lives beside that scene, so reopen does not depend on a machine-local absolute export path
  - runtime-scene authoring persistence now writes explicit cleared authored-texture state (`null`) when an object no longer has a bound authored texture, so overlay merge removes stale bindings instead of preserving them by omission
  - invalid authored-texture binds now record a bounded object-local invalid state instead of silently collapsing to unbound
  - the selected-object Material editor now surfaces that invalid state explicitly with:
    - attempted manifest path
    - bounded validation failure reason
  - runtime-scene authoring persistence now preserves invalid authored-texture entries by writing manifest path plus binding mode instead of erasing them to `null`
  - reopen now replays persisted invalid authored-texture entries and resurfaces the same invalid state when validation still fails
  - supported face-role mapping is explicit for planes and generated rectangular-prism faces
  - hit-time payload resolution now samples authored bitmap color and alpha before procedural fallback when a bound face is hit
  - the authored/procedural coexistence rule is now explicit:
    - authored bitmap sampling establishes the substrate/base color and alpha response for the hit
    - procedural base-role texture-stack layers do not replace that authored substrate
    - procedural overlay-role texture-stack layers may still modulate above it
    - legacy rust/fog `textureId` overlays remain compatible because they still enter through overlay-role evaluation
  - runtime-scene authoring persistence retains the manifest path/binding metadata rather than embedding image payload bytes
  - the authored-texture manifest reader now supports the first narrow dual-lane runtime contract:
    - base-lane faces load from `base_surfaces` when present, otherwise fall back to legacy `surfaces`
    - overlay-lane faces load from optional `overlay_surfaces`
    - top-level `overlay_material_intent_kind` is now preserved for runtime material-response mapping
  - dual-lane authored sampling now behaves as:
    - authored base lane establishes the substrate/base color and alpha response
    - authored overlay lane samples independently by the same face-role mapping
    - overlay alpha blends over the authored base before the remaining downstream material policy runs
    - overlay material intent is now mapped through the existing runtime texture-stack taxonomy so `grime`, `oil`, `rust`, and `fog` can modify BSDF/material response instead of acting as pure color decals
  - authored-texture manifests now also expose finer-grained per-surface material-intent metadata to the runtime:
    - `layer_material_intent_stable_ids` is read from manifest schema `v5`
    - runtime face metadata now derives bounded face-level base and overlay material-intent stable ids from those exported values
    - for the current lane-faithful export transition, the bounded face-metadata getter now merges overlay-face overlay intent into the returned face metadata instead of assuming overlay intent still appears in base-surface semantic arrays
    - explicit per-face summary fields are now the primary runtime contract:
      - `base_material_intent_kind`
      - `overlay_material_intent_kind`
    - current runtime compatibility behavior is now explicit:
      - when those summary fields are present, loader and payload logic prefer them over the raw `layer_material_intent_stable_ids` array
      - when they are absent, older manifests still fall back to the previous bounded one-per-family derivation from the raw array
    - the durable authored-texture runtime rule is now explicit:
      - one effective base/substrate intent per face
      - one effective overlay/environment intent per face
      - raw per-layer `layer_material_intent_stable_ids` arrays remain auxiliary/editor-facing metadata and are not a richer mixed-material runtime BSDF contract
    - hit-time payload resolution now consumes that richer metadata narrowly:
      - face-level base/substrate intent may modulate BSDF response after authored base-lane sampling
      - face-level overlay/environment intent may modulate BSDF response after authored overlay-lane sampling
      - face-level overlay intent now takes priority over the older coarse binding-wide `overlay_material_intent_kind`, which remains only as fallback
  - authored-texture manifest validation is now explicit instead of permissive:
    - bind only accepts explicitly allowed schema versions
    - schema `v5` manifests must provide a valid `emitted_output_kind`
    - `export_binding_kind` must be `SEPARATE_FACES`
    - supported authored-texture primitive kinds remain bounded to `PLANE` and `RECT_PRISM`
    - face-array loading is strict:
      - malformed entries fail the bind
      - duplicate face-role declarations fail the bind
      - unreadable or missing face image files fail the bind
    - base-lane completeness is enforced for the declared primitive kind
    - declared bounded overlay lanes must also be complete
    - invalid authored-texture binds now leave the object fully unbound instead of partially active
  - the authored-texture loader/runtime lane now also partially adopts shared `core_authored_texture >= 0.1.1`:
    - schema/version, binding/output/primitive vocabulary, face-role semantics, and manifest-contract validation now come from the shared core module
    - the cutover is intentionally narrow:
      - JSON parsing
      - image loading
      - invalid-binding UX
      - runtime-scene persistence policy
      remain app-local
    - current schema `v5` manifests now expect the strict shared lane shape:
      - `base_surfaces`
      - optional `overlay_surfaces`
      - no mixed legacy `surfaces` field in the current `v5` output path
    - `core_authored_texture >= 0.1.2` is now the actual runtime floor for this lane:
      - `FLATTENED_ONLY` manifests that still declare an overlay lane are rejected by the shared validator
      - runtime validation coverage now also explicitly rejects the mixed `schema v5` drift shape (`surfaces` plus `base_surfaces`)
      - semantic-net validation now also routes through the shared module
  - the authored-texture manifest reader now accepts optional semantic net metadata per face:
    - net layout kind
    - canonical net slot
    - orientation
    - ordered corner ids
    - ordered edge ids
    - adjacent face-role hints
    - manual layout offsets
  - semantic-net metadata is no longer best-effort:
    - plane faces now require:
      - `net_layout_kind = PLANE`
      - `net_slot = FRONT`
      - `corner_ids = [255,255,255,255]`
      - `edge_ids = [255,255,255,255]`
      - `adjacent_face_roles = [NONE,NONE,NONE,NONE]`
    - rect-prism faces now require:
      - `net_layout_kind = PRISM_CROSS`
      - `net_slot` matching the face role
      - orientation in `R0/R90/R180/R270`
      - four unique `corner_ids` in `0..7`
      - four unique `edge_ids` in `0..11`
      - four unique valid adjacent prism face roles that are neither `NONE` nor the face's own role
    - malformed semantic-net metadata now fails authored-texture bind instead of degrading silently
    - compatibility remains bounded:
      - legacy neutral adjacency aliases `SURFACE` and `UNSPECIFIED` still normalize to canonical `NONE`
  - that richer manifest metadata is now available through a bounded runtime face-metadata getter, while current face sampling still resolves by explicit face-role mapping
- Scene editor mode routing now has four top-level modes:
  - Path / Bézier
  - Objects
  - Camera
  - Material
- Material editor mode is the first focused object material lane:
  - focuses the current selected object or the most recently selected object
  - keeps the global transparent material preset as the Glass default seed
    while allowing object-local Glass transport overrides for transmission,
    IOR, absorption distance, and thin-walled behavior through the compact
    `Resp` pane; those overrides persist with the scene object and feed both
    Material preview and native `3D` material payload resolution
  - keeps the global Mirror material preset as the Mirror default seed while
    allowing object-local Mirror response overrides for reflectivity,
    roughness/sharpness, specular strength, and tint through the compact
    `Resp` pane; Mirror dominance/base attenuation are read back from the
    resolved payload, and override values feed both Material preview and native
    `3D` material payload resolution
  - the compact Material editor `Proof` pane now labels Mirror proof/readback
    state for default, tinted, rough, and illuminated Mirror objects; this is a
    request/readback affordance that names current coverage and missing proof
    packages, not a proof-generation launcher
  - shows an object-only focused viewport layer in 2D and retained native `3D` digest lanes
  - defaults native `3D` Material mode to an object-centered projector so the focused object orbits around its own center rather than its scene placement
  - retains the old scene-placement object-only projector path as an internal view mode for a later UI toggle
  - frames and wheel-zooms against the focused object's extents in object-centered native `3D` Material mode, with a much wider accumulated zoom range for close texture inspection and far-distance preview
  - renders the focused native `3D` object as filled projected triangles in Material mode, using a preview-safe capped version of the authored object color and drawing triangle separation lines
  - samples procedural material texture overlays directly in the flat Material preview, so texture type, strength, scale, U offset, and V offset are visible before path-traced rendering
  - previews v2 object stacks, including base patterns plus ordered rust/fog/grime/oil overlays, even when the legacy single `textureId` field is `None`
  - lets the base layer be toggled off in the layer list; the preview then draws a dim neutral substrate so overlay-only behavior can be inspected without changing the authored object color
  - keeps editor rendering bounded: no-texture focused objects use a fast solid fill path, while active rust/fog previews use capped block sampling instead of unbounded per-pixel draw-point emission
  - exposes a `Solid Faces` preview toggle that defaults on; the solid path uses opaque fill and editor-local depth buffering so rear triangles do not show through glass/fog/rust material previews
  - in Solid Faces mode, depth-checks triangle edge pixels so visible face diagonals render for per-triangle material inspection while rear triangle edges remain hidden
  - uses the native `3D` builder mesh as preview geometry truth, so planes resolve to `2` triangles and rectangular prisms resolve to `12`
  - can pick the nearest visible focused-object preview triangle from a Material canvas click and stores a volatile selected-triangle set for the current editor session
  - selects whole face groups by default in Material mode: plain click selects the face group under the cursor, and Shift-click toggles that face group
  - shows selected triangle count, selected face count, and selected face group ids in the Material left pane
  - renders selected faces with a depth-aware perimeter outline in the focused preview so the selected face texture remains visible
  - uses a compact Material pane summary and tighter texture controls so the `Face Groups` list owns more of the left pane
  - renders a compact bounded `Face Groups` list below the object-wide texture controls; the list inventories all focused-object face groups, shows selected triangles over total triangles per face, and clicking any row selects that whole face group as active
  - draws a scrollbar and routes pane wheel events to the group list when focused objects have more face groups than visible rows
  - keeps the group-list UI above the existing global editor buttons so Material-specific controls do not collide with shared `Add`, `Delete`, and `Select` actions
  - treats generated preview face groups as default texture islands: planes start as one two-triangle island and rectangular prisms start as six two-triangle islands
  - samples rust/fog preview textures from a cohesive face-space UV island across both triangles of a generated face, using a stable face-group seed so the texture does not restart at the diagonal split
  - lets active face groups override texture kind independently, so a generated face can be `None`, `Rust`, or `Fog` without changing the object-wide default or neighboring generated faces
  - routes strength, scale, Offset U, and Offset V sliders through the active face group when one is active, while preserving object-wide fallback controls when no group is active
  - routes the texture kind buttons through the active face group when one is active, while preserving object-wide texture kind controls when no group is active
  - object-default mode exposes the v2 layer stack immediately when no face is selected; layer rows support add, mute/enable, move up/down, delete, row selection, and row-level `On`/`Off` visibility toggles
  - now includes an object-wide `Authored Texture` section above the procedural layer stack; the section shows compact bound/unbound status, manifest basename plus face count when bound, and explicit `Pick/Replace Manifest` plus `Clear Binding` actions for the selected object
  - authored-texture bind state now survives the full selected-object workflow: bind in the Material editor, persist the runtime scene, reopen it later, and recover the same manifest path / binding mode / face-count summary for the selected object
  - the same selected-object workflow now also supports replacing one authored-texture manifest with another for the same object, persisting that replacement, clearing the binding later, and reopening back into an unbound state
  - active v2 base layers expose `Solid`, `Metal`, `Wood`, and `Brick` buttons in the current editor UI, while active v2 overlay layers expose `Rust`, `Fog`, `Grime`, and `Oil`
  - the active authored-texture follow-on lane in `drawing_program` now explicitly aligns its layer-role recipe to this runtime taxonomy instead of inventing a second label family:
    - role recipe:
      - `Base`
      - `Material Detail`
      - `Decal`
      - `Grime`
      - `Damage`
    - runtime intent families remain:
      - base/substrate:
        - `solid`
        - `metal`
        - `wood`
        - `brick`
        - `concrete`
        - `stone`
      - overlay/environment:
        - `rust`
        - `fog`
        - `grime`
        - `oil`
    - role and runtime intent are now deliberately separate, so later dual-lane authored layers can preserve cases like oily contamination without collapsing that intent into color-only paint
  - exposes compact texture parameter controls for active procedural overlays: pattern mode (`Default`, `Speck`, `Patch`, `Flow`), coverage, grain, edge softness, contrast, flow, color depth, and surface damage
  - placement controls remain sliders; secondary procedural parameters render as bounded knob cells with vertical drag and numeric readback
  - routes texture parameter controls through the active face group when one is active, while preserving object-wide parameter controls when no group is active
  - samples face-local texture overrides in the Material preview even when the object-wide texture kind is `None`
  - shows whether the active face is using object defaults or a volatile face override, and exposes `Reset Face` to clear the active override
  - exposes `Copy to Selected` when multiple face groups are selected; it copies the active face group's effective texture kind, placement, and parameter block to every other selected generated face group
  - persists object-wide procedural texture defaults and parameters plus generated face-group texture kind, placement, and parameters through ray-tracing runtime-scene `object_materials[].procedural_texture`
  - persists generated face-group texture kind, placement, and parameters for config-backed scenes through per-object `materialFacePlacements`
  - leaves group naming, custom group topology, and durable group controls for the next material-editor slices
- Native `3D` config persistence is now split cleanly from legacy `2D` state:
  - `integratorMode3D`
  - `temporalFrames3D`
  - `bounceDepth3D`
  - `rouletteThreshold3D`
  - environment light mode
  - ambient brightness / ambient-strength compatibility state
  - top-fill strength
  - Disney denoise toggle
  - runtime-scene and optional atmosphere-source paths
  - `meshAssetRoot` as a separate external runtime mesh sidecar root, distinct
    from scene input-root discovery
- Deep-render export now supports:
  - absolute start-frame selection
  - resume from highest existing saved frame
  - shared absolute-frame truth across output numbering and path sampling
  - a persisted, default-off `Async Render` mode for native tiled `3D` Deep
    Render. One immutable prepared-frame request is owned by a background job;
    matching-generation tile progress is retained and presented by the desktop
    thread, output advances only after verified frame commit, and close/Escape
    drains cancellation before request teardown
  - conservative synchronous fallback when native tiled routing is unavailable
    or dynamic volume/water frame dependencies are selected
  - authored Bezier light/camera path sampling in native `3D` deep render even
    when an older saved config still carries the legacy `interactiveMode=true`
    bit; deep-render config load treats deep render as the authoritative
    authored-motion mode instead of freezing the light at the interactive seed
- Main-menu `Scene + Mode` is mode-aware:
  - legacy `Interactive Mode` remains available in `2D` mode for the quick
    mouse-driven light sandbox
  - native/compact `3D` mode hides the oversized interactive primary control,
    compresses `3D Render`, `Async Render`, `Bounce`, `Auto MP4`, and integrator controls, and
    gives the always-visible runtime scene list more vertical space
- Export/video workflow state:
  - `frameDir` remains frame export root
  - `videoOutputRoot` remains persisted runtime config state
  - menu exposes grouped Data I/O + batch actions
  - Data I/O + Batch now exposes a compact worker-export entry point:
    `Worker Package: Scene-only` plus `Export Queue`
  - Data I/O + Batch also includes a read-only scene-project summary row when
    the selected scene source is a runtime scene under a `scene_project.json`
    project root; it reports scene/runtime, authoring, mesh sidecar,
    PhysicsSim cache, PhysicsSim bundle, and RayTracing render-request
    presence without changing paths or export modes
  - the menu worker export action is intentionally narrow: it requires the
    selected scene source to be a runtime scene; project-backed scenes resolve
    the manifest's active request pointer (defaulting to
    `ray_tracing/render_request.json`) and create it on first export with
    portable simulation-frame start/count/stride state, while loose scenes and
    `RAY_TRACING_WORKER_RENDER_REQUEST` keep compatibility behavior
  - project render-request discovery and JSON-preserving writeback live in
    `src/app/scene_project_render_request.c`; unsafe project-relative traversal
    is rejected and explicit external requests are never written
  - the same writer now has a focused headless entrypoint,
    `scene_project_render_request_tool`, for deterministic project-owned
    start/count/stride persistence without opening the desktop menu
  - `tools/validate_scene_project.py` runs the shared project envelope plus
    LineDrawing, PhysicsSim, and RayTracing adapter checks; its content identity
    is relocation-stable and is embedded in project-backed worker manifests
  - the existing menu action still writes the original scene-only queue item;
    the headless export CLI also provides an explicit
    `scene-plus-physics-cache` mode for project-backed scenes
  - that project mode snapshots canonical project/authoring/runtime/object
    files, referenced mesh sidecars, the project render request, and only the
    VF3D/pack/water frames selected by `simulation_frames.start/count/stride`
  - portable project manifests keep the project-root directory contract; the
    live worker projection uses the same canonical `assets/vf3d/...` and
    `assets/physics/...` paths, while hashes and lineage preserve the source
    active-run identity and selection window
  - the renderer projection stays within the live preview profile's 16-file
    limit by excluding portable-only project metadata from inline payload files
    without removing it from the portable snapshot
  - the original S5 item rendered successfully but exposed a completed-result
    schema mismatch; S5-B corrected the bounded inline result contract
  - fresh job `r2bscene05` then reached terminal completion, imported four
    RayTracing outputs on the VPS, and reached Visualizer `batch_ready`
  - H1 now proves a production-generated LineDrawing mesh project can receive a
    three-frame PhysicsSim cache and RayTracing request, relocate unchanged,
    retain the same 16-file content hash, and pass local package preparation
  - `tools/scene_project_worker_receipt.py` now imports terminal worker evidence
    into `ray_tracing/runs/<render-run-id>/` as retained render intent, optional
    render summary, and a hash-locked `ray_tracing_worker_receipt_v1` lineage
    record; offline validation rejects changed files and path escape attempts
  - receipt import is RayTracing-owned and does not rewrite LineDrawing
    `scene_authoring.json` or PhysicsSim active-cache state; frame/video fetch
    remains a separate explicit operation
  - main-menu frame-resume state now lives in the switchable compact
    `Frame Resume` center module; it owns resume-existing, start-frame editing,
    and next-existing readback, while compact `Preview` and `Start` actions stay
    in the always-visible right-side `Runtime Route` stack
  - the global persistence footer now starts to the right of the full-height
    `Scene + Mode` pane, so `Exit w/o Saving`, `Restore Defaults`, and `Save`
    no longer compete with left-pane scene/volume controls
- Main-menu Renderer Controls now use a tabbed center panel:
  - `Lighting` owns environment mode, environment preset, background
    brightness auto/manual, ambient brightness, top-fill strength, direct-light
    intensity, and falloff controls
  - `Performance` owns tile renderer, tile preview, Disney denoise, upscale
    mode, and tile size
  - the right `Render Settings` slider panel now stays focused on global
    frame/render dimensions and sample/depth controls
- Scene-editor digest truth now preserves pickable guide-only helper overlays without promoting them into native `3D` render geometry.
- Headless agent rendering now has a Phase 4 request-driven CLI:
  - request schema: `ray_tracing_agent_render_request_v1`
  - command target: `make -C ray_tracing ray-tracing-render-headless`
  - preflight command: `ray_tracing_render_headless --request <request.json> --preflight`
  - frame export command: `ray_tracing_render_headless --request <request.json> --render`
  - summary schema: `ray_tracing_headless_summary_v1`
  - current scope: runtime-scene apply, optional VF3D source validation, optional PhysicsSim water-surface sidecar validation, native `3D` route readiness, prepared-frame validation, and BMP frame export under `<output.root>/frames/`
  - PhysicsSim `scene_bundle.json` handoff is covered by `test-ray-tracing-render-headless-volume-handoff`, which now uses a room-style LineDrawing emitter fixture (floor, wall planes, contrast prism, emitter prism), then runs PhysicsSim headless VF3D export and RayTracing BMP export into `ray_tracing/build/agent_runs/physics_trio/volume_handoff_image_export/`
  - PhysicsSim Water Basin `scene_bundle.json.water_source` handoff is covered by `test-ray-tracing-render-headless-water-surface-handoff`, which runs `physics_sim_headless --water-mode --save-volume-frames`, imports `water_manifest_v1.json`, appends the selected PhysicsSim Y-up heightfield as native `3D` water-surface triangles, remaps height `y` into RayTracing scene-up `z` for physically horizontal rendered water, and renders BMP frames into `ray_tracing/build/agent_runs/physics_trio/water_surface_handoff_image_export/`
  - volume handoff summaries now report VF3D channel/grid/density debug fields, including `volume_summary.density_non_zero_cell_count`
  - water-surface summaries now report `water_surface_source_found`, `water_surface_loaded`, `water_surface_mesh_attached`, selected first/last frame paths, requested/loaded frame indices, grid dimensions, wet/dry/solid column counts, surface min/max/average/slope, material IOR/absorption metadata, resolved RayTracing water payload fields, and appended triangle count
  - headless agent render requests support `volume.visible=false` as a surface-only water review mode: the configured volume source remains available for `scene_bundle.json.water_source` sidecar import, while the visible VF3D volume body is not attached; summaries then report `volume_visible=false`, `volume_attached=false`, `volume_summary_built=false`, and `water_surface_mesh_attached=true` when the native water mesh loads correctly
  - RayTracing resolves PhysicsSim `water_manifest_v1.material.absorption_rgb` as absorption coefficients, derives a Beer-Lambert `water_surface.payload.tint_rgb` transmittance color over `absorption_distance_m`, and applies a water material payload with IOR, transparency, solid-dielectric state, and the derived tint to the native `3D` material path
  - `test-ray-tracing-render-headless-water-basin-surface-review` runs the square-footprint PhysicsSim Water Basin, imports the final `water_surface` sidecar through `scene_bundle.json.water_source`, remaps it into the native Z-up render frame, and renders a broader single-frame transparent-water review BMP into `ray_tracing/build/agent_runs/physics_trio/water_basin_surface_review_single_frame/`
  - `test-ray-tracing-render-headless-water-moving-light-review` runs the WTR-5.4 headless sequence proof: PhysicsSim exports a warmed/rippled Water Basin, RayTracing renders four consecutive transparent-water frames (`0008..0011`) with an authored moving light path, and the test verifies both water height evolution and measurable frame-to-frame image deltas under `ray_tracing/build/agent_runs/physics_trio/water_moving_light_review/`
  - `test-ray-tracing-render-headless-water-long-motion-review` runs the WTR-5.5 long-motion sparse-frame proof: PhysicsSim exports `201` Water Basin frames with `4` sim steps per frame, samples frames `40, 80, 120, 160, 200`, and renders full RayTracing basin BMP frames plus a contact sheet from `scene_bundle.json.water_source` under `ray_tracing/build/agent_runs/physics_trio/water_long_motion_review/`; this is the readable long-time-separation proof before WTR-6, while full-length video/output throughput remains later work
  - `test-ray-tracing-render-headless-water-object-coupling-review` runs the WTR-6 object-water proof: PhysicsSim exports a `water_pool_submerged_solid` Water Basin, RayTracing renders full basin frames `0008`, `0018`, and `0027` from `scene_bundle.json.water_source` with a visible block object, verifies object audit hits, water mesh attachment, secondary water hits, and frame deltas, and writes BMP frames plus `water_object_coupling_review.mp4` under `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_review/`
  - `test-ray-tracing-render-headless-water-object-coupling-long-review` adds the WTR-6.4 long-review path. Its default `smoke` profile proves warm-up plus stride-sampled object-water rendering; `WTR6_LONG_PROFILE=full` is the intended expensive review profile with `200` warm-up PhysicsSim frames, `100` rendered RayTracing frames, stride `5`, selected water frames `200..695`, and a looped overhead light path. The full profile is operator-invoked and is not part of routine smoke.
  - WTR-6.5 direct-light smoothed-wake preview passed with the current
    toolchain-built headless renderer under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_wtr65_direct_light_preview/`;
    the run rendered selected frames `12, 16, 20, 24, 28` at `320x240`,
    temporal-1/direct-light, verified stable block primary hits,
    transparent-water secondary hits, nonzero image deltas, `776`
    displacement samples, and zero capped displacement samples, and wrote
    `ray_tracing/frames/water_object_coupling_long_review.mp4`.
  - WTR-6.5 short Disney-v2 temporal-2 comparison also passed with a matched
    direct-light baseline under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_wtr65_direct_light_short_compare/`
    and
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_wtr65_disney_v2_t2_short_compare/`;
    both used selected frames `12, 18, 24` at `240x180` with `0.035 m`
    review ripples. Disney-v2 summaries reported `integrator_3d: disney_v2`
    and `render.temporal_frames: 2`; the comparison summary is
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_wtr65_disney_v2_t2_short_comparison_summary.json`.
  - WTR-6.6 now has a dry-run cache-first preview-matrix planner:
    `ray_tracing/tools/wtr66_preview_matrix_planner.py`. The planner writes a
    planned PhysicsSim cache manifest, selected-frame lists, render request
    JSONs, variant summaries, and a matrix summary under
    `ray_tracing/build/agent_runs/physics_trio/wtr66_preview_matrix_dry_run/`
    without launching PhysicsSim or RayTracing. The gate
    `test-ray-tracing-wtr66-preview-matrix-planner-dry-run` proves the default
    direct-light and Disney-v2 temporal-2 variants reuse the same planned
    `scene_bundle.json` cache path.
  - WTR-6.6B locally validates that cache-first plan through the detached job
    runner: `test-ray-tracing-wtr66-preview-matrix-local-job-runner` executes
    one `120`-frame PhysicsSim cache, submits direct-light and Disney-v2
    temporal-2 variants over frames `12, 18, 24`, and writes
    `ray_tracing/build/agent_runs/physics_trio/wtr66_preview_matrix_local_job_runner/local_job_runner_execution_summary.json`.
    The readback checks completed jobs, rendered frames, requested integrator
    and temporal-frame values, water mesh attachment, loaded water frame
    indices, nonzero block primary hits, and nonzero secondary hits. The gate
    prefers the current clang job-runner binary before legacy `build/<arch>`
    paths to avoid stale-binary ambiguity.
  - WTR-6.7C worker-backed external-cache consumption now has a repaired
    Linux PC RayTracing package boundary: `ray_tracing_headless_worker@0.4.13`
    is the current selected-frame worker package, nonzero render starts retain
    output names such as `frame_0120.bmp`, and VF3D plus
    `water_manifest_v1.json` imports resolve exact manifest `frame_index`
    entries before falling back to legacy array-position clamping. The proof
    `ray-tracing--wtr67c-cache-direct-probe--20260624T173200Z--wtr67crt05`
    loaded VF3D `frame_000120.vf3d` and water sidecar
    `water_surface_000120.json` for RayTracing frame `120`. This is a cache
    addressing/package-readiness proof only; visible moving-water quality
    still depends on PhysicsSim cache excitation and must be checked through
    water-sidecar temporal height diagnostics before longer RayTracing runs.
  - local WTR-6.4 artifact review also proved a corrected `100`-frame
    Disney-v2 temporal-2 slow-light render from the existing high-quality
    PhysicsSim VF3D cache under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_hq_local_64/ray_tracing_disney_v2_local_t2_100f_slowlight_corrected/`;
    all `100` summaries reported `integrator_3d: disney_v2`, native `3D`,
    volume attached, and water surface mesh attached. The sibling
    `ray_tracing_disney_v2_local_t2_100f_slowlight/` root is preserved as an
    actual direct-light artifact because it used a stale May binary.
  - generic runtime-scene light/camera seeds from `transform.position` are promoted into the native `3D` route, matching LineDrawing-generated scene shape
  - runtime-scene camera seeds now also accept authored orientation fields (`yaw` / `rotation_z` / `transform.rotation.z`, plus optional pitch fields), and when no camera orientation is authored the native `3D` render auto-aims the seeded camera toward the built scene center so headless runtime-scene exports do not fall into all-black horizontal views
  - authored moving-camera scenes now also accept `extensions.ray_tracing.authoring.camera_focus_target = { x, y, z }`; when present, headless runtime-scene sampling preserves authored camera-path translation/depth motion but recomputes yaw/pitch toward that focus target each sample, which is safer than hand-authoring moving camera orientation curves
  - render summaries now also report render-visibility truth through `render_stats.hit_pixels`, `render_stats.visible_pixels`, `render_stats.nonzero_pixels`, `render_stats.max_radiance`, and `render_stats.max_rgb`
  - render summaries now also expose `object_audit`, which reports per-object runtime-slot presence, built primitive/triangle counts, and primary camera-ray hit pixels for headless diagnosis
  - headless requests now accept additive inspection-only render tuning fields: `preset`, `camera_zoom`, `camera_position`, `camera_look_at`, `environment_light_mode`, `ambient_strength`, `top_fill_strength`, `environment_brightness`, `light_intensity`, `light_radius`, `forward_decay`, `volume_density_scale`, `volume_density_gamma`, `volume_scatter_gain`, `caustic_volume_scatter_gain`, `volume_caustic_scatter_gain`, `volume_absorption_gain`, `volume_opacity_clamp`, `volume_step_scale`, `secondary_diffuse_samples_3d`, `transmission_samples_3d`, `volume_tint`, and `volume_albedo`
  - headless `volume.debug_overlay=true` now renders direct scene-camera density contribution instead of only carrying a summary flag; it uses the same volume source, camera, surface clipping, density remap, step scale, and albedo/tint path, but bypasses light-dependent single scattering so sparse smoke structure can be diagnosed in-scene
  - preferred environment-light headless contract is:
    - `environment_light_mode = off|top_fill|ambient`
    - `ambient_strength` for the ambient surface-fill amount (`0.0..1.0`)
    - `top_fill_strength` for the top-fill lane (`0.0..20.0`)
  - runtime mesh asset instances are now part of the native `3D` headless path:
    - optional runtime per-vertex normals are transported into native triangles;
      hits retain geometric `Ng` for offsets/sidedness and barycentrically
      interpolated shading `Ns` for mirror, metal, and BSDF evaluation
    - position-only assets remain compatible through `Ns = Ng`, non-uniform
      instance scale uses the inverse transpose, and
      `RAY_TRACING_MESH_SHADING_MODE=flat` restores face-normal shading for
      comparison
    - runtime scenes may reference `mesh_asset_instance` objects through
      `geometry_ref.kind = "mesh_asset"`
    - file-backed assets are resolved beside the runtime scene from
      `assets/mesh_assets/<asset_id>.runtime.json` or
      `mesh_assets/<asset_id>.runtime.json`
    - desktop/runtime config also persists a separate `meshAssetRoot` value for
      moved or external sidecar libraries; the loader checks that root before
      falling back to input-root based discovery, and the menu exposes it as
      `Mesh Root` independently from the scene `Input Root`
    - the loader retains shared `mesh_asset_runtime_v1` documents, object ids,
      transforms, and scene object indices for material lookup
    - the native `3D` builder appends runtime mesh triangles into the same
      triangle scene used by primitive geometry
    - RayTracing accepts both runtime material shapes:
      `materials[].material_id` plus `albedo`, and LineDrawing/generated
      `materials[].id` plus `base_color`
    - imported STL geometry should enter RayTracing through the authored
      imported-mesh -> `mesh_asset_runtime_v1` sidecar flow, then appear in
      scenes as a normal `mesh_asset_instance`
    - object ids and scene object indices are preserved across this conversion
      so authored material settings can resolve onto the generated runtime mesh
      triangles at hit time
    - `extensions.ray_tracing.authoring.object_materials[*]` is the current
      object-wide authoring lane for imported mesh color/material overrides;
      the concrete `SceneObject` color, reflectivity, and roughness values are
      authoritative after preset assignment
    - default native `3D` first-hit tracing uses the prepared `tlas_blas`
      route for suitable static runtime mesh assets, while explicit
      `flattened_bvh` remains available as a compatibility/debug route
    - `make -C ray_tracing
      test-ray-tracing-render-headless-tlas-blas-repeated-instance-stress`
      verifies repeated static mesh instances through the default `tlas_blas`
      route and writes routine reports under
      `ray_tracing/build/agent_runs/ray_tracing/tlas_blas_repeated_instance_stress/`
    - dynamic water surfaces, deforming meshes, generated runtime geometry, and
      mesh-emissive internal sampling are not treated as static BLAS cache
      entries; they require a separate acceleration policy for frame/topology
      invalidation, refit, rebuild, and flattened fallback behavior
  - runtime mesh acceleration is active for this path:
    - native `3D` first-hit tracing defaults to prepared TLAS/BLAS for suitable
      static runtime mesh assets
    - flattened median-split scene BVH remains the explicit compatibility route
      and fallback for unsupported, unready, no-BVH, or stack-overflow cases
    - pure `tlas_blas` bridge-scene construction skips the flattened scene BVH
      build; explicit `flattened_bvh`, parity/debug routes, and fallback-capable
      routes still build the flattened compatibility BVH. For rollback/parity
      checks, set `RAY_TRACING_NATIVE3D_DISABLE_DEFAULT_TLAS_BVH_SKIP=1`.
    - retained runtime mesh asset sets record scene and sidecar file stamps, so
      runtime-scene preflight can reuse the just-loaded mesh documents when all
      stamps still match and can fall back to normal validation when any file
      changes
    - prepared TLAS state records a bounded geometry signature in addition to
      primitive and triangle counts; copied prepared scenes can rebind and
      trace through TLAS when their geometry is identical, while sampled
      same-count geometry drift is rejected with an explicit bind diagnostic
      instead of silently tracing with stale TLAS identity
    - runtime mesh asset loading now has a RayTracing-local persistent document
      pack cache for cold-process startup: cache files are derived from
      `mesh_asset_runtime_v1` sidecars, keyed by source path/size/mtime plus
      schema/host metadata, loaded into the same `CoreMeshAssetRuntimeDocument`
      contract as JSON, and ignored/replaced on stale, corrupt, partial, or
      incompatible reads. Operators can set
      `RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE` to `read_write`,
      `read_only`, `refresh`, or `off`, and can override the temp-root based
      cache location with `RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT`.
    - BLAS/TLAS route counters and flattened BVH build/traversal metrics are
      emitted in render summaries
    - static file-backed runtime mesh BLAS preparation now has a
      RayTracing-local persistent acceleration cache for cold-process startup:
      cache files store deterministic local runtime triangles plus mesh-local
      BVH nodes/indices, are keyed by source sidecar stamp/checksum plus
      renderer acceleration schema/build/layout/policy metadata, and are loaded
      only after the same-process BLAS cache misses. TLAS instance arrays,
      transforms, prepared-scene bind state, dynamic/deforming/generated
      geometry, mesh-emissive sampler state, and release-packaged prebuilt
      caches remain excluded.
    - persistent acceleration cache reads/writes reuse
      `RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE` (`read_write`,
      `read_only`, `refresh`, `off`) and use
      `RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT` as the preferred cache root,
      with `RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT` retained as a
      compatibility fallback.
    - headless summaries include `startup_load_timing_matrix`, which breaks
      native prepare/startup cost into named stages such as sidecar path
      resolution, runtime mesh document load/parse, runtime mesh document pack
      read/write, mesh document copy, mesh instance expansion, flattened
      triangle append/BVH build, mesh-local BLAS build, mesh-local persistent
      BLAS/BVH cache read/write, TLAS build/bind, dynamic water cache store/BVH
      build, and unaccounted prepare/runtime init
    - headless mesh-asset timing summaries report the selected persistent
      document-cache mode and refresh count so cache-controlled runs can be
      compared without guessing process environment
    - current render-cost defaults and diagnostics are documented in
      `docs/headless_agent_render_cli.md`: Disney v2 reflected-transmission
      first-subpass no-hit reuse is default-on with rollback
      `RAY_TRACING_DISNEY_V2_REFLECTED_FIRST_SUBPASS_NO_HIT_REUSE_PROBE=0`,
      while `RAY_TRACING_NATIVE_3D_TEMPORAL_RISK_EARLY_STOP=1`,
      `RAY_TRACING_NATIVE_3D_TEMPORAL_BUDGET_HEATMAP=1`,
      `RAY_TRACING_DIRECT_LIGHT_CLEAR_VISIBLE_DECISION_SAMPLE_PROBE=1`,
      `RAY_TRACING_RENDER_TRACE_COST_LEDGER=1`, and
      `RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER=1` remain opt-in headless proof
      or diagnostic envs. The R4 textured-glass operator proof did not promote
      temporal risk early-stop to a default.
    - prepared acceleration summaries report
      `blas_persistent_cache_hits`, misses, writes, invalidations, refreshes,
      read time, and write time separately from the existing in-process BLAS
      cache counters.
    - headless summaries also expose `dynamic_geometry_acceleration`, which
      classifies geometry into the current acceleration policy: static runtime
      mesh assets use `static_blas_tlas`; PhysicsSim water surfaces are
      classified by frame-stamp changes and first/last topology stability into
      static flattened, `dynamic_flattened_refit_candidate_per_frame`,
      `dynamic_flattened_rebuild_per_frame`, or flattened fallback policy;
      mesh-emissive internals use `sampler_cache_not_blas_tlas` when present;
      generated runtime meshes are static only after resolving as file-backed
      runtime sidecars; deforming meshes remain pending a dynamic acceleration
      contract
    - headless summaries also expose `dynamic_water_acceleration_cache`, which
      records the current water lifecycle owner diagnostics: observed frames,
      rebuild/reuse/refit/fallback counts, last status, and cached topology
      stamps. The owner also stores cached water triangle geometry plus a
      water-local BVH and reports geometry-cache readiness, store counts, store
      failures, and BVH node/leaf counts. The central scene acceleration
      callback now tests that cached water-local BVH for first-hit tracing and
      reports water route calls, hits, misses, errors, and unavailable states.
      Static TLAS fallback cleanup for copied dynamic scenes remains a separate
      follow-up from water-cache functionality.
    - prepared static runtime mesh geometry/BVH can be reused across
      multi-frame renders when the mesh scene is unchanged
  - deterministic runtime mesh proof assets currently include sphere fixtures
    from `asset_sphere_8x4` through `asset_sphere_128x64`; the `64x32`
    sphere is the current high-fidelity visual proof tier used for glossy
    moving-light review
  - LineDrawing runtime mesh scene-export parity is covered by
    `test-ray-tracing-render-headless-line-drawing-mesh-asset`:
    - fixture:
      `tests/fixtures/line_drawing_mesh_asset_high_quality/scene_runtime.json`
    - the mesh object uses LineDrawing's `mesh_asset_instance` object shape and
      a relative `extensions.line_drawing.runtime_mesh_path`
    - the headless proof renders `asset_sphere_128x64` as `16128` mesh
      triangles, with `16130` total scene triangles in the BVH after the floor
      primitive is included
  - The RayTracing scene editor retained 3D viewport now consumes the retained
    runtime mesh asset set for editor preview:
    - `PreviewRetainedSceneBuildLineSegments(...)` samples runtime mesh
      triangle edges after applying the same instance scale, rotation, and
      translation used by the native `3D` builder
    - `SceneEditorDigestOverlayResolveExtents(...)` includes loaded mesh
      vertices, so editor fit/zoom frames complex mesh assets instead of only
      primitive floors, prisms, bounds, or construction planes
    - current editor preview is a sampled connection-edge view, not the final
      LineDrawing feature-edge extractor; dense mesh readability is improved by
      a second camera-aware silhouette overlay that classifies loaded mesh edges
      by adjacent front/back-facing triangles for the active editor camera
    - the camera-aware silhouette overlay is intentionally limited to
      moderate mesh sizes; very large runtime meshes such as the external
      skull proof (`171246` triangles in the current BodyParts3D sidecar) keep
      the bounded sampled-edge preview but skip per-frame silhouette edge
      sorting/allocation so the editor can load and navigate the scene
    - menu scene-source discovery and row selection no longer preflight/apply
      runtime scenes on the click path; `scene_runtime.json` discovery is
      filename/path based and validation remains at apply/render boundaries
    - scene-editor session startup applies runtime-scene authoring state
      through `runtime_scene_bridge_apply_file_defer_mesh_assets(...)`; this
      editor path loads mesh sidecars up to the preview byte budget so low-poly
      runtime assets such as platforms appear in the retained preview, while
      oversized sidecars are skipped before JSON parse to preserve editor
      responsiveness
    - runtime-scene editor Apply/Save writes the RayTracing authoring overlay
      and then rehydrates the editor through the same preview-limited deferred
      mesh path, so saving a skull-scale scene does not synchronously full-load
      or parse over-budget mesh sidecars in the editor process
    - the object-mode left pane now reports the current scene-object list plus
      the retained loaded mesh-preview instance count; object rows distinguish
      loaded mesh sidecars, over-budget skipped mesh sidecars, and primitive
      scene objects such as planes, and rows can be clicked to select the
      matching scene object
    - for mixed skull/platform scenes, the expected editor reading is:
      one over-budget skull mesh row marked skipped, one primitive plane row,
      and one loaded platform mesh row counted in `Mesh Loaded`
    - loaded mesh preview edges now use a dedicated high-contrast editor
      diagnostic color and append a mesh-local bounds outline, so small
      default-material meshes such as the platform do not visually blend into
      the white plane primitive or the blue scene extents box
    - in 3D runtime-scene authoring, the object pane hides legacy
      Circle/Square/Polygon preset controls and keeps the space focused on
      scene inspection plus material/asset panels
    - the normal render/headless path still uses
      `runtime_scene_bridge_apply_file(...)` and loads full mesh assets
    - runtime mesh preview logic is now isolated in
      `src/app/preview_retained_scene_mesh.c`; the retained renderer stays
      focused on primitive retained lines, projection/drawing, orchestration,
      and light markers
    - the next parity boundary is converging the silhouette overlay with
      LineDrawing's authored feature-edge selection
  - imported mesh visual proof now flows from LineDrawing's imported STL
    harness into RayTracing through the same runtime mesh loader and native
    `3D` builder; the richer deterministic stepped-column fixture is the
    current non-tetrahedron baseline
  - external STL visual proof now includes a downloaded skull converted into a
    file-backed runtime mesh sidecar and rendered through the native `3D` path:
    - upright/glossy and corrected visible-blue skull proof runs have been
      published to the CodeWork visualizer
    - the corrected blue proof confirmed authored object material values with
      `packed_color = 3041023`, `reflectivity = 0.25`, and `roughness = 0.34`
      on a `31016` triangle imported skull mesh
    - the earlier blue spiral proof is retained as provenance but superseded as
      a material-color proof because it still read visually white
  - packed runtime mesh payload support exists as a measured RayTracing-local
    codec path, but JSON-backed `mesh_asset_runtime_v1` sidecars remain the
    production scene contract
  - worker-backed mesh renders can request native MP4 output through
    `output.video.enabled/path/fps`; Linux MP4 encoding falls back across
    available ffmpeg encoders instead of depending on `libx264`
  - live visualizer publication has proven both:
    - low-poly moving-light mesh-sphere runs through the normal worker publish
      flow
    - recovered high-fidelity `asset_sphere_64x32` glossy output manually
      published after a Linux PC upload-timeout failure
    - `environment_brightness` remains the lower-level compatibility override
      for the legacy `0..255` brightness state
  - runtime-scene authoring environment persistence now carries:
    - `light_mode`
    - `ambient_strength`
    - legacy-compatible `ambient_brightness`
    - `top_fill_strength`
  - the shipped `glass_preview` inspection preset defaults to `emission_transparency` plus a bounded low-cost preview budget (`secondary_diffuse_samples_3d = 8`, `transmission_samples_3d = 4`) unless explicit request fields replace those values
  - the shipped `glass_review` inspection preset defaults to `emission_transparency` plus a slower review budget (`secondary_diffuse_samples_3d = 24`, `transmission_samples_3d = 12`) unless explicit request fields replace those values
  - runtime-scene apply now skips authoring helper objects (`point_set`, `curve_path`, `edge_set`) when populating live render object slots, so helper records do not crowd out later renderable primitives such as thin transparent review slabs
  - the current volume-handoff smoke now uses an explicit oblique camera rig (`camera_zoom=0.95`, `camera_position=(-3.8,-7.2,2.2)`, `camera_look_at=(-0.2,0.8,1.2)`) plus moderated lighting/scatter (`light_intensity=2.6`, `light_radius=0.10`, `forward_decay=220.0`, `volume_density_scale=0.35`, `volume_density_gamma=0.65`, `volume_scatter_gain=3.0`, `volume_absorption_gain=0.15`, `volume_opacity_clamp=0.70`, `volume_step_scale=1.0`) and a near-white inspection albedo (`volume_albedo=(0.92,0.91,0.88)`) so the room, emitter prism, and plume region read as a side-view `3D` scene instead of an overhead floor patch
  - volume material remap overrides are read back in headless summaries and reinterpret the same VF3D scalar cache by separating density curve, scatter, absorption, albedo, and opacity clamp; defaults preserve the previous direct-density render behavior
  - volume summaries now also read back `debug_overlay_enabled`, and frame-selection inspection stamps request-side `affects_lighting` / `debug_overlay` flags onto the inspected attachment before summary generation so preflight and render summaries match request intent
  - runtime VF3D density reconstruction is now trilinear instead of nearest-cell lookup, reducing block-edge artifacting in headless and runtime volume shading
  - native `3D` direct-light renders now honor `temporalFrames3D` / `render.temporal_frames`; the old forced-single-subpass path is gone
  - headless render requests now support `render.denoise_enabled` as a
    detached-run override, and summaries serialize `denoise.*` plus
    `render_stats.denoise_*` counters for Disney-v2 visual-matrix comparisons
  - detached local supervision now has a first RayTracing adapter through `make -C ray_tracing ray-tracing-job-runner`, with `submit`, `status`, and `cancel` commands that stage canonicalized absolute-path request files into `build/agent_runs/jobs/<job_id>/`, then supervise `ray_tracing_render_headless` through `job_status.json`, `render_progress.json`, `stdout.log`, `stderr.log`, `pid.txt`, and `result_summary.json`; detached path policy requires absolute existing `--jobs-root` overrides, safe one-segment job ids, and absolute non-root direct request `output.root` paths while bundle artifacts remain job-local under `<job_root>/output/artifacts`
  - the first trio detached chain adapter now routes into that runner through
    `bin/run_trio_detached_job_chain.sh`, which waits on detached PhysicsSim
    completion and auto-submits the RayTracing child once the exported
    `scene_bundle.json` exists
  - `test-ray-tracing-job-runner-smoke` proves one detached `diffuse_bounce` render can complete from file-backed status alone without holding a live PTY
  - detached Phase 2 truth now adds explicit submit policies (`fail_if_exists`, `overwrite`, `resume`), truthful `job_status.json` timing/frame fields, state/stage reconciliation on `status`, and contiguous-frame resume for already-rendered prefix frames via a staged `sampling` window that preserves normalized-time sampling
  - detached live-job supervision now also refreshes `job_status.json` directly
    on render-stage transitions (`loading_scene`, `preflight_ready`,
    `rendering_frame`, `writing_frame`, `completed`, `failed`) instead of only
    relying on `render_progress.json`; this makes thread-heartbeat follow-ups
    and direct file inspection trustworthy without a live PTY
  - detached live-job supervision now also surfaces temporal-subpass progress
    through `temporal_subpasses_started`, `temporal_subpasses_completed`,
    `temporal_subpasses_total`, and `progress_ratio` in both
    `render_progress.json` and `job_status.json`, so a single expensive frame
    can show real progress while still inside `rendering_frame`
  - detached live-job supervision now also surfaces tile/time progress through
    `completed_tiles_in_subpass`, `total_tiles_in_subpass`,
    `elapsed_seconds`, and `estimated_remaining_seconds`, so remote worker
    readers can report active progress inside one long temporal subpass
  - detached live-job supervision now distinguishes "entered current subpass"
    from "committed current subpass"; a status like `started=6`,
    `completed=5`, `total=6` is now explicitly "final subpass active", not an
    implied failed render
  - detached status reconciliation now also emits a non-destructive `stalled`
    state when a process remains alive but `updated_at_utc` does not move for
    15 minutes; resumed progress can move that state back to `running`
  - `make -C ray_tracing ray-tracing-material-preview-headless` now ships a
    non-raytraced material swatch/contact-sheet lane that evaluates the same
    object-level material + procedural texture stack as runtime shading, but
    writes deterministic BMP previews and optional JSON summaries without
    running native `3D` integrators
  - material preview requests now support a sheet-level `background_color`
    (`0xRRGGBB`) plus per-variant synthetic `preview_overlay` blocks for fast
    grime/oil sweeps without rewriting the authored scene stack
  - `ray_tracing/tools/publish_material_preview_set.sh` now publishes one
    generated request into `ray_tracing/docs/material_preview_sets/<set_id>/`
    with redacted `request.json`, `preview.bmp`, optional `preview.png`,
    redacted `summary.json`, and an `index.md` that records left-to-right /
    top-to-bottom variant order
  - local repo-doc publication and live website publication are now explicitly
    separate lanes:
    - `ray_tracing/tools/publish_render_review_set.sh` and
      `ray_tracing/tools/publish_material_preview_set.sh` publish only into the
      repo docs surface
    - live visualizer publication goes through staged `visualizer-run/v1`
      drops under `_private_workspace_artifacts/codework_visualizer_runs/`
      plus the VPS import flow owned by `skills/codework-visualizer-drop/`
    - `ray_tracing/tools/publish_render_outputs.sh` is now the higher-level
      helper that can target `local`, `visualizer`, or `both`, defaulting to
      the visualizer lane and optionally staging the full frame sequence
    - `ray_tracing/tools/publish_latest_render_run.sh` now sits one layer above
      that wrapper and can infer the latest completed run root, its last frame,
      a default set id, and a humanized title for routine publish-after-render
      work
    - publish helpers now require absolute existing run roots for render-run
      publication and validate `set_id`, `drop_id`, `job_type`, and selected
      frame names as single path segments before constructing repo-doc or
      visualizer staging paths
    - repo-doc publication writes redacted public JSON copies for request and
      summary artifacts, replacing local/private paths before they enter
      `docs/render_review_sets/` or `docs/material_preview_sets/`
  - Linux worker packaging now self-tests archive hygiene: private/generated
    lanes are rejected from the worker tarball, macOS AppleDouble sidecars are
    suppressed, and executable bits are limited to the worker binaries and
    wrapper; release contract output reports only configured/unconfigured
    signing/notary/team state instead of printing credential values
    - the visualizer staging script now rejects malformed drop ids instead of
      letting bad lowercase timestamp separators survive until VPS import time
    - remote staging cleanup now has an explicit helper script for stale invalid
      drops that would otherwise keep polluting importer results
  - missing legacy scene/animation config files now log as informational
    fallback messages during headless request-driven runs instead of looking
    like hard render failures in stdout
  - remaining long-frame supervision limit: tile-level progress is now
    available, but there is still no bounce-level or deeper convergence signal
    inside one tile once a render is inside one long temporal subpass
  - `test-ray-tracing-job-runner-policy` now covers: clean submit, collision rejection without flags, partial contiguous resume after removing the final frame, and explicit overwrite rerender
  - multi-frame image export currently samples normalized time from `0.0` to `1.0` across the requested frame count
  - manifest-backed and scene-bundle-backed volume rendering now resolves VF3D
    or pack sources by requested render frame; headless summaries report
    selected first/last volume frame paths and loaded frame indices
  - scene-bundle-backed water rendering now resolves
    `scene_bundle.json.water_source` by requested render frame, loads the
    matching `physics_sim_water_surface_heightfield_v1` frame, skips dry quads,
    appends a `water_surface` runtime object/mesh, and records selected frame,
    material, and triangle-count diagnostics in the headless summary
  - the WTR-6 object-coupling review keeps the full RayTracing basin path:
    object geometry is authored into the runtime scene while the water surface
    still comes from PhysicsSim `scene_bundle.json.water_source`; the MP4 is a
    review packaging output over rendered BMP frames, not a replacement 2D
    preview path
  - WTR-6.4 long output is profile-driven through
    `tests/integration/run_ray_tracing_render_headless_water_object_coupling_long_review.sh`.
    It renders selected PhysicsSim water frames one request at a time so a
    long run can use warm-up/stride sampling while preserving per-frame
    summaries, request files, and BMP outputs for inspection.
- Menu/control-surface implementation is now split across:
  - `src/ui/menu/sdl_menu_render.c` for orchestration/layout pass ownership
  - `src/ui/menu/sdl_menu_render_controls.c` for shared text/button/control rendering helpers
  - `src/ui/menu/menu_batch_panel.c` for the grouped frame/video/export panel
    and its read-only scene-project summary row
  - `src/ui/menu/menu_resume_panel.c` for the compact frame-resume pane below
    `Data I/O + Batch`
  - `src/ui/menu/menu_scene_project_summary.c` for scene-project file presence
    detection from the selected runtime-scene path
  - `src/ui/menu/menu_worker_export.c` for the menu-triggered scene-only worker
    export command wrapper
  - `tools/scene_project_worker_export.py` for non-UI project snapshot
    selection, copying, path normalization, and lineage

## Recommended Scene-Authoring / Render Loop
- Treat `line_drawing` as the canonical source for room/object/camera/light
  authoring and `ray_tracing` as the downstream preview, render, and publish
  lane.
- The intended operator flow is:
  - export or compile `scene_authoring.json` and `scene_runtime.json` from the
    upstream request
  - use headless preview first for framing/material validation
  - use `ray_tracing_material_preview_headless` when the question is surface
    treatment rather than full lighting transport
  - use `glass_preview` / `glass_review` only when transmission quality is the
    actual thing under review
  - use detached job runner plus publish helpers only after preview/framing are
    stable
  - for worker-backed multi-frame review, use the proven seed ->
    `start_stage = ray_tracing` continuation -> optional publish backfill flow
    described in `docs/headless_continuation_visualizer_workflow.md`
  - prefer the higher-level operator wrapper
    `bin/run_ray_tracing_worker_continuation_flow.py` when the goal is
    frame-by-frame seed, continuation, or bounded visualizer backfill work
    rather than manual payload/thread shaping
  - active worker interruption is now part of the proven operator contract:
    `bin/control_codework_worker_job.py --action cancel` can stop an active
    RayTracing continuation mid-render, leaves the worker terminal state at
    `canceled`, and bypasses `visualizer_publish`
- Current authoring contract reminders:
  - `material_id = 5` is the transparent/glass preset
  - `material_id = 4` remains the explicit emissive preset
  - omitted `emissive_strength` now means `0.0`, not an implicitly lit object
  - layered object treatment should prefer `material_texture_stack` over
    widening more flat one-off object fields
  - imported STL/runtime mesh material work should extend the existing
    `material_texture_stack` contract for brushed metal, grime, roughness,
    oil, rust, fog, and similar layered treatments instead of adding more
    imported-mesh-specific material fields

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Support lanes: `config/`, `assets/`, `data/`, `tmp/`
- Active source subsystems:
  - `app`, `camera`, `config`, `editor`, `engine`, `export`, `geo`, `import`, `material`, `path`, `render`, `scene`, `tools`, `ui`

## Verification Contract
- Build:
  - `make -C ray_tracing clean && make -C ray_tracing`
- Fast C and contract lanes:
  - `make -C ray_tracing test`
  - `make -C ray_tracing test-runtime-scene-bridge-contract`
    - runs the focused core/apply and writeback/additive-merge bridge groups
      without requiring the full generic `test` lane
  - `make -C ray_tracing test-ray-tracing-runtime-host-lifecycle-contract`
  - `make -C ray_tracing test-ray-tracing-core-sim-runtime-frame-contract`
  - `make -C ray_tracing test-scene-editor-pane-host-contract`
- Headless request/render/material lanes:
  - `make -C ray_tracing test-ray-tracing-render-headless-preflight`
    - includes positive preflight plus deterministic request-path negative
      fixtures for missing scene path and invalid output root
  - `make -C ray_tracing test-ray-tracing-render-headless-image-export`
  - `make -C ray_tracing test-ray-tracing-material-preview-headless`
    - uses repo-local generated roots and includes deterministic material
      request negative fixtures for missing runtime scene and invalid output
      path
- Job-runner, publish, package, and release probes:
  - `make -C ray_tracing test-ray-tracing-job-runner-smoke`
  - `make -C ray_tracing test-ray-tracing-job-runner-bundle-smoke`
  - `make -C ray_tracing test-ray-tracing-job-runner-policy`
  - `make -C ray_tracing test-ray-tracing-publish-helper-validation`
  - `make -C ray_tracing test-ray-tracing-repo-doc-redaction`
  - `make -C ray_tracing test-ray-tracing-linux-worker-package-validator`
  - `make -C ray_tracing test-ray-tracing-release-contract-redaction`
  - `make -C ray_tracing package-linux-worker-self-test`
- Stable tests:
  - `make -C ray_tracing test-stable`
  - authoritative target membership is `STABLE_TEST_TARGETS` in
    `make/rules-test.mk`
- Smoke wording note:
  - `make -C ray_tracing run-headless-smoke`
  - currently routes through `test-stable` rather than a separate runtime-only lane
- Build-only readiness:
  - `make -C ray_tracing visual-harness`
- Source-run visual proof:
  - `make -C ray_tracing visual-artifact`
  - renders `ray_tracing/visual_artifacts/source_first_frame/frames/frame_0000.bmp`
  - validates the BMP as parseable and nonblank before printing
    `ray_tracing visual artifact ready: <artifact-path>`
  - writes validation metrics to
    `ray_tracing/visual_artifacts/source_first_frame/artifact_validation.json`
  - generated proof files are under ignored `ray_tracing/visual_artifacts/`
  - this is the R6 baseline visual proof; packaged-app visual capture remains
    release-gated and is not a separate `package-visual-artifact` target
- Current native `3D` regression coverage is also decomposed into narrower families under the stable lane:
  - config-animation source/volume and settings/export persistence
  - prepared-render parity and scatter-preview proofs
  - scene-geometry builder and trace contracts
- Legacy lane:
  - `make -C ray_tracing test-legacy`
  - currently empty unless a future compatibility lane is added
- Operator-invoked visual/review lanes:
  - expensive water/review or local worker-backed visual matrix targets remain
    manual unless a slice explicitly promotes one into `STABLE_TEST_TARGETS`

## Release and Packaging Snapshot
- Release-readiness and desktop packaging lanes are active and maintained.
- Source-run `visual-artifact` is the current first-frame visual proof
  baseline. Packaged-app visual proof is intentionally deferred unless package
  runtime mismatch becomes the active release/readiness question.
- Linux worker packaging now emits truthful host-architecture metadata for
  either `linux-x86_64` or `linux-aarch64` by default:
  - `make -C ray_tracing package-linux-worker`
  - the package manifest platform follows the Linux build host architecture
  - `LINUX_WORKER_PLATFORM=<value>` remains available for explicit override
- Linux PC package promotion requires the package manifest to report
  `platform=linux-x86_64`. The local Mac-side package lane can produce/stage
  Linux PC `x86_64` worker artifacts when the x86_64 package/toolchain mode is
  selected, so agents must verify the manifest rather than assuming Apple
  Silicon implies `linux-aarch64`.
- Standard package flow is available through `package-desktop*` targets.
- Release flow includes contract/audit/sign/notary/staple/verify/distribute gates.

## Runtime Data Policy
- Tracked defaults remain under `config/`.
- Runtime/generated state remains under `data/runtime` and other ignored runtime lanes.
- Export/video roots are runtime-config driven and menu-editable.

## Current Boundary
- Do not reopen closed `I5`/`I6` slices.
- Treat procedural material-texture sampling plus focused Material editor mode as the active post-`I6` material authoring foundation.
- Preserve Material mode's object-focused viewport controls while the next authoring controls land: `F` should frame the focused object, and wheel zoom should accumulate around that fit instead of resetting on every scroll tick.
- Keep the next material/material-authoring slice focused on authoring-side layer-role conventions, then coexistence polish between procedural overlays and authored bitmap bindings, plus durable group/preset controls on top of the now-live manifest path.
- Keep deep-render start/resume behavior stable while adjacent runtime-scene buckets settle.
- Keep the new menu-render, digest-pick, and native `3D` test-family seams aligned with their current helper/file boundaries while larger file-split work continues.
- Defer VF3D / `physics_sim` ingestion expansion until the next internal renderer boundary is chosen.

## History and Deep Lane References
- Full phase-by-phase details and archived slices are in private docs:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/ray_tracing/`
- This file is the compressed public current-state contract.
