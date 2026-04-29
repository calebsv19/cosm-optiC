#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_visibility_3d.h"
#include "test_runtime_scene_3d_geometry.h"
#include "test_support.h"

static int test_runtime_scene_3d_builder_uses_retained_seed_scope(void) {
    const char *runtime_json_primitives =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_scope\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_box\","
            "\"object_type\":\"box\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_plane\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":4.0,"
            "\"frame\":{\"origin\":{\"x\":5.0,\"y\":0.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}}},"
            "\"transform\":{\"position\":{\"x\":5.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_mesh\","
            "\"object_type\":\"triangle_mesh\","
            "\"transform\":{\"position\":{\"x\":10.0,\"y\":0.0,\"z\":-1.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    RuntimeScene3D scene;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json_primitives, &summary);
    assert_true("runtime_scene_3d_builder_scope_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene);
    assert_true("runtime_scene_3d_builder_scope_build_ok", ok);
    assert_true("runtime_scene_3d_builder_scope_primitive_count", scene.primitiveCount == 1);
    assert_true("runtime_scene_3d_builder_scope_triangle_count", scene.triangleMesh.triangleCount == 2);
    assert_true("runtime_scene_3d_builder_scope_object_id",
                strcmp(scene.primitives[0].source.objectId, "obj_plane") == 0);
    assert_true("runtime_scene_3d_builder_scope_plane_kind",
                scene.primitives[0].kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE);
    assert_close("runtime_scene_3d_builder_scope_plane_width",
                 scene.primitives[0].shape.plane.width,
                 6.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_scope_plane_height",
                 scene.primitives[0].shape.plane.height,
                 4.0,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_builder_builds_ps4d_triangle_scene(void) {
    RuntimeSceneBridgePreflight summary;
    RuntimeScene3D scene;
    int plane_index = -1;
    int center_index = -1;
    int offset_index = -1;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_file("../physics_sim/config/samples/ps4d_runtime_scene_visual_test.json",
                                         &summary);
    assert_true("runtime_scene_3d_builder_ps4d_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene);
    assert_true("runtime_scene_3d_builder_ps4d_build_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    assert_true("runtime_scene_3d_builder_ps4d_primitive_count", scene.primitiveCount == 3);
    assert_true("runtime_scene_3d_builder_ps4d_triangle_count", scene.triangleMesh.triangleCount == 26);

    plane_index = find_runtime_primitive_index_by_object_id(&scene, "plane_floor");
    center_index = find_runtime_primitive_index_by_object_id(&scene, "prism_center");
    offset_index = find_runtime_primitive_index_by_object_id(&scene, "prism_offset");
    assert_true("runtime_scene_3d_builder_ps4d_plane_index", plane_index == 0);
    assert_true("runtime_scene_3d_builder_ps4d_center_index", center_index == 1);
    assert_true("runtime_scene_3d_builder_ps4d_offset_index", offset_index == 2);
    assert_true("runtime_scene_3d_builder_ps4d_plane_triangles",
                count_runtime_triangles_for_primitive(&scene, plane_index) == 2);
    assert_true("runtime_scene_3d_builder_ps4d_center_triangles",
                count_runtime_triangles_for_primitive(&scene, center_index) == 12);
    assert_true("runtime_scene_3d_builder_ps4d_offset_triangles",
                count_runtime_triangles_for_primitive(&scene, offset_index) == 12);

    assert_true("runtime_scene_3d_builder_ps4d_plane_kind",
                scene.primitives[plane_index].kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE);
    assert_true("runtime_scene_3d_builder_ps4d_center_kind",
                scene.primitives[center_index].kind == RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM);
    assert_true("runtime_scene_3d_builder_ps4d_offset_kind",
                scene.primitives[offset_index].kind == RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM);
    assert_close("runtime_scene_3d_builder_ps4d_plane_width",
                 scene.primitives[plane_index].shape.plane.width,
                 8.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_ps4d_plane_height",
                 scene.primitives[plane_index].shape.plane.height,
                 6.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_ps4d_center_depth",
                 scene.primitives[center_index].shape.rectPrism.depth,
                 2.5,
                 1e-9);
    assert_close("runtime_scene_3d_builder_ps4d_offset_origin_x",
                 scene.primitives[offset_index].shape.rectPrism.origin.x,
                 2.5,
                 1e-9);

    for (int i = 0; i < scene.triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D *triangle = &scene.triangleMesh.triangles[i];
        assert_true("runtime_scene_3d_builder_ps4d_triangle_normal_unitish",
                    fabs(vec3_length(triangle->normal) - 1.0) < 1e-9);
    }
    for (int i = 0; i < 2; ++i) {
        const RuntimeTriangle3D *triangle = &scene.triangleMesh.triangles[i];
        assert_true("runtime_scene_3d_builder_ps4d_plane_triangle_primitive_index",
                    triangle->primitiveIndex == plane_index);
        assert_true("runtime_scene_3d_builder_ps4d_plane_triangle_scene_object_index",
                    triangle->sceneObjectIndex == scene.primitives[plane_index].source.sceneObjectIndex);
        assert_close("runtime_scene_3d_builder_ps4d_plane_triangle_normal_z",
                     triangle->normal.z,
                     1.0,
                     1e-9);
    }

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_builder_promotes_authored_light_camera_samples(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_samples\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":2.0,\"z\":3.0}}],"
        "\"cameras\":[{\"position\":{\"x\":4.0,\"y\":5.0,\"z\":6.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"light_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{"
                    "\"x\":0.0,\"y\":0.0,\"rotation\":0.0,\"handleLink\":false,"
                    "\"velocity1\":{\"vx\":2.0,\"vy\":0.0}"
                  "},"
                  "{"
                    "\"x\":10.0,\"y\":0.0,\"rotation\":0.0,\"handleLink\":false,"
                    "\"velocity2\":{\"vx\":-2.0,\"vy\":0.0}"
                  "}"
                "]"
              "},"
              "\"light_path_depth\":{"
                "\"points\":["
                  "{\"z\":1.0,\"velocity1\":{\"vz\":1.5}},"
                  "{\"z\":5.0,\"velocity2\":{\"vz\":-1.5}}"
                "]"
              "},"
              "\"camera_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{"
                    "\"x\":2.0,\"y\":1.0,\"rotation\":0.25,\"handleLink\":false,"
                    "\"velocity1\":{\"vx\":1.0,\"vy\":1.5}"
                  "},"
                  "{"
                    "\"x\":8.0,\"y\":5.0,\"rotation\":1.25,\"handleLink\":false,"
                    "\"velocity2\":{\"vx\":-1.0,\"vy\":-1.5}"
                  "}"
                "]"
              "},"
              "\"camera_path_depth\":{"
                "\"points\":["
                  "{\"z\":2.0,\"lookPitch\":0.10,\"velocity1\":{\"vz\":0.75}},"
                  "{\"z\":6.0,\"lookPitch\":0.40,\"velocity2\":{\"vz\":-0.75}}"
                "]"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    Point expected_light_xy = {0.0, 0.0};
    Point expected_camera_xy = {0.0, 0.0};
    double expected_light_z = 0.0;
    double expected_camera_z = 0.0;
    double expected_camera_yaw = 0.0;
    double expected_camera_pitch = 0.0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_samples_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 42.0;
    animSettings.lightRadius = 3.25;
    animSettings.forwardDecay = 17.5;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;

    expected_light_xy = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, 0.5);
    expected_light_z =
        CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath, &sceneSettings.bezierPath3D, 0.5);
    expected_camera_xy = GetPositionAlongPathNormalized(&sceneSettings.cameraPath, 0.5);
    expected_camera_yaw = GetRotationAlongPathNormalized(&sceneSettings.cameraPath, 0.5);
    expected_camera_z =
        CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath, &sceneSettings.cameraPath3D, 0.5);
    expected_camera_pitch =
        expected_camera_pitch_for_t(&sceneSettings.cameraPath, &sceneSettings.cameraPath3D, 0.5);

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.5);
    assert_true("runtime_scene_3d_builder_samples_build_ok", ok);
    assert_true("runtime_scene_3d_builder_samples_no_primitives", scene.primitiveCount == 0);
    assert_true("runtime_scene_3d_builder_samples_no_triangles", scene.triangleMesh.triangleCount == 0);
    assert_true("runtime_scene_3d_builder_samples_has_light", scene.hasLight);
    assert_true("runtime_scene_3d_builder_samples_has_camera", scene.hasCamera);
    assert_close("runtime_scene_3d_builder_samples_light_x",
                 scene.light.position.x,
                 expected_light_xy.x,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_y",
                 scene.light.position.y,
                 expected_light_xy.y,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_z",
                 scene.light.position.z,
                 expected_light_z,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_radius",
                 scene.light.radius,
                 3.25,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_intensity",
                 scene.light.intensity,
                 42.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_light_falloff",
                 scene.light.falloffDistance,
                 17.5,
                 1e-6);
    assert_true("runtime_scene_3d_builder_samples_light_falloff_mode",
                scene.light.falloffMode == FORWARD_FALLOFF_MODE_LINEAR);
    assert_close("runtime_scene_3d_builder_samples_camera_x",
                 scene.camera.position.x,
                 expected_camera_xy.x,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_y",
                 scene.camera.position.y,
                 expected_camera_xy.y,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_z",
                 scene.camera.position.z,
                 expected_camera_z,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_yaw",
                 scene.camera.rotation,
                 expected_camera_yaw,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_pitch",
                 scene.camera.lookPitch,
                 expected_camera_pitch,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_zoom",
                 scene.camera.zoom,
                 1.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_camera_near_plane",
                 scene.camera.nearPlane,
                 0.1,
                 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_scene_3d_builder_falls_back_to_seeded_camera_state(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_camera_fallback\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":-2.0,\"y\":3.0,\"z\":4.0}}],"
        "\"cameras\":[{\"position\":{\"x\":7.0,\"y\":8.0,\"z\":9.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_camera_fallback_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.camera.rotation = 0.75;
    sceneSettings.camera.zoom = 1.5;
    animSettings.lightIntensity = 11.0;
    animSettings.lightRadius = 1.5;
    animSettings.forwardDecay = 9.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_NONE;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.65);
    assert_true("runtime_scene_3d_builder_camera_fallback_build_ok", ok);
    assert_true("runtime_scene_3d_builder_camera_fallback_has_light", scene.hasLight);
    assert_true("runtime_scene_3d_builder_camera_fallback_has_camera", scene.hasCamera);
    assert_close("runtime_scene_3d_builder_camera_fallback_light_x",
                 scene.light.position.x,
                 -2.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_light_y",
                 scene.light.position.y,
                 3.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_light_z",
                 scene.light.position.z,
                 4.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_light_radius",
                 scene.light.radius,
                 1.5,
                 1e-6);
    assert_true("runtime_scene_3d_builder_camera_fallback_light_mode",
                scene.light.falloffMode == FORWARD_FALLOFF_MODE_NONE);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_x",
                 scene.camera.position.x,
                 7.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_y",
                 scene.camera.position.y,
                 8.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_z",
                 scene.camera.position.z,
                 9.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_yaw",
                 scene.camera.rotation,
                 0.75,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_pitch",
                 scene.camera.lookPitch,
                 0.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_fallback_camera_zoom",
                 scene.camera.zoom,
                 1.5,
                 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_ray_3d_triangle_intersection_contract(void) {
    RuntimeTriangle3D triangle = {0};
    Ray3D ray = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    triangle.p0 = vec3(0.0, 0.0, 0.0);
    triangle.p1 = vec3(1.0, 0.0, 0.0);
    triangle.p2 = vec3(0.0, 1.0, 0.0);
    triangle.normal = vec3(0.0, 0.0, 1.0);
    triangle.primitiveIndex = 3;
    triangle.sceneObjectIndex = 7;
    ray = RuntimeRay3D_Make(vec3(0.25, 0.25, 3.0), vec3(0.0, 0.0, -2.0));

    ok = RuntimeRay3D_IntersectTriangle(&ray, &triangle, 11, 0.001, 10.0, &hit);
    assert_true("runtime_ray_3d_triangle_hit_ok", ok);
    assert_close("runtime_ray_3d_triangle_hit_t", hit.t, 3.0, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_px", hit.position.x, 0.25, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_py", hit.position.y, 0.25, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_pz", hit.position.z, 0.0, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_nz", hit.normal.z, 1.0, 1e-6);
    assert_true("runtime_ray_3d_triangle_hit_triangle_index", hit.triangleIndex == 11);
    assert_true("runtime_ray_3d_triangle_hit_primitive_index", hit.primitiveIndex == 3);
    assert_true("runtime_ray_3d_triangle_hit_scene_object_index", hit.sceneObjectIndex == 7);
    assert_close("runtime_ray_3d_triangle_hit_bary_sum",
                 hit.baryU + hit.baryV + hit.baryW,
                 1.0,
                 1e-6);
    assert_true("runtime_ray_3d_triangle_hit_bary_inside",
                hit.baryU >= 0.0 && hit.baryV >= 0.0 && hit.baryW >= 0.0);
    return 0;
}

static int test_runtime_ray_3d_scene_first_hit_contract(void) {
    RuntimeScene3D scene;
    Ray3D ray = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_ray_3d_scene_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_ray_3d_scene_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 2;
    scene.triangleMesh.triangleCount = 2;

    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "rear_plane");
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 41;
    snprintf(scene.primitives[1].source.objectId,
             sizeof(scene.primitives[1].source.objectId),
             "%s",
             "front_plane");
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[1].source.sceneObjectIndex = 42;

    scene.triangleMesh.triangles[0].p0 = vec3(-1.0, -1.0, 0.0);
    scene.triangleMesh.triangles[0].p1 = vec3(1.0, -1.0, 0.0);
    scene.triangleMesh.triangles[0].p2 = vec3(-1.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 0.0, 1.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 41;

    scene.triangleMesh.triangles[1].p0 = vec3(-1.0, -1.0, 1.0);
    scene.triangleMesh.triangles[1].p1 = vec3(1.0, -1.0, 1.0);
    scene.triangleMesh.triangles[1].p2 = vec3(-1.0, 1.0, 1.0);
    scene.triangleMesh.triangles[1].normal = vec3(0.0, 0.0, 1.0);
    scene.triangleMesh.triangles[1].primitiveIndex = 1;
    scene.triangleMesh.triangles[1].sceneObjectIndex = 42;

    ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 3.0), vec3(0.0, 0.0, -1.0));
    ok = RuntimeRay3D_TraceSceneFirstHit(&scene, &ray, 0.001, 10.0, &hit);
    assert_true("runtime_ray_3d_scene_first_hit_ok", ok);
    assert_close("runtime_ray_3d_scene_first_hit_t", hit.t, 2.0, 1e-6);
    assert_close("runtime_ray_3d_scene_first_hit_pz", hit.position.z, 1.0, 1e-6);
    assert_true("runtime_ray_3d_scene_first_hit_triangle_index", hit.triangleIndex == 1);
    assert_true("runtime_ray_3d_scene_first_hit_primitive_index", hit.primitiveIndex == 1);
    assert_true("runtime_ray_3d_scene_first_hit_scene_object_index", hit.sceneObjectIndex == 42);
    assert_true("runtime_ray_3d_scene_first_hit_object_id",
                strcmp(hit.source.objectId, "front_plane") == 0);
    assert_true("runtime_ray_3d_scene_first_hit_kind",
                hit.source.kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_ray_3d_offset_contract(void) {
    Ray3D forward =
        RuntimeRay3D_MakeOffset(vec3(1.0, 2.0, 3.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, 4.0), 0.05);
    Ray3D backward =
        RuntimeRay3D_MakeOffset(vec3(1.0, 2.0, 3.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, -4.0), 0.05);

    assert_close("runtime_ray_3d_offset_forward_origin_z", forward.origin.z, 3.05, 1e-6);
    assert_close("runtime_ray_3d_offset_backward_origin_z", backward.origin.z, 2.95, 1e-6);
    assert_close("runtime_ray_3d_offset_forward_dir_len", vec3_length(forward.direction), 1.0, 1e-6);
    assert_close("runtime_ray_3d_offset_backward_dir_len", vec3_length(backward.direction), 1.0, 1e-6);
    return 0;
}

static int test_runtime_light_emitter_3d_center_hit_contract(void) {
    RuntimeScene3D scene;
    Ray3D ray = {0};
    RuntimeLightEmitterHit3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(0.0, 0.0, 0.0);
    scene.light.radius = 1.0;
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_NONE;
    ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 3.0), vec3(0.0, 0.0, -1.0));

    ok = RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0.001, 10.0, &result);
    assert_true("runtime_light_emitter_3d_center_hit_ok", ok);
    assert_true("runtime_light_emitter_3d_center_hit_flag", result.hit);
    assert_close("runtime_light_emitter_3d_center_hit_t", result.t, 2.0, 1e-6);
    assert_close("runtime_light_emitter_3d_center_hit_pz", result.position.z, 1.0, 1e-6);
    assert_close("runtime_light_emitter_3d_center_hit_nz", result.normal.z, 1.0, 1e-6);
    assert_close("runtime_light_emitter_3d_center_hit_radial", result.radialFalloff, 1.0, 1e-6);
    assert_close("runtime_light_emitter_3d_center_hit_attenuation", result.attenuation, 1.0, 1e-6);
    assert_close("runtime_light_emitter_3d_center_hit_radiance", result.radiance, 10.0, 1e-6);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_light_emitter_3d_trace_geometry_tie_wins_contract(void) {
    RuntimeScene3D scene;
    Ray3D ray = {0};
    RuntimeLightEmitterTrace3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(0.0, 0.0, 0.0);
    scene.light.radius = 1.0;
    scene.light.intensity = 10.0;
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_light_emitter_3d_tie_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_light_emitter_3d_tie_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 21;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "front_plane");
    scene.triangleMesh.triangles[0].p0 = vec3(-1.0, -1.0, 1.0);
    scene.triangleMesh.triangles[0].p1 = vec3(1.0, -1.0, 1.0);
    scene.triangleMesh.triangles[0].p2 = vec3(-1.0, 1.0, 1.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 0.0, 1.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 21;

    ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 3.0), vec3(0.0, 0.0, -1.0));
    ok = RuntimeLightEmitter3D_ResolveFirstHit(&scene, &ray, 0.001, 10.0, &result);
    assert_true("runtime_light_emitter_3d_tie_resolve_ok", ok);
    assert_true("runtime_light_emitter_3d_tie_geometry_hit", result.geometryHit);
    assert_true("runtime_light_emitter_3d_tie_emitter_hit", result.emitterHit);
    assert_true("runtime_light_emitter_3d_tie_geometry_wins", !result.emitterWins);
    assert_close("runtime_light_emitter_3d_tie_geometry_t",
                 result.geometryHitInfo.t,
                 result.emitterHitInfo.t,
                 1e-6);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_light_emitter_3d_trace_emitter_wins_contract(void) {
    RuntimeScene3D scene;
    Ray3D ray = {0};
    RuntimeLightEmitterTrace3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(0.0, 0.0, 0.0);
    scene.light.radius = 1.0;
    scene.light.intensity = 10.0;
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_light_emitter_3d_win_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_light_emitter_3d_win_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 22;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "rear_plane");
    scene.triangleMesh.triangles[0].p0 = vec3(-2.0, -2.0, -2.0);
    scene.triangleMesh.triangles[0].p1 = vec3(2.0, -2.0, -2.0);
    scene.triangleMesh.triangles[0].p2 = vec3(-2.0, 2.0, -2.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 0.0, 1.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 22;

    ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 3.0), vec3(0.0, 0.0, -1.0));
    ok = RuntimeLightEmitter3D_ResolveFirstHit(&scene, &ray, 0.001, 10.0, &result);
    assert_true("runtime_light_emitter_3d_win_resolve_ok", ok);
    assert_true("runtime_light_emitter_3d_win_geometry_hit", result.geometryHit);
    assert_true("runtime_light_emitter_3d_win_emitter_hit", result.emitterHit);
    assert_true("runtime_light_emitter_3d_win_emitter_wins", result.emitterWins);
    assert_true("runtime_light_emitter_3d_win_emitter_nearer",
                result.emitterHitInfo.t < result.geometryHitInfo.t);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_light_emitter_3d_radial_falloff_contract(void) {
    RuntimeScene3D scene;
    Ray3D center_ray = {0};
    Ray3D edge_ray = {0};
    RuntimeLightEmitterHit3DResult center_result = {0};
    RuntimeLightEmitterHit3DResult edge_result = {0};
    bool center_ok = false;
    bool edge_ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    scene.light.position = vec3(0.0, 0.0, 0.0);
    scene.light.radius = 1.0;
    scene.light.intensity = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_NONE;
    center_ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 3.0), vec3(0.0, 0.0, -1.0));
    edge_ray = RuntimeRay3D_Make(vec3(0.95, 0.0, 3.0), vec3(0.0, 0.0, -1.0));

    center_ok = RuntimeLightEmitter3D_IntersectRay(&scene, &center_ray, 0.001, 10.0, &center_result);
    edge_ok = RuntimeLightEmitter3D_IntersectRay(&scene, &edge_ray, 0.001, 10.0, &edge_result);
    assert_true("runtime_light_emitter_3d_radial_center_ok", center_ok);
    assert_true("runtime_light_emitter_3d_radial_edge_ok", edge_ok);
    assert_true("runtime_light_emitter_3d_radial_edge_positive",
                edge_result.radialFalloff > 0.0);
    assert_true("runtime_light_emitter_3d_radial_edge_lower",
                edge_result.radialFalloff < center_result.radialFalloff);
    assert_true("runtime_light_emitter_3d_radial_radiance_lower",
                edge_result.radiance < center_result.radiance);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_visibility_3d_visible_contract(void) {
    RuntimeScene3D scene;
    HitInfo3D surface_hit = {0};
    RuntimeLight3D light = {0};
    HitInfo3D blocker_hit = {0};
    double light_distance = 0.0;
    bool blocked = false;
    bool visible = false;

    RuntimeScene3D_Init(&scene);
    surface_hit.position = vec3(0.0, 0.0, 0.0);
    surface_hit.normal = vec3(0.0, 0.0, 1.0);
    light.position = vec3(0.0, 0.0, 3.0);

    blocked = RuntimeVisibility3D_TraceToLight(&scene,
                                               surface_hit.position,
                                               surface_hit.normal,
                                               light.position,
                                               &blocker_hit,
                                               &light_distance);
    visible = RuntimeVisibility3D_HasLineOfSightFromHit(&scene, &surface_hit, &light);

    assert_true("runtime_visibility_3d_visible_not_blocked", !blocked);
    assert_true("runtime_visibility_3d_visible_los", visible);
    assert_close("runtime_visibility_3d_visible_distance", light_distance, 3.0, 1e-6);
    assert_true("runtime_visibility_3d_visible_reset_triangle", blocker_hit.triangleIndex == -1);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_visibility_3d_blocked_contract(void) {
    RuntimeScene3D scene;
    HitInfo3D surface_hit = {0};
    RuntimeLight3D light = {0};
    HitInfo3D blocker_hit = {0};
    double light_distance = 0.0;
    bool blocked = false;
    bool visible = false;

    RuntimeScene3D_Init(&scene);
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_visibility_3d_blocked_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_visibility_3d_blocked_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "blocker");
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 88;

    scene.triangleMesh.triangles[0].p0 = vec3(-1.0, -1.0, 1.5);
    scene.triangleMesh.triangles[0].p1 = vec3(1.0, -1.0, 1.5);
    scene.triangleMesh.triangles[0].p2 = vec3(-1.0, 1.0, 1.5);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 0.0, -1.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 88;

    surface_hit.position = vec3(0.0, 0.0, 0.0);
    surface_hit.normal = vec3(0.0, 0.0, 1.0);
    light.position = vec3(0.0, 0.0, 3.0);

    blocked = RuntimeVisibility3D_TraceToLight(&scene,
                                               surface_hit.position,
                                               surface_hit.normal,
                                               light.position,
                                               &blocker_hit,
                                               &light_distance);
    visible = RuntimeVisibility3D_HasLineOfSightFromHit(&scene, &surface_hit, &light);

    assert_true("runtime_visibility_3d_blocked_blocked", blocked);
    assert_true("runtime_visibility_3d_blocked_not_visible", !visible);
    assert_close("runtime_visibility_3d_blocked_distance", light_distance, 3.0, 1e-6);
    assert_true("runtime_visibility_3d_blocked_triangle_index", blocker_hit.triangleIndex == 0);
    assert_true("runtime_visibility_3d_blocked_primitive_index", blocker_hit.primitiveIndex == 0);
    assert_true("runtime_visibility_3d_blocked_scene_object_index", blocker_hit.sceneObjectIndex == 88);
    assert_true("runtime_visibility_3d_blocked_object_id",
                strcmp(blocker_hit.source.objectId, "blocker") == 0);
    assert_close("runtime_visibility_3d_blocked_hit_z", blocker_hit.position.z, 1.5, 1e-6);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_camera_projector_3d_center_ray_contract(void) {
    RuntimeCamera3D camera = {0};
    RuntimeCameraProjector3D projector = {0};
    Ray3D ray = {0};
    bool ok = false;

    camera.position = vec3(1.0, 2.0, 3.0);
    camera.rotation = 0.0;
    camera.lookPitch = 0.0;
    camera.zoom = 1.0;
    camera.nearPlane = 0.1;

    ok = RuntimeCameraProjector3D_Build(&camera, 201, 101, &projector);
    assert_true("runtime_camera_projector_3d_build_ok", ok);
    if (!ok) return 0;

    ray = RuntimeCameraProjector3D_MakePrimaryRay(&projector, 100.0, 50.0);
    assert_close("runtime_camera_projector_3d_center_origin_x", ray.origin.x, 1.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_origin_y", ray.origin.y, 2.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_origin_z", ray.origin.z, 3.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_dir_x", ray.direction.x, 0.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_dir_y", ray.direction.y, -1.0, 1e-6);
    assert_close("runtime_camera_projector_3d_center_dir_z", ray.direction.z, 0.0, 1e-6);

    ray = RuntimeCameraProjector3D_MakePrimaryRay(&projector, 200.0, 50.0);
    assert_true("runtime_camera_projector_3d_right_ray_x_positive", ray.direction.x > 0.0);
    assert_true("runtime_camera_projector_3d_right_ray_y_negative", ray.direction.y < 0.0);
    return 0;
}

static int test_runtime_camera_projector_3d_pitch_contract(void) {
    RuntimeCamera3D camera = {0};
    RuntimeCameraProjector3D projector = {0};
    Ray3D ray = {0};
    bool ok = false;

    camera.position = vec3(0.0, 0.0, 0.0);
    camera.rotation = 0.0;
    camera.lookPitch = M_PI / 4.0;
    camera.zoom = 1.0;
    camera.nearPlane = 0.1;

    ok = RuntimeCameraProjector3D_Build(&camera, 101, 101, &projector);
    assert_true("runtime_camera_projector_3d_pitch_build_ok", ok);
    if (!ok) return 0;

    ray = RuntimeCameraProjector3D_MakePrimaryRay(&projector, 50.0, 50.0);
    assert_true("runtime_camera_projector_3d_pitch_z_positive", ray.direction.z > 0.0);
    assert_true("runtime_camera_projector_3d_pitch_y_negative", ray.direction.y < 0.0);
    assert_close("runtime_camera_projector_3d_pitch_dir_len",
                 vec3_length(ray.direction),
                 1.0,
                 1e-6);
    return 0;
}

static int test_runtime_camera_projector_3d_zoom_contract(void) {
    RuntimeCamera3D base_camera = {0};
    RuntimeCameraProjector3D wide_projector = {0};
    RuntimeCameraProjector3D zoomed_projector = {0};
    Ray3D wide_ray = {0};
    Ray3D zoomed_ray = {0};
    bool ok_wide = false;
    bool ok_zoomed = false;

    base_camera.position = vec3(0.0, 0.0, 0.0);
    base_camera.rotation = 0.0;
    base_camera.lookPitch = 0.0;
    base_camera.nearPlane = 0.1;

    base_camera.zoom = 1.0;
    ok_wide = RuntimeCameraProjector3D_Build(&base_camera, 201, 101, &wide_projector);
    base_camera.zoom = 2.0;
    ok_zoomed = RuntimeCameraProjector3D_Build(&base_camera, 201, 101, &zoomed_projector);
    assert_true("runtime_camera_projector_3d_zoom_build_wide_ok", ok_wide);
    assert_true("runtime_camera_projector_3d_zoom_build_zoomed_ok", ok_zoomed);
    if (!ok_wide || !ok_zoomed) return 0;

    wide_ray = RuntimeCameraProjector3D_MakePrimaryRay(&wide_projector, 200.0, 50.0);
    zoomed_ray = RuntimeCameraProjector3D_MakePrimaryRay(&zoomed_projector, 200.0, 50.0);
    assert_true("runtime_camera_projector_3d_zoom_narrows_horizontal_spread",
                fabs(zoomed_ray.direction.x) < fabs(wide_ray.direction.x));
    return 0;
}


int run_test_runtime_scene_3d_geometry_tests(void) {
    int before = test_support_failures();

    test_runtime_scene_3d_builder_uses_retained_seed_scope();
    test_runtime_scene_3d_builder_builds_ps4d_triangle_scene();
    test_runtime_scene_3d_builder_promotes_authored_light_camera_samples();
    test_runtime_scene_3d_builder_falls_back_to_seeded_camera_state();
    test_runtime_ray_3d_triangle_intersection_contract();
    test_runtime_ray_3d_scene_first_hit_contract();
    test_runtime_ray_3d_offset_contract();
    test_runtime_light_emitter_3d_center_hit_contract();
    test_runtime_light_emitter_3d_trace_geometry_tie_wins_contract();
    test_runtime_light_emitter_3d_trace_emitter_wins_contract();
    test_runtime_light_emitter_3d_radial_falloff_contract();
    test_runtime_visibility_3d_visible_contract();
    test_runtime_visibility_3d_blocked_contract();
    test_runtime_camera_projector_3d_center_ray_contract();
    test_runtime_camera_projector_3d_pitch_contract();
    test_runtime_camera_projector_3d_zoom_contract();

    return test_support_failures() - before;
}
