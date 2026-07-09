# Caustic Probe - Glass Sphere

This fixture is the L4/L5 local proof scene for caustic readiness. The scene
gives current renderers and the Disney v2 caustic sidecar the same canonical
target:

- one bright finite overhead light
- one transparent glass sphere
- one matte receiver floor
- fixed camera and deterministic render request settings

The probe runner renders the matrix, converts frames to PNG review artifacts,
and writes receiver-region metrics:

- receiver luma concentration ratio
- receiver max/mean ratio
- hotspot area ratio

Current expected interpretation: direct-light and emission-transparency runs
remain baseline measurements. `request_disney_v2.json` enables the opt-in
Disney v2 caustic sidecar through `inspection.caustic_mode = "analytic"` and
uses the high-fidelity `asset_sphere_256x128` runtime mesh for receiver-funnel
proof renders. Summary readback records the requested mode plus sidecar sample
and radiance counters.

Run:

```bash
make -C ray_tracing test-ray-tracing-caustic-probe-matrix
```
