# Sequence playback — Stage 2

Stage 2 connects `ray_sequence_clock_v1` to the authenticated immutable rigid-result consumer, the existing one-camera evaluated-scene service and native headless rendering. This is a source feature requiring a new build; an existing released or installed app does not gain it through a scene file alone.

## Saved contract and clocks

Store the following object at `extensions.ray_tracing.authoring.sequence`. The example is the qualified diagnostic fixture; its revision belongs to that result, not an arbitrary replacement scene.

```json
{
  "schema": "ray_sequence_clock_v1",
  "version": 1,
  "rate": {
    "numerator": 24,
    "denominator": 1
  },
  "range": {
    "start_frame": 12,
    "frame_count": 48
  },
  "source": {
    "id": "kinetic_machinery_spill_physics_01",
    "revision": "8211597b4e0c8627e1bde4aabbdc28b2effde04a3e23b11e54f93b08dd5eb18c",
    "rate": {
      "numerator": 240,
      "denominator": 1
    },
    "first_tick": 0,
    "last_tick": 587
  },
  "segments": [
    {
      "id": "play",
      "start_frame": 0,
      "frame_count": 15,
      "kind": "play",
      "source_start": {
        "numerator": 0,
        "denominator": 1
      },
      "speed": {
        "numerator": 1,
        "denominator": 1
      }
    },
    {
      "id": "inspect",
      "start_frame": 15,
      "frame_count": 18,
      "kind": "hold",
      "source_start": {
        "numerator": 5,
        "denominator": 8
      },
      "speed": {
        "numerator": 0,
        "denominator": 1
      }
    },
    {
      "id": "resume",
      "start_frame": 33,
      "frame_count": 15,
      "kind": "play",
      "source_start": {
        "numerator": 5,
        "denominator": 8
      },
      "speed": {
        "numerator": 1,
        "denominator": 1
      }
    }
  ]
}
```

Segment `start_frame` values are local offsets from `range.start_frame`. The output range is half-open. Presentation time begins at zero even with a nonzero absolute frame origin. The output contains exactly `frame_count` frames and lasts exactly `frame_count / rate` seconds; the last frame's timestamp is one frame interval before that duration. See [the pure clock contract](sequence_clock.md) for arithmetic, join and envelope rules.

A play segment advances source time at its declared positive speed; a hold freezes one exact result tick; the following play resumes from that same source time. The existing camera and authored lights use presentation time throughout. There is no solver execution, inferred interpolation, duplicated simulation state or mutable transport clock. Seeking, shuffled evaluation and fresh-process reopening use the same mapping.

This adapter requires an admitted `rigid_result` bundle: source ID equals the scene ID, revision equals the verified bundle revision, and the declared interval fits its complete recorded coverage. The current rigid-result format authenticates 240 ticks/second, so an alternative declared source rate refuses. If `scene_timeline` is present, its presentation rate and range must match exactly. Unknown sequence fields, null declarations, discontinuities, fractional source ticks and unavailable coverage refuse rather than falling back to legacy time.

Sequence JSON round-trips through the existing scene overlay owner. Stage 2 introduced no editor/MCP sequence authoring command; [Stages 5–6 now provide retained authoring](inspection_sequence_authoring.md). Internal `EvaluatedSceneSnapshot` schema 7 (schema 5 was introduced in Stage 2, schema 6 in Stages 3–4) carries explicit sequence provenance and validates it against the current result before geometry preparation; this is an internal snapshot layout revision, not a migration of legacy saved scene schemas. A rigid sequence plus an animated simulation-cache binding refuses. [Stage 7](sequence_sources_and_multiview.md) separately admits a pinned complete VF3D source; it does not mix it with the rigid adapter.

## Native frames and uniform video

Use the normal headless request route with the saved scene and its complete presentation frame range. Each successful native BMP receives a `.bmp.sequence.json` sidecar binding scene/image SHA-256, absolute frame, rational rate/PTS/source time, source tick/revision, camera position and body poses. Keep those sidecars with the originals.

Disable legacy `output.video.enabled` for a sequence request; preflight refuses that encoder because it does not own the new rational frame schedule. After the native frames complete, use:

```sh
python3 tools/sequence_video.py --scene /absolute/scene_runtime.json \
  --frames /absolute/render/frames --output /absolute/sequence.mp4
```

The output must not already exist. The directory must contain exactly the complete numbered BMP range and matching receipts. The encoder verifies mapping and hashes, pins inputs across encoding, uses one constant rational input rate and validates every decoded PTS, frame count and exact container duration. MP4 track and movie timescales are explicit, including fractional rates such as 30000/1001. It produces a `.mp4.json` proof. Missing/extra frames, tampered receipts and changed inputs refuse; no duration-list concatenation, frame renaming or FPS conversion is used. FFmpeg/ffprobe are external dependencies. The current MP4 route requires nonnegative frame origins, at most one million frames and a reduced rate numerator no greater than INT32_MAX.

Native geometry preparation is qualified for integer presentation frames only. The pure API can evaluate exact rational subframes, but native fractional shutter/subframe geometry is not qualified in Stage 2. The existing legacy normalized-time seam derives an integer sequence frame and refuses fractional positions instead of inventing a source state.

## Qualification and boundaries

The retained six-body test covers 54 selected/shuffled/repeated samples, 324 exact body comparisons, native prepared geometry, Preview elapsed-time versus direct-frame parity, overlay save/reopen, provenance forgery refusal and nine invalid source/clock/schema cases. An 18-frame hold preserves body poses and geometry while all 18 camera images differ. The 48-frame 24 FPS video is exactly two seconds; a separate three-frame 30000/1001 video is exactly 1001/10000 seconds. Both decode and have exact rational PTS and duration ticks.

This diagnostic uses only the existing qualified Kinetic Machinery Spill ticks 0–587; it does not extend physics qualification. Low-resolution proof images demonstrate timing, not composition or final quality. Full application compilation, focused Preview/writeback/transport regression and clock/codec sanitizers pass. Stage 2 qualification did not include installed GUI acceptance, desktop refresh, new physics, multiple cameras, shots, inspection orbits or general simulation adapters. Stages 3–4 now add [stable named cameras, independent views, shots and camera clock policies](camera_sequence.md). [Stages 5–6 now add state-bound inspection orbits and retained native/MCP authoring](inspection_sequence_authoring.md).
