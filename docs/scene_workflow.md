# Local scene projects and portable render inputs

`tools/scene_workflow.py` provides a scene-only CLI workflow without opening
optiC, submitting remote work, or publishing to Visualizer. It preserves
`scene_runtime_v1` and the native agent render-request format. It does not replace
Physics Trio's `scene_project.json` contract. Retained atmosphere/cache attachments
are collected with their complete contained dependencies; legacy request-level
simulation/volume projects still use the existing Physics Trio exporter.

Build the native renderer first:

```sh
make BUILD_TOOLCHAIN=clang ray-tracing-render-headless
```

Use the resulting renderer's absolute path in these commands. A new project
owns an editable scene, material/mesh/source dependencies, final render intent,
frozen revisions, revision-specific previews and diagnostic/final bundles.
Existing project destinations are never overwritten.

```sh
python3 tools/scene_workflow.py init --project <new-project> \
  --scene <source-scene.json> --request <render-request.json> --name "My scene"
python3 tools/scene_workflow.py capture --project <project> \
  --revision draft-r1 --renderer <renderer>
python3 tools/scene_workflow.py preview --project <project> --revision draft-r1 \
  --run-id composition-r1 --renderer <renderer> --mode sampled \
  --samples 5 --width 320 --height 180
python3 tools/scene_workflow.py preview --project <project> --revision draft-r1 \
  --run-id motion-r1 --renderer <renderer> --mode consecutive \
  --samples 12 --width 320 --height 180
python3 tools/scene_workflow.py status --project <project>
```

Open `scene_runtime.json` in the editor for changes, then capture a **new**
revision. Capture is additive: it refuses replacement of existing revisions.
`scene_workflow.json` is app-specific workflow metadata, not renderer input.

Preview `index.html` shows native BMPs with authored frame numbers. JSON
receipts record the exact revision/manifest, mode, resolution, frame numbers,
image hashes, completion/failure and logs. Sampled previews include evenly
spaced authored frames; consecutive previews start at the render intent's first
frame. Both retain the full animation sampling window. Sampled images do not
prove continuous motion. These previews keep the selected integrator but use
one temporal pass, disable denoising/video, and bound CPU to one worker/25%.
They are composition/motion diagnostics, not final-quality material acceptance.
Failed runs remain inspectable and cannot qualify for approval.

After a human visually approves the exact preview:

```sh
python3 tools/scene_workflow.py accept --project <project> --revision draft-r1 \
  --preview-id composition-r1 --note "Human review: framing and materials approved"
python3 tools/scene_workflow.py export --project <project> --revision draft-r1 \
  --output <new-final-bundle>
python3 tools/scene_workflow.py verify --bundle <bundle>
```

Approval requires completed matching preview evidence and unchanged image
hashes. Render success never approves a scene automatically. The
`visually_approved` state means approved **for rendering**, not final-render
completion or the existing scene-iteration system's `latest_good` promotion.
Status reports scene, render-intent or dependency drift after approval. An
explicit export of an older approved revision still exports that frozen revision.

Use `export --purpose diagnostic` to test a draft package without approval.
Its command receipt explicitly reports whether the revision is visually approved.
A diagnostic export does not advance the accepted pointer. The portable bundle
itself has no submission authority; future submit integration must consult the
project's approval record and exact manifest digest.

Bundles contain only the current revision and needed dependencies, selected runtime
rendering defaults and render request. They exclude preview results, job history and other
revisions. Render settings, inspection overrides, budgets and video FPS are
preserved; scene/output/progress/video locations are made package-relative.
`results/` is the only mutable output tree. `bundle_manifest.json` binds file
sizes/hashes, revision, worker version, source commit and local renderer hash.
The local binary hash identifies the preview renderer; Linux binaries will have
different hashes and need their own verified source/build identity. Worker
version equality alone does not establish compatibility.

Explicit file references are recursively collected, including managed source
STLs, mesh runtime/authoring/preview sidecars and image dependencies. Implicit
mesh IDs receive explicit project-relative runtime paths. Missing files,
absolute/traversing paths, symlinks, output collisions, changing inputs and digest
mismatches fail closed. Files must already be contained in the source scene's
folder; this tool does not silently import external resources. Curve and
procedural-solid asset references require their dedicated export lanes.

Relocate a bundle and run its native `render_request.json` with
`RAY_TRACING_PROGRAM_ROOT` set to the bundle root and a task-owned
`RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT`. No original scene directory
should be needed. Runtime library/toolchain dependencies of the renderer are
separate from portable scene assets.

This foundation has no desktop UI controls, MCP registration, remote submit,
result transfer or video assembly wrapper. Completion/video verification and
non-destructive archive/restore are described below; frame cleanup remains a later
explicit decision. It does not replace or refresh the installed app.

Verification:

```sh
python3 -m py_compile tools/scene_workflow.py tools/scene_workflow_bundle.py
python3 tests/test_scene_workflow.py
python3 -m unittest discover -s tests -p 'test_scene_project*.py'
```

## Completion, video verification and archival

`scene_workflow_delivery.py` adds reusable terminal-job reconciliation and
non-destructive archive/restore. FFmpeg and FFprobe must be available for video
verification. No video is recreated when the completed job already produced it.

```sh
python3 tools/scene_workflow_delivery.py reconcile --project <project> \
  --run-root <scene-directory> --job jobs/<job-id> --renderer <renderer>
python3 tools/scene_workflow_delivery.py archive --project <project> \
  --run-root <scene-directory> --run-id <render-run-id> \
  --retention compact --output <new-archive-directory>
python3 tools/scene_workflow_delivery.py restore --archive <archive-directory> \
  --output <new-restored-directory>
```

Reconcile requires successful terminal job/summary/progress identities, the exact
frame sequence and valid BMP headers/lengths. It hashes every frame, checks video
resolution/rate/count and fully decodes the movie. It preserves the prior
preflight aliases and atomically replaces output summary/progress with authoritative
terminal files. Replay verifies the same predecessor and refuses changed scenes,
requests, movie or frames. It records `completed_verified` separately from visual
approval. The actual rendered scene is captured independently: a later editable
draft is never assumed to be the scene that produced a finished movie.

`completed_runs/<run-id>/inputs` retains a portable copy of the declared frozen
render input and dependencies. The renderer hash records the supplied local
renderer for restoration; it is not independently a historical execution-binary
attestation. Preserve the original job/build evidence when exact historical
renderer identity matters.

Compact archives retain workflow documents, editable and frozen inputs, assets,
completed job evidence, the movie and first/middle/last final frames. Full archives
add every final frame. Preview caches are omitted. Original source files, frames
and movie are never deleted. Restore checks archive SHA-256, rejects unknown,
duplicate, linked or escaping members, verifies every retained file hash/length
and validates the portable completed input bundle before publishing the restore.
After restoration, render a small frame from these inputs to prove usability.

Cold-archive delivery uses the established host handoff/export-dropbox and archive
helper. Local archive creation alone is not remote delivery proof. Verify the
remote archive's payload hashes against the local manifest and use a fresh
source/destination reconciliation read before treating delivery as complete.
Archive import, result fetching and render submission remain distinct operations.
No archive or completion action grants visual acceptance or cleanup authority.

After a verified cold-archive copy, attach its fresh readback and the restored
render proof to the project:

```sh
python3 tools/scene_workflow_delivery.py record-archive --project <project> \
  --run-id <render-run-id> --archive-manifest <archive>/archive_manifest.json \
  --remote-receipt <fixed-helper-post-copy-readback.json> \
  --restore-receipt <restored-native-render-proof.json>
```

This requires `dry_run_destination_matches_source`, every source/destination
file matching, exact local/remote archive and manifest hashes, no deletion, and
successful restored-render proof for the same completed input scene. Status
validates completion and archive receipt hashes. A successful upload or copy
receipt alone cannot produce `archived_verified_sources_retained`.

Checksum paths in archive exports are rooted at the intake item (`payload/`).
Local restoration accepts these names relative to the archive directory. This
matches the established cold-archive helper; do not strip the prefix before PC
submission. The historical first Aurora dry-run caught this contract mismatch;
the same export-item identity was repaired before any archive apply.
