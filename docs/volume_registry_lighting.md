# Registered incident lights in volume single scattering

The bounded Fire integration adds registered RGB incident lights to the existing native volume single-scatter path. Previously that path read only the white compatibility `scene.light`, so material emitters could light a receiver without lighting smoke. Surface shading and its BRDF remain in their existing path.

`RuntimeIncidentLight3D_Sample` supplies source position, direction, distance, RGB incident weight and endpoint identity. It reuses existing emitter facing, attenuation and rectangle radiometry. Rectangle sampling covers four uniform area strata, without a receiver surface normal. Point sources use one sample; finite legacy sphere lights use four existing disk-proxy samples. `RuntimeVisibility3D_TransmittanceFromPointToEmitterRGB` reuses geometric blocking, endpoint exclusion, transparent material transmission and volume extinction without inventing a surface hit.

A populated registry is authoritative: enabled sources contribute additively, disabled sources stay disabled, and the compatibility light is not counted again. An empty registry retains the original compatibility scatter path. The phase function, camera transmittance, smoke tint, scatter gain and caustic contribution remain as before. Registry-backed sphere lighting now uses its declared RGB and four samples, so compatibility-only and registry-backed sphere pictures need not be identical.

Disk and arbitrary mesh emitters are explicitly unqualified in this slice. Rectangle proxies registered from simple emissive panels are supported. Physical radiometry is supported only for rectangles, matching the existing radiometry helper. There is no temperature channel, blackbody emission, airborne ember source, multiple scattering or new VF3D format.

The JSON render summary adds `volume_registry_source_evaluations`, `volume_registry_contributing_samples`, `volume_registry_visibility_rays`, `volume_registry_unsupported_sources` and `volume_compatibility_fallback_samples`. The existing direct-volume sample counter is widened to 64 bits, including summary output and overflow-safe temporal normalization; tests cover counts above INT_MAX. The new registry counters are raw work totals across pixels, march steps, lights, workers and temporal subpasses; they are not unique light counts or temporal averages. Unsupported sources count per occupied sample evaluation.

Verification uses the native `runtime_volume_3d` group, existing direct-light/radiometry/volume parity/scatter preview/trace-cost groups and the physical-units targets:

```sh
make BUILD_TOOLCHAIN=clang ray-tracing-render-headless build/toolchains/clang/arm64/tests/test_runner
TEST_RUNNER_GROUP=runtime_volume_3d build/toolchains/clang/arm64/tests/test_runner
make dump-sema-runtime-volume-3d-scatter dump-sema-runtime-visibility-3d dump-sema-runtime-incident-light-3d
```

Focused controls establish colored additive lighting without a compatibility light, disabled lights without fallback, one-sided facing, radiance scaling, symmetric rectangular samples, geometric blockers and endpoint identity. Existing empty-registry white and caustic tests pass. The GrowthSim surface-only control remains pixel-identical after the repair.

The cost is proportional to occupied volume march samples times registered sources times source samples, with geometric and volume visibility for each incident connection. No source truncation is used. A dense many-emitter preview needs measured budgets before high-resolution animation. Light selection, visibility amortization and error-bounded negligible-contribution rejection belong in a separate performance slice with comparison against this all-source reference.
