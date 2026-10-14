# Live scene lifecycle through UI and MCP

The editor File menu and MCP share native commands for Open scene, Create scene
and Save As. The source catalog has 40 named commands and two fixed compatibility
tools (42 total). A live saved document must already be attached with
`scene_connect`; lifecycle commands operate on that same editor owner.

| Tool | Required arguments | Behavior |
| --- | --- | --- |
| `scene_open` | `scene_path`, `expected_revision` | Validate and apply a regular saved native runtime scene before replacing the current document. Refuse unsaved edits. |
| `scene_create` | `scene_path`, `expected_revision` | Publish a new empty native 3D scene, with a static camera and primary point light, and open it. Refuse unsaved edits and existing destinations. |
| `scene_save_as` | `scene_path`, `expected_revision` | Publish the current retained working scene under a new filename, copying contained dependencies when the directory changes, then attach it. Preserve shared Undo/Redo history. Never overwrite another file. |

Paths must be absolute, with an existing real parent directory. File menu dialogs
accept the same path: Cmd/Ctrl+A clears it, Enter applies and Escape cancels.
Existing Save and Leave editor behavior remains available. A different destination
parent must already exist. Cross-directory Save As keeps the relative asset layout
and legacy JSON declarations, and includes dependencies referenced by working,
Undo and Redo documents. It copies explicit resource references, implicit native
mesh runtime files, existing same-stem preview sidecars, image channels, and
retained atmosphere/cache/bundle frames and sidecars. Nested manifests keep their
owner-relative resolution. Unreferenced files, settings, MCP endpoint files and
render output directories are not copied.

Copies are independent files, using APFS clone extents where available. Each
source/copy hash is checked before publication and again before switching. Existing
identical destination resources are verified and reused; a conflicting resource
refuses without replacing destination-owned data. The scene filename itself must
be new. Private staging, file/directory ownership receipts, no-clobber publication
and native rollback preserve the original scene and attachment if copy or runtime
application fails. A published file modified by another writer is never deleted
by rollback: staging/receipt is retained for explicit recovery. Successful staging
is removed after the new scene applies. The original scene/dependencies stay intact.

This bounded copy path allows at most 512 MiB and 2048 unique dependency files,
16 MiB per dependency JSON, and 64 MiB for the working/history snapshot. It checks
free destination storage before copying. Missing, escaping, absolute file references,
symlink dependencies/destination components, and unsupported native curve/procedural
solid asset references refuse before switching. Descriptive legacy metadata remains
unchanged. This is native saved-scene dependency copying, not whole Physics Trio
project export or external asset ingestion. Staging requiring recovery is named
`.optic-save-as-*`; inspect its `receipt.json` before using the bundled helper's
`--finish rollback` or `--finish commit` with `--stage`. Do not remove changed
files or pre-existing destination data as an automatic recovery step.

If a client times out during copying, reconcile with `scene_read` and the new
scene path before retrying; an existing destination is never overwritten.

Read `scene_read` before each mutation. Save unsaved changes with `scene_save`
before Open or Create. Save As captures retained working edits without changing
the original saved file. Open/Create establish a new document history; Save As
keeps the working history. Lifecycle transitions increment revisions monotonically
rather than resetting them, so queued commands for an earlier document refuse.
Invalid JSON, unsupported scenes, active interactions/renders, occupied endpoints
and existing destination files leave the current document attached. A failed
runtime application restores the previous document/runtime.

The native control socket remains attached to the same editor during a transition.
New bridge connections update their active scene path from successful native
readback. Older running bridges retain a scene-path endpoint alias to the same
editor owner; their next `scene_read` reports the actual current document and
revision. A stale mutation cannot reuse the old document revision. New bridges
also send a native scene-path guard. `scene_connect` refuses an inactive alias
rather than claiming the old document is still open. Up to 32 additional scene
aliases are retained per editor session; further new paths refuse without changes.
Aliases are removed when that editor closes. No MCP connector or chat restart is
required for a transition.

For clients with a cached tool list, call `editor_catalog`, review its current
`catalog_id`, then invoke `editor_command` with `tool` set to one of the lifecycle
tools and its exact arguments. The fixed bridge tools must have been present at
the client's initial connection, as described in [catalog updates](mcp_catalog_updates.md).

Verification covers actual File menu/path input and shared native commands;
named and fixed MCP dispatch; two attached clients; monotonic revision/path guards;
dirty-scene refusal; no-clobber publication; invalid-open recovery; Save As Undo;
legacy producer metadata and relative physical-volume hash preservation; creation,
object/material/camera/lighting authoring, Save/reopen; cross-directory UI/MCP copy and fresh-process reopen with source assets offline;
injected native application failure rollback; and exact Direct Light BMP parity
between original and relocated scenes. Direct Light/Disney V2 single-frame
controls retain their existing render contract. See [render control](render_mcp.md)
for render job/result limits. This slice does not add whole simulation-project export, scene
close/delete, batch rendering, remote jobs or additional light inventory editing.
