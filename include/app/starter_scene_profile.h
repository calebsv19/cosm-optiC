#ifndef RAY_TRACING_STARTER_SCENE_PROFILE_H
#define RAY_TRACING_STARTER_SCENE_PROFILE_H

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>

#define RAY_TRACING_STARTER_SCENE_PROFILE_SCHEMA "ray_tracing_starter_scene_profile_v1"
#define RAY_TRACING_STARTER_SCENE_PROFILE_ID_MAX 96

typedef enum RayTracingStarterSceneProfileStatus {
    RAY_TRACING_STARTER_PROFILE_DISABLED = 0,
    RAY_TRACING_STARTER_PROFILE_PRESERVE_USER_SCENE,
    RAY_TRACING_STARTER_PROFILE_PRESERVE_AFTER_ACTIVATION,
    RAY_TRACING_STARTER_PROFILE_TEMPLATE_MISSING,
    RAY_TRACING_STARTER_PROFILE_WORKING_COPY_INVALID,
    RAY_TRACING_STARTER_PROFILE_READY_TO_SEED,
    RAY_TRACING_STARTER_PROFILE_READY_TO_ACTIVATE,
    RAY_TRACING_STARTER_PROFILE_INVALID
} RayTracingStarterSceneProfileStatus;

typedef struct RayTracingStarterSceneProfile {
    char schema[64];
    char profileId[RAY_TRACING_STARTER_SCENE_PROFILE_ID_MAX];
    bool enabled;
    char templateDirectory[PATH_MAX];
    char workingDirectory[PATH_MAX];
    char runtimeSceneRelativePath[PATH_MAX];
    int preferredSpaceMode;
    int preferredEditorMode;
    bool openSceneEditor;
} RayTracingStarterSceneProfile;

typedef struct RayTracingStarterSceneProfileContext {
    char programRoot[PATH_MAX];
    bool activationMarkerExists;
    bool hasExplicitSceneSelection;
    char activeRuntimeScenePath[PATH_MAX];
} RayTracingStarterSceneProfileContext;

typedef struct RayTracingStarterSceneProfilePlan {
    RayTracingStarterSceneProfileStatus status;
    bool shouldSeedWorkingCopy;
    bool shouldActivateWorkingCopy;
    bool shouldOpenSceneEditor;
    char templateRuntimeScenePath[PATH_MAX];
    char workingRuntimeScenePath[PATH_MAX];
} RayTracingStarterSceneProfilePlan;

void ray_tracing_starter_scene_profile_defaults(RayTracingStarterSceneProfile* profile);

bool ray_tracing_starter_scene_profile_load(const char* path,
                                            RayTracingStarterSceneProfile* out_profile,
                                            char* error,
                                            size_t error_size);

bool ray_tracing_starter_scene_profile_plan(
    const RayTracingStarterSceneProfile* profile,
    const RayTracingStarterSceneProfileContext* context,
    RayTracingStarterSceneProfilePlan* out_plan,
    char* error,
    size_t error_size);

const char* ray_tracing_starter_scene_profile_status_label(
    RayTracingStarterSceneProfileStatus status);

#endif
