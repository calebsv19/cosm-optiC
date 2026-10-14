# Motion plans and follower orientation through MCP

Six tools expose retained movement intent and follower orientation through the
same command owners as the native Paths UI. Inspecting does not activate animation
or migrate a legacy scene. Writes require the current `expected_revision`, refuse
active drafts/rendering, and use shared Undo/Redo. Cached clients discover and
invoke them with `editor_catalog` and `editor_command`, without a connector restart.

| Tool | Behavior |
| --- | --- |
| `motion_plan_read` | Full retained plan intent/dependency; null when absent |
| `motion_plan_preview` | Validate a complete request and return effective limits and waypoint arrivals/departures without changing scene or playhead |
| `motion_plan_apply` | Apply complete movement intent to an attached follower |
| `motion_plan_restore` | Remove the plan and restore original progress evaluation |
| `follower_orientation_read` | Read active follower orientation settings |
| `follower_orientation_set` | Patch orientation; preserve omitted fields |

Targets are `object/<id>`, `camera/main` and the prepared primary `light/<id>`.
Attach through the spatial path tools first. Locked object plan and orientation
writes refuse. The shared retained validators remain authoritative.

## Movement request

A complete request contains `start_time`, positive `max_speed`, `acceleration` and
`braking`, plus 2–32 `waypoints`. Every waypoint contains normalized `progress`
(0–1), nonnegative `speed` and `hold`, boolean `fixed_arrival`, and `arrival`.
Progress is arc distance fraction, independent of Bezier parameter. Times are
seconds on the existing timeline's local clock. Speed is world metres/second;
acceleration and braking are world metres/second squared, including world scale.
The first and final waypoint must be at rest. Start time is nonnegative.

Preview requires a fresh revision and a finished editor draft. It returns actual
arrival/departure times, waypoint distances in metres and effective limits after
native curvature/corner constraints. Impossible arrivals, moving holds/reversals,
missing required corner stops, singular geometry and timelines too short for the
schedule refuse without a partial apply. Use the conflict message to revise the
request; values are not silently relaxed.

Apply preserves original progress channels/keys. A plan locks only its selected
route geometry, progress source, clock and world scale. Restore before changing
those dependencies, then Apply again. Reapply can update intent while retaining
the dependency. Orientation, unrelated channels, environment and other routes
remain independently editable. Restore removes the plan; shared Undo restores it
exactly. Unknown plan fields and same-count waypoint producer fields survive
reapply. A structurally replaced waypoint list has no stable waypoint IDs;
per-waypoint metadata is retained when the count stays unchanged.

## Orientation patch

All followers support dimensionless `start_up`, `start_roll_degrees`,
`end_roll_degrees` and `end_roll_enabled`. Zero up uses the native world +Z
reference. Rolls are unwrapped degrees bounded to ±36000; endpoint roll follows
the native distance interpolation policy. Omitted values are preserved. Editing
a dormant end roll does not enable it; set `end_roll_enabled` explicitly.

Objects additionally support `follow_direction`, `forward_axis` (0–5 maps to
+X, -X, +Y, -Y, +Z, -Z), and `rotation_offset_degrees` (XYZ, ±36000 each). Cameras
support `camera_mode` (`authored`, `route`, `target_aim`) and `use_focus_target`.
Focus requires an authored focus target. Target aim without a focus target uses
the existing native orientation fallback. Authored mode retains legacy yaw/pitch
semantics. Light up/roll settings persist; point-light radiance remains independent
of orientation, as in the native UI. Object-only and camera-only fields refuse on
other target types.

Retained binding/source identities, legacy carriers, producer metadata and
optional omissions remain preserved. No generated timing keys, second transform
owner or automatic legacy conversion is introduced.

## Verification

`tests/integration/test_motion_orientation_mcp.py --binary <native-acceptance-binary>
--output-root <fresh-directory>` exercises actual native input, MCP/cached dispatch,
preview purity, holds/fixed arrivals, dependency/refusal/history rules, producer
metadata and source preservation, attachment preservation, Save and fresh reopen.
Each target's plan and object/camera orientation effects are rendered separately.
Direct Light hashes compare exactly across Save/reopen and Undo. Fixed-sampling
native Direct Light and Disney V2 oracles verify exact fresh-process pixels;
independent Disney jobs use distinct sample sequences.
