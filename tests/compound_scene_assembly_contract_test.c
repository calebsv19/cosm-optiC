#include "core_mesh_asset.h"
#include "import/compound_scene_evaluated_scene.h"
#include "render/compound_scene_assembly.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "compound scene assembly contract failed line=%d check=%s\n", \
            __LINE__, #condition); return 1; \
} } while (0)

typedef struct RendererOwnedSentinels {
    unsigned int topology[6];
    char camera_id[32];
    char light_id[32];
    unsigned int samples;
    unsigned char final_image[8];
} RendererOwnedSentinels;

static bool build_base_snapshot(RayEvaluatedSceneSnapshot* output) {
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
    inputs.light.intensity = 2.0;
    inputs.light.property_provenance.valid = true;
    inputs.camera.valid = true;
    inputs.camera.fov_y_degrees = 50.0;
    inputs.camera.aspect_ratio = 4.0 / 3.0;
    inputs.camera.zoom = 1.0;
    inputs.object_transforms = transforms;
    inputs.object_transform_count = 3u;
    inputs.diagnostics = "S9-F typed assembly contract";
    return RayEvaluatedSceneSnapshotBuild(&inputs, output) ==
        TIMELINE_STATUS_OK;
}

static void populate_manifest(RayCompoundSceneBindingManifest* manifest) {
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

static RayCompoundSceneSourceGeometryView source_view(
    const RayCompoundSceneSourceBinding* source,
    const RayCompoundSceneRendererBinding* renderer,
    const CoreMeshAssetRuntimeDocument* mesh,
    RayCompoundSceneVec3* positions) {
    for (size_t i = 0u; i < mesh->vertex_count; ++i)
        positions[i] = (RayCompoundSceneVec3){
            mesh->vertices[i].position.x, mesh->vertices[i].position.y,
            mesh->vertices[i].position.z};
    return (RayCompoundSceneSourceGeometryView){
        source->body_id, renderer->object_id, renderer->mesh_asset_id,
        source->source_asset_id, source->source_sha256, positions,
        mesh->vertex_count};
}

static int run_contract(const char* handoff_path, const char* c2_path,
                        const char* c1_path) {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure import_failure;
    RayCompoundSceneBindingManifest manifest;
    RayCompoundSceneEvaluatedSceneFailure evaluated_failure;
    RayCompoundSceneAssemblyFailure failure;
    RayEvaluatedSceneSnapshot base = {0};
    RayEvaluatedSceneSnapshot evaluated = {0};
    CoreMeshAssetRuntimeDocument meshes[2];
    RayCompoundSceneVec3 sources[2][64] = {{{0}}};
    RayCompoundSceneVec3 outputs[2][64];
    RayCompoundSceneVec3 output_sentinel[2][64];
    RayCompoundSceneStaticObjectSpec statics[3] = {
        {"room_floor", "primitive/plane", "material/matte_floor"},
        {"room_backdrop", "primitive/backdrop", "material/matte_wall"},
        {"reference_marker", "primitive/marker", "material/reference"}};
    RayCompoundSceneAssemblyRequest request = {0};
    RayCompoundSceneAssembly assembly = {0};
    RayCompoundSceneAssembly repeated = {0};
    const RendererOwnedSentinels renderer = {
        {0u, 1u, 2u, 2u, 3u, 0u}, "camera/hero", "light/key", 128u,
        {0x52u, 0x41u, 0x59u, 0x49u, 0x4du, 0x41u, 0x47u, 0x45u}};
    const RendererOwnedSentinels renderer_original = renderer;
    memset(outputs, 0xa5, sizeof(outputs));
    memcpy(output_sentinel, outputs, sizeof(outputs));
    ray_compound_scene_handoff_init(&handoff);
    core_mesh_asset_runtime_document_init(&meshes[0]);
    core_mesh_asset_runtime_document_init(&meshes[1]);
    CHECK(ray_compound_scene_handoff_read(
        handoff_path, &handoff, &import_failure));
    CHECK(core_mesh_asset_runtime_document_load_file(c2_path, &meshes[0]).code ==
          CORE_OK);
    CHECK(core_mesh_asset_runtime_document_load_file(c1_path, &meshes[1]).code ==
          CORE_OK);
    CHECK(meshes[0].vertex_count == 48u && meshes[0].triangle_count == 28u);
    CHECK(meshes[1].vertex_count == 36u && meshes[1].triangle_count == 20u);
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    populate_manifest(&manifest);
    CHECK(build_base_snapshot(&base));
    CHECK(ray_compound_scene_evaluated_scene_apply_exact(
        &handoff, &manifest, 480u, &base, &evaluated, &evaluated_failure));
    request.handoff = &handoff;
    request.manifest = &manifest;
    request.snapshot = &evaluated;
    for (size_t i = 0u; i < 2u; ++i) {
        request.simulated_sources[i] = source_view(
            &handoff.bindings[i], &manifest.bindings[i], &meshes[i],
            sources[i]);
        request.simulated_targets[i] =
            (RayCompoundSceneGeometryTarget){outputs[i], 64u};
    }
    request.static_objects = statics;
    request.static_object_count = 3u;
    CHECK(ray_compound_scene_assembly_build_exact(
        &request, &assembly, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_NONE);
    CHECK(ray_compound_scene_assembly_validate(&assembly));
    CHECK(assembly.tick == 480u && assembly.simulated_count == 2u &&
          assembly.static_count == 3u && assembly.object_count == 5u);
    CHECK(assembly.objects[0].membership ==
          RAY_COMPOUND_SCENE_MEMBERSHIP_SIMULATED);
    CHECK(assembly.objects[1].membership ==
          RAY_COMPOUND_SCENE_MEMBERSHIP_SIMULATED);
    CHECK(!strcmp(assembly.objects[0].source_asset_id, "c2_u_channel_v1"));
    CHECK(!strcmp(assembly.objects[1].source_asset_id, "c1_l_bracket_v1"));
    CHECK(assembly.objects[0].vertex_count == 48u &&
          assembly.objects[1].vertex_count == 36u);
    for (size_t i = 2u; i < 5u; ++i) {
        CHECK(assembly.objects[i].membership ==
              RAY_COMPOUND_SCENE_MEMBERSHIP_STATIC);
        CHECK(assembly.objects[i].body_id == -1);
        CHECK(assembly.objects[i].source_asset_id[0] == '\0');
    }
    CHECK(!strcmp(assembly.objects[2].material_id, "material/matte_floor"));
    CHECK(memcmp(outputs, output_sentinel, sizeof(outputs)) != 0);
    CHECK(memcmp(&renderer, &renderer_original, sizeof(renderer)) == 0);

    CHECK(ray_compound_scene_assembly_build_exact(
        &request, &repeated, &failure));
    CHECK(repeated.assembly_digest == assembly.assembly_digest);
    CHECK(!memcmp(repeated.simulated_geometry[0].world_positions,
                  assembly.simulated_geometry[0].world_positions,
                  48u * sizeof(RayCompoundSceneVec3)));

    RayCompoundSceneAssembly result_sentinel = assembly;
    memcpy(output_sentinel, outputs, sizeof(outputs));
    request.simulated_targets[1].world_position_capacity = 35u;
    CHECK(!ray_compound_scene_assembly_build_exact(
        &request, &assembly, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_CAPACITY);
    CHECK(!memcmp(&assembly, &result_sentinel, sizeof(assembly)));
    CHECK(!memcmp(outputs, output_sentinel, sizeof(outputs)));
    request.simulated_targets[1].world_position_capacity = 64u;

    statics[0].object_id[0] = '\0';
    CHECK(!ray_compound_scene_assembly_build_exact(
        &request, &assembly, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_MEMBERSHIP);
    CHECK(!memcmp(&assembly, &result_sentinel, sizeof(assembly)));
    CHECK(!memcmp(outputs, output_sentinel, sizeof(outputs)));
    snprintf(statics[0].object_id, sizeof(statics[0].object_id), "%s",
             "sim_body_c2");
    CHECK(!ray_compound_scene_assembly_build_exact(
        &request, &assembly, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_MEMBERSHIP);

    statics[0] = (RayCompoundSceneStaticObjectSpec){
        "room_floor", "primitive/plane", "material/matte_floor"};
    snprintf(statics[1].object_id, sizeof(statics[1].object_id), "%s",
             "room_floor");
    CHECK(!ray_compound_scene_assembly_build_exact(
        &request, &assembly, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_MEMBERSHIP);
    CHECK(!memcmp(outputs, output_sentinel, sizeof(outputs)));
    snprintf(statics[1].object_id, sizeof(statics[1].object_id), "%s",
             "room_backdrop");
    statics[2].material_id[0] = '\0';
    CHECK(!ray_compound_scene_assembly_build_exact(
        &request, &assembly, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_MEMBERSHIP);
    CHECK(!memcmp(outputs, output_sentinel, sizeof(outputs)));
    snprintf(statics[2].material_id, sizeof(statics[2].material_id), "%s",
             "material/reference");

    RayCompoundSceneSourceGeometryView valid_second =
        request.simulated_sources[1];
    request.simulated_sources[1] = request.simulated_sources[0];
    CHECK(!ray_compound_scene_assembly_build_exact(
        &request, &assembly, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_MEMBERSHIP);
    CHECK(!memcmp(outputs, output_sentinel, sizeof(outputs)));
    request.simulated_sources[1] = valid_second;
    request.simulated_sources[1].source_sha256 = "wrong-source-sha";
    CHECK(!ray_compound_scene_assembly_build_exact(
        &request, &assembly, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_ASSEMBLY_FAILURE_GEOMETRY);
    CHECK(!memcmp(&assembly, &result_sentinel, sizeof(assembly)));
    CHECK(!memcmp(outputs, output_sentinel, sizeof(outputs)));

    printf("compound scene assembly contract passed digest=%016llx "
           "objects=5 simulated=2 static=3 vertices=48+36 triangles=28+20\n",
           (unsigned long long)result_sentinel.assembly_digest);
    core_mesh_asset_runtime_document_free(&meshes[0]);
    core_mesh_asset_runtime_document_free(&meshes[1]);
    ray_compound_scene_handoff_free(&handoff);
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s HANDOFF C2_RUNTIME_MESH C1_RUNTIME_MESH\n",
                argv[0]);
        return 2;
    }
    return run_contract(argv[1], argv[2], argv[3]);
}
