#include "app/starter_scene_startup.h"

#include <stdio.h>
#include <string.h>

bool ray_tracing_starter_scene_startup_evaluate(
    const char* program_root,
    bool has_persisted_animation_config,
    const char* active_runtime_scene_path,
    RayTracingStarterSceneStartup* out_startup,
    char* error,
    size_t error_size) {
    RayTracingStarterSceneProfile profile;
    RayTracingStarterSceneProfileContext context;
    RayTracingStarterSceneProfilePlan plan;
    char profile_path[PATH_MAX];

    if (error && error_size > 0u) error[0] = '\0';
    if (!program_root || !program_root[0] || !out_startup) {
        if (error && error_size > 0u) {
            (void)snprintf(error, error_size, "program root and startup output are required");
        }
        return false;
    }
    memset(out_startup, 0, sizeof(*out_startup));
    out_startup->status = RAY_TRACING_STARTER_PROFILE_INVALID;
    if (snprintf(profile_path,
                 sizeof(profile_path),
                 "%s/config/starter_scene_profile.json",
                 program_root) >= (int)sizeof(profile_path)) {
        if (error && error_size > 0u) {
            (void)snprintf(error, error_size, "starter profile path is too long");
        }
        return false;
    }
    if (!ray_tracing_starter_scene_profile_load(profile_path,
                                                 &profile,
                                                 error,
                                                 error_size)) {
        return false;
    }

    memset(&context, 0, sizeof(context));
    (void)snprintf(context.programRoot, sizeof(context.programRoot), "%s", program_root);
    context.hasExplicitSceneSelection = has_persisted_animation_config;
    if (active_runtime_scene_path) {
        (void)snprintf(context.activeRuntimeScenePath,
                       sizeof(context.activeRuntimeScenePath),
                       "%s",
                       active_runtime_scene_path);
    }
    if (!ray_tracing_starter_scene_profile_plan(&profile,
                                                 &context,
                                                 &plan,
                                                 error,
                                                 error_size)) {
        return false;
    }

    out_startup->status = plan.status;
    if (!plan.shouldActivateWorkingCopy) return true;

    out_startup->shouldActivate = true;
    out_startup->shouldOpenSceneEditor = plan.shouldOpenSceneEditor;
    out_startup->preferredSpaceMode = profile.preferredSpaceMode;
    out_startup->preferredEditorMode = profile.preferredEditorMode;
    (void)snprintf(out_startup->runtimeScenePath,
                   sizeof(out_startup->runtimeScenePath),
                   "%s",
                   plan.workingRuntimeScenePath);
    return true;
}
