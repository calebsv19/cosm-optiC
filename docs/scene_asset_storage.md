# Scene asset storage and cache identity

Persistent native mesh packs now use the SHA-256 of the complete original mesh
file, core schema, cache schema and pointer ABI as their identity. Identical mesh
bytes moved into another scene revision or worker job use one pack. Changed mesh
bytes or compatibility parameters select a different key. The original runtime
mesh remains authoritative; packed geometry and UV formats are unchanged. Cache
schema 4 uses content identity, so historical path/mtime cache entries are not
selected by this path. They remain regenerable old cache data until normal
cache cleanup; this change does not delete them automatically.

Snapshot dependency copying uses independent APFS copy-on-write clones where
supported, with ordinary copy fallback for other source filesystems. Mutable
hard links are not used. Existing dependency containment, source/destination
hash checks, immutable revision identity and preview/final acceptance continue
to apply. Existing scene and asset files are not migrated or rewritten.

`tools/scene_asset_storage.py` also provides new task-owned generated staging.
Successful producer work removes its own staging; exceptions preserve useful
intermediates/diagnostics. The Prismatic Aquarium UV producer uses this helper
for OBJ, unwrapped OBJ and compiled UV carrier output. It retains original mesh,
final UV mesh, authoring parameters, compiler/unwrap hashes, geometry-preservation
receipt and compact compile log. The UV producer also releases projected text and compiled geometry before final
attachment, avoids deep-copying the native mesh, and checks streaming hashes of
vertices/normals/triangles before and after attachment. Pre-existing UV evidence directories are not
adopted or removed by this helper.

Local proof: two relocated 65,024-triangle scenes write/reuse one native mesh
pack; first render writes once, second hits once and writes nothing. Both renders
match the pre-storage-change BMP hash. A separate actual 3,968-triangle UV producer
run preserves vertices/normals/triangles exactly, emits 11,904 UV corners and
removes only its new successful staging. Worker distribution and canonical
source adoption remain separate from Main Edit implementation.
