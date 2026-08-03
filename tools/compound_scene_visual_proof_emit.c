#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "animation/evaluated_scene_snapshot.h"
#include "core_mesh_asset.h"
#include "import/compound_scene_binding_manifest.h"
#include "import/compound_scene_evaluated_scene.h"
#include "import/compound_scene_handoff_import.h"
#include "render/compound_scene_detached_geometry.h"

static bool build_renderer_snapshot(RayEvaluatedSceneSnapshot* output) {
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
    inputs.light.position = (TimelineVec3){-4.0, -4.0, 8.0};
    inputs.light.color = (TimelineVec3){1.0, 0.95, 0.88};
    inputs.light.intensity = 4.0;
    inputs.light.property_provenance.valid = true;
    inputs.camera.valid = true;
    inputs.camera.position = (TimelineVec3){0.0, -11.0, 6.5};
    inputs.camera.fov_y_degrees = 52.0;
    inputs.camera.aspect_ratio = 4.0 / 3.0;
    inputs.camera.zoom = 1.0;
    inputs.object_transforms = transforms;
    inputs.object_transform_count = 3u;
    inputs.diagnostics = "S9-E renderer-owned visual proof";
    return RayEvaluatedSceneSnapshotBuild(&inputs, output) ==
        TIMELINE_STATUS_OK;
}

static void populate_renderer_bindings(
    RayCompoundSceneBindingManifest* manifest) {
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

static const RayCompoundSceneSourceBinding* find_c2_binding(
    const RayCompoundSceneHandoff* handoff) {
    for (size_t i = 0u; i < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT; ++i) {
        if (!strcmp(handoff->bindings[i].source_asset_id,
                    "c2_u_channel_v1"))
            return &handoff->bindings[i];
    }
    return NULL;
}

static void print_vec3(RayCompoundSceneVec3 v) {
    printf("[%.17g,%.17g,%.17g]", v.x, v.y, v.z);
}

static RayCompoundSceneVec3 source_position(
    const CoreMeshAssetRuntimeDocument* mesh, size_t index) {
    return (RayCompoundSceneVec3){mesh->vertices[index].position.x,
                                  mesh->vertices[index].position.y,
                                  mesh->vertices[index].position.z};
}

static RayCompoundSceneVec3 source_normal_tip(
    const CoreMeshAssetRuntimeDocument* mesh, size_t index,
    RayCompoundSceneVec3 center) {
    return (RayCompoundSceneVec3){
        center.x + mesh->vertices[index].normal.x,
        center.y + mesh->vertices[index].normal.y,
        center.z + mesh->vertices[index].normal.z};
}

static int emit_frame(const RayCompoundSceneHandoff* handoff,
                      const RayCompoundSceneBindingManifest* manifest,
                      const CoreMeshAssetRuntimeDocument* mesh,
                      const RayCompoundSceneSourceBinding* binding,
                      uint64_t tick, bool first) {
    RayEvaluatedSceneSnapshot base = {0};
    RayEvaluatedSceneSnapshot evaluated = {0};
    RayCompoundSceneEvaluatedSceneFailure evaluated_failure;
    RayCompoundSceneDetachedGeometryFailure geometry_failure;
    RayCompoundSceneVec3* source = NULL;
    RayCompoundSceneVec3* world = NULL;
    RayCompoundSceneVec3 center_world[1] = {{0.0, 0.0, 0.0}};
    RayCompoundSceneVec3* normal_tips = NULL;
    RayCompoundSceneVec3* normal_tips_world = NULL;
    RayCompoundSceneDetachedGeometry geometry = {0};
    RayCompoundSceneDetachedGeometry center_geometry = {0};
    RayCompoundSceneDetachedGeometry normal_geometry = {0};
    RayCompoundSceneSourceGeometryView view = {0};
    RayCompoundSceneSourceGeometryView center_view = {0};
    RayCompoundSceneSourceGeometryView normal_view = {0};
    int result = 1;

    source = calloc(mesh->vertex_count, sizeof(*source));
    world = calloc(mesh->vertex_count, sizeof(*world));
    normal_tips = calloc(mesh->vertex_count, sizeof(*normal_tips));
    normal_tips_world = calloc(mesh->vertex_count, sizeof(*normal_tips_world));
    if (!source || !world || !normal_tips || !normal_tips_world ||
        !build_renderer_snapshot(&base) ||
        !ray_compound_scene_evaluated_scene_apply_exact(
            handoff, manifest, tick, &base, &evaluated, &evaluated_failure))
        goto done;
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        source[i] = source_position(mesh, i);
        normal_tips[i] = source_normal_tip(mesh, i, binding->source_center_m);
    }
    view = (RayCompoundSceneSourceGeometryView){
        binding->body_id, "sim_body_c2", "mesh_c2_u_channel",
        binding->source_asset_id, binding->source_sha256, source,
        mesh->vertex_count};
    geometry.world_positions = world;
    geometry.world_position_capacity = mesh->vertex_count;
    if (!ray_compound_scene_detached_geometry_apply_exact(
            handoff, manifest, &evaluated, &view, &geometry,
            &geometry_failure))
        goto done;
    center_view = view;
    center_view.source_positions = &binding->source_center_m;
    center_view.vertex_count = 1u;
    center_geometry.world_positions = center_world;
    center_geometry.world_position_capacity = 1u;
    if (!ray_compound_scene_detached_geometry_apply_exact(
            handoff, manifest, &evaluated, &center_view, &center_geometry,
            &geometry_failure))
        goto done;
    normal_view = view;
    normal_view.source_positions = normal_tips;
    normal_geometry.world_positions = normal_tips_world;
    normal_geometry.world_position_capacity = mesh->vertex_count;
    if (!ray_compound_scene_detached_geometry_apply_exact(
            handoff, manifest, &evaluated, &normal_view, &normal_geometry,
            &geometry_failure))
        goto done;

    printf("%s{\"tick\":%llu,\"bounds_min\":", first ? "" : ",",
           (unsigned long long)tick);
    print_vec3(geometry.bounds_min);
    printf(",\"bounds_max\":");
    print_vec3(geometry.bounds_max);
    printf(",\"center\":");
    print_vec3(center_world[0]);
    printf(",\"vertices\":[");
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        if (i) printf(",");
        print_vec3(world[i]);
    }
    printf("],\"normals\":[");
    for (size_t i = 0u; i < mesh->vertex_count; ++i) {
        RayCompoundSceneVec3 n = {
            normal_tips_world[i].x - center_world[0].x,
            normal_tips_world[i].y - center_world[0].y,
            normal_tips_world[i].z - center_world[0].z};
        const double length = sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (length <= 0.0) goto done;
        n.x /= length;
        n.y /= length;
        n.z /= length;
        if (i) printf(",");
        print_vec3(n);
    }
    printf("]}");
    result = 0;

done:
    free(normal_tips_world);
    free(normal_tips);
    free(world);
    free(source);
    return result;
}

int main(int argc, char** argv) {
    static const uint64_t ticks[] = {0u, 240u, 480u, 720u};
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure import_failure;
    RayCompoundSceneBindingManifest manifest;
    CoreMeshAssetRuntimeDocument mesh;
    CoreResult mesh_result;
    const RayCompoundSceneSourceBinding* binding;
    int result = 1;
    if (argc != 3) {
        fprintf(stderr, "usage: %s <handoff.txt> <source-mesh.runtime.json>\n",
                argv[0]);
        return 2;
    }
    ray_compound_scene_handoff_init(&handoff);
    core_mesh_asset_runtime_document_init(&mesh);
    if (!ray_compound_scene_handoff_read(argv[1], &handoff,
                                         &import_failure)) {
        fprintf(stderr, "handoff import failed: %s\n",
                ray_compound_scene_import_failure_name(import_failure));
        goto done;
    }
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    populate_renderer_bindings(&manifest);
    binding = find_c2_binding(&handoff);
    mesh_result = core_mesh_asset_runtime_document_load_file(argv[2], &mesh);
    if (!binding || mesh_result.code != CORE_OK ||
        strcmp(mesh.contract.asset_id, "mesh_c2_u_channel") ||
        strcmp(mesh.contract.source_asset_id, binding->source_asset_id) ||
        mesh.vertex_count != 48u || mesh.triangle_count != 28u ||
        mesh.vertex_normal_count != mesh.vertex_count) {
        fprintf(stderr, "source mesh provenance or topology mismatch\n");
        goto done;
    }
    printf("{\"schema\":\"ray_compound_scene_s9e_visual_proof_frames_v1\","
           "\"handoff_digest\":\"%016llx\",\"source_asset_id\":\"%s\","
           "\"source_sha256\":\"%s\",\"mesh_asset_id\":\"%s\","
           "\"vertex_count\":%zu,\"triangle_count\":%zu,\"frames\":[",
           (unsigned long long)handoff.handoff_digest,
           binding->source_asset_id, binding->source_sha256,
           mesh.contract.asset_id, mesh.vertex_count, mesh.triangle_count);
    for (size_t i = 0u; i < sizeof(ticks) / sizeof(ticks[0]); ++i) {
        if (emit_frame(&handoff, &manifest, &mesh, binding, ticks[i],
                       i == 0u))
            goto done;
    }
    printf("],\"ownership\":{\"collision_proxy_rendered\":false,"
           "\"source_mesh_renderer_owned\":true,"
           "\"material_camera_light_sampling_final_image_renderer_owned\":true}}\n");
    result = 0;

done:
    core_mesh_asset_runtime_document_free(&mesh);
    ray_compound_scene_handoff_free(&handoff);
    return result;
}
