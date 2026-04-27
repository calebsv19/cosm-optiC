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
#include "app/render_export_batch.h"
#include "app/runtime_time.h"
#include "config/config_manager.h"
#include "scene/object_manager.h"
#include "render/ray_tracing2.h"
#include "editor/bezier_editor.h"
#include "path/path_system.h"
#include "render/timer_hud_api.h"
#include "render/timer_hud_adapter.h"
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
#include "render/vk_shared_device.h"
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
#if USE_VULKAN
static VkRenderer renderer_storage;
#endif
SceneObject sceneObjects[10];  // Define object array storage
int objectCount = 0;  // Define object count

bool running;
double accumulator;
double currentTime;
int frameCounter;
int loopCount;
static bool quitRequested = false;

double t_increment;
double t_param = 0.0;  // Parameter (0 to 1) for interpolation along the path.
int direction = 1;      // +1 for forward, -1 for reverse.
static const char* s_fluidManifestOverride = NULL;
#include "render/fluid/fluid_state.h"

void UpdateSimulation(double* accumulator, double* currentTime, int* loopCount);

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
        strncpy(animSettings.fluidManifest, s_fluidManifestOverride, sizeof(animSettings.fluidManifest) - 1);
        animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
        animSettings.sceneSource = SCENE_SOURCE_FLUID_MANIFEST;
    }
    LoadSceneConfig();
    ApplyAnimationWindowSizeOverride();
    if (!AnimationRestoreActiveSceneSource(true)) {
        fprintf(stderr,
                "[startup] active scene source could not be applied; fallback persisted.\n");
    }
    ApplyAnimationWindowSizeOverride();
    UpdateObjects();
    WINDOW_WIDTH = sceneSettings.windowWidth;       
    WINDOW_HEIGHT = sceneSettings.windowHeight; 
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return -1;
    }

    // Create window
    window = SDL_CreateWindow("Raytracing Animation",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

#if USE_VULKAN
    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 0.0f;
    cfg.clear_color[1] = 0.0f;
    cfg.clear_color[2] = 0.0f;
    cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(window, &cfg)) {
        fprintf(stderr, "vk_shared_device_init failed.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        fprintf(stderr, "vk_shared_device_get failed.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    VkResult init = vk_renderer_init_with_device(&renderer_storage, shared_device, window, &cfg);
    if (init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed: %d\n", init);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    renderer = (SDL_Renderer*)&renderer_storage;
    vk_renderer_set_logical_size((VkRenderer*)renderer, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
#else
    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
#endif

    setRenderContext(renderer, window, WINDOW_WIDTH, WINDOW_HEIGHT);
    timer_hud_register_backend();
    ts_init();

    // Validate Bézier path
    if (sceneSettings.bezierPath.numPoints < 2) {
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
    if (renderer) {
        ray_tracing_text_reset_renderer(renderer);
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)renderer);
        vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
        SDL_DestroyRenderer(renderer);
#endif
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
#if USE_VULKAN
    vk_shared_device_shutdown();
#endif
    SDL_Quit();
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


void UpdateLightPosition(double* lightX, double* lightY) {
    if (animSettings.interactiveMode) {
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

static void UpdateCameraPosition(double t) {
    if (animSettings.interactiveMode) {
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
    if (quitRequested) {
        *running = false;
        return;
    }
    ts_frame_start();
    setRenderContext(renderer, window, sceneSettings.windowWidth, sceneSettings.windowHeight);
    render_set_clear_color(renderer, 0, 0, 0, 255);
    if (!render_begin_frame()) {
        if (render_device_lost()) {
            *running = false;
        }
        ts_frame_end();
        return;
    }
        
    // Render scene objects
    SetLightPosition(lightX, lightY);
    RenderRayTracingScene(renderer);
        
    // Handle deep render mode frame saving
    if (animSettings.deepRenderMode) {
        ts_start_timer("Frame Save");
        SaveFrame((*frameCounter)++);
        ts_stop_timer("Frame Save");
        if (*frameCounter >= animSettings.frameLimit) {
            printf("Deep render mode complete. Final frame saved.\n");
            SDL_Delay(500);
            *running = false;
        }
    }

    ts_render();
    render_end_frame();
    ts_frame_end();
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
    UpdateLightPosition(&out_inputs->light_x, &out_inputs->light_y);
    UpdateCameraPosition(t_param);
}

static void SubmitRenderFrame(const RayTracingFrameRenderInputs* inputs,
                              int* frameCounter,
                              bool* running) {
    if (!inputs) {
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

void RunMainLoop(void) {
    running = true;
    accumulator = 0.0;
    currentTime = 0.0;
    frameCounter = 0;
    loopCount = 0;
    t_param = 1.0 / animSettings.framesForTravel;
    
    printf("DEBUG: RunMainLoop started with interactiveMode=%d, deepRenderMode=%d\n",
           animSettings.interactiveMode, animSettings.deepRenderMode);

    const bool runtime_diag_enabled = EnvEnabled("RAY_TRACING_RUNTIME_DIAG");
    double runtime_diag_next_log = 0.0;
    KitRuntimeDiagInputTotals input_totals = {0};
    
    while (running && !quitRequested) {
        const double frame_begin = (double)runtime_time_now_ns() / 1000000000.0;
        KitRuntimeDiagInputFrame input_frame = {0};
        RayTracingInputRoutingResult input_result = {0};
        RayTracingFrameEventsBridge events_bridge = {
            .running = &running,
            .input_frame = &input_frame,
            .input_result = &input_result,
        };
        RayTracingFrameEventsRequest events_request = {
            .events_fn = HandleFrameEventsViaBridge,
            .user_data = &events_bridge,
        };
        RayTracingFrameEventsOutcome events_outcome = {0};
        if (!ray_tracing_app_frame_events(&events_request, &events_outcome) ||
            !events_outcome.handled) {
            RunInputRoutingFrame(&running, &input_frame, &input_result);
        }
        const double after_events = (double)runtime_time_now_ns() / 1000000000.0;
        RayTracingFrameUpdateBridge update_bridge = {
            .accumulator = &accumulator,
            .current_time = &currentTime,
            .loop_count = &loopCount,
            .should_update = (!animSettings.interactiveMode || animSettings.deepRenderMode),
        };
        RayTracingFrameUpdateRequest update_request = {
            .update_fn = UpdateFrameViaBridge,
            .user_data = &update_bridge,
        };
        RayTracingFrameUpdateOutcome update_outcome = {0};
        if (!ray_tracing_app_frame_update(&update_request, &update_outcome) ||
            !update_outcome.updated) {
            if (update_bridge.should_update) {
                UpdateSimulation(&accumulator, &currentTime, &loopCount);
            }
        }
        const double after_update = (double)runtime_time_now_ns() / 1000000000.0;

        const double after_route = (double)runtime_time_now_ns() / 1000000000.0;
        RayTracingFrameRenderInputs render_inputs = {0};
        RayTracingFrameRouteBridge route_bridge = {
            .render_inputs = &render_inputs,
        };
        RayTracingFrameRouteRequest route_request = {
            .route_fn = RouteFrameViaBridge,
            .user_data = &route_bridge,
        };
        RayTracingFrameRouteOutcome route_outcome = {0};
        if (!ray_tracing_app_frame_route(&route_request, &route_outcome) ||
            !route_outcome.routed) {
            DeriveRenderInputs(&render_inputs);
        }
        const double after_render_derive = (double)runtime_time_now_ns() / 1000000000.0;
        const double before_present = (double)runtime_time_now_ns() / 1000000000.0;
        RayTracingRenderSubmitBridge submit_bridge = {
            .inputs = &render_inputs,
            .frame_counter = &frameCounter,
            .running = &running,
        };
        RayTracingRenderSubmitRequest submit_request = {
            .submit_fn = SubmitRenderFrameViaBridge,
            .user_data = &submit_bridge,
        };
        RayTracingRenderSubmitOutcome submit_outcome = {0};
        if (!ray_tracing_app_render_submit(&submit_request, &submit_outcome) ||
            !submit_outcome.submitted) {
            SubmitRenderFrame(&render_inputs, &frameCounter, &running);
        }
        const double after_render = (double)runtime_time_now_ns() / 1000000000.0;
        CheckLoopConditions(&running, loopCount, frameCounter);
        kit_runtime_diag_input_totals_accumulate(&input_totals, &input_frame);

        if (runtime_diag_enabled) {
            KitRuntimeDiagStageMarks marks = {
                .frame_begin = frame_begin,
                .after_events = after_events,
                .after_update = after_update,
                .after_queue = after_update,
                .after_integrate = after_update,
                .after_route = after_route,
                .after_render_derive = after_render_derive,
                .before_present = before_present,
                .after_render = after_render,
            };
            KitRuntimeDiagTimings timings = {0};
            kit_runtime_diag_compute_timings(&marks, &timings);
            if (runtime_diag_next_log <= 0.0 || after_render >= runtime_diag_next_log) {
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
                runtime_diag_next_log = after_render + 1.0;
            }
        }

        SDL_Delay(16);  // Prevent CPU overload, ~60FPS
    }
 
    if (animSettings.deepRenderMode && !quitRequested) {
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
    CleanupRayTracing();    
    ray_tracing_text_reset_renderer(renderer);
#if USE_VULKAN
    vk_renderer_wait_idle((VkRenderer*)renderer);
    vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
    SDL_DestroyRenderer(renderer);
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();
}


#ifdef MAIN_DRIVER  
static void ParseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--fluid-manifest") == 0 && i + 1 < argc) {
            s_fluidManifestOverride = argv[++i];
        } else if (strcmp(arg, "--fluid-frame") == 0 && i + 1 < argc) {
            g_fluidFrameIndex = atoi(argv[++i]);
        }
    }
}

int ray_tracing_app_main_legacy(int argc, char* argv[]) {
    ParseArgs(argc, argv);
    (void)argc;
    (void)argv;
    // Load animation settings from config file
    LoadAllSettings();
    t_increment = 1.0 / animSettings.framesForTravel;
    printf("Loaded animation config in main.\n");

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

        if (animSettings.autoMP4 && animSettings.deepRenderMode) {
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
    ray_tracing_app_set_legacy_entry(ray_tracing_app_main_legacy);
    return ray_tracing_app_main(argc, argv);
}
#endif
