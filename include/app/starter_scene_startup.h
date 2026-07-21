#ifndef RAY_TRACING_STARTER_SCENE_STARTUP_H
#define RAY_TRACING_STARTER_SCENE_STARTUP_H

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>

#include "app/starter_scene_profile.h"

typedef struct RayTracingStarterSceneStartup {
    RayTracingStarterSceneProfileStatus status;
    bool shouldActivate;
    bool shouldOpenSceneEditor;
    int preferredSpaceMode;
    int preferredEditorMode;
    char runtimeScenePath[PATH_MAX];
} RayTracingStarterSceneStartup;

bool ray_tracing_starter_scene_startup_evaluate(
    const char* program_root,
    bool has_persisted_animation_config,
    const char* active_runtime_scene_path,
    RayTracingStarterSceneStartup* out_startup,
    char* error,
    size_t error_size);

#endif
