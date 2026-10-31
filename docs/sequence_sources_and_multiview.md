# Recorded volume sources and synchronized delivery — Stages 7–8

The sequence framework now admits one additional source family: complete recorded VF3D v1 volume frames through the existing retained atmosphere manifest. Synchronized delivery renders named camera angles and an edited view, validates their physical-state agreement, and produces a comparison matrix. This is a source feature requiring a new build; no solver, release or installed-app changes are implied.

## Complete recorded-volume adapter

Use the existing `authoring.sequence` Play/Hold/Resume clock with an enabled, animated `atmosphere_attachment` whose source kind is `manifest` and missing policy is `error`. The sequence source ID is the scene ID, its revision is the manifest's full SHA-256, its rate matches the attachment's source rate, and its inclusive tick envelope contains the actual recorded IDs. The retained presentation timeline must match sequence rate/range exactly. Sequence time owns source selection; attachment playback offsets do not create a second mapping in sequence mode.

Add `authoring.sequence_volume`:

```json
{
  "schema": "ray_sequence_volume_v1",
  "frames": [
    {"frame_index": 0, "sha256": "<64 lowercase hexadecimal characters>"},
    {"frame_index": 1, "sha256": "<64 lowercase hexadecimal characters>"}
  ]
}
```

The strict binding has exactly these two root fields. Each frame row has exactly the two shown fields. Pins and the manifest must describe every tick of the same sorted contiguous envelope, capped at 10,000 frames. File admission checks manifest hash, project containment, frame hashes, actual header IDs and complete payload loading before publishing the new runtime scene. Sparse/missing frames, changed bytes, bad rates and incompatible ownership refuse without replacing the prior scene.

The existing VF3D importer owns density, velocity XYZ, pressure and solid-mask data. Full-file hashes pin all bytes, including the header. During Hold, camera and explicitly presentation-driven lighting may continue while the exact recorded fields remain frozen. Resume selects the next declared source sample. Evaluation and native preparation recheck manifest and selected payload identity; changing bytes after evaluation refuses preparation. Existing volume placement and optical appearance remain retained scene settings.

This first adapter accepts a plain raw-VF3D manifest only. It does not admit particle caches, temperature/combustion fields, nested scene bundles, animated water sidecars, sparse-frame fallback or simultaneous rigid-result/volume sequence sources. Rigid-body COM inspection declarations cannot be attached to this volume source. Existing named camera paths and clocks can move through the Hold. Further source families require separate complete-state proof; they are not implicitly certified by this adapter. No numerical simulation is run by optiC.

Native sequence-volume preparation is qualified at integer presentation samples. No interpolation, fractional shutter geometry or motion-blur claim is made. The internal snapshot layout remains schema 7; its existing sequence and cache identities can now coexist with matching source tick/rate and native provenance checks.

## Bind, edit and save

Prepare a retained atmosphere attachment and desired sequence/timeline, then write a new bound scene beside the input:

```sh
python3 tools/sequence_volume.py --scene /project/seed.json --output /project/bound.json
```

The helper pins the actual manifest and frames, updates source identity/rate/envelope, and sets exact missing-frame policy. It refuses to overwrite output or move relative assets into a different parent directory. Native admission remains authoritative for schema, clock and payload validation.

`sequence_apply` accepts `sequence_volume` alongside the other complete typed sections. Read/read-section also expose it. A volume binding, its atmosphere declaration and sequence must remain compatible; configure the attachment through its existing authoring owner and apply the sequence/binding atomically where needed. Source segments, cameras and shots use the same retained history, native controls and Save owner. The existing `sequence_source_setup` convenience command remains specific to authenticated rigid results; volume setup uses the binding helper. Saved overlay writeback retains `sequence_volume` with the other authoritative declarations.

## Synchronized angle delivery

Create a native headless render request with the retained scene path and desired image dimensions, then select declared cameras:

```sh
python3 tools/sequence_multiview.py \
  --request /project/request.json \
  --camera camera/A --camera camera/B \
  --cli /build/tools/cli/ray_tracing_render_headless \
  --output /project/new-delivery
```

The fresh output contains `edit/video.mp4`, `angle_1/video.mp4`, `angle_2/video.mp4`, `matrix.mp4`, native receipts/logs, per-view temporal reports and `result.json`. Matrix camera order follows the command arguments, left to right, then top to bottom, with up to two columns. Up to eight unique cameras are supported. `--no-edit` omits the edited output. Lighting/appearance inspection settings are preserved; camera overrides refuse because named evaluated views own pose/lens. Output frame count/range always comes from the saved sequence. Native temporal accumulation is explicitly one sample per output frame for this delivery path.

A fixed view uses its saved camera clock policy. A selected-time camera does not become continuous merely because it is rendered as a dedicated angle. Cuts select a view at an existing presentation sample; each output retains the same declared duration and simulation mapping.

Each angle passes the uniform sequence encoder independently. The delivery then compares source identity, exact presentation/simulation timestamps, tick and complete rigid-body/cache state at every frame. Missing/extra frames, mismatched camera or cache receipts, duplicate views, changed inputs and occupied delivery roots refuse. The matrix is independently decoded and checked for exact rational PTS, rate, frame count and duration. Source scene and native input hashes are rechecked before completion.

To assemble separately rendered or chunked native frames without rendering again, use `--frames-manifest /project/frame-folders.json`. It maps exactly `edit` (unless omitted) and each requested camera ID to an absolute native frame directory. Example:

```json
{"edit":"/project/edit-frames", "camera/A":"/project/a-frames", "camera/B":"/project/b-frames"}
```

Each folder must contain one complete canonical set of native BMPs and receipts for the full sequence. Rendering order and chunk boundaries do not determine playback time. Reassembly writes a new delivery and leaves the supplied frame folders unchanged. This local route does not submit remote jobs or introduce concurrent global-runtime rendering. Failure leaves a `failure.json` and existing outputs for inspection; do not overwrite an uncertain run.

## Temporal review and acceptance

Temporal reports measure exact-image repeats, RGB changes, camera distance/speed, speed changes, view-angle changes, orientation up-axis changes when available, and vertical-FOV changes. Declared cuts are marked separately, so a purposeful cut is not misclassified as within-shot acceleration. The report is diagnostic: it does not infer artistic quality or declare exposure/shimmer acceptable from one metric.

Every automatic delivery ends with `visual_acceptance: review_required` and a checklist covering target/contact readability, occlusion/framing, cut purpose/direction, roll/speed and shimmer/noise/exposure continuity. Retain a separate review record with the inspected samples and accepted scope. Technical timing success does not silently become a finished-render quality claim.

The qualified volume example has 18 frames at 6 FPS, exactly three seconds. Native shuffled/repeated samples prove frozen complete fields, moving camera, deterministic resumption, atomic refusals and changed-after-evaluation rejection. A separate collision example uses the existing qualified rigid prefix and exact 24 FPS timing. Both exercise edited output and synchronized independent angles. Full application compile and focused clock/motion/timeline/runtime regressions remain gates.

## Roadmap boundary

Stages 0–8 now have working bounded implementations. Stage 7 is qualified for rigid results plus this complete VF3D v1 adapter; other source families remain future adapters. Stage 8 supplies synchronized local delivery and an explicit temporal/visual review process, not automatic cinematography or universal image-quality certification. The [event-camera planner](event_camera_planning.md) now adds explicit target framing for recorded rigid events. Recorded rigid surface picking is implemented in the explicitly opened Events planner. Contact-event discovery, additional source adapters, higher-quality showcase work and source adoption/release remain separate tasks.

For the combined release gates and Linux canary boundary, see [camera framework qualification](camera_framework_qualification.md).
