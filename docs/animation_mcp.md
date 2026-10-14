# Animation editing over the live editor MCP

Animation commands use the same retained timeline, key inspector, curve editor,
property validator, evaluator and Undo/Redo owner as the native Render workspace.
Reads never activate or normalize a legacy scene. Save retains the timeline in
`extensions.ray_tracing.authoring.scene_timeline`; unrelated source metadata,
materials, objects, legacy path carriers and atmosphere attachments remain intact.

Start with `scene_read` and `animation_read`. The latter reports timeline
presence/validity, rational `rate`, absolute-frame `range`, world scale and stable
channel IDs. `animation_track_read(track_id)` returns one complete channel with
keys, declared units, interpolation, tangent modes and temporal handles. Reads
are bounded per channel instead of returning a potentially oversized document.

Use `animation_activate(expected_revision)` for the native Set up animation
command. Existing valid timelines are preserved; malformed timelines are refused.
Setup seeds lens and available legacy camera/light path timing through the existing
UI behavior. Object XYZ channels require explicit setup first:
`animation_object_channels(object_id, expected_revision)` uses Animate object.
The object must be visible and unlocked; incompatible path followers are refused.
`animation_add_channel(track_id, property_id, expected_revision)` adds/selects
camera yaw/pitch or light intensity from a matching existing target channel.

Key commands use stable `track_id` and absolute integer `frame`:

- `animation_set_key`: add/update scalar `value`; leaves the playhead in place.
- `animation_move_key`: retime to `destination_frame`; leaves the playhead in place.
- `animation_delete_key`: remove one key, retaining at least one per channel.
- `animation_interpolation`: `step`, `linear`, or `cubic_bezier` to the next key.
- `animation_tangent`: `broken`, `auto_smooth`, `auto_clamped`, or `flat`.
- `animation_handles`: incoming/outgoing frame and value offsets, using the native
  temporal-extent fitting and property validator.

The Render workspace's **Timeline settings** panel and MCP share three management
commands:

- `animation_set_clock(rate_numerator, rate_denominator, start_frame, frame_count)`
  sets exact rational FPS and the inclusive range ending at `start_frame +
  frame_count - 1`. It keeps absolute key frames and values. A range excluding any
  existing key, including a disabled channel's key, is refused. The playhead stays
  in place if possible and otherwise clamps to the new range. Rate numerator and
  denominator are positive integers up to 1,000,000; start/end frames are bounded
  to -1,000,000,000 through 1,000,000,000. Negative timeline frames remain valid;
  the existing render command accepts nonnegative frames only.
- `animation_set_channel_enabled(track_id, enabled)` keeps the channel and its
  keys. Disabled channels remain discoverable and can be enabled through the
  panel's channel selector or MCP. Object XYZ axes act together in one command.
- `animation_remove_channel(track_id)` removes the selected channel, or the
  matching object XYZ group. Shared Undo restores the complete retained entries.
  Locked objects and required spatial-path/binding channels are refused.

Management edits patch only the requested fields or entries, preserving unknown
clock/channel/key metadata and legacy light timing. Save preserves raw legacy
light timing when its parsed meaning matches the runtime carrier; genuine legacy
UI edits still export their changed values.

Every mutation requires the fresh scene `expected_revision`. Active UI numeric
entries, key/curve drags, object operations and renders block agent edits.
Nonfinite values, wrong types, unavailable/disabled targets, invalid property
bounds, frame collisions and out-of-range edits are refused without a document
change. Scalar values use the channel's declared units: camera yaw/pitch radians,
lens degrees, object XYZ world distance, and light relative intensity. Seek a
frame with `scene_seek`; use the existing single-frame render commands to render
an absolute frame from the working scene without an implicit Save.

For clients with a cached tool list, call `editor_catalog`, then invoke any new
animation tool through `editor_command` with that exact catalog ID. The fixed
bridge preserves its scene attachment and connection across catalog changes.
See [catalog updates](mcp_catalog_updates.md).

This slice exposes shared timeline management and channel/key editing. Arbitrary timeline
replacement, new spatial path
geometry, simulation/procedural channels and batch/remote rendering remain
separate controls. Existing spatial paths and timelines continue to load through
the legacy-compatible owner. Exact raster qualification uses a point light;
stochastic finite-radius lighting is not promised to be bitwise identical.

Direct Light job images are compared exactly across Save/reopen. Disney V2 jobs
verify completion, artifacts and identical retained timeline source; a native
fixed-sampling comparison qualifies exact fresh-reopen pixels separately from
per-job stochastic sampling.
