# RayTracing Memory Check Audit

This note records the default-off fisiCs memory-check audit lane for
`ray_tracing`.

## Command

```sh
make -C ray_tracing memory-check-audit
```

The target first rebuilds the RayTracing app with the fisiCs
`physics-units,memory-check` overlay, then runs a focused
`runtime_triangle_bvh_3d` harness compiled with the same memory-check overlay.
The harness writes:

- stdout: `ray_tracing/build/memory_check/ray_tracing.stdout`
- stderr/report: `ray_tracing/build/memory_check/ray_tracing.stderr`

## Latest Result

Last audited: 2026-06-07.

The latest audit was clean:

```text
[fisics:memory-check] summary: active=0 leaked_bytes=0 allocs=51 frees=51 double_free=0 unknown_free=0 tracker_failures=0
```

Interpretation:

- `active=0` and `leaked_bytes=0`: no tracked allocations remained live at
  process exit.
- `allocs=51` and `frees=51`: the focused BVH harness balanced all tracked
  allocation/free pairs.
- `double_free=0` and `unknown_free=0`: no invalid free classes were reported.
- `tracker_failures=0`: the memory-check runtime tracker itself did not report
  internal accounting failures.

## Scope

This is a default-off diagnostic lane. It does not change normal Clang,
desktop, or headless render behavior. The current harness intentionally focuses
on a compact native `3D` ray/BVH path so memory reports stay small enough to
read while still exercising heap-backed scene/BVH structures.
