# Live MCP catalog updates

The optiC stdio bridge advertises `tools.listChanged: true`. After the client
completes initialization, updates to the application-owned
`tools/scene_editor_mcp_catalog.py` are checked while idle and before requests.
A successful change emits `notifications/tools/list_changed`; the client can
then call `tools/list` to discover the new definitions.

The stdio process, connection and currently attached absolute scene path remain
alive. Catalog replacement does not reconnect, edit a document, clear history,
save, or run a renderer. New calls use the validated replacement method/schema
pair; a call already executing finishes with its selected method. Editor
revision guards and command validation remain authoritative.

Publish a complete catalog file using an atomic file replacement. The loader
compiles exact source bytes rather than relying on Python import caches. It
validates tool names, method mappings, strict required argument schemas and
JSON-serializable definitions before switching. Syntax/validation/read failures
retain the last good catalog and attachment, with diagnostics on stderr rather
than protocol stdout. Identical definitions do not emit redundant notifications.
The catalog is trusted installed program code, never a scene-provided path or
arbitrary code supplied through a tool.

The supported bundle includes all four adjacent files:
`scene_editor_mcp.py`, `scene_editor_mcp_catalog.py`,
`scene_editor_mcp_reload.py`, and `scene_editor_mcp_compat.py`. Catalog reload changes definitions and dispatch
mappings. Transport implementation changes and new native editor commands require
their corresponding program updates. Advertising a tool does not make an older
native editor implement that command; unsupported commands remain refused.

A bridge process started before this mechanism was installed still runs its
previous Python code. It cannot acquire the loader from a catalog update.
New connections started with the updated bridge support this contract. Routine
catalog expansion thereafter does not require restarting that bridge. Whether a
particular client exposes announced tools to an already-running conversation
must be qualified in that client; emitting a valid notification alone does not
prove conversation-level discovery. Avoid using a global client restart as a
routine catalog-development step.

## Compatibility for cached client catalogs

Two fixed tools are available on every connection started with the updated
bridge, alongside the named scene/atmosphere/material/object/camera/lighting/render commands (40 in the
current source catalog, for 42 MCP tools total):

- `editor_catalog({})` returns the current command descriptions, input schemas,
  attached `scene_path` and a `catalog_id`. It does not attach or edit a scene.
- `editor_command` accepts that `catalog_id`, a listed `tool` name and its
  `arguments`. It uses the same attachment, argument validation and native
  document command as a direct named-tool call.

For example, read the catalog, then use its returned ID:

```json
{
  "catalog_id": "<catalog_id returned by editor_catalog>",
  "tool": "material_read",
  "arguments": {"object_id": "<object_id returned by material_list>"}
}
```

Send that object to `editor_command`. For a mutation, include the current scene
`expected_revision` inside `arguments`. Shared Undo/Redo, locks, native command
validation and File Save still apply. The fixed command tool is conservatively
annotated as potentially mutating, even when dispatching a read-only command.

The catalog ID covers command definitions and dispatch mappings. A stale ID is
refused: read `editor_catalog` again and review the updated schema before retrying.
Unknown or removed commands, raw native method names and recursive dispatch to
the fixed tools are refused. Both direct and compatibility calls validate the
current bridge schema vocabulary, including types, required fields, extra-field
rules, bounds and array lengths. Unsupported schema features fail closed;
this adapter is not a general JSON Schema engine. An invalid catalog update
retains the last good ID, schemas, methods and scene attachment.

This allows a client to keep caching its initial tool list while discovering
additions and schema changes through ordinary calls to those same two tools.
The fixed tools are transport code, outside the reloadable catalog. An older
already-running bridge cannot acquire these handlers through catalog reload;
leave those active connections intact and use the updated bridge on an ordinary
new connection. Once these tools are present, subsequent supported catalog
expansion does not require restarting an active chat or bridge. Native command
implementation updates still require corresponding editor support.

## Codex client qualification

The desktop-bundled Codex engine `0.162.0-alpha.17.2` was qualified through its
app-server protocol using separate ephemeral sessions and only a copied optiC
bridge. A fresh connection discovered the catalog after a tool was added and
successfully invoked that read-only tool. A further fresh connection discovered
its removal. The original test connection retained its scene attachment; saved
scene bytes, persistent client configuration and existing process identities
were unchanged. No model turn, desktop restart or existing-thread reload was used.

That engine did **not** refresh its existing connection's tool inventory in
response to `notifications/tools/list_changed`. The captured transport showed
the notification arriving but no subsequent `tools/list` request, including
after a five-second wait and an app-server inventory read. Therefore fresh
connection discovery is verified for this version; automatic live discovery in
an already-connected Codex conversation remains unsupported in this test.
The server's reload behavior does not remove that client limitation.

To repeat this client check without changing existing server configuration:

```sh
python3 -B tests/integration/test_codex_mcp_catalog_updates.py \
  --codex-binary /absolute/path/to/desktop-bundled/codex \
  --scene /absolute/path/to/open/scene.json \
  --output /absolute/path/to/new/task-owned/evidence
```

The compatibility qualification uses the same desktop engine with a 21-tool
initial inventory (19 commands plus two fixed tools). After a copied catalog
adds a command, that connection's cached inventory stays at 21, while
`editor_catalog` discovers the addition and `editor_command` successfully invokes
it through the original connection and attachment. Fresh connections see 22
tools after addition and 21 after removal. Current-schema validation, stale ID
refusal, removal refusal, last-good fallback and native revision guards pass.
Saved scene bytes, persistent configuration and existing process identities are
preserved. This verifies the fixed compatibility path; it does not imply that
Codex now responds to list-change notifications.

The result records cached-connection compatibility and fresh-connection discovery
separately from live-notification discovery. The test updates a disposable copied catalog, never the catalog used
by another chat. It disables other MCP servers in that test process through
in-memory overrides and cleans up its own connections on exit.

Verification from the repository root:

```sh
python3 -B tests/integration/test_scene_editor_mcp_reload.py
python3 -B tests/integration/test_scene_editor_mcp_reload.py --live-scene /absolute/path/to/open/scene.json
```

The first uses a contained local socket owner to verify transport behavior.
The second performs read-only calls against the already-open native document.
Both test idle notifications, additions, schema changes, removal, invalid-update
fallback, same-process attachment preservation and clean EOF shutdown.
