# Worker Operation Profiles

RayTracing owns the operation-specific part of worker compatibility. The
generic compute runtime owns static ELF inspection, authenticated runtime
inventory, and the fail-closed join between those records.

`config/worker_operation_profiles/headless_cpu_still_v1.json` describes the
CPU still-render operation for
`bin/ray_tracing_render_headless`. It applies only when
`output.video.enabled` is `false`.

For that operation:

- FFmpeg is not required because video assembly is disabled.
- No GPU device, Vulkan ICD, or display server is required by the selected
  runtime path.
- `libvulkan.so.1` remains a required linked library because it appears in the
  binary's `DT_NEEDED` set. An operation profile cannot waive dynamic-loader,
  linked-library, or symbol-version requirements.

The profile binds the source files supporting these claims by SHA-256. The
generic local matcher physically rereads those files without following
symlinks before accepting the profile as match input. The separate compute
runtime selector can now bind this profile to one immutable job only after it
physically rereads that job's payload, matches the declared Ray request schema
and `schema_version` discriminator, and observes
`output.video.enabled=false`. That selection still does not acquire or match a
runtime, launch QEMU, or grant execution admission.

`config/worker_operation_profiles/headless_cpu_still_guest_invocation_v1.json`
owns the separate application-execution declaration for the same operation.
It binds the CPU-still operation-profile digest and declares:

- the eight application arguments required by
  `ray_tracing_render_headless`, including `--summary-file-only` so the
  bounded guest stdout channel does not duplicate the durable summary
  artifact;
- the guest working directory;
- the immutable request plus sibling runtime-scene staging paths; and
- the bounded render-summary and first-frame output contracts.

The compute runtime may generically validate and physically close those inputs,
but it does not interpret RayTracing request, scene, or output meaning. The
current closure remains a no-payload, no-admission, no-QEMU receipt; a later
composition layer must stage the declared bytes outside the reusable package
and include them in whole-tree guest verification.
