#ifndef RAY_TRACING_MENU_SCENE_PROJECT_SUMMARY_H
#define RAY_TRACING_MENU_SCENE_PROJECT_SUMMARY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct RayTracingMenuSceneProjectSummary {
    bool selected_runtime_scene;
    bool project_detected;
    bool has_scene_project;
    bool has_scene_runtime;
    bool has_scene_authoring;
    bool has_mesh_assets;
    bool has_physics_cache_manifest;
    bool has_physics_scene_bundle;
    bool has_render_request;
    int mesh_asset_count;
    char project_root[512];
    char label[96];
    char detail[160];
} RayTracingMenuSceneProjectSummary;

bool ray_tracing_menu_scene_project_summary_current(RayTracingMenuSceneProjectSummary *out_summary);
bool ray_tracing_menu_scene_project_summary_for_runtime_scene(
    const char *runtime_scene_path,
    RayTracingMenuSceneProjectSummary *out_summary);

#endif
