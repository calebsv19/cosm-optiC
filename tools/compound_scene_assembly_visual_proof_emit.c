#include "animation/evaluated_scene_snapshot.h"
#include "core_mesh_asset.h"
#include "import/compound_scene_evaluated_scene.h"
#include "render/compound_scene_assembly.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool build_base(RayEvaluatedSceneSnapshot* output) {
    RayEvaluatedObjectTransform transforms[3] = {0};
    RayEvaluatedSceneSnapshotInputs inputs = {0};
    TimelineEvaluationContext context = {0};
    const char* ids[3] = {"sim_body_c2", "set_dressing_static",
                          "sim_body_c1"};
    if (TimelineEvaluationContextBuild(
            (TimelineRate){60u, 1u}, (TimelineRange){0, 720u},
            (TimelineSample){0, 0u, 1u}, &context) != TIMELINE_STATUS_OK)
        return false;
    for (size_t i = 0u; i < 3u; ++i) {
        transforms[i].valid = true;
        snprintf(transforms[i].target_id, sizeof(transforms[i].target_id),
                 "%s", ids[i]);
        transforms[i].source =
            RAY_EVALUATED_OBJECT_TRANSFORM_COMPATIBILITY_MOTION;
        transforms[i].has_position = true;
        transforms[i].has_rotation = true;
        transforms[i].frame = context;
    }
    inputs.source = RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE;
    inputs.playback_mode = RAY_EVALUATED_PLAYBACK_STOP;
    inputs.frame = context;
    inputs.identity.scene_revision = 41u;
    inputs.identity.timeline_revision = 9u;
    inputs.light.valid = true;
    inputs.light.enabled = true;
    snprintf(inputs.light.target_id, sizeof(inputs.light.target_id), "%s",
             "light/key");
    snprintf(inputs.light.runtime_light_id,
             sizeof(inputs.light.runtime_light_id), "%s", "key");
    inputs.light.color = (TimelineVec3){1.0, 1.0, 1.0};
    inputs.light.intensity = 4.0;
    inputs.light.property_provenance.valid = true;
    inputs.camera.valid = true;
    inputs.camera.fov_y_degrees = 52.0;
    inputs.camera.aspect_ratio = 4.0 / 3.0;
    inputs.camera.zoom = 1.0;
    inputs.object_transforms = transforms;
    inputs.object_transform_count = 3u;
    inputs.diagnostics = "S9-F two-body assembly visual proof";
    return RayEvaluatedSceneSnapshotBuild(&inputs, output) ==
        TIMELINE_STATUS_OK;
}

static void bindings(RayCompoundSceneBindingManifest* manifest) {
    snprintf(manifest->bindings[0].object_id,
             sizeof(manifest->bindings[0].object_id), "%s", "sim_body_c2");
    snprintf(manifest->bindings[0].mesh_asset_id,
             sizeof(manifest->bindings[0].mesh_asset_id), "%s",
             "mesh_c2_u_channel");
    snprintf(manifest->bindings[1].object_id,
             sizeof(manifest->bindings[1].object_id), "%s", "sim_body_c1");
    snprintf(manifest->bindings[1].mesh_asset_id,
             sizeof(manifest->bindings[1].mesh_asset_id), "%s",
             "mesh_c1_l_bracket");
}

static void vec(RayCompoundSceneVec3 value) {
    printf("[%.17g,%.17g,%.17g]", value.x, value.y, value.z);
}

int main(int argc, char** argv) {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure import_failure;
    RayCompoundSceneBindingManifest manifest;
    RayCompoundSceneEvaluatedSceneFailure evaluated_failure;
    RayCompoundSceneAssemblyFailure assembly_failure;
    RayEvaluatedSceneSnapshot base = {0};
    RayEvaluatedSceneSnapshot evaluated = {0};
    CoreMeshAssetRuntimeDocument meshes[2];
    RayCompoundSceneVec3* source[2] = {NULL, NULL};
    RayCompoundSceneVec3* world[2] = {NULL, NULL};
    RayCompoundSceneStaticObjectSpec statics[3] = {
        {"set_dressing_floor", "primitive/plane", "mat_floor"},
        {"set_dressing_backdrop", "primitive/backdrop", "mat_backdrop"},
        {"set_dressing_reference_marker", "primitive/marker", "mat_marker"}};
    RayCompoundSceneAssemblyRequest request = {0};
    RayCompoundSceneAssembly assembly = {0};
    int result = 1;
    if (argc != 4) {
        fprintf(stderr, "usage: %s HANDOFF C2_RUNTIME_MESH C1_RUNTIME_MESH\n",
                argv[0]);
        return 2;
    }
    ray_compound_scene_handoff_init(&handoff);
    core_mesh_asset_runtime_document_init(&meshes[0]);
    core_mesh_asset_runtime_document_init(&meshes[1]);
    if (!ray_compound_scene_handoff_read(argv[1], &handoff, &import_failure) ||
        core_mesh_asset_runtime_document_load_file(argv[2], &meshes[0]).code !=
            CORE_OK ||
        core_mesh_asset_runtime_document_load_file(argv[3], &meshes[1]).code !=
            CORE_OK) goto done;
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    bindings(&manifest);
    if (!build_base(&base) ||
        !ray_compound_scene_evaluated_scene_apply_exact(
            &handoff, &manifest, 480u, &base, &evaluated,
            &evaluated_failure)) goto done;
    request.handoff = &handoff;
    request.manifest = &manifest;
    request.snapshot = &evaluated;
    request.static_objects = statics;
    request.static_object_count = 3u;
    for (size_t i = 0u; i < 2u; ++i) {
        source[i] = calloc(meshes[i].vertex_count, sizeof(*source[i]));
        world[i] = calloc(meshes[i].vertex_count, sizeof(*world[i]));
        if (!source[i] || !world[i]) goto done;
        for (size_t j = 0u; j < meshes[i].vertex_count; ++j)
            source[i][j] = (RayCompoundSceneVec3){
                meshes[i].vertices[j].position.x,
                meshes[i].vertices[j].position.y,
                meshes[i].vertices[j].position.z};
        request.simulated_sources[i] = (RayCompoundSceneSourceGeometryView){
            handoff.bindings[i].body_id, manifest.bindings[i].object_id,
            manifest.bindings[i].mesh_asset_id,
            handoff.bindings[i].source_asset_id,
            handoff.bindings[i].source_sha256, source[i],
            meshes[i].vertex_count};
        request.simulated_targets[i] =
            (RayCompoundSceneGeometryTarget){world[i], meshes[i].vertex_count};
    }
    if (!ray_compound_scene_assembly_build_exact(
            &request, &assembly, &assembly_failure)) goto done;
    printf("{\"schema\":\"ray_compound_scene_s9f_visual_proof_v1\","
           "\"assembly_schema\":\"%s\",\"assembly_digest\":\"%016llx\","
           "\"handoff_digest\":\"%016llx\",\"tick\":%llu,\"objects\":[",
           assembly.schema, (unsigned long long)assembly.assembly_digest,
           (unsigned long long)assembly.handoff_digest,
           (unsigned long long)assembly.tick);
    for (size_t i = 0u; i < assembly.object_count; ++i) {
        const RayCompoundSceneObjectRecord* object = &assembly.objects[i];
        printf("%s{\"object_id\":\"%s\",\"membership\":\"%s\","
               "\"geometry_id\":\"%s\",\"material_id\":\"%s\"}",
               i ? "," : "", object->object_id,
               ray_compound_scene_membership_name(object->membership),
               object->geometry_id, object->material_id);
    }
    printf("],\"bodies\":[");
    for (size_t i = 0u; i < 2u; ++i) {
        const RayCompoundSceneObjectRecord* object = &assembly.objects[i];
        printf("%s{\"object_id\":\"%s\",\"source_asset_id\":\"%s\","
               "\"source_sha256\":\"%s\",\"geometry_digest\":\"%016llx\","
               "\"vertex_count\":%zu,\"triangle_count\":%zu,\"bounds_min\":",
               i ? "," : "", object->object_id, object->source_asset_id,
               object->source_sha256,
               (unsigned long long)object->geometry_digest,
               object->vertex_count, meshes[i].triangle_count);
        vec(object->bounds_min);
        printf(",\"bounds_max\":");
        vec(object->bounds_max);
        printf(",\"vertices\":[");
        for (size_t j = 0u; j < object->vertex_count; ++j) {
            if (j) printf(",");
            vec(world[i][j]);
        }
        printf("]}");
    }
    printf("],\"ownership\":{\"collision_proxy_rendered\":false,"
           "\"source_mesh_renderer_owned\":true,"
           "\"static_membership_explicit\":true,"
           "\"default_request_or_worker_integration\":false}}\n");
    result = 0;
done:
    for (size_t i = 0u; i < 2u; ++i) {
        free(world[i]);
        free(source[i]);
        core_mesh_asset_runtime_document_free(&meshes[i]);
    }
    ray_compound_scene_handoff_free(&handoff);
    return result;
}
