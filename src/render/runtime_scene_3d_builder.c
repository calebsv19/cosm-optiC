#include "render/runtime_scene_3d_builder_internal.h"

static RuntimeLight3D runtime_scene_3d_builder_compat_light_from_source(
    const RuntimeLightSource3D* source) {
    RuntimeLight3D light = {0};
    if (!source) return light;
    light.position = source->position;
    light.radius = source->radius;
    if (light.radius <= 0.0) {
        if (source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_DISK && source->width > 0.0) {
            light.radius = source->width * 0.5;
        } else if (source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_RECT &&
                   (source->width > 0.0 || source->height > 0.0)) {
            light.radius = 0.5 * sqrt(source->width * source->width +
                                      source->height * source->height);
        }
    }
    light.intensity = source->intensity;
    light.falloffDistance = source->falloffDistance;
    light.falloffMode = source->falloffMode;
    return light;
}

static RuntimeLightSource3D runtime_scene_3d_builder_normalized_light_source(
    const RuntimeLightSource3D* source) {
    RuntimeLightSource3D normalized = {0};
    double authored_radius = animSettings.lightRadius;

    if (!source) return normalized;
    normalized = *source;
    if (normalized.radius <= 0.0 &&
        (normalized.kind == RUNTIME_LIGHT_SOURCE_3D_KIND_POINT ||
         normalized.kind == RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE) &&
        authored_radius > 0.0) {
        normalized.radius = authored_radius;
        normalized.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE;
    }
    normalized.falloffMode = animSettings.forwardFalloffMode;
    return normalized;
}

static bool runtime_scene_3d_builder_apply_light_seed_state(RuntimeScene3D* scene) {
    RuntimeSceneBridge3DLightSeedState light_state = {0};
    if (!scene) return false;
    runtime_scene_bridge_get_last_3d_light_seed_state(&light_state);
    if (!light_state.valid || light_state.light_count <= 0) {
        return true;
    }
    RuntimeLightSet3D_Reset(&scene->lightSet);
    for (int i = 0; i < light_state.light_count; ++i) {
        RuntimeLightSource3D source =
            runtime_scene_3d_builder_normalized_light_source(&light_state.lights[i]);
        if (!RuntimeLightSet3D_Append(&scene->lightSet, &source, NULL)) {
            runtime_scene_3d_builder_set_diag("authored light seed build failed: append failed");
            return false;
        }
    }
    if (scene->lightSet.lightCount > 0) {
        const RuntimeLightSource3D* first_enabled =
            RuntimeLightSet3D_GetEnabled(&scene->lightSet, 0);
        if (!first_enabled) first_enabled = &scene->lightSet.lights[0];
        scene->light = runtime_scene_3d_builder_compat_light_from_source(first_enabled);
        scene->hasLight = first_enabled->enabled;
    }
    return true;
}

static bool runtime_scene_3d_builder_apply_authored_samples(RuntimeScene3D* scene,
                                                            double normalized_t) {
    RuntimeLight3D light = {0};
    RuntimeCamera3D camera = {0};
    RuntimeSceneBridge3DScaffoldState scaffold = {0};
    if (!scene) return false;

    RuntimeEnvironment3D_ResolveFromAnimationConfig(&scene->environment, &animSettings);

    if (!runtime_scene_3d_builder_apply_light_seed_state(scene)) {
        return false;
    }
    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    if (scaffold.has_light_path && RuntimeScene3DSampleAuthoredLight(normalized_t, &light)) {
        scene->light = light;
        scene->hasLight = true;
        if (scene->lightSet.lightCount > 0) {
            if (!RuntimeLightSet3D_UpdateFirstEnabledFromCompatibilityLight(&scene->lightSet,
                                                                            &scene->light)) {
                runtime_scene_3d_builder_set_diag("authored light seed motion update failed");
                return false;
            }
        } else if (!RuntimeLightSet3D_BuildFromCompatibilityLight(&scene->lightSet,
                                                                  &scene->light,
                                                                  scene->hasLight)) {
            runtime_scene_3d_builder_set_diag("compat light seed build failed");
            return false;
        }
    }
    if (RuntimeScene3DSampleAuthoredCamera(normalized_t, &camera)) {
        scene->camera = camera;
        scene->hasCamera = true;
    }
    return true;
}

static bool runtime_scene_3d_builder_build_from_primitive_seed_state_at_t(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state,
    double normalized_t,
    bool require_ready_bvh) {
    int retained_primitive_count = 0;
    int expected_triangle_count = 0;
    struct timespec stage_start = {0};

    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    if (!scene || !seed_state || !seed_state->valid) {
        runtime_scene_3d_builder_set_diag("primitive seed build failed: invalid seed state");
        return false;
    }

    RuntimeScene3D_Reset(scene);
    if (!runtime_scene_3d_builder_apply_authored_samples(scene, normalized_t)) {
        return false;
    }
    retained_primitive_count = seed_state->primitive_count;
    for (int i = 0; i < seed_state->primitive_count; ++i) {
        RuntimePrimitive3DKind kind =
            runtime_scene_3d_builder_map_kind(seed_state->primitives[i].kind);
        if (!RuntimePrimitive3DKindSupportedByR0(kind)) continue;
        expected_triangle_count += runtime_scene_3d_builder_triangle_count_for_kind(kind);
    }

    if (retained_primitive_count <= 0) return true;
    if (!runtime_scene_3d_builder_reserve_primitives(scene, retained_primitive_count)) {
        runtime_scene_3d_builder_set_diag("primitive seed build failed: primitive reserve failed");
        return false;
    }
    if (!runtime_scene_3d_builder_reserve_triangles(scene, expected_triangle_count)) {
        runtime_scene_3d_builder_set_diag("primitive seed build failed: triangle reserve failed");
        return false;
    }

    for (int i = 0; i < seed_state->primitive_count; ++i) {
        RuntimePrimitive3DKind kind =
            runtime_scene_3d_builder_map_kind(seed_state->primitives[i].kind);
        RuntimePrimitive3D* primitive = NULL;
        if (!RuntimePrimitive3DKindSupportedByR0(kind)) continue;

        primitive = &scene->primitives[scene->primitiveCount];
        runtime_scene_3d_builder_fill_primitive(primitive, &seed_state->primitives[i]);
        if (!runtime_scene_3d_builder_append_triangles(scene, scene->primitiveCount, primitive)) {
            runtime_scene_3d_builder_set_diag("primitive seed build failed: triangle append failed");
            RuntimeScene3D_Reset(scene);
            return false;
        }
        scene->primitiveCount += 1;
    }

    if (require_ready_bvh) {
        if (!runtime_scene_3d_builder_rebuild_tlas(scene) ||
            !runtime_scene_3d_builder_rebuild_bvh(scene)) {
            RuntimeScene3D_Reset(scene);
            return false;
        }
    }
    runtime_scene_3d_builder_timing_mutable()->primitive_seed_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
    return true;
}

bool RuntimeScene3DBuilder_BuildFromPrimitiveSeedStateAtT(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state,
    double normalized_t) {
    return runtime_scene_3d_builder_build_from_primitive_seed_state_at_t(scene,
                                                                         seed_state,
                                                                         normalized_t,
                                                                         true);
}

bool RuntimeScene3DBuilder_BuildFromPrimitiveSeedState(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state) {
    return RuntimeScene3DBuilder_BuildFromPrimitiveSeedStateAtT(scene, seed_state, 0.0);
}

bool RuntimeScene3DBuilder_BuildFromBridgeSeeds(RuntimeScene3D* scene) {
    return RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(scene, 0.0);
}

bool RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(RuntimeScene3D* scene, double normalized_t) {
    RuntimeSceneBridge3DPrimitiveSeedState seed_state = {0};
    const RayTracingRuntimeMeshAssetSet* mesh_assets = NULL;
    int mesh_instance_count = 0;
    int mesh_asset_count = 0;
    struct timespec total_start = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &total_start);
    runtime_scene_3d_builder_set_diag("ok");
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
    if (!runtime_scene_3d_builder_build_from_primitive_seed_state_at_t(scene,
                                                                       &seed_state,
                                                                       normalized_t,
                                                                       false)) {
        char diag[2048];
        const char* lower_diag = RuntimeScene3DBuilder_LastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "bridge primitive seed build failed: seed_valid=%s seed_primitive_count=%d seed_plane_count=%d seed_rect_prism_count=%d seed_excluded_count=%d lower=%s",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 seed_state.plane_primitive_count,
                 seed_state.rect_prism_primitive_count,
                 seed_state.excluded_primitive_count,
                 lower_diag ? lower_diag : "unknown");
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    mesh_assets = ray_tracing_runtime_mesh_assets_last();
    mesh_instance_count = mesh_assets ? mesh_assets->instance_count : -1;
    mesh_asset_count = mesh_assets ? mesh_assets->asset_count : -1;
    if (!runtime_scene_3d_builder_append_mesh_asset_set(scene, mesh_assets, false)) {
        char diag[2048];
        const char* lower_diag = RuntimeScene3DBuilder_LastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "bridge mesh append failed: seed_valid=%s seed_primitive_count=%d mesh_instance_count=%d mesh_asset_count=%d lower=%s",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 mesh_instance_count,
                 mesh_asset_count,
                 lower_diag ? lower_diag : "unknown");
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    if (!scene) {
        runtime_scene_3d_builder_set_diag("bridge scene build failed: null scene");
        return false;
    }
    if (scene->primitiveCount <= 0 || scene->triangleMesh.triangleCount <= 0) {
        if (scene->hasLight || scene->hasCamera || scene->lightSet.lightCount > 0) {
            runtime_scene_3d_builder_set_diag("ok");
            runtime_scene_3d_builder_timing_mutable()->total_ms +=
                runtime_scene_3d_builder_elapsed_ms_since(&total_start);
            return true;
        }
        char diag[2048];
        snprintf(diag,
                 sizeof(diag),
                 "bridge geometry unavailable: seed_valid=%s seed_primitive_count=%d seed_plane_count=%d seed_rect_prism_count=%d seed_excluded_count=%d mesh_instance_count=%d mesh_asset_count=%d",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 seed_state.plane_primitive_count,
                 seed_state.rect_prism_primitive_count,
                 seed_state.excluded_primitive_count,
                 mesh_instance_count,
                 mesh_asset_count);
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    if (!runtime_scene_3d_builder_rebuild_prepared_accel(scene, mesh_assets)) {
        char diag[4096];
        const char* lower_diag = RuntimeScene3DBuilder_LastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "bridge TLAS rebuild failed: seed_valid=%s seed_primitive_count=%d seed_plane_count=%d seed_rect_prism_count=%d seed_excluded_count=%d mesh_instance_count=%d mesh_asset_count=%d primitive_count=%d triangle_count=%d lower=%s",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 seed_state.plane_primitive_count,
                 seed_state.rect_prism_primitive_count,
                 seed_state.excluded_primitive_count,
                 mesh_instance_count,
                 mesh_asset_count,
                 scene ? scene->primitiveCount : -1,
                 scene ? scene->triangleMesh.triangleCount : -1,
                 lower_diag ? lower_diag : "unknown");
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    if (runtime_scene_3d_builder_should_build_flattened_bvh() &&
        !runtime_scene_3d_builder_rebuild_bvh(scene)) {
        char diag[4096];
        const char* lower_diag = RuntimeScene3DBuilder_LastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "bridge bvh rebuild failed: seed_valid=%s seed_primitive_count=%d seed_plane_count=%d seed_rect_prism_count=%d seed_excluded_count=%d mesh_instance_count=%d mesh_asset_count=%d primitive_count=%d triangle_count=%d lower=%s",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 seed_state.plane_primitive_count,
                 seed_state.rect_prism_primitive_count,
                 seed_state.excluded_primitive_count,
                 mesh_instance_count,
                 mesh_asset_count,
                 scene ? scene->primitiveCount : -1,
                 scene ? scene->triangleMesh.triangleCount : -1,
                 lower_diag ? lower_diag : "unknown");
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    runtime_scene_3d_builder_set_diag("ok");
    runtime_scene_3d_builder_timing_mutable()->total_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&total_start);
    return true;
}

bool RuntimeScene3DBuilder_BuildRouteProbeFromBridgeSeedsAtT(RuntimeScene3D* scene,
                                                             double normalized_t) {
    RuntimeSceneBridge3DPrimitiveSeedState seed_state = {0};
    const RayTracingRuntimeMeshAssetSet* mesh_assets = NULL;
    runtime_scene_3d_builder_set_diag("ok");
    mesh_assets = ray_tracing_runtime_mesh_assets_last();
    if (!mesh_assets || mesh_assets->instance_count <= 0) {
        runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
        if (!seed_state.valid || seed_state.primitive_count <= 0) {
            runtime_scene_3d_builder_set_diag("route probe failed: no mesh assets and no valid primitive seeds");
            return false;
        }
        for (int i = 0; i < seed_state.primitive_count; ++i) {
            if (!seed_state.primitives[i].has_dimensions) {
                runtime_scene_3d_builder_set_diag("route probe failed: primitive seed missing dimensions");
                return false;
            }
        }
        return RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(scene, normalized_t);
    }

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
    if (!runtime_scene_3d_builder_build_from_primitive_seed_state_at_t(scene,
                                                                       &seed_state,
                                                                       normalized_t,
                                                                       false)) {
        return false;
    }
    return runtime_scene_3d_builder_append_mesh_asset_set(scene, mesh_assets, false);
}
