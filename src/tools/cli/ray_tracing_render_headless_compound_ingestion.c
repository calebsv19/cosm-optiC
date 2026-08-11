#include "tools/ray_tracing_render_headless_internal.h"

#include "render/compound_scene_runtime_apply.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ray_tracing_headless_compound_ingestion_init(
    RayTracingHeadlessCompoundIngestion* ingestion) {
    if (!ingestion) return;
    memset(ingestion, 0, sizeof(*ingestion));
    ray_compound_scene_ingestion_file_init(&ingestion->file);
}

void ray_tracing_headless_compound_ingestion_free(
    RayTracingHeadlessCompoundIngestion* ingestion) {
    if (!ingestion) return;
    for (size_t i = 0; i < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT; ++i) {
        free(ingestion->source_positions[i]);
        free(ingestion->world_positions[i]);
    }
    ray_compound_scene_ingestion_file_free(&ingestion->file);
    memset(ingestion, 0, sizeof(*ingestion));
}

static const RayTracingRuntimeMeshAssetInstance* find_instance(
    const RayTracingRuntimeMeshAssetSet* set, const char* object_id,
    const char* mesh_asset_id) {
    if (!set) return NULL;
    for (int i = 0; i < set->instance_count; ++i)
        if (!strcmp(set->instances[i].object_id, object_id) &&
            !strcmp(set->instances[i].asset_id, mesh_asset_id)) return &set->instances[i];
    return NULL;
}

bool ray_tracing_headless_compound_ingestion_prepare(
    const RayTracingAgentRenderRequest* request,
    const RayEvaluatedSceneSnapshot* snapshot,
    RayTracingHeadlessCompoundIngestion* ingestion,
    char* diagnostics,
    size_t diagnostics_size) {
    RayCompoundSceneIngestionFailure failure = RAY_COMPOUND_SCENE_INGESTION_FAILURE_INPUT;
    if (!request || !snapshot || !ingestion) return false;
    ray_tracing_headless_compound_ingestion_init(ingestion);
    if (!request->has_compound_scene_ingestion_path) return true;
    if (!ray_compound_scene_ingestion_file_read(request->compound_scene_ingestion_path,
                                                 &ingestion->file, diagnostics,
                                                 diagnostics_size)) goto fail;
    ingestion->mesh_assets = ray_tracing_runtime_mesh_assets_last();
    if (!ingestion->mesh_assets) goto fail;
    for (size_t i = 0; i < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT; ++i) {
        const RayCompoundSceneRendererBinding* binding =
            &ingestion->file.descriptor.bindings.bindings[i];
        const RayTracingRuntimeMeshAssetInstance* instance =
            find_instance(ingestion->mesh_assets, binding->object_id, binding->mesh_asset_id);
        const CoreMeshAssetRuntimeDocument* document = NULL;
        if (!instance || instance->asset_index < 0 ||
            instance->asset_index >= ingestion->mesh_assets->asset_count) goto fail;
        document = &ingestion->mesh_assets->assets[instance->asset_index].document;
        ingestion->source_positions[i] = calloc(document->vertex_count,
                                                 sizeof(*ingestion->source_positions[i]));
        ingestion->world_positions[i] = calloc(document->vertex_count,
                                                sizeof(*ingestion->world_positions[i]));
        if (!ingestion->source_positions[i] || !ingestion->world_positions[i]) goto fail;
        for (size_t v = 0; v < document->vertex_count; ++v) {
            ingestion->source_positions[i][v].x = document->vertices[v].position.x;
            ingestion->source_positions[i][v].y = document->vertices[v].position.y;
            ingestion->source_positions[i][v].z = document->vertices[v].position.z;
        }
        ingestion->assembly_request.simulated_sources[i] =
            (RayCompoundSceneSourceGeometryView){binding->body_id, binding->object_id,
                binding->mesh_asset_id, binding->source_asset_id, binding->source_sha256,
                ingestion->source_positions[i], document->vertex_count};
        ingestion->assembly_request.simulated_targets[i] =
            (RayCompoundSceneGeometryTarget){ingestion->world_positions[i], document->vertex_count};
    }
    if (!ray_compound_scene_ingestion_resolve_exact(&ingestion->file.descriptor,
                                                     &ingestion->file.handoff,
                                                     &ingestion->file.room, snapshot,
                                                     &ingestion->assembly_request,
                                                     &ingestion->result, &failure)) goto fail;
    ingestion->active = true;
    if (diagnostics && diagnostics_size) snprintf(diagnostics, diagnostics_size, "ok");
    return true;
fail:
    if (diagnostics && diagnostics_size)
        snprintf(diagnostics, diagnostics_size,
                 "compound ingestion resolution failed: %d", (int)failure);
    ray_tracing_headless_compound_ingestion_free(ingestion);
    return false;
}

bool ray_tracing_headless_compound_ingestion_mutate(RuntimeScene3D* scene,
                                                     void* user_data,
                                                     char* diagnostics,
                                                     size_t diagnostics_size) {
    RayTracingHeadlessCompoundIngestion* ingestion = user_data;
    if (!ingestion || !ingestion->active) return false;
    return ray_compound_scene_runtime_apply_exact(scene, ingestion->mesh_assets,
                                                  &ingestion->file.descriptor,
                                                  &ingestion->result, diagnostics,
                                                  diagnostics_size);
}
