#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "material/material.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_visibility_3d.h"
#include "test_runtime_scene_3d_geometry_internal.h"
#include "test_support.h"

static int test_runtime_ray_3d_triangle_intersection_contract(void) {
    RuntimeTriangle3D triangle = {0};
    Ray3D ray = {0};
    HitInfo3D hit = {0};
    bool ok = false;

    triangle.p0 = vec3(0.0, 0.0, 0.0);
    triangle.p1 = vec3(1.0, 0.0, 0.0);
    triangle.p2 = vec3(0.0, 1.0, 0.0);
    triangle.normal = vec3(0.0, 0.0, 1.0);
    triangle.hasVertexNormals = true;
    triangle.vertexNormal0 = vec3(0.0, 0.0, 1.0);
    triangle.vertexNormal1 = vec3_normalize(vec3(1.0, 0.0, 1.0));
    triangle.vertexNormal2 = vec3_normalize(vec3(0.0, 1.0, 1.0));
    triangle.hasObjectTextureCoords = true;
    triangle.objectTexture0 = vec3(0.10, 0.20, 0.30);
    triangle.objectTexture1 = vec3(0.70, 0.20, 0.30);
    triangle.objectTexture2 = vec3(0.10, 0.80, 0.30);
    triangle.primitiveIndex = 3;
    triangle.sceneObjectIndex = 7;
    triangle.localTriangleIndex = 5;
    ray = RuntimeRay3D_Make(vec3(0.25, 0.25, 3.0), vec3(0.0, 0.0, -2.0));

    ok = RuntimeRay3D_IntersectTriangle(&ray, &triangle, 11, 0.001, 10.0, &hit);
    assert_true("runtime_ray_3d_triangle_hit_ok", ok);
    assert_close("runtime_ray_3d_triangle_hit_t", hit.t, 3.0, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_px", hit.position.x, 0.25, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_py", hit.position.y, 0.25, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_pz", hit.position.z, 0.0, 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_ng_z", hit.geometricNormal.z, 1.0, 1e-6);
    assert_true("runtime_ray_3d_triangle_hit_ns_interpolated",
                hit.shadingNormal.x > 0.1 && hit.shadingNormal.y > 0.1 &&
                    hit.shadingNormal.z > 0.9);
    assert_close("runtime_ray_3d_triangle_hit_normal_alias_x",
                 hit.normal.x,
                 hit.shadingNormal.x,
                 1e-9);
    assert_true("runtime_ray_3d_triangle_hit_ns_same_hemisphere",
                vec3_dot(hit.geometricNormal, hit.shadingNormal) > 0.0);
    assert_true("runtime_ray_3d_triangle_hit_triangle_index", hit.triangleIndex == 11);
    assert_true("runtime_ray_3d_triangle_hit_local_triangle_index", hit.localTriangleIndex == 5);
    assert_true("runtime_ray_3d_triangle_hit_primitive_index", hit.primitiveIndex == 3);
    assert_true("runtime_ray_3d_triangle_hit_scene_object_index", hit.sceneObjectIndex == 7);
    assert_close("runtime_ray_3d_triangle_hit_bary_sum",
                 hit.baryU + hit.baryV + hit.baryW,
                 1.0,
                 1e-6);
    assert_true("runtime_ray_3d_triangle_hit_bary_inside",
                hit.baryU >= 0.0 && hit.baryV >= 0.0 && hit.baryW >= 0.0);
    assert_true("runtime_ray_3d_triangle_hit_object_texture_coord",
                hit.hasObjectTextureCoord);
    assert_close("runtime_ray_3d_triangle_hit_object_texture_x",
                 hit.objectTextureCoord.x,
                 0.25,
                 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_object_texture_y",
                 hit.objectTextureCoord.y,
                 0.35,
                 1e-6);
    assert_close("runtime_ray_3d_triangle_hit_object_texture_z",
                 hit.objectTextureCoord.z,
                 0.30,
                 1e-6);

    triangle.twoSided = true;
    ray = RuntimeRay3D_Make(vec3(0.25, 0.25, -3.0), vec3(0.0, 0.0, 2.0));
    ok = RuntimeRay3D_IntersectTriangle(&ray, &triangle, 12, 0.001, 10.0, &hit);
    assert_true("runtime_ray_3d_triangle_two_sided_back_hit_ok", ok);
    assert_true("runtime_ray_3d_triangle_two_sided_back_hit_ns_oriented",
                hit.normal.z < -0.9);
    assert_close("runtime_ray_3d_triangle_two_sided_back_hit_ngz",
                 hit.geometricNormal.z,
                 -1.0,
                 1e-6);
    assert_true("runtime_ray_3d_triangle_two_sided_back_hit_ns_hemisphere",
                vec3_dot(hit.geometricNormal, hit.shadingNormal) > 0.0);

    triangle.twoSided = false;
    ray = RuntimeRay3D_Make(vec3(0.25, 0.25, -3.0), vec3(0.0, 0.0, 2.0));
    ok = RuntimeRay3D_IntersectTriangle(&ray, &triangle, 13, 0.001, 10.0, &hit);
    assert_true("runtime_ray_3d_triangle_single_sided_back_hit_ok", ok);
    assert_true("runtime_ray_3d_triangle_single_sided_shading_normal_oriented",
                hit.normal.z < -0.9);
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

static int test_runtime_ray_3d_shading_normal_terminator_contract(void) {
    HitInfo3D hit = {0};
    Vec3 direction = vec3(0.0, 0.0, 1.0);
    Vec3 corrected;
    Vec3 incident;
    Vec3 reflected;

    hit.geometricNormal = vec3(0.0, 0.0, 1.0);
    hit.shadingNormal = vec3_normalize(vec3(0.0, 1.0, -0.25));
    hit.normal = hit.shadingNormal;
    corrected = HitInfo3D_ShadingNormalForDirection(&hit, direction);
    assert_true("runtime_ray_3d_terminator_preserves_lit_geometric_face",
                vec3_dot(corrected, direction) > 0.0);
    assert_true("runtime_ray_3d_terminator_keeps_geometric_hemisphere",
                vec3_dot(corrected, hit.geometricNormal) > 0.0);

    hit.shadingNormal = vec3_normalize(vec3(0.0, 0.4, 1.0));
    corrected = HitInfo3D_ShadingNormalForDirection(&hit, direction);
    assert_close("runtime_ray_3d_terminator_leaves_valid_smooth_normal_z",
                 corrected.z,
                 hit.shadingNormal.z,
                 1e-9);

    hit.geometricNormal = vec3(0.0, 0.0, 1.0);
    hit.shadingNormal = vec3_normalize(vec3(0.8, 0.0, 0.6));
    hit.normal = hit.shadingNormal;
    incident = vec3_scale(direction, -1.0);
    corrected = HitInfo3D_ShadingNormalForReflection(&hit, direction);
    reflected = vec3_normalize(vec3_sub(
        incident,
        vec3_scale(corrected, 2.0 * vec3_dot(corrected, incident))));
    assert_true("runtime_ray_3d_reflection_normal_exits_geometric_hemisphere",
                vec3_dot(reflected, hit.geometricNormal) > 1e-6);

    hit.shadingNormal = vec3_normalize(vec3(0.2, 0.0, 0.98));
    hit.normal = hit.shadingNormal;
    corrected = HitInfo3D_ShadingNormalForReflection(&hit, direction);
    assert_close("runtime_ray_3d_reflection_normal_preserves_valid_smooth_x",
                 corrected.x,
                 hit.shadingNormal.x,
                 1e-9);
    assert_close("runtime_ray_3d_reflection_normal_preserves_valid_smooth_z",
                 corrected.z,
                 hit.shadingNormal.z,
                 1e-9);
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

static int test_runtime_light_emitter_3d_light_set_sphere_hit_contract(void) {
    RuntimeScene3D scene;
    RuntimeLightSource3D light;
    Ray3D ray = {0};
    RuntimeLightEmitterHit3DResult result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeLightSource3D_Init(&light);
    snprintf(light.id, sizeof(light.id), "%s", "registered_sphere_light");
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE;
    light.origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT;
    light.enabled = true;
    light.position = vec3(0.0, 0.0, 0.0);
    light.radius = 0.75;
    light.intensity = 8.0;
    light.falloffDistance = 8.0;
    light.falloffMode = FORWARD_FALLOFF_MODE_NONE;
    assert_true("runtime_light_emitter_3d_light_set_append",
                RuntimeLightSet3D_Append(&scene.lightSet, &light, NULL));
    scene.hasLight = false;
    ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 3.0), vec3(0.0, 0.0, -1.0));

    ok = RuntimeLightEmitter3D_IntersectRay(&scene, &ray, 0.001, 10.0, &result);
    assert_true("runtime_light_emitter_3d_light_set_hit_ok", ok);
    assert_true("runtime_light_emitter_3d_light_set_hit_flag", result.hit);
    assert_close("runtime_light_emitter_3d_light_set_hit_t", result.t, 2.25, 1e-6);
    assert_close("runtime_light_emitter_3d_light_set_hit_radiance", result.radiance, 8.0, 1e-6);

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

static int test_runtime_visibility_3d_volume_transport_contract(void) {
    RuntimeScene3D scene;
    HitInfo3D surface_hit = {0};
    RuntimeLight3D light = {0};
    HitInfo3D blocker_hit = {0};
    RuntimeVisibility3DTransmittance transmittance = {0};
    double light_distance = 0.0;
    bool blocked = false;
    bool visible = false;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.volume.enabled = true;
    scene.volume.affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&scene.volume.grid,
                                       1u,
                                       2u,
                                       2u,
                                       6u,
                                       0.0,
                                       0u,
                                       0.02,
                                       vec3(-0.5, -0.5, 0.5),
                                       0.5,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    assert_true("runtime_visibility_3d_volume_layout_ok", ok);
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(
        &scene.volume,
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK);
    assert_true("runtime_visibility_3d_volume_alloc_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }
    for (uint64_t i = 0; i < scene.volume.grid.cellCount; ++i) {
        scene.volume.channels.density[i] = 0.5f;
        scene.volume.channels.solidMask[i] = 0u;
    }

    surface_hit.position = vec3(0.0, 0.0, 0.0);
    surface_hit.normal = vec3(0.0, 0.0, 1.0);
    light.position = vec3(0.0, 0.0, 3.0);

    blocked = RuntimeVisibility3D_TraceToLight(&scene,
                                               surface_hit.position,
                                               surface_hit.normal,
                                               light.position,
                                               &blocker_hit,
                                               &light_distance);
    transmittance = RuntimeVisibility3D_TransmittanceFromHitRGB(&scene, &surface_hit, &light);
    visible = RuntimeVisibility3D_HasLineOfSightFromHit(&scene, &surface_hit, &light);

    assert_true("runtime_visibility_3d_volume_not_geometry_blocked", !blocked);
    assert_true("runtime_visibility_3d_volume_visible", visible);
    assert_close("runtime_visibility_3d_volume_distance", light_distance, 3.0, 1e-6);
    assert_true("runtime_visibility_3d_volume_luma_reduced",
                transmittance.luma < 0.3 && transmittance.luma > 0.1);
    assert_close("runtime_visibility_3d_volume_rg_equal", transmittance.r, transmittance.g, 1e-9);
    assert_close("runtime_visibility_3d_volume_rb_equal", transmittance.r, transmittance.b, 1e-9);
    assert_true("runtime_visibility_3d_volume_reset_triangle", blocker_hit.triangleIndex == -1);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_visibility_3d_solid_dielectric_direct_path_policy(void) {
    RuntimeMaterialPayload3D payload = {0};

    payload.valid = true;
    payload.materialId = MATERIAL_PRESET_TRANSPARENT;
    payload.transparency = 0.9;
    payload.thinWalled = false;

    RuntimeVisibility3D_SetBlockSolidDielectricDirectPaths(false);
    assert_true("runtime_visibility_solid_policy_default_off",
                !RuntimeVisibility3D_BlockSolidDielectricDirectPathsEnabled());
    assert_true("runtime_visibility_solid_policy_legacy_allows",
                !RuntimeVisibility3D_ShouldBlockDirectPathThroughPayload(&payload));

    RuntimeVisibility3D_SetBlockSolidDielectricDirectPaths(true);
    assert_true("runtime_visibility_solid_policy_enabled",
                RuntimeVisibility3D_BlockSolidDielectricDirectPathsEnabled());
    assert_true("runtime_visibility_solid_policy_blocks_solid_glass",
                RuntimeVisibility3D_ShouldBlockDirectPathThroughPayload(&payload));

    payload.thinWalled = true;
    assert_true("runtime_visibility_solid_policy_preserves_thin_sheet",
                !RuntimeVisibility3D_ShouldBlockDirectPathThroughPayload(&payload));
    payload.thinWalled = false;
    payload.materialId = 0;
    assert_true("runtime_visibility_solid_policy_preserves_alpha_layer",
                !RuntimeVisibility3D_ShouldBlockDirectPathThroughPayload(&payload));

    RuntimeVisibility3D_SetBlockSolidDielectricDirectPaths(false);
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

int run_test_runtime_scene_3d_geometry_trace_suite(void) {
    RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);

    int before = test_support_failures();

    test_runtime_scene_3d_geometry_trace("test_runtime_ray_3d_triangle_intersection_contract");
    test_runtime_ray_3d_triangle_intersection_contract();
    test_runtime_ray_3d_shading_normal_terminator_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_ray_3d_scene_first_hit_contract");
    test_runtime_ray_3d_scene_first_hit_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_ray_3d_offset_contract");
    test_runtime_ray_3d_offset_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_light_emitter_3d_center_hit_contract");
    test_runtime_light_emitter_3d_center_hit_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_light_emitter_3d_light_set_sphere_hit_contract");
    test_runtime_light_emitter_3d_light_set_sphere_hit_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_light_emitter_3d_trace_geometry_tie_wins_contract");
    test_runtime_light_emitter_3d_trace_geometry_tie_wins_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_light_emitter_3d_trace_emitter_wins_contract");
    test_runtime_light_emitter_3d_trace_emitter_wins_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_light_emitter_3d_radial_falloff_contract");
    test_runtime_light_emitter_3d_radial_falloff_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_visibility_3d_visible_contract");
    test_runtime_visibility_3d_visible_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_visibility_3d_blocked_contract");
    test_runtime_visibility_3d_blocked_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_visibility_3d_volume_transport_contract");
    test_runtime_visibility_3d_volume_transport_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_visibility_3d_solid_dielectric_direct_path_policy");
    test_runtime_visibility_3d_solid_dielectric_direct_path_policy();
    test_runtime_scene_3d_geometry_trace("test_runtime_camera_projector_3d_center_ray_contract");
    test_runtime_camera_projector_3d_center_ray_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_camera_projector_3d_pitch_contract");
    test_runtime_camera_projector_3d_pitch_contract();
    test_runtime_scene_3d_geometry_trace("test_runtime_camera_projector_3d_zoom_contract");
    test_runtime_camera_projector_3d_zoom_contract();
    return test_support_failures() - before;
}
