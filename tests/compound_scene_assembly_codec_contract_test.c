#include "core_mesh_asset.h"
#include "import/compound_scene_assembly_codec.h"
#include "import/compound_scene_evaluated_scene.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "assembly codec contract failed line=%d check=%s\n", __LINE__, #c); return 1; } } while (0)

static bool base_snapshot(RayEvaluatedSceneSnapshot* output) {
    RayEvaluatedObjectTransform transforms[3] = {0};
    RayEvaluatedSceneSnapshotInputs inputs = {0};
    TimelineEvaluationContext context = {0};
    const char* ids[3] = {"sim_body_c2", "set_dressing_static", "sim_body_c1"};
    if (TimelineEvaluationContextBuild((TimelineRate){60u, 1u},
            (TimelineRange){0, 720u}, (TimelineSample){0, 0u, 1u},
            &context) != TIMELINE_STATUS_OK) return false;
    for (size_t i = 0; i < 3u; ++i) {
        transforms[i].valid = true;
        snprintf(transforms[i].target_id, sizeof(transforms[i].target_id), "%s", ids[i]);
        transforms[i].source = RAY_EVALUATED_OBJECT_TRANSFORM_COMPATIBILITY_MOTION;
        transforms[i].has_position = true; transforms[i].has_rotation = true;
        transforms[i].frame = context;
    }
    inputs.source = RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE;
    inputs.playback_mode = RAY_EVALUATED_PLAYBACK_STOP;
    inputs.frame = context;
    inputs.identity.scene_revision = 41u; inputs.identity.timeline_revision = 9u;
    inputs.light.valid = true; inputs.light.enabled = true;
    snprintf(inputs.light.target_id, sizeof(inputs.light.target_id), "%s", "light/key");
    snprintf(inputs.light.runtime_light_id, sizeof(inputs.light.runtime_light_id), "%s", "key");
    inputs.light.color = (TimelineVec3){1.0, 1.0, 1.0}; inputs.light.intensity = 2.0;
    inputs.light.property_provenance.valid = true;
    inputs.camera.valid = true; inputs.camera.fov_y_degrees = 50.0;
    inputs.camera.aspect_ratio = 4.0 / 3.0; inputs.camera.zoom = 1.0;
    inputs.object_transforms = transforms; inputs.object_transform_count = 3u;
    inputs.diagnostics = "S9-G archive codec contract";
    return RayEvaluatedSceneSnapshotBuild(&inputs, output) == TIMELINE_STATUS_OK;
}

static RayCompoundSceneSourceGeometryView source_view(
    const RayCompoundSceneSourceBinding* source,
    const RayCompoundSceneRendererBinding* renderer,
    const CoreMeshAssetRuntimeDocument* mesh, RayCompoundSceneVec3* positions) {
    for (size_t i = 0; i < mesh->vertex_count; ++i)
        positions[i] = (RayCompoundSceneVec3){mesh->vertices[i].position.x,
            mesh->vertices[i].position.y, mesh->vertices[i].position.z};
    return (RayCompoundSceneSourceGeometryView){source->body_id,
        renderer->object_id, renderer->mesh_asset_id, source->source_asset_id,
        source->source_sha256, positions, mesh->vertex_count};
}

static int run(const char* packet, const char* c2, const char* c1,
               const char* archive_path) {
    const uint64_t ticks[4] = {0u, 240u, 480u, 720u};
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure import_failure;
    RayCompoundSceneBindingManifest manifest;
    CoreMeshAssetRuntimeDocument meshes[2];
    RayEvaluatedSceneSnapshot base = {0};
    RayEvaluatedSceneSnapshot evaluated[4] = {0};
    RayCompoundSceneAssembly assemblies[4] = {0};
    RayCompoundSceneVec3 sources[2][64] = {{{0}}};
    RayCompoundSceneVec3 outputs[4][2][64] = {{{{0}}}};
    RayCompoundSceneStaticObjectSpec static_specs[3] = {
        {"set_dressing_floor", "primitive/plane", "mat_floor"},
        {"set_dressing_backdrop", "primitive/backdrop", "mat_backdrop"},
        {"set_dressing_reference_marker", "primitive/marker", "mat_marker"}};
    RayCompoundSceneExternalAssetReference assets[2] = {0};
    RayCompoundSceneStaticSurfaceRecord statics[3] = {0};
    RayCompoundSceneAssemblyArchive archive = {0}, parsed = {0}, readback = {0};
    RayCompoundSceneAssemblyCodecFailure codec_failure;
    RayCompoundSceneAssemblyFrameRecord replay = {0};
    char text[RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY];
    char tampered[RAY_COMPOUND_SCENE_ASSEMBLY_ARCHIVE_TEXT_CAPACITY];
    ray_compound_scene_handoff_init(&handoff);
    core_mesh_asset_runtime_document_init(&meshes[0]);
    core_mesh_asset_runtime_document_init(&meshes[1]);
    CHECK(ray_compound_scene_handoff_read(packet, &handoff, &import_failure));
    CHECK(core_mesh_asset_runtime_document_load_file(c2, &meshes[0]).code == CORE_OK);
    CHECK(core_mesh_asset_runtime_document_load_file(c1, &meshes[1]).code == CORE_OK);
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    snprintf(manifest.bindings[0].object_id, sizeof(manifest.bindings[0].object_id), "%s", "sim_body_c2");
    snprintf(manifest.bindings[0].mesh_asset_id, sizeof(manifest.bindings[0].mesh_asset_id), "%s", "mesh_c2_u_channel");
    snprintf(manifest.bindings[1].object_id, sizeof(manifest.bindings[1].object_id), "%s", "sim_body_c1");
    snprintf(manifest.bindings[1].mesh_asset_id, sizeof(manifest.bindings[1].mesh_asset_id), "%s", "mesh_c1_l_bracket");
    CHECK(base_snapshot(&base));
    for (size_t b = 0; b < 2u; ++b)
        source_view(&handoff.bindings[b], &manifest.bindings[b], &meshes[b], sources[b]);
    for (size_t f = 0; f < 4u; ++f) {
        RayCompoundSceneEvaluatedSceneFailure evaluated_failure;
        RayCompoundSceneAssemblyFailure assembly_failure;
        RayCompoundSceneAssemblyRequest request = {0};
        CHECK(ray_compound_scene_evaluated_scene_apply_exact(&handoff, &manifest,
            ticks[f], &base, &evaluated[f], &evaluated_failure));
        request.handoff = &handoff; request.manifest = &manifest;
        request.snapshot = &evaluated[f]; request.static_objects = static_specs;
        request.static_object_count = 3u;
        for (size_t b = 0; b < 2u; ++b) {
            request.simulated_sources[b] = source_view(&handoff.bindings[b],
                &manifest.bindings[b], &meshes[b], sources[b]);
            request.simulated_targets[b] = (RayCompoundSceneGeometryTarget){
                outputs[f][b], 64u};
        }
        CHECK(ray_compound_scene_assembly_build_exact(&request, &assemblies[f],
            &assembly_failure));
    }
    for (size_t b = 0; b < 2u; ++b) {
        assets[b].body_id = handoff.bindings[b].body_id;
        snprintf(assets[b].object_id, sizeof(assets[b].object_id), "%s", manifest.bindings[b].object_id);
        snprintf(assets[b].mesh_asset_id, sizeof(assets[b].mesh_asset_id), "%s", manifest.bindings[b].mesh_asset_id);
        snprintf(assets[b].runtime_path, sizeof(assets[b].runtime_path),
            "assets/mesh_assets/%s.runtime.json", manifest.bindings[b].mesh_asset_id);
        snprintf(assets[b].runtime_sha256, sizeof(assets[b].runtime_sha256), "%s",
            b ? "388cefb45dc3efe12f905c39945cea96de0a987441104dae3ac82d20487e8f43" :
                "1cfde852d4d594303aa692e35122bedb52ebccd5d1dc79f448e16fa0f287b0e8");
        snprintf(assets[b].source_asset_id, sizeof(assets[b].source_asset_id), "%s", handoff.bindings[b].source_asset_id);
        snprintf(assets[b].source_sha256, sizeof(assets[b].source_sha256), "%s", handoff.bindings[b].source_sha256);
        assets[b].source_binding_digest = handoff.bindings[b].binding_digest;
    }
    const double floor_z = ray_compound_scene_assembly_clearance_floor_z(assemblies, 4u, 0.25);
    CHECK(isfinite(floor_z));
    const char* ids[3] = {"set_dressing_floor", "set_dressing_backdrop", "set_dressing_reference_marker"};
    const char* geometries[3] = {"primitive/plane", "primitive/backdrop", "primitive/marker"};
    const char* materials[3] = {"mat_floor", "mat_backdrop", "mat_marker"};
    for (size_t s = 0; s < 3u; ++s) {
        snprintf(statics[s].object_id, sizeof(statics[s].object_id), "%s", ids[s]);
        snprintf(statics[s].geometry_id, sizeof(statics[s].geometry_id), "%s", geometries[s]);
        snprintf(statics[s].material_id, sizeof(statics[s].material_id), "%s", materials[s]);
        statics[s].authority = RAY_COMPOUND_SCENE_STATIC_AUTHORITY_RENDERER_SET_DRESSING;
        statics[s].half_extent_u_m = 10.0; statics[s].half_extent_v_m = 10.0;
    }
    statics[0].origin_m.z = floor_z; statics[0].normal.z = 1.0;
    statics[1].origin_m.y = 20.0; statics[1].normal.y = -1.0;
    statics[2].origin_m.x = -20.0; statics[2].normal.x = 1.0;
    CHECK(ray_compound_scene_assembly_archive_build(&handoff, assets, statics,
        3u, assemblies, 4u, &archive, &codec_failure));
    CHECK(archive.statics[0].minimum_clearance_m >= 0.249999999);
    CHECK(!archive.statics[0].collision_surface_digest);
    CHECK(ray_compound_scene_assembly_archive_format(&archive, text, sizeof(text)));
    CHECK(ray_compound_scene_assembly_archive_parse(text, &parsed, &codec_failure));
    CHECK(parsed.archive_digest == archive.archive_digest);
    CHECK(ray_compound_scene_assembly_archive_write(&archive, archive_path));
    CHECK(ray_compound_scene_assembly_archive_read(archive_path, &readback, &codec_failure));
    CHECK(readback.archive_digest == archive.archive_digest);
    CHECK(ray_compound_scene_assembly_archive_replay_exact(&readback, 480u, &replay));
    CHECK(replay.assembly_digest == assemblies[2].assembly_digest);
    CHECK(!ray_compound_scene_assembly_archive_replay_exact(&readback, 481u, &replay));
    snprintf(tampered, sizeof(tampered), "%s", text);
    char* floor = strstr(tampered, "renderer_set_dressing");
    CHECK(floor); floor[0] = 'R';
    CHECK(!ray_compound_scene_assembly_archive_parse(tampered, &parsed, &codec_failure));
    statics[0].origin_m.z = floor_z + 0.5;
    CHECK(!ray_compound_scene_assembly_archive_build(&handoff, assets, statics,
        3u, assemblies, 4u, &parsed, &codec_failure));
    printf("compound scene assembly codec contract passed frames=4 digest=%016llx floor_z=%a clearance=%a collision_surfaces=0\n",
        (unsigned long long)archive.archive_digest, floor_z,
        archive.statics[0].minimum_clearance_m);
    core_mesh_asset_runtime_document_free(&meshes[0]);
    core_mesh_asset_runtime_document_free(&meshes[1]);
    ray_compound_scene_handoff_free(&handoff);
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s HANDOFF C2_RUNTIME_MESH C1_RUNTIME_MESH ARCHIVE\n", argv[0]);
        return 2;
    }
    return run(argv[1], argv[2], argv[3], argv[4]);
}
