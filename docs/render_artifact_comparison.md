# RayTracing Render Artifact Comparison

Status: first deterministic local comparison contract implemented.

## Ownership

RayTracing owns render-output meaning. The compute coordinator may bind,
launch, and retain this comparison, but it must not reinterpret image pixels,
headless summary fields, metric thresholds, visual acceptance, or promotion
policy.

The comparison tool is:

```text
tools/compare_render_artifacts.py
```

It compares one source and one target artifact set. Each side must provide:

- exact lifecycle and lifecycle-result identity
- program-release and worker-package versions
- one physically reread `ray_tracing_headless_summary_v1` JSON file
- one physically reread BMP frame
- expected SHA-256 for both files

Artifact roots must be real directories, not symlinks. Summary and frame paths
must be direct, contained relative paths. Missing files, duplicate JSON keys,
digest drift, unsafe paths, unsupported BMP encodings, or malformed fields
fail before a result is written.

## Request Contract

The request uses:

```text
schema_family: codework_ray_tracing_artifact_comparison
schema_variant: render_artifact_pair_request_v1
```

The request binds the schema-80 pair id and pair-result digest. Its fixed
operation contract permits semantic comparison and keeps all of these false:

- visual acceptance
- promotion
- `current` read or mutation
- network, remote, or VPS contact
- custom-OS boot

The first policy uses deterministic integer limits:

- normalized mean absolute channel delta in parts per million
- changed-pixel ratio in parts per million
- maximum absolute channel delta in `[0, 255]`

No floating threshold arithmetic is used.

## Summary Compatibility

Before image thresholds can pass, both summaries must agree on the fixed v1
semantic identity fields:

- render route and native-3D disposition
- completed frame count
- volume source/visibility
- integrator
- start/count/dimensions/time/temporal count/denoise disposition
- inspection preset, trace route, caustic mode, and caustic product mode

Run ids and output filesystem paths are intentionally excluded because fresh
attempts must have distinct identities and roots.

## Result Contract

The create-only canonical JSON result uses:

```text
schema_family: codework_ray_tracing_artifact_comparison
schema_variant: deterministic_bmp_and_summary_v1
```

Its state is one of:

- `semantic_match`
- `semantic_difference`
- `incompatible_artifacts`

The report includes exact source/target lineage and artifact digests, every
summary-field difference, integer image metrics, the applied threshold policy,
and explicit false values for visual acceptance, promotion, `current`,
network, remote, VPS, and custom-OS effects.

`semantic_match` means the declared automated policy passed. It is not human
visual acceptance and does not recommend promotion.

## Local Verification

```sh
make -C ray_tracing test-ray-tracing-artifact-comparison
```

The next boundary is durable coordinator retention of this result against a
schema-80 pair whose terminal artifacts are genuine RayTracing BMP and summary
outputs. The existing synthetic schema-80 unit artifacts are intentionally not
reclassified as render evidence.
