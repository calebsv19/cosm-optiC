#include "procedural/procedural_surface_mesh_asset_adapter.h"

#include <stdio.h>
#include <string.h>

static CoreResult invalid_argument(const char *message) {
    return (CoreResult){CORE_ERR_INVALID_ARG, message};
}

CoreResult ProceduralSurfaceMeshAsset_FromPrism(
    const ProceduralSurfacePrismMesh *mesh,
    const ProceduralSurfacePrismMeshSummary *summary,
    const char *asset_id,
    const char *source_asset_id,
    CoreMeshAssetRuntimeDocument *out_document) {
    CoreMeshAssetRuntimeDocument document;
    CoreResult result;
    size_t triangle_offset = 0u;
    if (!mesh || !summary || !asset_id || !source_asset_id || !out_document ||
        !mesh->vertices || !mesh->triangles ||
        mesh->vertex_count != summary->vertex_count ||
        mesh->triangle_count != summary->triangle_count ||
        summary->surface_group_count != PROCEDURAL_SURFACE_PRISM_FACE_COUNT ||
        summary->boundary_edge_count != 0u ||
        summary->connected_component_count != 1u ||
        summary->euler_characteristic != 2 ||
        !(summary->signed_volume_units3 > 0.0)) {
        return invalid_argument("validated closed prism mesh is required");
    }
    core_mesh_asset_runtime_document_init(&document);
    result = core_mesh_asset_runtime_contract_set_asset_id(
        &document.contract, asset_id);
    if (result.code != CORE_OK) goto fail;
    result = core_mesh_asset_runtime_contract_set_source_asset_id(
        &document.contract, source_asset_id);
    if (result.code != CORE_OK) goto fail;
    result = core_mesh_asset_runtime_document_set_vertex_count(
        &document, mesh->vertex_count);
    if (result.code != CORE_OK) goto fail;
    result = core_mesh_asset_runtime_document_set_triangle_count(
        &document, mesh->triangle_count);
    if (result.code != CORE_OK) goto fail;
    result = core_mesh_asset_runtime_document_set_surface_group_count(
        &document, PROCEDURAL_SURFACE_PRISM_FACE_COUNT);
    if (result.code != CORE_OK) goto fail;

    document.contract.local_bounds.min = (CoreObjectVec3){
        summary->bounds_min.x, summary->bounds_min.y, summary->bounds_min.z};
    document.contract.local_bounds.max = (CoreObjectVec3){
        summary->bounds_max.x, summary->bounds_max.y, summary->bounds_max.z};
    document.contract.topology_closed_volume = true;
    document.contract.topology_manifold_expected = true;
    document.vertex_normal_count = document.vertex_count;
    document.normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;

    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        document.vertices[i].position = (CoreObjectVec3){
            mesh->vertices[i].position.x,
            mesh->vertices[i].position.y,
            mesh->vertices[i].position.z};
        document.vertices[i].normal = (CoreObjectVec3){
            mesh->vertices[i].normal.x,
            mesh->vertices[i].normal.y,
            mesh->vertices[i].normal.z};
    }
    for (size_t group = 0u; group < PROCEDURAL_SURFACE_PRISM_FACE_COUNT; ++group) {
        size_t group_count = 0u;
        CoreMeshAssetSurfaceGroup *surface_group = &document.surface_groups[group];
        snprintf(surface_group->group_id, sizeof(surface_group->group_id),
                 "%s", ProceduralSurfacePrismFace_Name(
                     (ProceduralSurfacePrismFace)group));
        surface_group->triangle_start = triangle_offset;
        for (size_t i = 0u; i < mesh->triangle_count; ++i) {
            if ((size_t)mesh->triangles[i].surface_group == group) ++group_count;
        }
        surface_group->triangle_count = group_count;
        triangle_offset += group_count;
    }
    if (triangle_offset != mesh->triangle_count) {
        result = invalid_argument("prism surface groups do not cover triangles");
        goto fail;
    }
    for (size_t i = 0u; i < mesh->triangle_count; ++i) {
        const ProceduralSurfacePrismTriangle *source = &mesh->triangles[i];
        CoreMeshAssetRuntimeTriangle *target = &document.triangles[i];
        target->a = source->a;
        target->b = source->b;
        target->c = source->c;
        snprintf(target->surface_group_id, sizeof(target->surface_group_id),
                 "%s", ProceduralSurfacePrismFace_Name(source->surface_group));
    }
    result = core_mesh_asset_runtime_document_validate(&document);
    if (result.code != CORE_OK) goto fail;
    *out_document = document;
    return core_result_ok();
fail:
    core_mesh_asset_runtime_document_free(&document);
    return result;
}
