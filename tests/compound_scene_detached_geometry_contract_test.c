#include "render/compound_scene_detached_geometry.h"
#include "import/compound_scene_evaluated_scene.h"

#include "animation/timeline_property_registry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "compound detached geometry contract failed " \
        "line=%d check=%s\n", __LINE__, #condition); return 1; \
} } while (0)

typedef struct RendererOwnedSentinels {
    unsigned int triangles[6];
    char material_id[32];
    double camera[6];
    double light[6];
    unsigned int samples_per_pixel;
    unsigned char final_image_marker[8];
} RendererOwnedSentinels;

static bool close_value(double a, double b) {
    return fabs(a - b) <= 1e-11;
}

static bool close_vec(RayCompoundSceneVec3 a, RayCompoundSceneVec3 b) {
    return close_value(a.x, b.x) && close_value(a.y, b.y) &&
        close_value(a.z, b.z);
}

static RayCompoundSceneVec3 add(RayCompoundSceneVec3 a,
                                RayCompoundSceneVec3 b) {
    return (RayCompoundSceneVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static RayCompoundSceneVec3 independent_rotate(
    RayEvaluatedQuaternion q, RayCompoundSceneVec3 v) {
    const double xx = q.x * q.x;
    const double yy = q.y * q.y;
    const double zz = q.z * q.z;
    const double xy = q.x * q.y;
    const double xz = q.x * q.z;
    const double yz = q.y * q.z;
    const double wx = q.w * q.x;
    const double wy = q.w * q.y;
    const double wz = q.w * q.z;
    return (RayCompoundSceneVec3){
        (1.0 - 2.0 * (yy + zz)) * v.x + 2.0 * (xy - wz) * v.y +
            2.0 * (xz + wy) * v.z,
        2.0 * (xy + wz) * v.x + (1.0 - 2.0 * (xx + zz)) * v.y +
            2.0 * (yz - wx) * v.z,
        2.0 * (xz - wy) * v.x + 2.0 * (yz + wx) * v.y +
            (1.0 - 2.0 * (xx + yy)) * v.z};
}

static RayCompoundSceneVec3 independent_expected(
    const RayCompoundSceneSourceBinding* binding,
    const RayEvaluatedObjectTransform* transform,
    RayCompoundSceneVec3 source) {
    const RayCompoundSceneVec3 centered = {
        source.x - binding->source_center_m.x,
        source.y - binding->source_center_m.y,
        source.z - binding->source_center_m.z};
    const RayCompoundSceneVec3 principal = {
        binding->principal_to_source.m[0][0] * centered.x +
            binding->principal_to_source.m[1][0] * centered.y +
            binding->principal_to_source.m[2][0] * centered.z,
        binding->principal_to_source.m[0][1] * centered.x +
            binding->principal_to_source.m[1][1] * centered.y +
            binding->principal_to_source.m[2][1] * centered.z,
        binding->principal_to_source.m[0][2] * centered.x +
            binding->principal_to_source.m[1][2] * centered.y +
            binding->principal_to_source.m[2][2] * centered.z};
    return add((RayCompoundSceneVec3){transform->position.x,
                                       transform->position.y,
                                       transform->position.z},
               independent_rotate(transform->orientation_quaternion,
                                  principal));
}

static TimelineEvaluationContext test_context(void) {
    TimelineEvaluationContext context = {0};
    (void)TimelineEvaluationContextBuild(
        (TimelineRate){24u, 1u}, (TimelineRange){0, 100u},
        (TimelineSample){12, 0u, 1u}, &context);
    return context;
}

static bool build_base_snapshot(RayEvaluatedSceneSnapshot* output) {
    RayEvaluatedObjectTransform transforms[3] = {0};
    RayEvaluatedSceneSnapshotInputs inputs = {0};
    const TimelineEvaluationContext context = test_context();
    const char* ids[3] = {"sim_body_c2", "set_dressing_static",
                          "sim_body_c1"};
    for (size_t i = 0u; i < 3u; ++i) {
        transforms[i].valid = true;
        snprintf(transforms[i].target_id, sizeof(transforms[i].target_id),
                 "%s", ids[i]);
        transforms[i].source =
            RAY_EVALUATED_OBJECT_TRANSFORM_COMPATIBILITY_MOTION;
        transforms[i].has_position = true;
        transforms[i].has_rotation = true;
        transforms[i].position = (TimelineVec3){(double)i, 2.0, 3.0};
        transforms[i].rotation_radians = (TimelineVec3){0.0, 0.0, 0.0};
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
    inputs.light.position = (TimelineVec3){4.0, 5.0, 6.0};
    inputs.light.color = (TimelineVec3){0.9, 0.8, 0.7};
    inputs.light.intensity = 3.0;
    inputs.light.property_provenance.valid = true;
    inputs.camera.valid = true;
    inputs.camera.position = (TimelineVec3){7.0, 8.0, 9.0};
    inputs.camera.fov_y_degrees = 55.0;
    inputs.camera.aspect_ratio = 16.0 / 9.0;
    inputs.camera.zoom = 1.2;
    inputs.object_transforms = transforms;
    inputs.object_transform_count = 3u;
    inputs.diagnostics = "renderer-owned geometry base";
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

static const RayEvaluatedObjectTransform* find_transform(
    const RayEvaluatedSceneSnapshot* snapshot, const char* object_id) {
    for (size_t i = 0u; i < snapshot->object_transform_count; ++i) {
        if (!strcmp(snapshot->object_transforms[i].target_id, object_id))
            return &snapshot->object_transforms[i];
    }
    return NULL;
}

static RayCompoundSceneSourceGeometryView source_view(
    const RayCompoundSceneSourceBinding* source_binding,
    const RayCompoundSceneRendererBinding* renderer_binding,
    const RayCompoundSceneVec3* positions, size_t count) {
    return (RayCompoundSceneSourceGeometryView){
        source_binding->body_id,
        renderer_binding->object_id,
        renderer_binding->mesh_asset_id,
        source_binding->source_asset_id,
        source_binding->source_sha256,
        positions,
        count};
}

static int check_exact_geometry(const char* fixture_path) {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure import_failure;
    RayCompoundSceneBindingManifest manifest;
    RayCompoundSceneEvaluatedSceneFailure evaluated_failure;
    RayCompoundSceneDetachedGeometryFailure geometry_failure;
    RayEvaluatedSceneSnapshot base = {0};
    RayEvaluatedSceneSnapshot evaluated = {0};
    RayEvaluatedSceneSnapshot evaluated_original = {0};
    const RendererOwnedSentinels renderer = {
        {0u, 1u, 2u, 2u, 3u, 0u}, "material/brushed_copper",
        {7.0, 8.0, 9.0, 0.0, 0.0, 55.0},
        {4.0, 5.0, 6.0, 0.9, 0.8, 0.7}, 128u,
        {0x52u, 0x41u, 0x59u, 0x49u, 0x4du, 0x41u, 0x47u, 0x45u}};
    const RendererOwnedSentinels renderer_original = renderer;
    ray_compound_scene_handoff_init(&handoff);
    CHECK(ray_compound_scene_handoff_read(
        fixture_path, &handoff, &import_failure));
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    populate_renderer_bindings(&manifest);
    CHECK(build_base_snapshot(&base));
    CHECK(ray_compound_scene_evaluated_scene_apply_exact(
        &handoff, &manifest, 240u, &base, &evaluated, &evaluated_failure));
    evaluated_original = evaluated;

    for (size_t binding_index = 0u; binding_index < manifest.binding_count;
         ++binding_index) {
        const RayCompoundSceneSourceBinding* binding =
            &handoff.bindings[binding_index];
        const RayCompoundSceneRendererBinding* renderer_binding =
            &manifest.bindings[binding_index];
        const RayEvaluatedObjectTransform* transform =
            find_transform(&evaluated, renderer_binding->object_id);
        RayCompoundSceneVec3 source_positions[4] = {
            binding->source_center_m,
            add(binding->source_center_m,
                (RayCompoundSceneVec3){0.70, -0.15, 0.20}),
            add(binding->source_center_m,
                (RayCompoundSceneVec3){-0.25, 0.55, 0.10}),
            add(binding->source_center_m,
                (RayCompoundSceneVec3){0.05, -0.20, 0.80})};
        const RayCompoundSceneVec3 source_original[4] = {
            source_positions[0], source_positions[1],
            source_positions[2], source_positions[3]};
        RayCompoundSceneVec3 world_positions[4] = {{-99.0, -99.0, -99.0}};
        RayCompoundSceneDetachedGeometry output = {0};
        RayCompoundSceneSourceGeometryView view = source_view(
            binding, renderer_binding, source_positions, 4u);
        output.world_positions = world_positions;
        output.world_position_capacity = 4u;
        CHECK(ray_compound_scene_detached_geometry_apply_exact(
            &handoff, &manifest, &evaluated, &view, &output,
            &geometry_failure));
        CHECK(geometry_failure ==
              RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_NONE);
        CHECK(output.valid && output.body_id == binding->body_id);
        CHECK(!strcmp(output.object_id, renderer_binding->object_id));
        CHECK(!strcmp(output.mesh_asset_id, renderer_binding->mesh_asset_id));
        CHECK(!strcmp(output.source_asset_id, binding->source_asset_id));
        CHECK(!strcmp(output.source_sha256, binding->source_sha256));
        CHECK(output.handoff_digest == handoff.handoff_digest);
        CHECK(output.source_binding_digest == binding->binding_digest);
        CHECK(output.source_tick == 240u && output.vertex_count == 4u);
        CHECK(close_vec(world_positions[0],
                        (RayCompoundSceneVec3){transform->position.x,
                                               transform->position.y,
                                               transform->position.z}));
        for (size_t i = 0u; i < 4u; ++i) {
            CHECK(close_vec(world_positions[i],
                            independent_expected(binding, transform,
                                                 source_positions[i])));
        }
        CHECK(memcmp(source_positions, source_original,
                     sizeof(source_positions)) == 0);
        CHECK(output.bounds_min.x <= output.bounds_max.x &&
              output.bounds_min.y <= output.bounds_max.y &&
              output.bounds_min.z <= output.bounds_max.z);
    }
    CHECK(memcmp(&evaluated, &evaluated_original, sizeof(evaluated)) == 0);
    CHECK(memcmp(&renderer, &renderer_original, sizeof(renderer)) == 0);
    ray_compound_scene_handoff_free(&handoff);
    return 0;
}

static int check_transactional_rejections(const char* fixture_path) {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure import_failure;
    RayCompoundSceneBindingManifest manifest;
    RayCompoundSceneEvaluatedSceneFailure evaluated_failure;
    RayCompoundSceneDetachedGeometryFailure failure;
    RayEvaluatedSceneSnapshot base = {0};
    RayEvaluatedSceneSnapshot evaluated = {0};
    RayCompoundSceneVec3 source_positions[2];
    RayCompoundSceneVec3 world_positions[2] = {
        {91.0, 92.0, 93.0}, {94.0, 95.0, 96.0}};
    RayCompoundSceneVec3 world_sentinel[2];
    RayCompoundSceneDetachedGeometry output = {0};
    RayCompoundSceneDetachedGeometry output_sentinel;
    ray_compound_scene_handoff_init(&handoff);
    CHECK(ray_compound_scene_handoff_read(
        fixture_path, &handoff, &import_failure));
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    populate_renderer_bindings(&manifest);
    CHECK(build_base_snapshot(&base));
    CHECK(ray_compound_scene_evaluated_scene_apply_exact(
        &handoff, &manifest, 240u, &base, &evaluated, &evaluated_failure));
    source_positions[0] = handoff.bindings[0].source_center_m;
    source_positions[1] = add(source_positions[0],
                              (RayCompoundSceneVec3){1.0, 0.0, 0.0});
    RayCompoundSceneSourceGeometryView view = source_view(
        &handoff.bindings[0], &manifest.bindings[0], source_positions, 2u);
    output.valid = true;
    output.body_id = -77;
    output.world_positions = world_positions;
    output.world_position_capacity = 2u;
    output_sentinel = output;
    memcpy(world_sentinel, world_positions, sizeof(world_positions));

    RayCompoundSceneSourceGeometryView wrong_mesh = view;
    wrong_mesh.mesh_asset_id = "mesh/not_bound";
    CHECK(!ray_compound_scene_detached_geometry_apply_exact(
        &handoff, &manifest, &evaluated, &wrong_mesh, &output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_SOURCE);
    CHECK(memcmp(&output, &output_sentinel, sizeof(output)) == 0);
    CHECK(memcmp(world_positions, world_sentinel, sizeof(world_positions)) == 0);

    RayCompoundSceneSourceGeometryView wrong_sha = view;
    wrong_sha.source_sha256 =
        "0000000000000000000000000000000000000000000000000000000000000000";
    CHECK(!ray_compound_scene_detached_geometry_apply_exact(
        &handoff, &manifest, &evaluated, &wrong_sha, &output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_SOURCE);
    CHECK(memcmp(&output, &output_sentinel, sizeof(output)) == 0);
    CHECK(memcmp(world_positions, world_sentinel, sizeof(world_positions)) == 0);

    RayCompoundSceneDetachedGeometry short_output = output;
    short_output.world_position_capacity = 1u;
    const RayCompoundSceneDetachedGeometry short_sentinel = short_output;
    CHECK(!ray_compound_scene_detached_geometry_apply_exact(
        &handoff, &manifest, &evaluated, &view, &short_output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_CAPACITY);
    CHECK(memcmp(&short_output, &short_sentinel, sizeof(short_output)) == 0);
    CHECK(memcmp(world_positions, world_sentinel, sizeof(world_positions)) == 0);

    RayEvaluatedSceneSnapshot wrong_provenance = evaluated;
    wrong_provenance.object_transforms[0].source_binding_digest ^= UINT64_C(1);
    CHECK(!ray_compound_scene_detached_geometry_apply_exact(
        &handoff, &manifest, &wrong_provenance, &view, &output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_TARGET);
    CHECK(memcmp(&output, &output_sentinel, sizeof(output)) == 0);
    CHECK(memcmp(world_positions, world_sentinel, sizeof(world_positions)) == 0);

    RayCompoundSceneVec3 invalid_positions[2] = {
        source_positions[0], {NAN, 0.0, 0.0}};
    RayCompoundSceneSourceGeometryView invalid_geometry = view;
    invalid_geometry.source_positions = invalid_positions;
    CHECK(!ray_compound_scene_detached_geometry_apply_exact(
        &handoff, &manifest, &evaluated, &invalid_geometry, &output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_DETACHED_GEOMETRY_FAILURE_GEOMETRY);
    CHECK(memcmp(&output, &output_sentinel, sizeof(output)) == 0);
    CHECK(memcmp(world_positions, world_sentinel, sizeof(world_positions)) == 0);
    CHECK(!strcmp(ray_compound_scene_detached_geometry_failure_name(failure),
                  "geometry"));
    ray_compound_scene_handoff_free(&handoff);
    return 0;
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    CHECK(check_exact_geometry(argv[1]) == 0);
    CHECK(check_transactional_rejections(argv[1]) == 0);
    puts("compound detached geometry contract: PASS tick=240 bindings=2 "
         "source_to_principal=exact quaternion=exact positions=detached "
         "topology_material_camera_light_sampling_final_image=renderer_owned "
         "runtime_scene_mutated=0 acceleration_rebuilt=0 image_rendered=0");
    return 0;
}
