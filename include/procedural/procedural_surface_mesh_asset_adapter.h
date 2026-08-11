#ifndef PROCEDURAL_SURFACE_MESH_ASSET_ADAPTER_H
#define PROCEDURAL_SURFACE_MESH_ASSET_ADAPTER_H

#include "procedural/procedural_surface_prism_mesh.h"

#include "core_mesh_asset.h"

/*
 * Transactional adapter matching core_mesh_asset load semantics. The caller
 * supplies an initialized, empty output document and owns its eventual free.
 */
CoreResult ProceduralSurfaceMeshAsset_FromPrism(
    const ProceduralSurfacePrismMesh *mesh,
    const ProceduralSurfacePrismMeshSummary *summary,
    const char *asset_id,
    const char *source_asset_id,
    CoreMeshAssetRuntimeDocument *out_document);

#endif
