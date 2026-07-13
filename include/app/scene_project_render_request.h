#ifndef RAY_TRACING_SCENE_PROJECT_RENDER_REQUEST_H
#define RAY_TRACING_SCENE_PROJECT_RENDER_REQUEST_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct RayTracingSceneProjectRenderRequest {
    bool project_backed;
    bool project_owned;
    bool request_exists;
    char project_root[PATH_MAX];
    char runtime_scene_path[PATH_MAX];
    char request_path[PATH_MAX];
    char request_relpath[PATH_MAX];
    char physics_cache_relpath[PATH_MAX];
    char output_root_relpath[PATH_MAX];
    int simulation_start_frame;
    int simulation_frame_count;
    int simulation_frame_stride;
} RayTracingSceneProjectRenderRequest;

bool ray_tracing_scene_project_render_request_resolve(
    const char *runtime_scene_path,
    const char *explicit_request_path,
    RayTracingSceneProjectRenderRequest *out_request,
    char *error,
    size_t error_size);

bool ray_tracing_scene_project_render_request_write(
    RayTracingSceneProjectRenderRequest *request,
    int simulation_start_frame,
    int simulation_frame_count,
    int simulation_frame_stride,
    char *error,
    size_t error_size);

#endif
