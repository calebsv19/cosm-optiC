# optiC / RayTracing Agent Guide

This file is the repo-local start point for agents working in `ray_tracing`.
The public product name is `optiC`; the repository and program key are
`ray_tracing`.

## Read First

1. `README.md`
2. `docs/AGENT_CONTROL.md`
3. `docs/AGENT_DEMO_PACK.md`
4. `docs/headless_agent_render_cli.md`
5. `docs/headless_material_preview_cli.md`
6. `docs/current_truth.md`
7. `docs/desktop_packaging.md`

Private CodeWork planning docs, remote worker lanes, production registry records,
and website publication data are not required for the supported local agent
workflow.

## Public Package Discovery

For the current public desktop package and AI-agent download metadata, use the
Ecosystem agent manifest:

```text
https://ecosystem.calebsv.tech/agents/programs/optic.json
```

That manifest is the public package-discovery bridge for `optiC 0.5.0`. It
lists the current macOS `arm64` and `x86_64` ZIPs, immutable version URLs,
SHA-256 values, size metadata, the public agent guide, and this GitHub
repository. Treat the manifest as read-only release metadata unless an operator
explicitly starts a website/release publication lane.

## Safe Local Commands

Run commands from the workspace parent so `make -C ray_tracing ...` resolves:

```bash
make -C ray_tracing ray-tracing-render-headless
make -C ray_tracing test-ray-tracing-render-headless-preflight
make -C ray_tracing test-ray-tracing-render-headless-image-export
make -C ray_tracing ray-tracing-material-preview-headless
make -C ray_tracing test-ray-tracing-material-preview-headless
make -C ray_tracing visual-artifact
```

For a full local confidence pass, use the verification ladder in
`docs/current_truth.md`. Some gates are intentionally broader or slower than the
first agent workflow.

## First Agent Workflow

Use `docs/AGENT_CONTROL.md` for the exact first workflow. The smallest supported
path is:

1. build the headless render CLI
2. run the preflight fixture
3. run the image-export fixture
4. run the canonical demo pack in `docs/AGENT_DEMO_PACK.md` when richer proof is
   needed
5. inspect the generated summary and BMP output under ignored local artifact roots

Do not use remote queue submission, live visualizer publication, package
promotion, or registry mutation as the first workflow.

## Boundaries

Allowed in routine local agent work:

- public docs updates
- request/schema documentation updates
- local fixture validation
- generated ignored artifacts under `build/`, `visual_artifacts/`, or other
  documented ignored output roots

Require explicit operator approval:

- editing `VERSION`
- building release artifacts
- signing, notarizing, uploading, publishing, or deploying artifacts
- mutating `production_registry`
- changing website download or `/agents/` metadata
- starting remote worker jobs or report-inbox handoff flows
- broad renderer, material, caustic, UI, worker, or package source changes

## Dirty Tree Policy

This repo often has active in-flight work. Before editing, run:

```bash
git -C ray_tracing status --short
```

Treat unrelated modified or untracked files as user-owned. Do not revert them.
Keep documentation-only changes separate from renderer/source changes.

## Public Path Policy

Public docs should prefer portable relative paths such as
`ray_tracing/tests/fixtures/...` and placeholder roots such as `<repo>/...`.
Maintainer-local paths, private artifact roots, and host labels may appear only
when the doc is explicitly describing an internal CodeWork workflow.
