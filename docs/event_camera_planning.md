# Event-aware camera placement

This opt-in extension solves a view at a known recorded event before planning its lead-in and inspection. It consumes the authenticated rigid-result sequence and existing camera rig; it never reruns the numerical solver or infers contact from visual proximity. It requires a new build.

## Authoring workflow

1. Choose an exact recorded source tick and an existing Hold whose first source sample is that tick. Select the observed object/pair/point and an orbit pivot.
2. Specify lens, framing margin, target screen offset and preferred viewing azimuth/elevation. The solver chooses distance from recorded bounds rather than inheriting an arbitrary incoming camera.
3. Request three deterministic proposals and review their composition/visibility. A proposal includes its source-bound hash; selection requires that hash and the current document revision.
4. Select a proposal. The retained owner creates `camera/event_<id>` and a default shot over the declared lead/resume interval. Before the Hold it stays at the solved event pose, aimed at the future event location. Objects enter that composition as the recorded simulation advances.
5. During the exact Hold, the camera follows an M5-timed rest-to-rest orbit and recomputes its aim to keep the declared screen offset. Focus and pivot may differ.
6. On Resume, explicitly hold the endpoint or return to entry for a closed orbit; keep looking at the event location or track the recorded target. After the authored end the camera holds its final planned pose.
7. Scrub/review, Undo/Redo and Save. A fresh reopen reevaluates the plan against the authenticated source. A new reviewed proposal with the same event ID atomically replans that owned camera; it does not add a duplicate or silently edit another owner's camera.

MCP and the explicitly opened Render **Events** tab share the native retained document/history owner. The minimal planner supports recorded surface-point picking and Target/View/Review pages. The planner supports stationary or radial dolly lead-ins. Automatic contact discovery and volume-feature targeting remain future work.

## Minimal Events tab and visual feature picking

In the **Render** workspace, explicitly open **Events – event camera planner**. It starts closed, replaces the subject inspector while active and leaves the bottom timeline/transport available. Camera, Light and Paths remain separate subject controls. Leaving Events/Render closes its interaction; Escape cancels an armed pick or numeric draft. Focus loss/resize disarms picking and releases numeric input. Opening another document clears transient planner state. No scene history is changed until reviewed Apply.

- **Target:** reload the first saved event (or create the fresh `ui_event` draft), choose an existing Hold, cycle recorded objects and COM/pair-COM/pair-bounds focus, pick a surface focus or independent surface pivot, copy focus to pivot and set feature radius. Selecting a Hold or arming a pick seeks the existing Hold entry; it never creates or retimes source segments. Other focus forms and named drafts remain available through MCP.
- **View:** vertical FOV, minimum output aspect, margin, screen offsets, viewing azimuth/elevation and orbit angle in degrees; explicit Resume position and aim. Orbit speed/acceleration/braking and lead/end timing remain in the full MCP intent contract. Invalid drafts do not produce selectable cameras.
- **Review:** generate three proposals, inspect their world-camera guides and obstruction status, select a usable candidate and Apply through one shared Undo command. Use existing Preview after Apply for the rendered shot. The guides are not rendered candidate thumbnails. Generate again after changing intent; a document revision change discards proposals and requires reloading a fresh draft.

The right inspector reads **presentation**, **simulation**, **active camera motion** and **inspection** clocks from the same evaluated state used by rendering. It identifies the active view, selected event tick and Lead/Hold/Resume phase; its source phase bar and phase buttons connect to the existing timeline. During Hold, simulation time stays fixed while presentation/camera/inspection can advance. Before applying a draft, these are the current active camera's clocks, not a second draft clock. Lead/event/midpoint/Resume navigation pauses and seeks the existing transport without writing the scene.

While Events is open, the world mesh preview consumes the same recorded source-to-world affine mapping as the picker, including transformed normals and time-sensitive surface-cache invalidation. Closing Events restores the ordinary authoring view. A selected feature becomes `body_point` in accepted visible-mesh source coordinates and remains attached when tracked after Resume; it is not a COM-relative or screen-space offset. The hit metadata includes source revision/tick, triangle index, barycentrics and world position.

The exact pick ray uses the full authenticated recorded mesh and skips hidden bodies. A conservative frontmost displayed-object check also rejects features behind other displayed meshes and supported floor/box primitives. Preview LOD edges may refuse a pick; arbitrary primitive/material transparency/volume visibility is not certified. Use Solid mode to inspect the geometric surface being chosen. This is a point on a recorded surface, not a persistent face-selection set, automatic collision/contact feature, or volume-feature tracker.

### Additional MCP routes

| Tool | Arguments | Effect |
| --- | --- | --- |
| `sequence_event_planner_read` | `{}` | Read transient open/page/draft/proposals, evaluated clocks, drawn frame/tick, viewport token and rectangle |
| `sequence_event_draft` | `intent`, `expected_revision` | Validate/load a transient draft and explicitly enable Events in Render; no scene/history mutation |
| `sequence_event_pick` | `x`, `y`, `viewport_token`, `expected_revision` | Read an exact surface feature at the drawn event sample; does not change the draft or scene |

Coordinates and the returned viewport rectangle use **logical window pixels**, identified by `coordinate_space: "logical_window_pixels"`; a Retina capture may contain twice as many physical pixels. Read the current rectangle/token before querying. Tokens change with viewport projection, document revision or presentation sample; an out-of-view, stale, subframe or wrong-event query refuses. Querying requires an open Events viewport at the selected event tick. UI clicking performs the same query and copies the returned reference into focus or pivot. MCP callers can insert `event_feature.feature` into their intent, load it with `sequence_event_draft`, then use the existing candidates/select contract. Draft loading pauses transport but does not switch workspaces automatically; open Render to display it.

Native acceptance covers 1280×800 and 1024×720 logical windows, all compact bottom controls, actual surface click/display-depth agreement, independent focus/pivot, numeric commit/cancel, hidden-page controls, stale token/revision/time refusal, real stdio draft/read/pick, candidate Apply, shared history, Save and fresh-process reopen. Dedicated planner state remains transient; saved scene contracts are unchanged.

## Intent contract

The strict `ray_event_camera_intent_v1` has exactly these nine root fields. This example matches a 24 FPS presentation with 102 output frames and an existing 72-frame Hold at local output offset 15:

```json
{
  "schema": "ray_event_camera_intent_v1",
  "id": "bracket_event",
  "hold": {"start_frame": 15, "frame_count": 72, "source_tick": 150},
  "focus": {"kind": "body_com", "targets": ["copper_bracket"]},
  "pivot": {"kind": "body_com", "targets": ["copper_bracket"]},
  "framing": {
    "fov_y": 55,
    "aspect": 1.3333333333333333,
    "margin": 0.12,
    "offset": [0, 0],
    "azimuth": 0.6,
    "elevation": 0.45
  },
  "orbit": {"angle": 4.71238898038469, "max_speed": 30, "acceleration": 100, "braking": 100},
  "lead": {"kind": "stationary", "start_frame": 0},
  "resume": {"position": "endpoint", "aim": "track", "end_frame": 102}
}
```

Frame offsets are local to the saved presentation range, with half-open intervals. `hold` must exactly equal one source Hold; at least two rendered frames are required. Lead starts at or before the Hold, and Resume ends at or after it within the presentation range. The orbit reaches its endpoint at `(frame_count - 1) / FPS`, so its last rendered Hold sample is at rest. No frame is inserted or retimed by selecting the camera.

FOV is vertical degrees (5–120). Azimuth/elevation and orbit angle are radians. Azimuth locates the camera around the focus in world XY: zero starts on negative Y, positive angles move toward positive X. Elevation is above that plane, bounded to [-0.9, 1.3]. The three proposals use the requested preference, azimuth + pi/4, and azimuth - pi/4 with elevation raised by 0.25 (clamped to 1.3).

Screen offset uses normalized half-frame coordinates: [-1,1], positive right/up, [0,0] centered. Margin is an inset in those same units: 0.12 leaves 6 percent of the full image width/height at each edge. `abs(offset_axis) + margin` must be below 0.95. Margin is bounded to [0.02,0.5]. Aspect [0.25,4] is the minimum supported viewport ratio for the solved placement. Wider viewports recompute aim to retain the requested normalized offset; a narrower viewport refuses rather than cropping silently. Render candidate comparisons at the intended delivery aspect. Event snapshots bind to the actual native render dimensions before preparation and receipt writing; the declared vertical FOV reaches the projector and primary rays. Logical editor dimensions do not silently substitute for the output aspect. A failed viewport bind leaves its input and output unchanged. The placement solver also keeps its sampled framing bounds beyond the renderer's 0.1 m near plane.

Speed, acceleration and braking use world metres and seconds. The nonzero signed orbit angle is bounded to eight pi. The solver backs away to fit the requested bounds and sampled orbit, but refuses if framing and motion/time limits cannot be satisfied together. It does not relax the limits silently. Candidate fit checks sample 65 orbit poses; native evaluation additionally enforces the retained bounds/margin at each evaluated lead/Hold sample. This is not a continuous-time visibility certificate or fractional-shutter geometry qualification.

## Focus and pivot references

Both fields accept the following strict variants:

| Kind | Fields beyond `kind` | Meaning |
| --- | --- | --- |
| `body_com` | `targets`: one body ID | Recorded COM; frame the body's complete recorded world bounds |
| `pair_com` | `targets`: two distinct body IDs | Mass-weighted recorded COM; frame both bodies |
| `pair_bounds` | `targets`: two distinct body IDs | Center of their combined world AABB; frame both bodies |
| `body_point` | `targets`: one ID, `point`: XYZ, `radius` | Point in the accepted visible source mesh's coordinates; frame a world-space region around it |
| `body_side` | `targets`: one ID, `side`, `radius` | Center of a source-mesh bounding face: `x+`, `x-`, `y+`, `y-`, `z+`, `z-` |
| `world_point` | `point`: world XYZ, `radius` | Explicit fixed world reference and framing region |

`body_point` must lie within the source mesh bounds; use an actual source coordinate, not a guessed COM-relative offset. The same core affine mapping used by native rigid geometry transforms these references into world space, including source COM/principal frame. `body_side` is a bounding-face reference, not a certified surface point or contact feature. Viewing direction remains an independent preference. A detail radius must be positive and at most one million world metres.

Pair COM requires both recorded masses to be positive. A fixed body's zero mass does not define the requested weighted pair center; use `pair_bounds` for a geometric composition. A world reference inside opaque geometry may have no clear entry view. These are placement choices, not automatic contact detection or target-feature extraction.

This planner currently requires a rigid-result sequence, including for `world_point`. It does not yet attach this intent contract to volume-only sources, detect moving volume features or claim a fire/particle camera adapter. Existing volume paths/Holds remain supported independently by Stage 7.

## Moving dolly lead-in

Keep the existing stationary lead unchanged, or replace only `lead` with:

```json
"lead": {
  "kind": "dolly",
  "start_frame": 0,
  "distance_scale": 1.5,
  "max_speed": 30,
  "acceleration": 100,
  "braking": 100
}
```

These six fields are strict. `distance_scale` must be greater than 1 and at most 100; the motion limits must be finite and positive. Distances are world metres, speed is metres/second and acceleration/braking are metres/second squared. The Events Target page offers a lead-kind toggle and scale field. Toggling to dolly starts with scale 1.5, speed 30 and acceleration/braking 100; full limits and start timing remain available through MCP intents. Switching back to stationary removes the dolly-only fields.

The event view is solved first. If its position is E and the recorded focus is F, the dolly starts at `F + distance_scale * (E - F)`, keeps the event lens and future-event aim, and moves along that straight line to E. This uses the focus, even when the orbit pivot is a different feature. Before `lead.start_frame` the named camera holds this pulled-back start pose. Default shot selection starts at the lead frame; an earlier shot may still select another camera.

The existing M5 rest-to-rest scheduler fits the lead to `(hold.start_frame - lead.start_frame) / FPS`, arriving at the first Hold sample at zero speed. Hold orbit timing remains `(hold.frame_count - 1) / FPS`; simulation/source mapping and total video length are unchanged. Insufficient duration or motion limits produce unusable candidates and refuse selection. Position and velocity join continuously; this is not a jerk-limited or continuous-acceleration blend. Presentation time drives the lead, so cuts and detached views do not reset it. Resume policies are unchanged.

Dolly candidates additionally retain `lead_position`, `lead_duration`, `lead_distance`, `lead_possible_occlusion_samples` and `lead_visibility_samples`. Frozen event bounds are checked at 65 lead samples and center-ray visibility at 17 samples. A blocked lead start makes the candidate unusable. These checks concern the future recorded event geometry, not every moving body's earlier position or camera collision avoidance. Review native frames. Stationary candidates omit these fields and retain their previous hashes.

The current native camera evaluator supports fractional presentation samples. The mesh preparation path still rejects fractional samples with `rigid result requires an exact covered tick`, including within a Hold; this slice preserves that existing renderer boundary. Dense camera timing tests and evaluated subframe checks are separate from rendered integer-frame proof.

## Proposal review and selection

The MCP tools are:

- `sequence_event_candidates` → native `sequence.event_candidates`, arguments `{ "intent": <intent> }`. This is read-only and returns `event_candidates` with intent, current revision and three candidate records.
- `sequence_event_select` → `sequence.event_select`, arguments `{ "expected_revision": <current>, "intent": <intent>, "candidate": <0..2>, "proposal_sha256": <returned hash> }`. Selection/replanning is one retained Undo operation and pauses transport.
- `sequence_event_remove` → `sequence.event_remove`, arguments `{ "expected_revision": <current>, "id": <event ID> }`. Removes the plan/lock and leaves camera/shot seeds editable.

Each resolved candidate contains position, yaw/pitch, FOV, focus/pivot positions, framing bounds, entry usability, sampled obstruction counts and a canonical SHA-256 binding the intent, resolved result and source declaration. Recorded-body visibility uses a bounds broad phase followed by the existing native triangle intersector. The 17 visibility samples inspect the target-center ray and exclude the declared subject bodies. A blocked entry cannot be selected; partial orbit obstructions remain reported for review. Static/non-result geometry, whole-subject visibility, camera/geometry overlap, and visibility between samples still require native image review.

Use selection plus native Preview/Undo to compare usable proposals. Do not treat `usable` as artistic acceptance. The retained proof also includes three native candidate stills, including the rejected entry view, so the obstruction decision is reviewable.

`sequence_read`/`sequence_read_section` expose event intent/resolution summaries with `locked: true`, omitting opaque dependency payloads. These summaries are not complete replacement declarations. Saved scenes retain full `authoring.event_cameras` entries: strict `ray_event_camera_v1`, `id`, `camera_id`, `intent`, `selection`, `resolved` and `dependency`. Up to eight plans and the rig's existing 16-camera limit apply.

## Independent clocks, continuity and stale plans

The event evaluates from the saved presentation sample and source mapping, never from selection history. Selection installs a default shot, but later cuts can hide or return to the camera without pausing/restarting the event. Independent named views evaluate the same event at the same frame. Before the lead starts the camera holds its selected lead start view; after Resume end it holds its last planned pose.

The lead and orbit start share position, orientation and lens exactly. A partial orbit's endpoint and the first resumed sample share the same recorded target state and view. `resume.position` accepts `endpoint` or `entry`; `entry` is admitted only for a complete turn. A partial return would require an explicit future transition/cut. `resume.aim` accepts `event` (fixed recorded focus) or `track` (reevaluate the chosen reference at subsequent source ticks). Tracking ends at `end_frame - 1` and then holds. Tracking follows recorded movement directly; no arbitrary incoming-velocity/jerk blend or smoothed pursuit is promised.

Full intent/resolution and dependency records are deep retained copies. Source revision/envelope, world scale, planned camera and source-sequence changes stale the plan. File admission independently resolves the recorded target and recomputes the selected proposal, rejecting forged saved resolution. Competing tracks/routes/legacy inspections cannot drive the same event camera. Other cameras, lights and materials remain separate owners. All writes use revision and active-interaction guards, shared Undo/Redo and Save/reopen. Native preparation reevaluates camera pose/lens/clock/inspection provenance and refuses forged snapshots.

The internal evaluated snapshot layout remains schema 7. Event cameras use inspection ID `event_<id>` and the existing exact inspection-local time fields. `sequence_video.py` validates their phase against the saved event Hold, in addition to source/shot/clock and uniform video timing.

## Qualification

Normal and ASan/UBSan geometry tests each pass 24,366 checks. Native qualification covers shuffled/repeated seeking, future-event stationary lead, exact centered/offset framing, pair mass weighting, source-local point/separate pivot, closed entry return, partial endpoint and post-resume tracking/end hold, detached/cut behavior, source/geometry/plan refusals and forged snapshot rejection. Actual prepared rays are checked with a 75-degree lens, nonzero offset and differing logical/render aspect; narrower-than-reviewed output is refused atomically. Real stdio MCP, atomic replan, shared history, Save and fresh-process native reopen are tested. The maintained native driver is `tests/integration/check_event_camera_runtime.py` with a compact qualified Kinetic Machinery Spill fixture supplied through `--base`.

The selected diagnostic is exactly 102 native frames at 24 FPS, duration 17/4 seconds. It uses only the existing qualified rigid prefix; no solver, installed app or release changes are implied. Candidate images and the video establish this diagnostic composition and behavior, not universal cinematic quality or automatic camera planning for arbitrary simulations.

Moving-lead qualification adds `tests/integration/check_event_camera_lead_runtime.py`, which runs the stationary prerequisite, compares identical source/body states and Hold/Resume views, tests delayed starts, cuts, detached views, subframe evaluation and invalid/forged/stale plans. `make test-event-camera-lead test-event-camera-lead-sanitize` checks dense radial geometry, fixed aim, exact endpoints, speed/acceleration limits and deterministic seeking. Automatic event discovery requires an authoritative pair/contact event stream from the physics owner; visual proximity is not a substitute.
