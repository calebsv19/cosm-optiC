# Camera transport weight audit

This development diagnostic separates raw transport weights from denoising and
photon-map reconstruction. The observer does not change shading. The camera transport now uses explicit boundary orientation and bounded medium bookkeeping.

For a normal headless request, select at most 16 native pixel coordinates using
`RAY_TRACING_NATIVE_3D_PIXEL_PROBE=x,y;x,y`. Add
`RAY_TRACING_TRANSPORT_WEIGHT_PROBE=1` to emit `TRANSPORT_WEIGHT` JSON lines on
stderr. Records include source hit identity, incoming/outgoing directions,
material and selection weights, reflection PDF/Fresnel, the camera
interface resolver's actual medium indices, parent weight, sampled factor, roulette survival,
and finite direct-light contributions. Capture is thread-local, limited to 128
events per selected pixel shading call, and uses no image-sized storage. Its
interface fields describe the current resolver; nested-medium numerical checks
qualify the tested interface sequences, not arbitrary scene topology. Reflection half-vector fields are relevant only to reflection.

`RAY_TRACING_NATIVE_3D_PIXEL_PROBE_ONLY=1` shades just the selected coordinates
with the normal camera/tile/sample context. Its sparse frame is diagnostic output
and must not be presented as a beauty render. Malformed coordinate selection
turns observation off. With no diagnostic settings, ordinary shading is unchanged.

Build the small numerical audit in a fresh contained output root:

```sh
make BUILD_TOOLCHAIN=clang CSTD='-std=c11 -O2' \
  BUILD_DIR=<fresh-output>/build \
  WORKER_VERSION_GENERATED_DIR=<fresh-output>/generated transport-weight-audit
python3 tests/tools/run_transport_weight_audit.py \
  --binary <fresh-output>/build/tests/tools/transport_weight_audit \
  --output <fresh-output>/physical_qualification.json
```

The numerical executable emits observations. The Python qualification wrapper
returns nonzero when physical checks fail; successful compilation or execution
alone does not qualify transport. It covers explicit Snell/slab directions,
sampled reflection PDF and weight identities, reflection-side mixture PDFs,
face-forward camera slab exit, total internal reflection, clear-interface radiance
transmission, independent grazing-branch batches and roulette compensation.

The repaired local qualification passes 39 checks for indices 1.45, 1.333 and
1.52. Camera intersections retain entry/exit orientation separately from
face-forward shading normals. Camera continuation reuses the bounded medium
stack to resolve surrounding indices and reject mismatched exits. TIR reports
Fresnel 1, combines dielectric reflection/transmission selection into reflection,
retains the medium and uses the dielectric continuation depth budget. Transmission
uses radiance-mode `(etaFrom / etaTo)^2` weighting with no legacy weight floor or
upper clamp. Thin-walled crossings do not mutate the stack. Ideal dielectric
emitter hits use delta-event MIS weight 1. Photon flux weighting is unchanged.

The orientation flags fit existing hit-record padding (952 bytes before/after in
the qualified Mac build). Medium state is local and bounded; no frame-sized
guide buffers were added. Interface response and transmission sampling live in
small sibling modules rather than growing the large transmission implementation.

The retained four floor impulses previously resembled rare grazing reflections.
Corrected medium indices classify the relevant internal events as TIR; their
folded compensation is approximately 1 rather than 11–21. Selected aquarium
replay keeps camera samples fixed with photons and denoising disabled.

These checks qualify the bounded clear-interface repair. They do not certify
full-frame energy, rough dielectric microfacet transport, unknown nested media
at the camera origin, animated water, every bright pixel, or Linux workers.
Ordinary fixed material branch allocation remains unchanged outside TIR.
Fresnel-aware selection still needs equal-budget, independent-seed comparisons
and matching PDF/MIS qualification; no general candidate was promoted. See
[PBRT dielectric transport](https://www.pbr-book.org/4ed/Reflection_Models/Dielectric_BSDF)
for the physical reference; authored tint/reflectivity policies require separate
assessment from pure clear-interface energy tests.
