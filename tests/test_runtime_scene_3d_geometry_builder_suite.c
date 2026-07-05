#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_volume_3d.h"
#include "test_runtime_scene_3d_geometry_internal.h"
#include "test_support.h"

static void test_runtime_scene_3d_builder_init_asymmetric_mesh_set(
    RayTracingRuntimeMeshAssetSet* set,
    RayTracingRuntimeMeshAssetInstance* instance,
    CoreMeshAssetRuntimeVertex* vertices,
    CoreMeshAssetRuntimeTriangle* triangles) {
    ray_tracing_runtime_mesh_asset_set_init(set);
    set->asset_count = 1;
    set->instance_count = 1;
    snprintf(set->assets[0].asset_id, sizeof(set->assets[0].asset_id), "asset_asym_pivot");
    snprintf(set->assets[0].path, sizeof(set->assets[0].path), "/private/tmp/asset_asym_pivot.runtime.json");
    set->assets[0].document.vertex_count = 3u;
    set->assets[0].document.vertices = vertices;
    set->assets[0].document.triangle_count = 1u;
    set->assets[0].document.triangles = triangles;
    set->assets[0].document.contract.local_bounds.min = (CoreObjectVec3){0.0, 0.0, 0.0};
    set->assets[0].document.contract.local_bounds.max = (CoreObjectVec3){2.0, 2.0, 0.0};
    vertices[0].position = (CoreObjectVec3){0.0, 0.0, 0.0};
    vertices[1].position = (CoreObjectVec3){2.0, 0.0, 0.0};
    vertices[2].position = (CoreObjectVec3){0.0, 2.0, 0.0};
    triangles[0].a = 0u;
    triangles[0].b = 1u;
    triangles[0].c = 2u;
    *instance = set->instances[0];
    snprintf(instance->object_id, sizeof(instance->object_id), "obj_asym_pivot");
    snprintf(instance->asset_id, sizeof(instance->asset_id), "asset_asym_pivot");
    instance->asset_index = 0;
    instance->scene_object_index = -1;
    instance->scale_x = 1.0;
    instance->scale_y = 1.0;
    instance->scale_z = 1.0;
    instance->rotation_z = 3.14159265358979323846 / 2.0;
    set->instances[0] = *instance;
}

static void test_runtime_scene_3d_builder_mesh_xy_bounds(
    const RuntimeScene3D* scene,
    double* out_min_x,
    double* out_max_x,
    double* out_min_y,
    double* out_max_y) {
    const RuntimeTriangle3D* tri = &scene->triangleMesh.triangles[0];
    Vec3 points[3] = {tri->p0, tri->p1, tri->p2};
    *out_min_x = *out_max_x = points[0].x;
    *out_min_y = *out_max_y = points[0].y;
    for (int i = 1; i < 3; ++i) {
        if (points[i].x < *out_min_x) *out_min_x = points[i].x;
        if (points[i].x > *out_max_x) *out_max_x = points[i].x;
        if (points[i].y < *out_min_y) *out_min_y = points[i].y;
        if (points[i].y > *out_max_y) *out_max_y = points[i].y;
    }
}

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

static int test_runtime_scene_3d_builder_mesh_bounds_center_rotation_keeps_center_stable(void) {
    RayTracingRuntimeMeshAssetSet set;
    RayTracingRuntimeMeshAssetInstance instance;
    CoreMeshAssetRuntimeVertex vertices[3];
    CoreMeshAssetRuntimeTriangle triangles[1];
    RuntimeScene3D scene;
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    bool ok = false;

    test_runtime_scene_3d_builder_init_asymmetric_mesh_set(&set,
                                                          &instance,
                                                          vertices,
                                                          triangles);
    set.instances[0].rotation_pivot_policy =
        RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER;
    RuntimeScene3D_Init(&scene);
    ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
    assert_true("runtime_scene_3d_builder_mesh_bounds_center_append_ok", ok);
    assert_true("runtime_scene_3d_builder_mesh_bounds_center_triangle_count",
                scene.triangleMesh.triangleCount == 1);
    test_runtime_scene_3d_builder_mesh_xy_bounds(&scene, &min_x, &max_x, &min_y, &max_y);
    assert_close("runtime_scene_3d_builder_mesh_bounds_center_center_x",
                 (min_x + max_x) * 0.5,
                 1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_mesh_bounds_center_center_y",
                 (min_y + max_y) * 0.5,
                 1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_mesh_bounds_center_source_vertex_x",
                 vertices[1].position.x,
                 2.0,
                 1e-9);
    assert_true("runtime_scene_3d_builder_mesh_bounds_center_source_index",
                triangles[0].b == 1u);
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_builder_mesh_authored_origin_rotation_remains_compatible(void) {
    RayTracingRuntimeMeshAssetSet set;
    RayTracingRuntimeMeshAssetInstance instance;
    CoreMeshAssetRuntimeVertex vertices[3];
    CoreMeshAssetRuntimeTriangle triangles[1];
    RuntimeScene3D scene;
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    bool ok = false;

    test_runtime_scene_3d_builder_init_asymmetric_mesh_set(&set,
                                                          &instance,
                                                          vertices,
                                                          triangles);
    set.instances[0].rotation_pivot_policy =
        RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_AUTHORED_ORIGIN;
    RuntimeScene3D_Init(&scene);
    ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
    assert_true("runtime_scene_3d_builder_mesh_authored_origin_append_ok", ok);
    assert_true("runtime_scene_3d_builder_mesh_authored_origin_triangle_count",
                scene.triangleMesh.triangleCount == 1);
    test_runtime_scene_3d_builder_mesh_xy_bounds(&scene, &min_x, &max_x, &min_y, &max_y);
    assert_close("runtime_scene_3d_builder_mesh_authored_origin_center_x",
                 (min_x + max_x) * 0.5,
                 -1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_mesh_authored_origin_center_y",
                 (min_y + max_y) * 0.5,
                 1.0,
                 1e-9);
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_builder_mesh_custom_rotation_pivot(void) {
    RayTracingRuntimeMeshAssetSet set;
    RayTracingRuntimeMeshAssetInstance instance;
    CoreMeshAssetRuntimeVertex vertices[3];
    CoreMeshAssetRuntimeTriangle triangles[1];
    RuntimeScene3D scene;
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    bool ok = false;

    test_runtime_scene_3d_builder_init_asymmetric_mesh_set(&set,
                                                          &instance,
                                                          vertices,
                                                          triangles);
    set.instances[0].rotation_pivot_policy =
        RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM;
    set.instances[0].rotation_pivot_x = 2.0;
    set.instances[0].rotation_pivot_y = 0.0;
    set.instances[0].rotation_pivot_z = 0.0;
    RuntimeScene3D_Init(&scene);
    ok = RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, &set);
    assert_true("runtime_scene_3d_builder_mesh_custom_pivot_append_ok", ok);
    assert_true("runtime_scene_3d_builder_mesh_custom_pivot_triangle_count",
                scene.triangleMesh.triangleCount == 1);
    test_runtime_scene_3d_builder_mesh_xy_bounds(&scene, &min_x, &max_x, &min_y, &max_y);
    assert_close("runtime_scene_3d_builder_mesh_custom_pivot_min_x",
                 min_x,
                 0.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_mesh_custom_pivot_max_x",
                 max_x,
                 2.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_mesh_custom_pivot_min_y",
                 min_y,
                 -2.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_mesh_custom_pivot_max_y",
                 max_y,
                 0.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_mesh_custom_pivot_source_vertex_x",
                 vertices[1].position.x,
                 2.0,
                 1e-9);
    assert_true("runtime_scene_3d_builder_mesh_custom_pivot_source_index",
                triangles[0].b == 1u);
    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_builder_plane_position_owns_frame_origin(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_plane_position_owner\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"moved_floor\","
            "\"object_type\":\"plane_primitive\","
            "\"primitive\":{\"kind\":\"plane_primitive\",\"width\":5.0,\"height\":4.0,"
            "\"frame\":{\"origin\":{\"x\":9.0,\"y\":8.0,\"z\":7.0},"
            "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}}},"
            "\"transform\":{"
              "\"position\":{\"x\":1.0,\"y\":2.0,\"z\":-0.58},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
            "}"
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
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_plane_position_owner_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene);
    assert_true("runtime_scene_3d_builder_plane_position_owner_build_ok", ok);
    assert_true("runtime_scene_3d_builder_plane_position_owner_primitive_count", scene.primitiveCount == 1);
    assert_true("runtime_scene_3d_builder_plane_position_owner_kind",
                scene.primitives[0].kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE);
    assert_close("runtime_scene_3d_builder_plane_position_owner_origin_x",
                 scene.primitives[0].shape.plane.origin.x,
                 1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_position_owner_origin_y",
                 scene.primitives[0].shape.plane.origin.y,
                 2.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_position_owner_origin_z",
                 scene.primitives[0].shape.plane.origin.z,
                 -0.58,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_position_owner_width",
                 scene.primitives[0].shape.plane.width,
                 5.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_position_owner_height",
                 scene.primitives[0].shape.plane.height,
                 4.0,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_builder_plane_scale_rotation_owns_seed_dimensions_and_basis(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_plane_scale_rotation_owner\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"scaled_rotated_floor\","
            "\"object_type\":\"plane_primitive\","
            "\"primitive\":{\"kind\":\"plane_primitive\",\"width\":5.0,\"height\":4.0,"
            "\"frame\":{\"origin\":{\"x\":9.0,\"y\":8.0,\"z\":7.0},"
            "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}}},"
            "\"transform\":{"
              "\"position\":{\"x\":1.0,\"y\":2.0,\"z\":-0.58},"
              "\"scale\":{\"x\":1.5,\"y\":0.5,\"z\":3.0},"
              "\"rotation\":{\"x\":0.0,\"y\":0.0,\"z\":90.0}"
            "}"
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
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_plane_scale_rotation_owner_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene);
    assert_true("runtime_scene_3d_builder_plane_scale_rotation_owner_build_ok", ok);
    assert_true("runtime_scene_3d_builder_plane_scale_rotation_owner_primitive_count", scene.primitiveCount == 1);
    assert_true("runtime_scene_3d_builder_plane_scale_rotation_owner_kind",
                scene.primitives[0].kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_origin_x",
                 scene.primitives[0].shape.plane.origin.x,
                 1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_origin_y",
                 scene.primitives[0].shape.plane.origin.y,
                 2.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_origin_z",
                 scene.primitives[0].shape.plane.origin.z,
                 -0.58,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_width",
                 scene.primitives[0].shape.plane.width,
                 7.5,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_height",
                 scene.primitives[0].shape.plane.height,
                 2.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_axis_u_x",
                 scene.primitives[0].shape.plane.axisU.x,
                 0.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_axis_u_y",
                 scene.primitives[0].shape.plane.axisU.y,
                 1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_axis_v_x",
                 scene.primitives[0].shape.plane.axisV.x,
                 -1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_axis_v_y",
                 scene.primitives[0].shape.plane.axisV.y,
                 0.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_plane_scale_rotation_owner_normal_z",
                 scene.primitives[0].shape.plane.normal.z,
                 1.0,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    return 0;
}

static int test_runtime_scene_3d_builder_rotates_rect_prism_seed_basis(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_rect_prism_rotation\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"rotated_block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"primitive\":{\"kind\":\"rect_prism_primitive\",\"width\":2.0,\"height\":4.0,\"depth\":6.0},"
            "\"transform\":{"
              "\"position\":{\"x\":1.0,\"y\":2.0,\"z\":3.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0},"
              "\"rotation\":{\"x\":0.0,\"y\":0.0,\"z\":90.0}"
            "}"
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
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_rect_prism_rotation_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene);
    assert_true("runtime_scene_3d_builder_rect_prism_rotation_build_ok", ok);
    assert_true("runtime_scene_3d_builder_rect_prism_rotation_primitive_count", scene.primitiveCount == 1);
    assert_true("runtime_scene_3d_builder_rect_prism_rotation_kind",
                scene.primitives[0].kind == RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM);
    assert_close("runtime_scene_3d_builder_rect_prism_rotation_origin_x",
                 scene.primitives[0].shape.rectPrism.origin.x,
                 1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_rect_prism_rotation_width",
                 scene.primitives[0].shape.rectPrism.width,
                 2.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_rect_prism_rotation_height",
                 scene.primitives[0].shape.rectPrism.height,
                 4.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_rect_prism_rotation_depth",
                 scene.primitives[0].shape.rectPrism.depth,
                 6.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_rect_prism_rotation_axis_u_x",
                 scene.primitives[0].shape.rectPrism.axisU.x,
                 0.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_rect_prism_rotation_axis_u_y",
                 scene.primitives[0].shape.rectPrism.axisU.y,
                 1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_rect_prism_rotation_axis_v_x",
                 scene.primitives[0].shape.rectPrism.axisV.x,
                 -1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_rect_prism_rotation_axis_v_y",
                 scene.primitives[0].shape.rectPrism.axisV.y,
                 0.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_rect_prism_rotation_normal_z",
                 scene.primitives[0].shape.rectPrism.normal.z,
                 1.0,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_scene_3d_volume_contract_defaults(void) {
    RuntimeScene3D scene;

    RuntimeScene3D_Init(&scene);

    assert_true("runtime_scene_3d_volume_contract_renderer_owns_geometry",
                scene.ownership.rendererOwnsGeometryTruth);
    assert_true("runtime_scene_3d_volume_contract_renderer_owns_volume",
                scene.ownership.rendererOwnsVolumeAttachmentTruth);
    assert_true("runtime_scene_3d_volume_contract_attachment_optional",
                scene.ownership.volumeAttachmentIsOptional);
    assert_true("runtime_scene_3d_volume_contract_sources_separate",
                scene.ownership.geometryAndVolumeSourcesRemainSeparate);
    assert_true("runtime_scene_3d_volume_contract_legacy_fluid_separate",
                scene.ownership.legacyPlanarFluidOverlayRemainsSeparate);
    assert_true("runtime_scene_3d_volume_contract_source_none",
                scene.volume.sourceKind == RUNTIME_VOLUME_3D_SOURCE_NONE);
    assert_true("runtime_scene_3d_volume_contract_disabled",
                !scene.volume.enabled);
    assert_true("runtime_scene_3d_volume_contract_no_data",
                !scene.volume.hasData);
    assert_true("runtime_scene_3d_volume_contract_affects_lighting_default",
                scene.volume.affectsLighting);
    assert_true("runtime_scene_3d_volume_contract_debug_default_off",
                !scene.volume.debugOverlayEnabled);

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
        assert_true("runtime_scene_3d_builder_ps4d_plane_triangle_two_sided",
                    triangle->twoSided);
        assert_close("runtime_scene_3d_builder_ps4d_plane_triangle_normal_z",
                     triangle->normal.z,
                     1.0,
                     1e-9);
    }
    for (int i = 2; i < scene.triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D *triangle = &scene.triangleMesh.triangles[i];
        assert_true("runtime_scene_3d_builder_ps4d_rect_prism_triangle_single_sided",
                    !triangle->twoSided);
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
        "\"objects\":["
          "{"
            "\"object_id\":\"motion_floor\","
            "\"object_type\":\"plane_primitive\","
            "\"primitive\":{\"kind\":\"plane_primitive\",\"width\":4.0,\"height\":4.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
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
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_TOP_FILL;
    animSettings.topFillStrength = 2.5;
    animSettings.environmentBrightness = 96.0;
    animSettings.environmentPreset = ENVIRONMENT_PRESET_WARM_SKY;
    animSettings.environmentBackgroundLightingAuthored = true;
    animSettings.environmentBackgroundBrightnessAuto = false;
    animSettings.environmentBackgroundBrightness = 0.72;
    animSettings.environmentBackgroundColorR = 0.75;
    animSettings.environmentBackgroundColorG = 0.85;
    animSettings.environmentBackgroundColorB = 1.0;

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
    assert_true("runtime_scene_3d_builder_samples_retains_primitive", scene.primitiveCount == 1);
    assert_true("runtime_scene_3d_builder_samples_retains_triangles", scene.triangleMesh.triangleCount == 2);
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
    assert_true("runtime_scene_3d_builder_samples_environment_mode",
                scene.environment.lightMode == ENVIRONMENT_LIGHT_MODE_TOP_FILL);
    assert_close("runtime_scene_3d_builder_samples_environment_top_fill_strength",
                 scene.environment.topFillIntensity,
                 2.5,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_environment_ambient_strength",
                 scene.environment.ambientIntensity,
                 96.0 / 255.0,
                 1e-6);
    assert_true("runtime_scene_3d_builder_samples_environment_preset",
                scene.environment.preset == ENVIRONMENT_PRESET_WARM_SKY);
    assert_true("runtime_scene_3d_builder_samples_environment_background_explicit",
                !scene.environment.backgroundIntensityDerivedFromAmbient);
    assert_close("runtime_scene_3d_builder_samples_environment_background_brightness",
                 scene.environment.backgroundIntensity,
                 0.72,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_environment_background_top_red",
                 scene.environment.backgroundTopColor.x,
                 0.90 * 0.75,
                 1e-6);
    assert_close("runtime_scene_3d_builder_samples_environment_background_top_blue",
                 scene.environment.backgroundTopColor.z,
                 0.62,
                 1e-6);
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

static int test_runtime_scene_3d_builder_imports_authored_light_set(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_light_set\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":["
          "{"
            "\"id\":\"key_red\","
            "\"kind\":\"sphere\","
            "\"position\":{\"x\":1.0,\"y\":2.0,\"z\":3.0},"
            "\"radius\":0.5,"
            "\"color\":{\"r\":1.0,\"g\":0.0,\"b\":0.0},"
            "\"intensity\":4.0,"
            "\"falloff_distance\":9.0,"
            "\"enabled\":true"
          "},"
          "{"
            "\"id\":\"panel_blue\","
            "\"kind\":\"rect\","
            "\"position\":{\"x\":-1.0,\"y\":2.5,\"z\":4.0},"
            "\"width\":2.0,"
            "\"height\":1.0,"
            "\"color\":{\"x\":0.0,\"y\":0.0,\"z\":0.5},"
            "\"intensity\":1.5,"
            "\"enabled\":true"
          "},"
          "{"
            "\"id\":\"disabled_disk\","
            "\"kind\":\"disk\","
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"width\":3.0,"
            "\"intensity\":8.0,"
            "\"enabled\":false"
          "}"
        "],"
        "\"cameras\":[{\"position\":{\"x\":4.0,\"y\":5.0,\"z\":6.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridge3DLightSeedState light_state = {0};
    RuntimeScene3D scene;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    animSettings.forwardDecay = 7.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_light_set_apply_ok", ok);
    assert_true("runtime_scene_3d_builder_light_set_preflight_count", summary.light_count == 3);
    runtime_scene_bridge_get_last_3d_light_seed_state(&light_state);
    assert_true("runtime_scene_3d_builder_light_set_seed_valid", light_state.valid);
    assert_true("runtime_scene_3d_builder_light_set_seed_count", light_state.light_count == 3);
    assert_true("runtime_scene_3d_builder_light_set_seed_enabled", light_state.enabled_count == 2);
    assert_true("runtime_scene_3d_builder_light_set_seed_sphere", light_state.sphere_count == 1);
    assert_true("runtime_scene_3d_builder_light_set_seed_rect", light_state.rect_count == 1);
    assert_true("runtime_scene_3d_builder_light_set_seed_disk", light_state.disk_count == 1);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.25);
    assert_true("runtime_scene_3d_builder_light_set_build_ok", ok);
    assert_true("runtime_scene_3d_builder_light_set_scene_has_light", scene.hasLight);
    assert_true("runtime_scene_3d_builder_light_set_scene_count", scene.lightSet.lightCount == 3);
    assert_true("runtime_scene_3d_builder_light_set_scene_enabled", scene.lightSet.enabledCount == 2);
    assert_true("runtime_scene_3d_builder_light_set_compat_first",
                strcmp(scene.lightSet.lights[0].id, "key_red") == 0);
    assert_close("runtime_scene_3d_builder_light_set_first_x",
                 scene.light.position.x,
                 2.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_light_set_first_z",
                 scene.light.position.z,
                 6.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_light_set_first_radius",
                 scene.light.radius,
                 1.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_light_set_first_intensity",
                 scene.light.intensity,
                 4.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_light_set_rect_width",
                 scene.lightSet.lights[1].width,
                 4.0,
                 1e-9);
    assert_close("runtime_scene_3d_builder_light_set_rect_blue",
                 scene.lightSet.lights[1].color.z,
                 0.5,
                 1e-9);
    assert_true("runtime_scene_3d_builder_light_set_disabled_kept",
                !scene.lightSet.lights[2].enabled);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_scene_3d_builder_moves_first_registered_light_from_path(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_light_set_motion\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":["
          "{"
            "\"id\":\"key_moving\","
            "\"kind\":\"sphere\","
            "\"position\":{\"x\":100.0,\"y\":100.0,\"z\":20.0},"
            "\"radius\":0.5,"
            "\"intensity\":2.0,"
            "\"enabled\":true"
          "},"
          "{"
            "\"id\":\"fill_static\","
            "\"kind\":\"sphere\","
            "\"position\":{\"x\":-4.0,\"y\":5.0,\"z\":6.0},"
            "\"radius\":0.75,"
            "\"intensity\":1.0,"
            "\"enabled\":true"
          "}"
        "],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"light_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{\"x\":0.0,\"y\":0.0,\"rotation\":0.0,\"handleLink\":false,"
                  "\"velocity1\":{\"vx\":2.0,\"vy\":0.0}},"
                  "{\"x\":10.0,\"y\":4.0,\"rotation\":0.0,\"handleLink\":false,"
                  "\"velocity2\":{\"vx\":-2.0,\"vy\":0.0}}"
                "]"
              "},"
              "\"light_path_depth\":{"
                "\"points\":["
                  "{\"z\":2.0,\"velocity1\":{\"vz\":1.0}},"
                  "{\"z\":8.0,\"velocity2\":{\"vz\":-1.0}}"
                "]"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridge3DPrimitiveSeedState seed_state = {0};
    RuntimeScene3D scene;
    Point expected_light_xy = {0.0, 0.0};
    double expected_light_z = 0.0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_light_set_motion_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightRadius = 1.25;
    animSettings.lightIntensity = 9.0;
    animSettings.forwardDecay = 12.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    expected_light_xy = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, 0.5);
    expected_light_z =
        CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath,
                                            &sceneSettings.bezierPath3D,
                                            0.5);

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
    ok = RuntimeScene3DBuilder_BuildFromPrimitiveSeedStateAtT(&scene, &seed_state, 0.5);
    assert_true("runtime_scene_3d_builder_light_set_motion_build_ok", ok);
    assert_true("runtime_scene_3d_builder_light_set_motion_count", scene.lightSet.lightCount == 2);
    assert_close("runtime_scene_3d_builder_light_set_motion_first_x",
                 scene.lightSet.lights[0].position.x,
                 expected_light_xy.x,
                 1e-6);
    assert_close("runtime_scene_3d_builder_light_set_motion_first_y",
                 scene.lightSet.lights[0].position.y,
                 expected_light_xy.y,
                 1e-6);
    assert_close("runtime_scene_3d_builder_light_set_motion_first_z",
                 scene.lightSet.lights[0].position.z,
                 expected_light_z,
                 1e-6);
    assert_close("runtime_scene_3d_builder_light_set_motion_first_radius",
                 scene.lightSet.lights[0].radius,
                 1.25,
                 1e-6);
    assert_close("runtime_scene_3d_builder_light_set_motion_first_intensity",
                 scene.lightSet.lights[0].intensity,
                 9.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_light_set_motion_static_x",
                 scene.lightSet.lights[1].position.x,
                 -4.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_light_set_motion_static_y",
                 scene.lightSet.lights[1].position.y,
                 5.0,
                 1e-6);
    assert_close("runtime_scene_3d_builder_light_set_motion_compat_x",
                 scene.light.position.x,
                 scene.lightSet.lights[0].position.x,
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
        "\"lights\":[{\"light_id\":\"legacy_directional_key\",\"light_type\":\"directional\","
        "\"position\":{\"x\":-2.0,\"y\":3.0,\"z\":4.0}}],"
        "\"cameras\":[{\"position\":{\"x\":7.0,\"y\":8.0,\"z\":9.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    bool ok = false;

    animSettings.lightIntensity = 11.0;
    animSettings.lightRadius = 1.5;
    animSettings.forwardDecay = 9.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_NONE;
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

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.65);
    assert_true("runtime_scene_3d_builder_camera_fallback_build_ok", ok);
    assert_true("runtime_scene_3d_builder_camera_fallback_has_light", scene.hasLight);
    assert_true("runtime_scene_3d_builder_camera_fallback_has_camera", scene.hasCamera);
    assert_true("runtime_scene_3d_builder_camera_fallback_light_set_count",
                scene.lightSet.lightCount == 1);
    assert_true("runtime_scene_3d_builder_camera_fallback_light_set_sphere",
                scene.lightSet.lights[0].kind == RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE);
    assert_close("runtime_scene_3d_builder_camera_fallback_light_set_radius",
                 scene.lightSet.lights[0].radius,
                 1.5,
                 1e-6);
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

static int test_runtime_scene_3d_builder_applies_authored_camera_focus_target(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_builder_camera_focus_target\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":2.0,\"z\":3.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"camera_focus_target\":{"
                "\"x\":0.5,\"y\":1.5,\"z\":1.25"
              "},"
              "\"camera_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{"
                    "\"x\":-4.0,\"y\":-6.0,\"rotation\":0.20,\"handleLink\":false,"
                    "\"velocity1\":{\"vx\":1.0,\"vy\":0.5}"
                  "},"
                  "{"
                    "\"x\":-3.0,\"y\":-5.0,\"rotation\":0.25,\"handleLink\":false,"
                    "\"velocity2\":{\"vx\":-1.0,\"vy\":-0.5}"
                  "}"
                "]"
              "},"
              "\"camera_path_depth\":{"
                "\"points\":["
                  "{\"z\":2.0,\"lookPitch\":0.05,\"velocity1\":{\"vz\":0.25}},"
                  "{\"z\":2.5,\"lookPitch\":0.10,\"velocity2\":{\"vz\":-0.25}}"
                "]"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    Point expected_camera_xy = {0.0, 0.0};
    double expected_camera_z = 0.0;
    double expected_camera_yaw = 0.0;
    double expected_camera_pitch = 0.0;
    double target_x = 0.5;
    double target_y = 1.5;
    double target_z = 1.25;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    double horizontal = 0.0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_3d_builder_camera_focus_target_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    expected_camera_xy = GetPositionAlongPathNormalized(&sceneSettings.cameraPath, 0.5);
    expected_camera_z =
        CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath, &sceneSettings.cameraPath3D, 0.5);
    dx = target_x - expected_camera_xy.x;
    dy = target_y - expected_camera_xy.y;
    dz = target_z - expected_camera_z;
    horizontal = hypot(dx, dy);
    expected_camera_yaw = atan2(dx, -dy);
    expected_camera_pitch = atan2(dz, horizontal);

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.5);
    assert_true("runtime_scene_3d_builder_camera_focus_target_build_ok", ok);
    assert_true("runtime_scene_3d_builder_camera_focus_target_has_camera", scene.hasCamera);
    assert_close("runtime_scene_3d_builder_camera_focus_target_camera_x",
                 scene.camera.position.x,
                 expected_camera_xy.x,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_focus_target_camera_y",
                 scene.camera.position.y,
                 expected_camera_xy.y,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_focus_target_camera_z",
                 scene.camera.position.z,
                 expected_camera_z,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_focus_target_camera_yaw",
                 scene.camera.rotation,
                 expected_camera_yaw,
                 1e-6);
    assert_close("runtime_scene_3d_builder_camera_focus_target_camera_pitch",
                 scene.camera.lookPitch,
                 expected_camera_pitch,
                 1e-6);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

int run_test_runtime_scene_3d_geometry_builder_suite(void) {
    int before = test_support_failures();

    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_volume_contract_defaults");
    test_runtime_scene_3d_volume_contract_defaults();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_uses_retained_seed_scope");
    test_runtime_scene_3d_builder_uses_retained_seed_scope();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_mesh_bounds_center_rotation_keeps_center_stable");
    test_runtime_scene_3d_builder_mesh_bounds_center_rotation_keeps_center_stable();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_mesh_authored_origin_rotation_remains_compatible");
    test_runtime_scene_3d_builder_mesh_authored_origin_rotation_remains_compatible();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_mesh_custom_rotation_pivot");
    test_runtime_scene_3d_builder_mesh_custom_rotation_pivot();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_plane_position_owns_frame_origin");
    test_runtime_scene_3d_builder_plane_position_owns_frame_origin();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_plane_scale_rotation_owns_seed_dimensions_and_basis");
    test_runtime_scene_3d_builder_plane_scale_rotation_owns_seed_dimensions_and_basis();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_builds_ps4d_triangle_scene");
    test_runtime_scene_3d_builder_builds_ps4d_triangle_scene();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_promotes_authored_light_camera_samples");
    test_runtime_scene_3d_builder_promotes_authored_light_camera_samples();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_imports_authored_light_set");
    test_runtime_scene_3d_builder_imports_authored_light_set();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_moves_first_registered_light_from_path");
    test_runtime_scene_3d_builder_moves_first_registered_light_from_path();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_falls_back_to_seeded_camera_state");
    test_runtime_scene_3d_builder_falls_back_to_seeded_camera_state();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_applies_authored_camera_focus_target");
    test_runtime_scene_3d_builder_applies_authored_camera_focus_target();
    test_runtime_scene_3d_geometry_trace("test_runtime_scene_3d_builder_rotates_rect_prism_seed_basis");
    test_runtime_scene_3d_builder_rotates_rect_prism_seed_basis();
    return test_support_failures() - before;
}
