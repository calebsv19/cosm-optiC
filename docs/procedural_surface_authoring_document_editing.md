# Surface Authoring Document Editing V1

The authoring canvas now supports one guarded reference edit at a time. The
document remains editable JSON source; the canvas does not evaluate graphs or
promote a scene.

## Runtime contract

Set `RAY_TRACING_SURFACE_AUTHORING_DOCUMENT_PATH` to the document JSON path.
The host loads and compiles that document on entry. Select a reference node
and press `E` to begin an edit. The input format is:

```text
reference_id|sha256_digest|output_domain_mask
```

Press Enter to stage the replacement or Escape to cancel the text edit. The
replacement is accepted only when the document, source mesh, and selected
reference digests still match the edit-start guards.

## Save and readback

Applying the authoring session atomically saves the canonical document to the
configured path, reloads it, recompiles its readback plan, and clears the
dirty state only after that readback succeeds. A failed save leaves the host
active and the staged document dirty. Cancelling a staged edit restores its
undo document and never writes the file.

The adapter APIs are:

- `ProceduralSurfaceAuthoringDocumentV1_ReplaceReferenceTransactional`
- `ProceduralSurfaceAuthoringDocumentV1_SaveJsonFileAtomic`
- `ProceduralSurfaceAuthoringDocumentV1_ReadbackJsonFile`

Scene promotion, packaging, release, and version changes are outside this
contract.
