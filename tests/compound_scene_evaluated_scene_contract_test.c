#include "import/compound_scene_evaluated_scene.h"

#include "animation/timeline_property_registry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "compound evaluated scene contract failed " \
        "line=%d check=%s\n", __LINE__, #condition); return 1; \
} } while (0)

typedef struct Matrix3 {
    double m[3][3];
} Matrix3;

static bool close_value(double a, double b) {
    return fabs(a - b) <= 1e-12;
}

static Matrix3 quaternion_matrix(RayCompoundSceneQuat q) {
    Matrix3 result = {{{
        1.0 - 2.0 * (q.y * q.y + q.z * q.z),
        2.0 * (q.x * q.y - q.w * q.z),
        2.0 * (q.x * q.z + q.w * q.y)}, {
        2.0 * (q.x * q.y + q.w * q.z),
        1.0 - 2.0 * (q.x * q.x + q.z * q.z),
        2.0 * (q.y * q.z - q.w * q.x)}, {
        2.0 * (q.x * q.z - q.w * q.y),
        2.0 * (q.y * q.z + q.w * q.x),
        1.0 - 2.0 * (q.x * q.x + q.y * q.y)}}};
    return result;
}

static Matrix3 euler_matrix(TimelineVec3 rotation) {
    const double cx = cos(rotation.x);
    const double sx = sin(rotation.x);
    const double cy = cos(rotation.y);
    const double sy = sin(rotation.y);
    const double cz = cos(rotation.z);
    const double sz = sin(rotation.z);
    Matrix3 result = {{{
        cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx}, {
        sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx}, {
        -sy, cy * sx, cy * cx}}};
    return result;
}

static bool matrices_match(Matrix3 a, Matrix3 b) {
    for (size_t row = 0u; row < 3u; ++row) {
        for (size_t column = 0u; column < 3u; ++column) {
            if (!close_value(a.m[row][column], b.m[row][column])) return false;
        }
    }
    return true;
}

static TimelineEvaluationContext test_context(void) {
    TimelineEvaluationContext context = {0};
    if (TimelineEvaluationContextBuild(
            (TimelineRate){24u, 1u}, (TimelineRange){0, 100u},
            (TimelineSample){12, 0u, 1u}, &context) != TIMELINE_STATUS_OK) {
        memset(&context, 0, sizeof(context));
    }
    return context;
}

static RayEvaluatedObjectTransform test_transform(
    const char* id, TimelineEvaluationContext context, double marker) {
    RayEvaluatedObjectTransform transform = {0};
    transform.valid = true;
    snprintf(transform.target_id, sizeof(transform.target_id), "%s", id);
    transform.source = RAY_EVALUATED_OBJECT_TRANSFORM_COMPATIBILITY_MOTION;
    transform.has_position = true;
    transform.has_rotation = true;
    transform.position = (TimelineVec3){marker, marker + 1.0, marker + 2.0};
    transform.rotation_radians =
        (TimelineVec3){marker * 0.1, marker * 0.2, marker * 0.3};
    transform.frame = context;
    return transform;
}

static bool build_base_snapshot(RayEvaluatedSceneSnapshot* output) {
    RayEvaluatedObjectTransform transforms[3] = {0};
    RayEvaluatedSceneSnapshotInputs inputs = {0};
    const TimelineEvaluationContext context = test_context();
    transforms[0] = test_transform("sim_body_c2", context, 10.0);
    transforms[1] = test_transform("set_dressing_static", context, 20.0);
    transforms[2] = test_transform("sim_body_c1", context, 30.0);
    inputs.source = RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE;
    inputs.playback_mode = RAY_EVALUATED_PLAYBACK_STOP;
    inputs.frame = context;
    inputs.identity.scene_revision = 41u;
    inputs.identity.timeline_revision = 9u;
    inputs.light.valid = true;
    inputs.light.enabled = true;
    snprintf(inputs.light.target_id, sizeof(inputs.light.target_id),
             "%s", "light/key");
    snprintf(inputs.light.runtime_light_id,
             sizeof(inputs.light.runtime_light_id), "%s", "key");
    inputs.light.position = (TimelineVec3){4.0, 5.0, 6.0};
    inputs.light.color = (TimelineVec3){0.9, 0.8, 0.7};
    inputs.light.intensity = 3.0;
    inputs.light.property_provenance.valid = true;
    inputs.camera.valid = true;
    inputs.camera.position = (TimelineVec3){7.0, 8.0, 9.0};
    inputs.camera.yaw_radians = 0.25;
    inputs.camera.pitch_radians = -0.1;
    inputs.camera.fov_y_degrees = 55.0;
    inputs.camera.aspect_ratio = 16.0 / 9.0;
    inputs.camera.zoom = 1.2;
    inputs.object_transforms = transforms;
    inputs.object_transform_count = 3u;
    inputs.simulation.source = RAY_EVALUATED_SIMULATION_NONE;
    inputs.diagnostics = "renderer-owned base snapshot";
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
    const RayEvaluatedSceneSnapshot* snapshot, const char* id) {
    for (size_t i = 0u; i < snapshot->object_transform_count; ++i) {
        if (strcmp(snapshot->object_transforms[i].target_id, id) == 0)
            return &snapshot->object_transforms[i];
    }
    return NULL;
}

static const RayCompoundSceneBodyTransform* find_body(
    const RayCompoundSceneFrame* frame, int body_id) {
    for (size_t i = 0u; i < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT; ++i) {
        if (frame->bodies[i].body_id == body_id) return &frame->bodies[i];
    }
    return NULL;
}

static int check_exact_merge(const char* fixture_path) {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure import_failure;
    RayCompoundSceneBindingManifest manifest;
    RayCompoundSceneEvaluatedSceneFailure failure;
    RayEvaluatedSceneSnapshot base = {0};
    RayEvaluatedSceneSnapshot original = {0};
    RayEvaluatedSceneSnapshot output = {0};
    RayCompoundSceneFrame frame = {0};
    const uint64_t tick = 240u;
    ray_compound_scene_handoff_init(&handoff);
    CHECK(ray_compound_scene_handoff_read(
        fixture_path, &handoff, &import_failure));
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    populate_renderer_bindings(&manifest);
    CHECK(build_base_snapshot(&base));
    original = base;
    CHECK(ray_compound_scene_handoff_replay_exact(&handoff, tick, &frame));
    CHECK(ray_compound_scene_evaluated_scene_apply_exact(
        &handoff, &manifest, tick, &base, &output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_NONE);
    CHECK(memcmp(&base, &original, sizeof(base)) == 0);
    CHECK(RayEvaluatedSceneSnapshotValidate(&output) == TIMELINE_STATUS_OK);
    CHECK(output.schema_version == 3u);
    CHECK(output.object_transform_count == base.object_transform_count);
    CHECK(memcmp(&output.light, &base.light, sizeof(base.light)) == 0);
    CHECK(memcmp(&output.camera, &base.camera, sizeof(base.camera)) == 0);
    CHECK(memcmp(&output.identity, &base.identity, sizeof(base.identity)) == 0);
    CHECK(memcmp(find_transform(&output, "set_dressing_static"),
                 find_transform(&base, "set_dressing_static"),
                 sizeof(RayEvaluatedObjectTransform)) == 0);

    for (size_t i = 0u; i < manifest.binding_count; ++i) {
        const RayCompoundSceneRendererBinding* binding = &manifest.bindings[i];
        const RayCompoundSceneBodyTransform* body =
            find_body(&frame, binding->body_id);
        const RayEvaluatedObjectTransform* transform =
            find_transform(&output, binding->object_id);
        CHECK(body != NULL && transform != NULL);
        CHECK(transform->source ==
              RAY_EVALUATED_OBJECT_TRANSFORM_COMPOUND_SCENE_EXACT);
        CHECK(transform->has_position && transform->has_rotation &&
              transform->has_orientation_quaternion);
        CHECK(close_value(transform->position.x, body->position_m.x));
        CHECK(close_value(transform->position.y, body->position_m.y));
        CHECK(close_value(transform->position.z, body->position_m.z));
        CHECK(memcmp(&transform->orientation_quaternion, &body->orientation,
                     sizeof(body->orientation)) == 0);
        CHECK(matrices_match(quaternion_matrix(body->orientation),
                             euler_matrix(transform->rotation_radians)));
        CHECK(transform->source_handoff_digest == handoff.handoff_digest);
        CHECK(transform->source_binding_digest ==
              handoff.bindings[i].binding_digest);
        CHECK(transform->source_tick == tick);
        CHECK(TimelineEvaluationContextsReferToSameSample(
            &transform->frame, &base.frame));
    }
    CHECK(output.simulation.valid &&
          output.simulation.source == RAY_EVALUATED_SIMULATION_CACHE);
    CHECK(output.simulation.cache_revision == handoff.handoff_digest);
    CHECK(output.simulation.frame_index == (int64_t)tick &&
          output.simulation.source_frame_index == (int64_t)tick);
    CHECK(output.simulation.source_rate.frames_per_second_numerator == 240u &&
          output.simulation.source_rate.frames_per_second_denominator == 1u);
    CHECK(output.simulation.interpolation ==
          RAY_EVALUATED_SIMULATION_INTERPOLATION_STEP);
    CHECK(!strcmp(output.simulation.content_digest,
                  "fnv64:51a8af28de622ad8"));
    CHECK((output.invalidation_domains &
           TIMELINE_INVALIDATION_RIGID_TRANSFORM) != 0u);
    CHECK((output.invalidation_domains &
           TIMELINE_INVALIDATION_SIMULATION_CACHE) != 0u);

    RayEvaluatedSceneSnapshot in_place = base;
    CHECK(ray_compound_scene_evaluated_scene_apply_exact(
        &handoff, &manifest, tick, &in_place, &in_place, &failure));
    CHECK(memcmp(&in_place, &output, sizeof(output)) == 0);
    ray_compound_scene_handoff_free(&handoff);
    return 0;
}

static int check_transactional_rejections(const char* fixture_path) {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure import_failure;
    RayCompoundSceneBindingManifest manifest;
    RayCompoundSceneEvaluatedSceneFailure failure;
    RayEvaluatedSceneSnapshot base = {0};
    RayEvaluatedSceneSnapshot output = {0};
    RayEvaluatedSceneSnapshot sentinel = {0};
    ray_compound_scene_handoff_init(&handoff);
    CHECK(ray_compound_scene_handoff_read(
        fixture_path, &handoff, &import_failure));
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    populate_renderer_bindings(&manifest);
    CHECK(build_base_snapshot(&base));
    output = base;
    output.identity.scene_revision = 999u;
    sentinel = output;

    CHECK(!ray_compound_scene_evaluated_scene_apply_exact(
        &handoff, &manifest, handoff.frame_count, &base, &output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_TICK);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);

    RayCompoundSceneBindingManifest missing_target = manifest;
    snprintf(missing_target.bindings[1].object_id,
             sizeof(missing_target.bindings[1].object_id), "%s", "missing");
    CHECK(ray_compound_scene_binding_manifest_validate(
        &missing_target, &handoff));
    CHECK(!ray_compound_scene_evaluated_scene_apply_exact(
        &handoff, &missing_target, 240u, &base, &output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_TARGET);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);

    RayCompoundSceneBindingManifest invalid_manifest = manifest;
    invalid_manifest.handoff_digest ^= UINT64_C(1);
    CHECK(!ray_compound_scene_evaluated_scene_apply_exact(
        &handoff, &invalid_manifest, 240u, &base, &output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_MANIFEST);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);

    RayEvaluatedSceneSnapshot invalid_base = base;
    invalid_base.valid = false;
    CHECK(!ray_compound_scene_evaluated_scene_apply_exact(
        &handoff, &manifest, 240u, &invalid_base, &output, &failure));
    CHECK(failure ==
          RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_BASE_SNAPSHOT);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
    CHECK(!strcmp(ray_compound_scene_evaluated_scene_failure_name(failure),
                  "base_snapshot"));
    ray_compound_scene_handoff_free(&handoff);
    return 0;
}

static int check_ingestion_appends_missing_targets(const char* fixture_path) {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure import_failure;
    RayCompoundSceneBindingManifest manifest;
    RayCompoundSceneEvaluatedSceneFailure failure;
    RayEvaluatedSceneSnapshot base = {0};
    RayEvaluatedSceneSnapshot original = {0};
    RayEvaluatedSceneSnapshot output = {0};
    ray_compound_scene_handoff_init(&handoff);
    CHECK(ray_compound_scene_handoff_read(
        fixture_path, &handoff, &import_failure));
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    populate_renderer_bindings(&manifest);
    snprintf(manifest.bindings[0].object_id,
             sizeof(manifest.bindings[0].object_id), "%s", "ingested_c2");
    snprintf(manifest.bindings[1].object_id,
             sizeof(manifest.bindings[1].object_id), "%s", "ingested_c1");
    CHECK(ray_compound_scene_binding_manifest_validate(&manifest, &handoff));
    CHECK(build_base_snapshot(&base));
    original = base;
    CHECK(ray_compound_scene_evaluated_scene_apply_ingestion_exact(
        &handoff, &manifest, 240u, &base, &output, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_EVALUATED_SCENE_FAILURE_NONE);
    CHECK(memcmp(&base, &original, sizeof(base)) == 0);
    CHECK(output.object_transform_count == base.object_transform_count + 2u);
    CHECK(find_transform(&output, "ingested_c2") != NULL);
    CHECK(find_transform(&output, "ingested_c1") != NULL);
    CHECK(RayEvaluatedSceneSnapshotValidate(&output) == TIMELINE_STATUS_OK);
    ray_compound_scene_handoff_free(&handoff);
    return 0;
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    CHECK(check_exact_merge(argv[1]) == 0);
    CHECK(check_transactional_rejections(argv[1]) == 0);
    CHECK(check_ingestion_appends_missing_targets(argv[1]) == 0);
    puts("compound evaluated scene contract: PASS tick=240 bindings=2 "
         "snapshot_schema=3 quaternion=exact euler=compatible "
         "camera_light=preserved strict_targets=preserved "
         "ingestion_append=2 geometry_applied=0");
    return 0;
}
