# Disney V2 Visual Matrix - Primitive Glass Corridor

This fixture set seeds the small canonical visual matrix for RayTracing
integrator comparisons. The scene is the existing primitive glass corridor with
transparent/emissive geometry, shared camera/light settings, and multiple
integrator layers.

Use the `request_disney_v2_denoise_off_12.json` and
`request_disney_v2_denoise_on_12.json` pair for apples-to-apples temporal
denoise checks. Both use `temporal_frames=12`; only `render.denoise_enabled`
changes.

The educational layer requests keep the same camera and scene while switching
the native `3D` integrator:

- `direct_light`
- `diffuse_bounce`
- `material`
- `emission_transparency`
- `disney`
- `disney_v2`
