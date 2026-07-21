#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "app/starter_scene_profile.h"
#include "app/starter_scene_startup.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            failures += 1; \
        } \
    } while (0)

static bool make_directory(const char* path) {
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

static bool write_file(const char* path, const char* text) {
    FILE* file = fopen(path, "w");
    bool ok = false;
    if (!file) return false;
    ok = fputs(text, file) >= 0;
    if (fclose(file) != 0) ok = false;
    return ok;
}

static void set_enabled_profile(RayTracingStarterSceneProfile* profile) {
    ray_tracing_starter_scene_profile_defaults(profile);
    (void)snprintf(profile->profileId, sizeof(profile->profileId), "fixture_v1");
    profile->enabled = true;
    (void)snprintf(profile->templateDirectory, sizeof(profile->templateDirectory),
                   "config/starter_scenes/fixture_v1");
    (void)snprintf(profile->workingDirectory, sizeof(profile->workingDirectory),
                   "data/runtime/scenes/fixture_v1");
    (void)snprintf(profile->runtimeSceneRelativePath,
                   sizeof(profile->runtimeSceneRelativePath), "scene_runtime.json");
    profile->preferredSpaceMode = 1;
    profile->preferredEditorMode = 1;
    profile->openSceneEditor = true;
}

static void cleanup_fixture(const char* root) {
    char path[PATH_MAX];
    (void)snprintf(path, sizeof(path), "%s/config/starter_scene_profile.json", root);
    (void)unlink(path);
    (void)snprintf(path, sizeof(path),
                   "%s/data/runtime/scenes/fixture_v1/scene_runtime.json", root);
    (void)unlink(path);
    (void)snprintf(path, sizeof(path), "%s/data/runtime/scenes/fixture_v1", root);
    (void)rmdir(path);
    (void)snprintf(path, sizeof(path), "%s/data/runtime/scenes", root);
    (void)rmdir(path);
    (void)snprintf(path, sizeof(path), "%s/data/runtime", root);
    (void)rmdir(path);
    (void)snprintf(path, sizeof(path), "%s/data", root);
    (void)rmdir(path);
    (void)snprintf(path, sizeof(path),
                   "%s/config/starter_scenes/fixture_v1/scene_runtime.json", root);
    (void)unlink(path);
    (void)snprintf(path, sizeof(path), "%s/config/starter_scenes/fixture_v1", root);
    (void)rmdir(path);
    (void)snprintf(path, sizeof(path), "%s/config/starter_scenes", root);
    (void)rmdir(path);
    (void)snprintf(path, sizeof(path), "%s/config", root);
    (void)rmdir(path);
    (void)rmdir(root);
}

int main(void) {
    RayTracingStarterSceneProfile profile;
    RayTracingStarterSceneProfileContext context;
    RayTracingStarterSceneProfilePlan plan;
    RayTracingStarterSceneStartup startup;
    char error[256];
    char temp_root[] = "/tmp/ray-tracing-starter-profile-XXXXXX";
    char path[PATH_MAX];

    CHECK(ray_tracing_starter_scene_profile_load("config/starter_scene_profile.json",
                                                  &profile, error, sizeof(error)),
          "shipped starter profile loads");
    CHECK(profile.enabled, "shipped starter profile is enabled after scene promotion");
    CHECK(strcmp(profile.templateDirectory, "config/samples/optic_build_week_showcase") == 0,
          "shipped starter profile selects the self-contained showcase");

    profile.enabled = false;
    memset(&context, 0, sizeof(context));
    (void)snprintf(context.programRoot, sizeof(context.programRoot), ".");
    CHECK(ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                  error, sizeof(error)),
          "disabled profile plans successfully");
    CHECK(plan.status == RAY_TRACING_STARTER_PROFILE_DISABLED,
          "disabled profile is inert");

    CHECK(mkdtemp(temp_root) != NULL, "temporary profile root created");
    set_enabled_profile(&profile);
    memset(&context, 0, sizeof(context));
    (void)snprintf(context.programRoot, sizeof(context.programRoot), "%s", temp_root);

    CHECK(ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                  error, sizeof(error)),
          "missing template produces a plan");
    CHECK(plan.status == RAY_TRACING_STARTER_PROFILE_TEMPLATE_MISSING,
          "missing template blocks seeding");

    context.activationMarkerExists = true;
    CHECK(ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                  error, sizeof(error)),
          "activation marker produces a plan");
    CHECK(plan.status == RAY_TRACING_STARTER_PROFILE_PRESERVE_AFTER_ACTIVATION,
          "activation marker preserves later user choices");
    context.activationMarkerExists = false;

    context.hasExplicitSceneSelection = true;
    (void)snprintf(context.activeRuntimeScenePath, sizeof(context.activeRuntimeScenePath),
                   "%s/data/runtime/scenes/user/scene_runtime.json", temp_root);
    CHECK(ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                  error, sizeof(error)),
          "explicit user scene produces a plan");
    CHECK(plan.status == RAY_TRACING_STARTER_PROFILE_PRESERVE_USER_SCENE,
          "explicit user scene is never replaced");
    context.hasExplicitSceneSelection = false;
    context.activeRuntimeScenePath[0] = '\0';

    context.hasExplicitSceneSelection = true;
    CHECK(ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                  error, sizeof(error)),
          "explicit non-runtime scene produces a plan");
    CHECK(plan.status == RAY_TRACING_STARTER_PROFILE_PRESERVE_USER_SCENE,
          "explicit 2D or fluid selection is never replaced");
    context.hasExplicitSceneSelection = false;

    (void)snprintf(path, sizeof(path), "%s/config", temp_root);
    CHECK(make_directory(path), "fixture config directory created");
    (void)snprintf(path, sizeof(path), "%s/config/starter_scene_profile.json", temp_root);
    CHECK(write_file(path,
                     "{\n"
                     "  \"schema\": \"ray_tracing_starter_scene_profile_v1\",\n"
                     "  \"profile_id\": \"fixture_v1\",\n"
                     "  \"enabled\": true,\n"
                     "  \"template_directory\": \"config/starter_scenes/fixture_v1\",\n"
                     "  \"working_directory\": \"data/runtime/scenes/fixture_v1\",\n"
                     "  \"runtime_scene\": \"scene_runtime.json\",\n"
                     "  \"preferred_space_mode\": 1,\n"
                     "  \"preferred_editor_mode\": 1,\n"
                     "  \"open_scene_editor\": true\n"
                     "}\n"),
          "fixture startup profile created");
    (void)snprintf(path, sizeof(path), "%s/config/starter_scenes", temp_root);
    CHECK(make_directory(path), "fixture starter scenes directory created");
    (void)snprintf(path, sizeof(path), "%s/config/starter_scenes/fixture_v1", temp_root);
    CHECK(make_directory(path), "fixture template directory created");
    (void)snprintf(path, sizeof(path),
                   "%s/config/starter_scenes/fixture_v1/scene_runtime.json", temp_root);
    CHECK(write_file(path, "{}\n"), "fixture template scene created");
    CHECK(ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                  error, sizeof(error)),
          "template-ready profile produces a plan");
    CHECK(plan.status == RAY_TRACING_STARTER_PROFILE_READY_TO_SEED &&
              plan.shouldSeedWorkingCopy,
          "template-ready profile requests non-destructive seeding");

    (void)snprintf(path, sizeof(path), "%s/data", temp_root);
    CHECK(make_directory(path), "fixture data directory created");
    (void)snprintf(path, sizeof(path), "%s/data/runtime", temp_root);
    CHECK(make_directory(path), "fixture runtime directory created");
    (void)snprintf(path, sizeof(path), "%s/data/runtime/scenes", temp_root);
    CHECK(make_directory(path), "fixture runtime scenes directory created");
    (void)snprintf(path, sizeof(path), "%s/data/runtime/scenes/fixture_v1", temp_root);
    CHECK(make_directory(path), "fixture working directory created");
    CHECK(ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                  error, sizeof(error)),
          "partial working copy produces a plan");
    CHECK(plan.status == RAY_TRACING_STARTER_PROFILE_WORKING_COPY_INVALID,
          "partial working copy is not overwritten or activated");

    (void)snprintf(path, sizeof(path),
                   "%s/data/runtime/scenes/fixture_v1/scene_runtime.json", temp_root);
    CHECK(write_file(path, "{}\n"), "fixture working scene created");
    CHECK(ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                  error, sizeof(error)),
          "complete working copy produces a plan");
    CHECK(plan.status == RAY_TRACING_STARTER_PROFILE_READY_TO_ACTIVATE &&
              plan.shouldActivateWorkingCopy && plan.shouldOpenSceneEditor,
          "complete working copy is ready for editor activation");

    CHECK(ray_tracing_starter_scene_startup_evaluate(temp_root,
                                                      false,
                                                      "",
                                                      &startup,
                                                      error,
                                                      sizeof(error)),
          "fresh startup evaluates successfully");
    CHECK(startup.shouldActivate && startup.shouldOpenSceneEditor,
          "fresh startup activates and opens the starter editor");
    CHECK(startup.preferredSpaceMode == 1 && startup.preferredEditorMode == 1,
          "fresh startup selects 3D Object mode");
    CHECK(strstr(startup.runtimeScenePath,
                 "/data/runtime/scenes/fixture_v1/scene_runtime.json") != NULL,
          "fresh startup selects the editable working scene");

    CHECK(ray_tracing_starter_scene_startup_evaluate(temp_root,
                                                      true,
                                                      "/tmp/user-scene.json",
                                                      &startup,
                                                      error,
                                                      sizeof(error)),
          "saved user startup evaluates successfully");
    CHECK(!startup.shouldActivate &&
              startup.status == RAY_TRACING_STARTER_PROFILE_PRESERVE_USER_SCENE,
          "saved user selection takes precedence over the starter scene");

    (void)snprintf(profile.workingDirectory, sizeof(profile.workingDirectory), "../escape");
    CHECK(!ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                   error, sizeof(error)),
          "unsafe relative path is rejected");

    set_enabled_profile(&profile);
    profile.preferredSpaceMode = 0;
    CHECK(!ray_tracing_starter_scene_profile_plan(&profile, &context, &plan,
                                                   error, sizeof(error)),
          "enabled starter profile cannot request 2D mode");

    cleanup_fixture(temp_root);

    if (failures != 0) {
        fprintf(stderr, "%d starter scene profile checks failed\n", failures);
        return 1;
    }
    printf("starter scene profile contract passed\n");
    return 0;
}
