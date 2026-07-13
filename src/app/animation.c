#ifndef MAIN_DRIVER
#define MAIN_DRIVER
#endif

#include "ui/sdl_menu.h"
#include "ui/text_zoom_shortcuts.h"
#include "app/animation.h"
#include "app/preview_session.h"
#include "app/animation_input_helpers.h"
#include "app/animation_output.h"
#include "app/data_paths.h"
#include "app/ray_tracing_runtime_host.h"
#include "app/ray_tracing_core_sim_runtime_frame.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include "app/ray_tracing_desktop_async_bridge.h"
#include "app/render_export_batch.h"
#include "app/runtime_time.h"
#include "config/config_manager.h"
#include "scene/object_manager.h"
#include "render/ray_tracing2.h"
#include "render/pipeline/ray_tracing2_preview_present.h"
#include "editor/bezier_editor.h"
#include "path/path_system.h"
#include "render/timer_hud_api.h"
#include "render/runtime_native_3d_progress_hud.h"
#include "render/ray_tracing_mode_backend.h"
#include "camera/camera.h"
#include "render/space_mode_adapter.h"
#include "render/render_helper.h"
#include "render/text_draw.h"
#include "engine/Render/render_pipeline.h"
#include "import/fluid_import.h"
#include "import/scene_bundle_import.h"
#include "import/shape_import.h"
#include "export/render_metrics_dataset.h"
#include "core_space.h"
#include "geo/shape_asset.h"
#include "geo/shape_adapter.h"
#include "ray_tracing/ray_tracing_app_main.h"
#include "kit_runtime_diag.h"
#include <json-c/json.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>  // For mkdir()
#include <sys/types.h>

// Global animation settings (Loaded from config)
int WINDOW_WIDTH;
int WINDOW_HEIGHT;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SceneObject sceneObjects[MAX_OBJECTS];  // Define object array storage
int objectCount = 0;  // Define object count

bool running;
double accumulator;
double currentTime;
int frameCounter;
int loopCount;
static bool quitRequested = false;
static int s_deepRenderStartFrameIndex = 0;
static CoreSimLoopState s_runtime_frame_loop;

double t_increment;
double t_param = 0.0;  // Parameter (0 to 1) for interpolation along the path.
int direction = 1;      // +1 for forward, -1 for reverse.
static const char* s_fluidManifestOverride = NULL;
#include "render/fluid/fluid_state.h"

void UpdateSimulation(double* accumulator, double* currentTime, int* loopCount);

static int ClampNonNegativeFrameIndex(int value) {
    return (value < 0) ? 0 : value;
}

static int ResolveDeepRenderStartFrameIndex(void) {
    int configured_start = ClampNonNegativeFrameIndex(animSettings.startFrameIndex);
    if (!animSettings.resumeFromExistingFrames) {
        return configured_start;
    }
    {
        RayTracingRenderExportStatus status = {0};
        if (ray_tracing_render_export_describe_active(&status)) {
            return ClampNonNegativeFrameIndex(status.next_frame_index);
        }
    }
    return configured_start;
}

static double AnimationNormalizedTForAbsoluteFrameIndex(int absolute_frame_index) {
    double span = 1.0;
    double progress = 0.0;
    if (absolute_frame_index < 0) absolute_frame_index = 0;
    if (animSettings.framesForTravel > 0) {
        span = (double)animSettings.framesForTravel;
    }
    progress = (double)absolute_frame_index / span;

    if (animSettings.bounceMode) {
        double cycle = fmod(progress, 2.0);
        if (cycle < 0.0) cycle += 2.0;
        return (cycle <= 1.0) ? cycle : (2.0 - cycle);
    }
    if (strcmp(animSettings.loopMode, "loop") == 0) {
        double wrapped = fmod(progress, 1.0);
        if (wrapped < 0.0) wrapped += 1.0;
        return wrapped;
    }
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    return progress;
}

static void TryLoadResumePreviewHistory(void) {
    RayTracingRenderExportStatus status = {0};
    RayTracingRuntimeRoute route;
    char last_frame_path[PATH_MAX];
    int last_frame_index = -1;

    if (!animSettings.deepRenderMode || !animSettings.resumeFromExistingFrames) {
        return;
    }

    route = RayTracingModeBackend_ResolveRoute();
    if (route.routeFamily != RAY_TRACING_ROUTE_NATIVE_3D) {
        return;
    }
    if (!ray_tracing_render_export_describe_active(&status)) {
        return;
    }
    if (status.next_frame_index <= 0 || status.frame_dir[0] == '\0') {
        return;
    }

    last_frame_index = status.next_frame_index - 1;
    if (snprintf(last_frame_path,
                 sizeof(last_frame_path),
                 "%s/frame_%04d.bmp",
                 status.frame_dir,
                 last_frame_index) >= (int)sizeof(last_frame_path)) {
        return;
    }

    if (RayTracing2PreviewPresent_LoadNative3DPreviewHistoryFromBMP(last_frame_path)) {
        printf("[preview] seeded native 3D preview history from resumed frame: %s\n",
               last_frame_path);
    }
}

static void PrepareDeepRenderFrameStateForLocalIndex(int local_frame_counter) {
    int absolute_frame_index = s_deepRenderStartFrameIndex + ClampNonNegativeFrameIndex(local_frame_counter);
    t_param = AnimationNormalizedTForAbsoluteFrameIndex(absolute_frame_index);
    currentTime = (double)absolute_frame_index * animSettings.frameDuration;
}

static bool EnvEnabled(const char *name) {
    const char *v = getenv(name);
    if (!v || !v[0]) return false;
    return strcmp(v, "1") == 0 || strcmp(v, "true") == 0 || strcmp(v, "TRUE") == 0 ||
           strcmp(v, "yes") == 0 || strcmp(v, "on") == 0;
}

char loopMode[16] = "stop";  // Increased buffer size for safety
int maxLoopCount = 1;  // Default to 1 loop if not set



int AnimationInit(void) {
    LoadAnimationConfig();
    if (s_fluidManifestOverride && s_fluidManifestOverride[0]) {
        (void)animation_config_set_scene_source_selection(&animSettings,
                                                          SCENE_SOURCE_FLUID_MANIFEST,
                                                          s_fluidManifestOverride);
    }
    LoadSceneConfig();
    ApplyAnimationWindowSizeOverride();
    if (!AnimationRestoreActiveSceneSource(true)) {
        fprintf(stderr,
                "[startup] active scene source could not be applied; selection preserved for editor/menu recovery.\n");
    }
    ApplyAnimationWindowSizeOverride();
    UpdateObjects();
    WINDOW_WIDTH = sceneSettings.windowWidth;       
    WINDOW_HEIGHT = sceneSettings.windowHeight; 
    if (ray_tracing_runtime_host_init(WINDOW_WIDTH, WINDOW_HEIGHT) != 0) {
        return -1;
    }

    // Native 3D runtime-scene starts may seed a single static light point.
    // The runtime path already treats 1 point as a valid stationary light.
    if (sceneSettings.bezierPath.numPoints < 1) {
        fprintf(stderr, "Error: Bézier path is uninitialized or invalid. Check scene_config.json.\n");
        AnimationCleanup();
        return -1;
    }
    // Initialize ray tracing scene
    InitRayTracingScene();
    printf("Completed initialization in animation.c\n");

    return 0;
}



void AnimationCleanup(void) {   
    RayTracingDeepRenderDesktopHost_Shutdown();
    RayTracingDesktopAsyncBridge_Shutdown();
    CleanupRayTracing();
    ray_tracing_runtime_host_shutdown();
}

typedef enum RayTracingInputActionType {
    RAY_TRACING_INPUT_ACTION_QUIT = 0,
    RAY_TRACING_INPUT_ACTION_KEYDOWN,
    RAY_TRACING_INPUT_ACTION_RAY_EVENT
} RayTracingInputActionType;

typedef struct RayTracingInputAction {
    RayTracingInputActionType type;
    SDL_Event event;
} RayTracingInputAction;

typedef struct RayTracingInputFrame {
    SDL_Event raw_events[256];
    uint16_t raw_count;
    RayTracingInputAction actions[256];
    uint16_t action_count;
} RayTracingInputFrame;

typedef struct RayTracingInputRoutingResult {
    bool requested_target_invalidation;
    bool requested_full_invalidation;
    uint32_t invalidation_reason_bits;
} RayTracingInputRoutingResult;

enum {
    RAY_TRACING_INPUT_INVALIDATION_REASON_ACTION = 1u << 0,
    RAY_TRACING_INPUT_INVALIDATION_REASON_EXIT = 1u << 1
};

static void InputFrame_Reset(RayTracingInputFrame *frame) {
    if (!frame) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
}

static void InputFrame_Intake(RayTracingInputFrame *frame, KitRuntimeDiagInputFrame *out_diag) {
    SDL_Event event;
    if (!frame) {
        return;
    }
    while (SDL_PollEvent(&event)) {
        if (frame->raw_count < (uint16_t)(sizeof(frame->raw_events) / sizeof(frame->raw_events[0]))) {
            frame->raw_events[frame->raw_count++] = event;
        }
        if (out_diag) {
            out_diag->raw_event_count += 1u;
        }
    }
}

static void InputFrame_Normalize(RayTracingInputFrame *frame) {
    uint16_t i = 0;
    if (!frame) {
        return;
    }
    for (i = 0; i < frame->raw_count; ++i) {
        const SDL_Event *raw = &frame->raw_events[i];
        RayTracingInputActionType action_type;
        bool supported = true;

        if (raw->type == SDL_QUIT) {
            action_type = RAY_TRACING_INPUT_ACTION_QUIT;
        } else if (raw->type == SDL_KEYDOWN) {
            action_type = RAY_TRACING_INPUT_ACTION_KEYDOWN;
        } else if (animSettings.interactiveMode &&
                   (raw->type == SDL_MOUSEMOTION || raw->type == SDL_MOUSEBUTTONDOWN)) {
            action_type = RAY_TRACING_INPUT_ACTION_RAY_EVENT;
        } else {
            supported = false;
            action_type = RAY_TRACING_INPUT_ACTION_RAY_EVENT;
        }

        if (!supported) {
            continue;
        }
        if (frame->action_count < (uint16_t)(sizeof(frame->actions) / sizeof(frame->actions[0]))) {
            frame->actions[frame->action_count].type = action_type;
            frame->actions[frame->action_count].event = *raw;
            frame->action_count += 1u;
        }
    }
}

static void InputFrame_Route(const RayTracingInputFrame *frame,
                             bool *running,
                             KitRuntimeDiagInputFrame *out_diag) {
    uint16_t i = 0;
    if (!frame || !running) {
        return;
    }
    for (i = 0; i < frame->action_count; ++i) {
        const RayTracingInputAction *action = &frame->actions[i];
        if (action->type == RAY_TRACING_INPUT_ACTION_QUIT) {
            *running = false;
            if (out_diag) {
                out_diag->action_count += 1u;
                out_diag->routed_global_count += 1u;
            }
            continue;
        }
        if (action->type == RAY_TRACING_INPUT_ACTION_KEYDOWN) {
            if (out_diag) {
                out_diag->action_count += 1u;
                out_diag->routed_global_count += 1u;
            }
            if (animation_handle_text_zoom_shortcut(&action->event.key)) {
                continue;
            }
            if (action->event.key.keysym.sym == SDLK_ESCAPE) {
                *running = false;
                continue;
            }
            animation_handle_fluid_overlay_key(action->event.key.keysym.sym);
            continue;
        }
        if (action->type == RAY_TRACING_INPUT_ACTION_RAY_EVENT) {
            SDL_Event mutable_event = action->event;
            ProcessRayTracingEvent(&mutable_event);
            if (out_diag) {
                out_diag->action_count += 1u;
                out_diag->routed_pane_count += 1u;
            }
        }
    }
}

static void InputFrame_Invalidate(const RayTracingInputFrame *frame,
                                  bool running,
                                  KitRuntimeDiagInputFrame *out_diag,
                                  RayTracingInputRoutingResult *out_result) {
    if (!out_result) {
        return;
    }
    memset(out_result, 0, sizeof(*out_result));
    if (frame && frame->action_count > 0) {
        out_result->requested_target_invalidation = true;
        out_result->invalidation_reason_bits |= RAY_TRACING_INPUT_INVALIDATION_REASON_ACTION;
        if (out_diag) {
            out_diag->target_invalidation_count += 1u;
            out_diag->invalidation_reason_bits |= RAY_TRACING_INPUT_INVALIDATION_REASON_ACTION;
        }
    }
    if (!running) {
        out_result->requested_full_invalidation = true;
        out_result->invalidation_reason_bits |= RAY_TRACING_INPUT_INVALIDATION_REASON_EXIT;
        if (out_diag) {
            out_diag->full_invalidation_count += 1u;
            out_diag->invalidation_reason_bits |= RAY_TRACING_INPUT_INVALIDATION_REASON_EXIT;
        }
    }
}

static void RunInputRoutingFrame(bool *running,
                                 KitRuntimeDiagInputFrame *out_diag,
                                 RayTracingInputRoutingResult *out_result) {
    RayTracingInputFrame frame;
    InputFrame_Reset(&frame);
    InputFrame_Intake(&frame, out_diag);
    InputFrame_Normalize(&frame);
    InputFrame_Route(&frame, running, out_diag);
    InputFrame_Invalidate(&frame, running ? *running : true, out_diag, out_result);
}

void HandleEvents(bool* running, KitRuntimeDiagInputFrame* out_diag) {
    RayTracingInputRoutingResult input_result = {0};
    if (out_diag) {
        memset(out_diag, 0, sizeof(*out_diag));
    }
    RunInputRoutingFrame(running, out_diag, &input_result);
}

void UpdateSimulation(double* accumulator, double* currentTime, int* loopCount) {
    uint64_t now_ns = runtime_time_now_ns();
    static uint64_t prev_ns = 0;
    if (prev_ns == 0) {
        prev_ns = now_ns;
    }
    double deltaTime = runtime_time_diff_seconds(now_ns, prev_ns);
    prev_ns = now_ns;
    *accumulator += deltaTime;

    // Only process if enough time has passed for one frame
    if (*accumulator < animSettings.frameDuration) {
        return;  // Not enough time passed, exit early
    }

    // Move forward on the Bézier path
    t_param += t_increment * direction;

    // Bounce mode logic
    if (animSettings.bounceMode) {
        if (t_param >= 1.0) {
            t_param = 1.0;
            direction = -1;
            (*loopCount)++;
        } else if (t_param <= 0.0) {
            t_param = 0.0;
            direction = 1;
            (*loopCount)++;   
        }
    } else { // Normal path following
        if (t_param > 1.0) {
            if (strcmp(animSettings.loopMode, "loop") == 0) {
                t_param = 0.0;
                (*loopCount)++;
            } else {
                t_param = 1.0;
            }
        }
    }

    printf("DEBUG: t_param = %.3f\n", t_param);

    *currentTime += animSettings.frameDuration;
    *accumulator -= animSettings.frameDuration;  // Reset accumulator after processing one frame
}


static bool AnimationShouldSampleAuthoredMotion(void) {
    return !animSettings.interactiveMode || animSettings.deepRenderMode;
}

void UpdateLightPosition(double* lightX, double* lightY) {
    if (!AnimationShouldSampleAuthoredMotion()) {
        GetCurrentLightPosition(lightX, lightY);
    } else {
        if (sceneSettings.bezierPath.numPoints < 1) {
            return;
        }
        {
            Point new_position = (sceneSettings.bezierPath.numPoints >= 2)
                                     ? GetPositionAlongPathNormalized(&sceneSettings.bezierPath, t_param)
                                     : sceneSettings.bezierPath.points[0];
            double light_z = (sceneSettings.bezierPath.numPoints >= 2)
                                 ? CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath,
                                                                       &sceneSettings.bezierPath3D,
                                                                       t_param)
                                 : sceneSettings.bezierPath3D.point_z[0];
            *lightX = new_position.x;
            *lightY = new_position.y;
            animSettings.lightHeight = light_z;
        }
    }
}

double AnimationCurrentNormalizedT(void) {
    return t_param;
}

int AnimationCurrentAbsoluteFrameIndex(void) {
    if (animSettings.deepRenderMode) {
        return s_deepRenderStartFrameIndex + ClampNonNegativeFrameIndex(frameCounter);
    }
    return ClampNonNegativeFrameIndex(frameCounter);
}

int AnimationConfiguredPathFrameCount(void) {
    return (animSettings.framesForTravel > 0) ? animSettings.framesForTravel : 0;
}

static void UpdateCameraPosition(double t) {
    if (!AnimationShouldSampleAuthoredMotion()) {
        return;
    }
    if (sceneSettings.cameraPath.numPoints < 1) {
        return;
    }
    Point p = (sceneSettings.cameraPath.numPoints >= 2)
                  ? GetPositionAlongPathNormalized(&sceneSettings.cameraPath, t)
                  : sceneSettings.cameraPath.points[0];
    double rot = (sceneSettings.cameraPath.numPoints >= 2)
                     ? GetRotationAlongPathNormalized(&sceneSettings.cameraPath, t)
                     : sceneSettings.cameraPath.rotations[0];
    double cam_z = (sceneSettings.cameraPath.numPoints >= 2)
                       ? CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath,
                                                             &sceneSettings.cameraPath3D,
                                                             t)
                       : sceneSettings.cameraPath3D.point_z[0];
    sceneSettings.camera.x = p.x;
    sceneSettings.camera.y = p.y;
    sceneSettings.cameraZ = cam_z;
    sceneSettings.camera.rotation = rot;
}


void RenderFrame(double lightX, double lightY, int* frameCounter, bool* running) {
    TimerHUDSession* timer_hud = timer_hud_session();
    if (quitRequested) {
        *running = false;
        return;
    }
    if (timer_hud) {
        ts_session_frame_start(timer_hud);
    }
    setRenderContext(renderer, window, sceneSettings.windowWidth, sceneSettings.windowHeight);
    render_set_clear_color(renderer, 0, 0, 0, 255);
    if (!render_begin_frame()) {
        if (render_device_lost()) {
            *running = false;
        }
        if (timer_hud) {
            ts_session_frame_end(timer_hud);
        }
        return;
    }
        
    // Render scene objects
    SetLightPosition(lightX, lightY);
    if (timer_hud) {
        ts_session_start_timer(timer_hud, "Render Scene Frame");
    }
    RenderRayTracingScene(renderer);
    if (timer_hud) {
        ts_session_stop_timer(timer_hud, "Render Scene Frame");
    }
        
    bool deep_render_frame_requested = false;
    bool deep_render_save_requested = true;
    int deep_render_frame_index = -1;

    // Handle deep render mode frame saving
    if (animSettings.deepRenderMode) {
        deep_render_frame_index = s_deepRenderStartFrameIndex + *frameCounter;
        deep_render_save_requested = SaveFrame(deep_render_frame_index);
        deep_render_frame_requested = true;
    }

    if (timer_hud) {
        ts_session_render(timer_hud);
    }
    RuntimeNative3DProgressHUD_Draw(renderer);
    bool frame_presented = render_end_frame();
    if (deep_render_frame_requested) {
        if (!deep_render_save_requested) {
            fprintf(stderr,
                    "Deep render stopped: failed to request frame export for frame_%04d.bmp.\n",
                    deep_render_frame_index);
            *running = false;
        } else if (!frame_presented) {
            fprintf(stderr,
                    "Deep render stopped: render frame end failed before frame_%04d.bmp was confirmed.\n",
                    deep_render_frame_index);
            *running = false;
        } else if (!AnimationFrameOutputExists(deep_render_frame_index)) {
            fprintf(stderr,
                    "Deep render stopped: frame_%04d.bmp was not written after render present.\n",
                    deep_render_frame_index);
            *running = false;
        } else {
            (*frameCounter)++;
            if (*frameCounter >= animSettings.frameLimit) {
                printf("Deep render mode complete. Final frame saved.\n");
                SDL_Delay(500);
                *running = false;
            }
        }
    }
    if (timer_hud) {
        ts_session_frame_end(timer_hud);
    }
}

typedef struct RayTracingFrameRenderInputs {
    double light_x;
    double light_y;
} RayTracingFrameRenderInputs;

typedef struct RayTracingFrameUpdateBridge {
    double *accumulator;
    double *current_time;
    int *loop_count;
    bool should_update;
} RayTracingFrameUpdateBridge;

typedef struct RayTracingFrameEventsBridge {
    bool *running;
    KitRuntimeDiagInputFrame *input_frame;
    RayTracingInputRoutingResult *input_result;
} RayTracingFrameEventsBridge;

typedef struct RayTracingFrameRouteBridge {
    RayTracingFrameRenderInputs *render_inputs;
} RayTracingFrameRouteBridge;

static void DeriveRenderInputs(RayTracingFrameRenderInputs* out_inputs) {
    if (!out_inputs) {
        return;
    }
    if (animSettings.deepRenderMode) {
        PrepareDeepRenderFrameStateForLocalIndex(frameCounter);
    }
    UpdateLightPosition(&out_inputs->light_x, &out_inputs->light_y);
    UpdateCameraPosition(t_param);
}

static void SubmitRenderFrame(const RayTracingFrameRenderInputs* inputs,
                              int* frameCounter,
                              bool* running) {
    if (!inputs) {
        return;
    }
    if (RayTracingDeepRenderDesktopHost_SubmitFrame(window,
                                                    renderer,
                                                    inputs->light_x,
                                                    inputs->light_y,
                                                    frameCounter,
                                                    running)) {
        return;
    }
    if (RayTracingDesktopAsyncBridge_SubmitFrame(window,
                                                renderer,
                                                inputs->light_x,
                                                inputs->light_y,
                                                frameCounter,
                                                running)) {
        return;
    }
    RenderFrame(inputs->light_x, inputs->light_y, frameCounter, running);
}

static bool HandleFrameEventsViaBridge(void *user_data) {
    RayTracingFrameEventsBridge *bridge = (RayTracingFrameEventsBridge *)user_data;
    if (!bridge || !bridge->running || !bridge->input_frame) {
        return false;
    }
    RunInputRoutingFrame(bridge->running, bridge->input_frame, bridge->input_result);
    return true;
}

static bool UpdateFrameViaBridge(void *user_data) {
    RayTracingFrameUpdateBridge *bridge = (RayTracingFrameUpdateBridge *)user_data;
    if (!bridge || !bridge->accumulator || !bridge->current_time || !bridge->loop_count) {
        return false;
    }
    if (bridge->should_update) {
        UpdateSimulation(bridge->accumulator, bridge->current_time, bridge->loop_count);
    }
    return true;
}

static bool RouteFrameViaBridge(void *user_data) {
    RayTracingFrameRouteBridge *bridge = (RayTracingFrameRouteBridge *)user_data;
    if (!bridge || !bridge->render_inputs) {
        return false;
    }
    DeriveRenderInputs(bridge->render_inputs);
    return true;
}

typedef struct RayTracingRenderSubmitBridge {
    const RayTracingFrameRenderInputs *inputs;
    int *frame_counter;
    bool *running;
} RayTracingRenderSubmitBridge;

typedef struct RayTracingLoopConditionsBridge {
    bool *running;
    int *loop_count;
    int *frame_counter;
} RayTracingLoopConditionsBridge;

static bool SubmitRenderFrameViaBridge(void *user_data) {
    RayTracingRenderSubmitBridge *bridge = (RayTracingRenderSubmitBridge *)user_data;
    if (!bridge || !bridge->inputs || !bridge->frame_counter || !bridge->running) {
        return false;
    }
    SubmitRenderFrame(bridge->inputs, bridge->frame_counter, bridge->running);
    return true;
}

void CheckLoopConditions(bool* running, int loopCount, int frameCounter) {
    if (quitRequested) {
        *running = false;
        return;
    }
    if (loopCount >= animSettings.maxLoopCount && strcmp(animSettings.loopMode, "loop") == 0) {
        *running = false;
    }
    
    // Stop when the animation reaches the last frame
    if (!animSettings.interactiveMode && !animSettings.deepRenderMode && t_param >= 1.0) {
        *running = false;
    }
    
    // Stop deep render mode when we reach the frame limit
    if (animSettings.deepRenderMode && frameCounter >= animSettings.frameLimit) {
        *running = false;
        printf("Deep render mode reached frame limit: %d/%d\n", frameCounter, animSettings.frameLimit);
    }
}

static bool CheckLoopConditionsViaBridge(void *user_data) {
    RayTracingLoopConditionsBridge *bridge = (RayTracingLoopConditionsBridge *)user_data;
    if (!bridge || !bridge->running || !bridge->loop_count || !bridge->frame_counter) {
        return false;
    }
    CheckLoopConditions(bridge->running, *bridge->loop_count, *bridge->frame_counter);
    return true;
}

static double RayTracingRuntimeNowSeconds(void *user_data) {
    (void)user_data;
    return (double)runtime_time_now_ns() / 1000000000.0;
}

void RunMainLoop(void) {
    running = true;
    accumulator = 0.0;
    currentTime = 0.0;
    frameCounter = 0;
    loopCount = 0;
    s_deepRenderStartFrameIndex = 0;
    if (animSettings.deepRenderMode) {
        s_deepRenderStartFrameIndex = ResolveDeepRenderStartFrameIndex();
        t_param = AnimationNormalizedTForAbsoluteFrameIndex(s_deepRenderStartFrameIndex);
        currentTime = (double)s_deepRenderStartFrameIndex * animSettings.frameDuration;
        TryLoadResumePreviewHistory();
    } else {
        t_param = 1.0 / animSettings.framesForTravel;
    }
    
    printf("DEBUG: RunMainLoop started with interactiveMode=%d, deepRenderMode=%d\n",
           animSettings.interactiveMode, animSettings.deepRenderMode);

    const bool runtime_diag_enabled = EnvEnabled("RAY_TRACING_RUNTIME_DIAG");
    double runtime_diag_next_log = 0.0;
    KitRuntimeDiagInputTotals input_totals = {0};

    if (!ray_tracing_core_sim_runtime_frame_loop_init(&s_runtime_frame_loop)) {
        fprintf(stderr, "ray_tracing: failed to initialize core_sim runtime frame loop.\n");
        running = false;
    }
    if (running) {
        (void)RayTracingDeepRenderDesktopHost_BeginRun(
            s_deepRenderStartFrameIndex, animSettings.frameLimit);
    }
    
    while ((running && !quitRequested) ||
           RayTracingDeepRenderDesktopHost_HasActiveWork()) {
        KitRuntimeDiagInputFrame input_frame = {0};
        RayTracingInputRoutingResult input_result = {0};
        RayTracingFrameRenderInputs render_inputs = {0};
        RayTracingFrameEventsBridge events_bridge = {
            .running = &running,
            .input_frame = &input_frame,
            .input_result = &input_result,
        };
        RayTracingFrameEventsRequest events_request = {
            .events_fn = HandleFrameEventsViaBridge,
            .user_data = &events_bridge,
        };
        RayTracingFrameUpdateBridge update_bridge = {
            .accumulator = &accumulator,
            .current_time = &currentTime,
            .loop_count = &loopCount,
            .should_update = (!animSettings.interactiveMode && !animSettings.deepRenderMode),
        };
        RayTracingFrameUpdateRequest update_request = {
            .update_fn = UpdateFrameViaBridge,
            .user_data = &update_bridge,
        };
        RayTracingFrameRouteBridge route_bridge = {
            .render_inputs = &render_inputs,
        };
        RayTracingFrameRouteRequest route_request = {
            .route_fn = RouteFrameViaBridge,
            .user_data = &route_bridge,
        };
        RayTracingRenderSubmitBridge submit_bridge = {
            .inputs = &render_inputs,
            .frame_counter = &frameCounter,
            .running = &running,
        };
        RayTracingRenderSubmitRequest submit_request = {
            .submit_fn = SubmitRenderFrameViaBridge,
            .user_data = &submit_bridge,
        };
        RayTracingLoopConditionsBridge loop_conditions_bridge = {
            .running = &running,
            .loop_count = &loopCount,
            .frame_counter = &frameCounter,
        };
        RayTracingCoreSimRuntimeFrameRequest runtime_frame_request = {
            .frame_dt_seconds = s_runtime_frame_loop.policy.fixed_dt_seconds,
            .now_seconds_fn = RayTracingRuntimeNowSeconds,
            .now_user_data = NULL,
            .events_request = events_request,
            .update_request = update_request,
            .route_request = route_request,
            .submit_request = submit_request,
            .loop_conditions_request = {
                .check_fn = CheckLoopConditionsViaBridge,
                .user_data = &loop_conditions_bridge,
            },
        };
        RayTracingCoreSimRuntimeFrameResult runtime_frame_result = {0};

        if (!ray_tracing_core_sim_runtime_frame_step(&s_runtime_frame_loop,
                                                     &runtime_frame_request,
                                                     &runtime_frame_result)) {
            fprintf(stderr,
                    "ray_tracing: core_sim runtime frame failed status=%s pass=%s message=%s\n",
                    core_sim_status_name(runtime_frame_result.sim_outcome.status),
                    runtime_frame_result.sim_outcome.failed_pass_name
                        ? runtime_frame_result.sim_outcome.failed_pass_name
                        : "unknown",
                    runtime_frame_result.sim_outcome.message
                        ? runtime_frame_result.sim_outcome.message
                        : "n/a");
            running = false;
        }
        kit_runtime_diag_input_totals_accumulate(&input_totals, &input_frame);

        if (runtime_diag_enabled) {
            const RayTracingCoreSimRuntimeFrameStageMarks *frame_marks =
                &runtime_frame_result.stage_marks;
            KitRuntimeDiagStageMarks marks = {
                .frame_begin = frame_marks->frame_begin,
                .after_events = frame_marks->after_events,
                .after_update = frame_marks->after_update,
                .after_queue = frame_marks->after_update,
                .after_integrate = frame_marks->after_update,
                .after_route = frame_marks->after_route,
                .after_render_derive = frame_marks->after_render_derive,
                .before_present = frame_marks->before_present,
                .after_render = frame_marks->after_render,
            };
            KitRuntimeDiagTimings timings = {0};
            kit_runtime_diag_compute_timings(&marks, &timings);
            if (runtime_diag_next_log <= 0.0 ||
                frame_marks->after_render >= runtime_diag_next_log) {
                printf("[rt_diag] frame=%.1fms events=%.1f update=%.1f route=%.1f derive=%.1f submit=%.1f render=%.1f present=%.1f input(frame_raw=%u frame_actions=%u) input(total_raw=%llu total_actions=%llu)\n",
                       timings.frame_ms,
                       timings.events_ms,
                       timings.update_ms,
                       timings.route_ms,
                       timings.render_derive_ms,
                       timings.render_submit_ms,
                       timings.render_ms,
                       timings.present_ms,
                       input_frame.raw_event_count,
                       input_frame.action_count,
                       (unsigned long long)input_totals.raw_event_count,
                       (unsigned long long)input_totals.action_count);
                runtime_diag_next_log = frame_marks->after_render + 1.0;
            }
        }

        SDL_Delay(16);  // Prevent CPU overload, ~60FPS
    }
 
    if (animSettings.deepRenderMode && !quitRequested &&
        !RayTracingDeepRenderDesktopHost_IsSelected()) {
        printf("Deep render mode complete. Press close on the window to exit.\n");
        bool waitingForExit = true;
        SDL_Event event; 
        while (waitingForExit && !quitRequested) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                waitingForExit = false; // return to menu
            } else if (event.type == SDL_KEYDOWN) {
                if (animation_handle_text_zoom_shortcut(&event.key)) {
                    continue;
                }
                animation_handle_fluid_overlay_key(event.key.keysym.sym);
            }
        }
        SDL_Delay(10);
    }
    }
    AnimationExportRenderMetricsDatasetIfEnabled();
}


#ifdef MAIN_DRIVER  
void AnimationParseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--fluid-manifest") == 0 && i + 1 < argc) {
            s_fluidManifestOverride = argv[++i];
        } else if (strcmp(arg, "--fluid-frame") == 0 && i + 1 < argc) {
            g_fluidFrameIndex = atoi(argv[++i]);
        }
    }
}

void AnimationLoadRuntimeDefaults(void) {
    LoadAllSettings();
    t_increment = 1.0 / animSettings.framesForTravel;
    printf("Loaded animation config in main.\n");
}

int AnimationRunAppSession(void) {
    // Menu → run loop, allowing return to menu after each run
    while (!quitRequested) {
        if (!RunMenu()) {
            printf("Menu closed. Exiting program.\n");
            return 0;
        }
        if (animSettings.previewMode) {
            fprintf(stderr, "[preview] clearing legacy standalone preview flag.\n");
            animSettings.previewMode = false;
            SaveAllSettings();
        }

        // Print selected settings
        printf("Selected Mode: %s\n",
               animSettings.interactiveMode ? "Interactive" :
               animSettings.deepRenderMode ? "Deep Render" :
               animSettings.bounceMode ? "Bounce Animation" : "Standard Animation");
        printf("Auto MP4 after render: %s\n", animSettings.autoMP4 ? "Enabled" : "Disabled");
        {
            char frame_dir[PATH_MAX];
            char video_output_path[PATH_MAX];
            if (ray_tracing_resolve_frame_output_dir(animSettings.frameDir,
                                                     frame_dir,
                                                     sizeof(frame_dir))) {
                printf("Saving frames in directory: %s\n", frame_dir);
            } else {
                printf("Saving frames in directory: <unresolved>\n");
            }
            if (ray_tracing_resolve_video_output_path(animSettings.videoOutputRoot,
                                                      video_output_path,
                                                      sizeof(video_output_path))) {
                printf("Auto MP4 output path: %s\n", video_output_path);
            } else {
                printf("Auto MP4 output path: <unresolved>\n");
            }
        }

        // Initialize animation
        if (AnimationInit() != 0) {
            printf("Error: Animation initialization failed. Exiting program.\n");
            return -1;
        }

        t_increment = 1.0 / animSettings.framesForTravel;

        printf("Starting animation loop...\n");
        RunMainLoop();

        if (animSettings.autoMP4 && animSettings.deepRenderMode &&
            (!RayTracingDeepRenderDesktopHost_IsSelected() ||
             RayTracingDeepRenderDesktopHost_CompletedSuccessfully())) {
            RayTracingRenderExportStatus export_status;
            printf("Generating MP4 automatically...\n");
            if (!ray_tracing_render_export_make_video(&export_status)) {
                fprintf(stderr, "[export] %s\n", export_status.message);
            } else {
                printf("[export] %s: %s\n",
                       export_status.message,
                       export_status.video_output_path);
            }
        }

        AnimationCleanup();
        SaveAllSettings();

        // If run ended without a quit request, drop back to the menu
        if (quitRequested) {
            break;
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    return ray_tracing_app_main(argc, argv);
}
#endif
