# Named cameras, independent views and shot clocks — Stages 3–4

This opt-in source feature extends the existing timeline and Stage 2 sequence playback. Camera identity, shot selection, camera-motion time, presentation time and simulation time have distinct owners. Existing scenes without `camera_rig` retain the original `camera/main` evaluation and paths.

## Saved rig

The existing scene/overlay owner retains `extensions.ray_tracing.authoring.camera_rig`. Version 1 has exactly six root fields. It owns up to 16 uniquely identified cameras and up to 128 uniquely identified shots. The following static example lasts exactly ten seconds (240 frames at 24 FPS):

```json
{
  "schema": "ray_camera_rig_v1",
  "version": 1,
  "rate": {
    "numerator": 24,
    "denominator": 1
  },
  "range": {
    "start_frame": 0,
    "frame_count": 240
  },
  "cameras": [
    {
      "id": "camera/A",
      "position": [
        0,
        -6,
        3
      ],
      "yaw": 0,
      "pitch": 0,
      "fov_y": 55,
      "zoom": 1,
      "clock": {
        "mode": "selected",
        "offset": {
          "numerator": 0,
          "denominator": 1
        },
        "speed": {
          "numerator": 1,
          "denominator": 1
        },
        "duration_frames": 240,
        "end_behavior": "hold"
      }
    },
    {
      "id": "camera/B",
      "position": [
        4,
        -5,
        4
      ],
      "yaw": 0,
      "pitch": 0,
      "fov_y": 55,
      "zoom": 1,
      "clock": {
        "mode": "continuous",
        "offset": {
          "numerator": 0,
          "denominator": 1
        },
        "speed": {
          "numerator": 1,
          "denominator": 1
        },
        "duration_frames": 240,
        "end_behavior": "hold"
      }
    },
    {
      "id": "camera/C",
      "position": [
        -3,
        -4,
        3
      ],
      "yaw": 0,
      "pitch": 0,
      "fov_y": 55,
      "zoom": 1,
      "clock": {
        "mode": "clip",
        "offset": {
          "numerator": 0,
          "denominator": 1
        },
        "speed": {
          "numerator": 1,
          "denominator": 1
        },
        "duration_frames": 240,
        "end_behavior": "hold"
      }
    }
  ],
  "shots": [
    {
      "id": "establish",
      "camera_id": "camera/A",
      "start_frame": 0,
      "frame_count": 48,
      "clip_start": {
        "numerator": 0,
        "denominator": 1
      }
    },
    {
      "id": "side_contact",
      "camera_id": "camera/B",
      "start_frame": 48,
      "frame_count": 48,
      "clip_start": {
        "numerator": 0,
        "denominator": 1
      }
    },
    {
      "id": "return_A",
      "camera_id": "camera/A",
      "start_frame": 96,
      "frame_count": 48,
      "clip_start": {
        "numerator": 0,
        "denominator": 1
      }
    },
    {
      "id": "inspection_C",
      "camera_id": "camera/C",
      "start_frame": 144,
      "frame_count": 48,
      "clip_start": {
        "numerator": 1,
        "denominator": 1
      }
    },
    {
      "id": "aftermath_A",
      "camera_id": "camera/A",
      "start_frame": 192,
      "frame_count": 48,
      "clip_start": {
        "numerator": 0,
        "denominator": 1
      }
    }
  ]
}
```

IDs are stable references, independent of array order, display order, position and selection. Use `camera/<name>` targets; shot IDs should describe their intended purpose. Camera bases use authored world positions, yaw/pitch in radians, vertical FOV in degrees, and positive zoom. Positions scale once through the existing scene world-scale conversion. The rig is authoritative for named camera bases; the top-level legacy `cameras` seed remains preserved for old scene compatibility, not a second owner of rig pose.

All shown fields are required, with strict types and unknown-field refusal. IDs, duplicate definitions, missing camera references, non-finite values, bad rate/range, zero speed, unsupported clock policies and unsupported end behavior refuse. A rig's rational rate and complete frame range must exactly match `scene_timeline` and the simulation `sequence` when present. An absent rigid simulation sequence is allowed for camera-only evaluation; camera selection does not require a numerical solver or physical result.

## Paths and authored channels

Use the existing `scene_timeline` camera channels with the stable target, for example `camera/A`: position, yaw, pitch and vertical FOV. Named cameras also bind existing `motion_paths` through `target_id`, `camera/route_progress` and the retained prior-position source contract. Linear/curved geometry, orientation frames, roll, stable target aim and their existing limits remain in the path owner. This stage generalizes the existing route/aim adapter; it does not create a second path system.

The rig evaluates camera tracks at that camera's mapped motion sample. It evaluates lights and ordinary object channels at the original presentation sample. Result-backed bodies still use the Stage 2 simulation mapping. Motion plans, where admitted by the existing path owner, retain their authored dependency/limit rules; retiming is not a new physical speed certification. New named rigs use explicit position channels or typed routes; legacy `camera/path_progress` is restored only for an explicitly converted `camera/main` route with its retained legacy path and orientation. Other named cameras do not inherit that global carrier.

The current core rigid reader recognizes only `camera/main` as a nonphysical path target. The app therefore passes a separate physical-consumer copy that omits only independently validated named camera route bindings. Every object binding/claim, physical declaration, result revision and asset remains in physical validation. This compatibility adapter does not edit retained inputs, rewrite result identities or expand physics qualification.

## Three deterministic motion clocks

Every camera declares a clock mode, nonnegative rational offset (seconds), positive rational speed, motion duration in frames and explicit final-state hold. These describe camera motion, never simulation progression.

| Mode | Camera motion before offset/speed | Returning to a camera |
| --- | --- | --- |
| `continuous` | Presentation time since output start | It has progressed while hidden |
| `selected` | Sum of its declared shot intervals before the sample | Continues where its last selected interval ended |
| `clip` | The latest entered shot's `clip_start` plus elapsed time within that shot | Enters at the declared clip time; may intentionally revisit or jump in its own motion |

For `selected`, activity comes only from the complete saved shot schedule. It does not depend on rendering a view, opening a viewport, play/pause calls or process history. During gaps, a named offscreen view retains its accumulated time. A `clip` camera is at offset before its first shot; after a shot it holds that clip's end until its next declared shot. Non-clip shots require zero `clip_start`, preventing silently ignored data.

Apply speed to the mode's elapsed time, then add offset. Camera motion holds at `(duration_frames - 1) / output_rate`. This is an explicit camera end policy, independent of the simulation's envelope or holds. Reverse, wrap, looping, implicit rounding and negative time are not admitted. Checked rational arithmetic supplies exact times and an integer-plus-rational-subframe timeline address; an unrepresentable native sample refuses. Camera tracks can use representable subframes, but Stage 2 native physical geometry remains qualified for integer presentation frames only.

In the example, A moves for its first two seconds, pauses while B is selected, then resumes at A's two-second motion position. B's motion time keeps advancing while A or C is selected. C starts its shot at motion time one second. None of these choices alters the ten-second output or its simulation sequence.

## Shot ownership and independent views

Shots partition all local output frame offsets, with no gaps, overlaps or unused tail. Every shot has positive length. Intervals are half-open: the incoming shot owns its start frame. Cuts are selections at an existing presentation sample; no frame is inserted, repeated, removed or retimed. These are intentional hard cuts, not dissolves or generated camera travel between endpoints. Creative framing/cut purpose still needs visual review.

`RayEvaluatedSceneCaptureSample` follows the active shot. `RayEvaluatedSceneCaptureViewSample(sample, camera_id, out)` returns a detached snapshot for one named view. That snapshot keeps the same frame, lights, source tick, body poses and simulation provenance, with the requested camera's independently evaluated pose/clock. It never changes selection or a shared active camera. Existing `camera/main` remains available for legacy scenes. A failed named-view request does not overwrite the caller's result.

A fixed view still follows the rig's saved clock policy. Requesting Camera B for an entire output does not make a `selected` B clock continuously active. This is essential for consistent comparisons of the same edit across separately rendered views. Detached snapshots are immutable values; concurrent rendering through the existing global runtime is not introduced by this API.

## Native render and video use

Headless requests omit `scene.camera_id` to follow the shot edit. To render one independent view for the same output interval, set:

```json
"scene": { "runtime_scene_path": "/absolute/scene_runtime.json", "camera_id": "camera/B" }
```

Native snapshots (internal schema 7; schema 6 introduced camera provenance) retain camera ID, current shot ID, selected status, clock policy, exact motion time and rig revision. Native preparation reevaluates against the current rig and rejects forged/stale pose, lens, orientation or clock provenance. This internal layout change is not a migration of legacy saved scenes. For simulation-sequence-backed renders, frame receipts additionally carry camera clock identity, position, yaw/pitch, FOV, zoom, aspect and orientation frame.

The maintained uniform video route currently requires the Stage 2 simulation `sequence` and its authenticated rigid source. Use `tools/sequence_video.py` normally for the edited output; add `--camera-id camera/B` for a fixed independent-view sequence. The encoder independently checks every native receipt's expected camera/shot/clock against the saved schedule with exact Fraction arithmetic, alongside source mapping, input hashes, PTS and duration. A fixed-view set passed as an edited shot set refuses. Legacy `output.video.enabled` refuses for rigs as well as simulation sequences; camera-only rigs can render native frames, while a general camera-only uniform video adapter remains outside this slice.

No automatic lens/position inspection override is allowed to masquerade as the declared rig: preparation rejects mismatches. Use authored rig definitions for those changes.

## Verification and next boundary

The pure clock tests cover 879 assertions in strict C11 and ASan/UBSan. Native qualification compares all integer frames in shuffled/repeated order for the selected edit and three independent views against an independent Fraction schedule, checks two separately aimed camera paths, final camera holds, camera-only evaluation, save/reopen, invalid replacement preservation and forged camera provenance refusal. Two 48-frame 24 FPS outputs cover the six-shot edit and a fixed view with identical physical states and PTS. The proof uses existing qualified Kinetic Machinery Spill data only, with no new physics.

Stages 5–6 now add [state-bound inspection orbits, retained native/MCP authoring, shared Undo/Redo, Save/reopen and separate clock UI](inspection_sequence_authoring.md). This remains a source feature, not an installed-app update. [Stages 7–8](sequence_sources_and_multiview.md) now add synchronized multiview exports and explicit temporal review. Concurrent global-runtime rendering remains outside the contract. A new build is required; the released app does not acquire these features from JSON alone.

## Event-aware placements

[Event camera planning](event_camera_planning.md) adds reviewed recorded-state views,
stationary lead-ins, aimed Hold orbits and explicit Resume/end behavior. It evaluates
independently of shot selection and retains intent plus resolved/source-bound data.
Ordinary camera rigs and legacy inspections keep their existing behavior.
