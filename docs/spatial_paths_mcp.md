# Spatial paths through MCP

The native scene editor exposes retained 3D route geometry for objects, the main
camera and the prepared primary light through the same commands as the Paths UI.
Inspection does not activate animation, convert legacy routes or save a scene.
Writes require the current `expected_revision`, refuse active editor drafts or
rendering, and use shared Undo/Redo. Existing clients can discover these commands
with `editor_catalog` and call `editor_command` with its current `catalog_id`.
No MCP restart is needed to use that compatibility bridge.

## Commands

| Tool | Purpose |
| --- | --- |
| `path_list` | Route IDs/names/counts, follower bindings and world scale |
| `path_read` | One full typed route with stable point IDs |
| `path_create` / `path_rename` | Create a two-point route or rename it |
| `path_set_point` | Complete position, incoming/outgoing offsets and line/cubic segment |
| `path_point_mode` | Explicit Smooth, Corner or Independent handle policy |
| `path_add_point` | Split a segment at its midpoint, or extend its end |
| `path_remove_point` / `path_remove` | Remove a point or an unbound route |
| `path_bind` | Attach/detach `object/<id>`, `camera/main`, or prepared `light/<id>` |
| `path_prepare_light` | Explicit shared setup of the existing animated light source |
| `path_convert_legacy` | Explicit audited camera/light legacy route conversion |

Positions and handle vectors use **authored scene units**. Multiply by
`path_list.motion_paths.world_scale` to obtain world metres. Handles are offsets
from the anchor. `path_set_point` keeps the existing handle policy: linked offsets
must be collinear and opposing; Corner offsets must be zero. Change policy
explicitly before providing independent offsets. Smooth uses the same native
geometry policy as the UI; loading never smooths or normalizes legacy geometry.

Routes contain 2–32 points, with at most 16 routes and 64 follower bindings.
Read routes individually to keep responses bounded. Point insertion is the exact
UI midpoint subdivision/end extension operation and returns its new stable ID.
Removing a route refuses enabled followers; inactive binding references are
removed with it while disabled timing channels remain retained.

## Authoring workflow

1. Read state and `path_list`; read the route before editing point IDs.
2. Create/edit a route and attach an unlocked visible object or the main camera.
3. For the primary light, call `path_prepare_light` separately first, then use its
   target identity from `animation_read`. Setup requires an unambiguous existing
   animated light source. It does not synthesize arbitrary lights.
4. Edit the enabled follower progress channel using the animation tools. Object,
   camera and light route progress values run from 0 to 1. Spatial handles and
   temporal key handles are independent.
5. Render a frame, Save or Save As, and reopen to verify retained state.

Attachment saves the exact predecessor position-channel IDs, disables those
channels, and creates/enables route progress. Rebinding keeps the predecessor;
detach restores it. Camera orientation/lens and light intensity remain separate.
Malformed timelines, missing predecessor identities, locked object attachment,
stale revisions and invalid types refuse without partial changes.

Legacy carriers and producer metadata remain retained. Path/follower commits
preserve unknown path/point/binding/timeline/channel/key fields and optional
omissions on unchanged values. Explicit conversion uses the existing native
bounded audit: a 2–32-point active legacy source, at most 4096 frames, no active
route binding or route timing to overwrite. Unsupported conversion refuses;
ordinary attachment remains available without conversion.

Motion-plan and follower-orientation MCP controls are documented in
[motion and orientation](motion_orientation_mcp.md). Arbitrary secondary-light route
creation and batch/animation rendering remain separate work.

## Verification

`tests/integration/test_spatial_paths_mcp.py --binary <native-acceptance-binary>
--output-root <fresh-directory>` exercises actual UI input, stdio MCP and the
cached-catalog bridge, shared history, metadata/attachment preservation, follower
source restoration, typed/stale/dependency refusals, Save and fresh-process reopen.
Direct Light output is compared exactly across Save/reopen. Direct Light and
Disney V2 also use a fixed-sampling native oracle for exact fresh-process pixels;
independent Disney jobs have distinct sampling sequences.
