#include "app/animation.h"
#include "app/animation_input_helpers.h"
#include "app/preview_mode_route.h"
#include "app/preview_camera_sample.h"
#include "app/preview_camera_projector.h"
#include "app/preview_playback.h"
#include "app/preview_retained_scene_renderer.h"
#include "app/runtime_time.h"
#include "config/config_manager.h"
#include "render/ray_tracing2.h"
#include "render/render_helper.h"
#include "render/space_mode_adapter.h"
#include "render/ray_tracing_mode_backend.h"
#include "engine/Render/render_pipeline.h"
#include "render/text_draw.h"
#include "import/runtime_scene_bridge.h"
#include "render/vk_shared_device.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double kPreviewBg = 60.0;

static void DrawPreviewMarker(SDL_Renderer* renderer, Point world, SDL_Color color, int radius) {
    SpaceModeViewContext view_ctx;
    CameraPoint screen;
    int dx = 0;
    int dy = 0;
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    view_ctx = SpaceModeAdapter_BuildViewContext(&sceneSettings.camera,
                                                 sceneSettings.windowWidth,
                                                 sceneSettings.windowHeight);
    screen = SpaceModeAdapter_WorldToScreen(&view_ctx, world.x, world.y);
    for (dx = -radius; dx <= radius; ++dx) {
        for (dy = -radius; dy <= radius; ++dy) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(renderer, (int)lround(screen.x) + dx, (int)lround(screen.y) + dy);
            }
        }
    }
}

static void DrawPreviewRouteStatus(SDL_Renderer* renderer,
                                   const PreviewModeRouteDecision* decision,
                                   const PreviewPlaybackSample* playback) {
    SDL_Rect line1 = {12, 10, 360, 22};
    SDL_Rect line2 = {12, 32, 520, 38};
    SDL_Rect line3 = {12, 68, 520, 22};
    SDL_Color primary = {220, 224, 232, 255};
    SDL_Color secondary = {170, 176, 188, 255};
    if (!renderer || !decision) return;
    RenderLabelTextLeft(renderer, line1, decision->branchLabel, primary);
    RenderLabelTextWrappedLeft(renderer, line2, decision->statusLine, secondary);
    if (playback && playback->valid) {
        RenderLabelTextWrappedLeft(renderer, line3, playback->status_line, secondary);
    }
}

static SDL_Rect PreviewCloseButtonRect(int window_width) {
    SDL_Rect rect = {0};
    int button_width = 132;
    int button_height = 34;
    int margin = 12;
    rect.x = window_width - button_width - margin;
    rect.y = margin;
    rect.w = button_width;
    rect.h = button_height;
    return rect;
}

static void DrawPreviewCloseButton(SDL_Renderer* renderer, SDL_Rect rect, bool hovered) {
    SDL_Color fill = hovered ? (SDL_Color){78, 82, 92, 255} : (SDL_Color){58, 62, 72, 255};
    SDL_Color border = {122, 128, 142, 255};
    SDL_Color text = {228, 232, 238, 255};
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
    RenderButtonTextWithColor(renderer, rect, "Close Preview", text);
}

static void PreviewSessionRestoreHostWindow(SDL_Window* host_window) {
    Uint32 host_flags = 0;
    int attempts = 0;
    if (!host_window) return;
    host_flags = SDL_GetWindowFlags(host_window);
    if (host_flags & SDL_WINDOW_MINIMIZED) {
        SDL_RestoreWindow(host_window);
    }
    SDL_CaptureMouse(SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowWindow(host_window);
    SDL_RaiseWindow(host_window);
    for (attempts = 0; attempts < 3; ++attempts) {
        (void)SDL_SetWindowInputFocus(host_window);
        SDL_PumpEvents();
        if (SDL_GetKeyboardFocus() == host_window) {
            break;
        }
        SDL_Delay(8);
        SDL_RaiseWindow(host_window);
    }
    SDL_FlushEvents(SDL_MOUSEMOTION, SDL_MOUSEWHEEL);
    SDL_FlushEvents(SDL_TEXTINPUT, SDL_TEXTEDITING);
}

static void RunPreviewInternal(bool standalone, SDL_Window* host_window, SDL_Renderer* host_renderer) {
    bool didInit = false;
    Uint32 preview_window_id = 0;
    SDL_Window* preview_window = NULL;
    SDL_Renderer* preview_renderer = NULL;
    uint64_t prev_ns = 0;
    double elapsed = 0.0;
    bool running_preview = true;
    bool close_button_pressed = false;
    Camera saved_camera = sceneSettings.camera;
    double saved_camera_z = sceneSettings.cameraZ;

    if (standalone) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL_Init Error (preview): %s\n", SDL_GetError());
            return;
        }
        didInit = true;
    } else if (!host_window || !host_renderer) {
        fprintf(stderr, "Embedded preview requires a host window and renderer.\n");
        return;
    }

    if (standalone) {
        preview_window = SDL_CreateWindow("Preview",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          sceneSettings.windowWidth,
                                          sceneSettings.windowHeight,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN |
                                              SDL_WINDOW_ALLOW_HIGHDPI);
        if (!preview_window) {
            fprintf(stderr, "SDL_CreateWindow Error (preview): %s\n", SDL_GetError());
            if (didInit) SDL_Quit();
            return;
        }
        preview_window_id = SDL_GetWindowID(preview_window);

#if USE_VULKAN
        {
            VkRendererConfig preview_cfg;
            VkRendererDevice* shared_device = NULL;
            VkRenderer* preview_vk = NULL;
            VkResult preview_init;
            vk_renderer_config_set_defaults(&preview_cfg);
            preview_cfg.enable_validation = SDL_FALSE;
            preview_cfg.clear_color[0] = 0.0f;
            preview_cfg.clear_color[1] = 0.0f;
            preview_cfg.clear_color[2] = 0.0f;
            preview_cfg.clear_color[3] = 1.0f;
            if (!vk_shared_device_init(preview_window, &preview_cfg)) {
                fprintf(stderr, "vk_shared_device_init failed (preview).\n");
                SDL_DestroyWindow(preview_window);
                if (didInit) SDL_Quit();
                return;
            }
            shared_device = vk_shared_device_get();
            if (!shared_device) {
                fprintf(stderr, "vk_shared_device_get failed (preview).\n");
                SDL_DestroyWindow(preview_window);
                if (didInit) SDL_Quit();
                return;
            }
            preview_vk = (VkRenderer*)malloc(sizeof(VkRenderer));
            if (!preview_vk) {
                SDL_DestroyWindow(preview_window);
                if (didInit) SDL_Quit();
                return;
            }
            preview_init = vk_renderer_init_with_device(preview_vk, shared_device, preview_window, &preview_cfg);
            if (preview_init != VK_SUCCESS) {
                fprintf(stderr, "vk_renderer_init failed (preview): %d\n", preview_init);
                free(preview_vk);
                SDL_DestroyWindow(preview_window);
                if (didInit) SDL_Quit();
                return;
            }
            preview_renderer = (SDL_Renderer*)preview_vk;
            vk_renderer_set_logical_size(preview_vk,
                                         (float)sceneSettings.windowWidth,
                                         (float)sceneSettings.windowHeight);
        }
#else
        preview_renderer = SDL_CreateRenderer(preview_window,
                                              -1,
                                              SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!preview_renderer) {
            fprintf(stderr, "SDL_CreateRenderer Error (preview): %s\n", SDL_GetError());
            SDL_DestroyWindow(preview_window);
            if (didInit) SDL_Quit();
            return;
        }
#endif
    } else {
        int host_width = 0;
        int host_height = 0;
        preview_window = host_window;
        preview_renderer = host_renderer;
        preview_window_id = SDL_GetWindowID(preview_window);
        SDL_GetWindowSize(preview_window, &host_width, &host_height);
        if (host_width > 0 && host_height > 0) {
            sceneSettings.windowWidth = host_width;
            sceneSettings.windowHeight = host_height;
        }
    }

    prev_ns = runtime_time_now_ns();
    while (running_preview) {
        SDL_Event event;
        SDL_Rect close_button_rect = PreviewCloseButtonRect(sceneSettings.windowWidth);
        int mouse_x = 0;
        int mouse_y = 0;
        bool close_button_hovered = false;
        PreviewPlaybackSample playback_sample = {0};
        double t = 0.0;
        Point light_point;
        double light_z = 0.0;
        Point camera_point = {sceneSettings.camera.x, sceneSettings.camera.y};
        PreviewCameraSample camera_sample = {0};
        PreviewCameraProjector preview_projector = {0};
        RuntimeSceneBridge3DDigestState preview_digest = {0};
        PreviewModeRouteDecision route_decision = {0};
        RayTracingRuntimeRoute preview_route = RayTracingModeBackend_ResolveRoute();
        RayTracingSceneDigestStatus preview_digest_status = {0};
        bool projector_ready = false;
        uint64_t now_ns;
        double dt;

        while (SDL_PollEvent(&event)) {
            if ((event.type == SDL_WINDOWEVENT &&
                 event.window.windowID == preview_window_id &&
                 event.window.event == SDL_WINDOWEVENT_CLOSE) ||
                (event.type == SDL_KEYDOWN &&
                 event.key.windowID == preview_window_id &&
                 event.key.keysym.sym == SDLK_ESCAPE)) {
                running_preview = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.button == SDL_BUTTON_LEFT &&
                event.button.windowID == preview_window_id &&
                event.button.x >= close_button_rect.x &&
                event.button.x <= close_button_rect.x + close_button_rect.w &&
                event.button.y >= close_button_rect.y &&
                event.button.y <= close_button_rect.y + close_button_rect.h) {
                close_button_pressed = true;
            }
            if (event.type == SDL_MOUSEBUTTONUP &&
                event.button.button == SDL_BUTTON_LEFT &&
                event.button.windowID == preview_window_id) {
                bool released_on_close_button =
                    event.button.x >= close_button_rect.x &&
                    event.button.x <= close_button_rect.x + close_button_rect.w &&
                    event.button.y >= close_button_rect.y &&
                    event.button.y <= close_button_rect.y + close_button_rect.h;
                if (close_button_pressed && released_on_close_button) {
                    running_preview = false;
                }
                close_button_pressed = false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (animation_handle_text_zoom_shortcut(&event.key)) {
                    continue;
                }
                animation_handle_fluid_overlay_key(event.key.keysym.sym);
            }
        }
        SDL_GetMouseState(&mouse_x, &mouse_y);
        close_button_hovered = (mouse_x >= close_button_rect.x &&
                                mouse_x <= close_button_rect.x + close_button_rect.w &&
                                mouse_y >= close_button_rect.y &&
                                mouse_y <= close_button_rect.y + close_button_rect.h);

        now_ns = runtime_time_now_ns();
        dt = runtime_time_diff_seconds(now_ns, prev_ns);
        prev_ns = now_ns;
        elapsed += dt;

        PreviewPlaybackEvaluate(elapsed,
                                animSettings.previewDuration,
                                animSettings.bounceMode,
                                animSettings.loopMode,
                                &playback_sample);
        t = playback_sample.normalized_t;

        light_point = (sceneSettings.bezierPath.numPoints >= 2)
                          ? GetPositionAlongPathNormalized(&sceneSettings.bezierPath, t)
                          : sceneSettings.bezierPath.points[0];
        light_z = (sceneSettings.bezierPath.numPoints >= 2)
                      ? CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath,
                                                            &sceneSettings.bezierPath3D,
                                                            t)
                      : sceneSettings.bezierPath3D.point_z[0];
        animSettings.lightHeight = light_z;

        if (PreviewCameraSampleEvaluate(&sceneSettings.camera,
                                        sceneSettings.cameraZ,
                                        &sceneSettings.cameraPath,
                                        &sceneSettings.cameraPath3D,
                                        t,
                                        sceneSettings.windowWidth,
                                        sceneSettings.windowHeight,
                                        &camera_sample) &&
            camera_sample.uses_authored_path) {
            camera_point.x = camera_sample.position_x;
            camera_point.y = camera_sample.position_y;
            sceneSettings.camera.x = camera_sample.position_x;
            sceneSettings.camera.y = camera_sample.position_y;
            sceneSettings.cameraZ = camera_sample.position_z;
            sceneSettings.camera.rotation = camera_sample.yaw_radians;
        }
        if (preview_route.requestedMode == SPACE_MODE_3D) {
            SDL_Rect preview_viewport = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight};
            projector_ready = PreviewCameraProjectorBuild(&camera_sample, preview_viewport, &preview_projector);
            runtime_scene_bridge_get_last_3d_digest_state(&preview_digest);
            preview_digest_status = RayTracingModeBackend_BuildSceneDigestStatus(&preview_route);
        }
        if (!PreviewModeRouteSelect(&preview_route,
                                    &preview_digest_status,
                                    projector_ready,
                                    &route_decision)) {
            memset(&route_decision, 0, sizeof(route_decision));
            route_decision.branch = PREVIEW_RENDER_BRANCH_LEGACY_2D;
            snprintf(route_decision.branchLabel, sizeof(route_decision.branchLabel), "Preview: 2D");
            snprintf(route_decision.statusLine,
                     sizeof(route_decision.statusLine),
                     "Preview route selection failed; using legacy 2D preview branch.");
        }

        setRenderContext(preview_renderer,
                         preview_window,
                         sceneSettings.windowWidth,
                         sceneSettings.windowHeight);
        render_set_clear_color(preview_renderer,
                               (Uint8)kPreviewBg,
                               (Uint8)kPreviewBg,
                               (Uint8)kPreviewBg + 5,
                               255);
        if (!render_begin_frame()) {
            if (render_device_lost()) {
                running_preview = false;
            }
            SDL_Delay(10);
            continue;
        }

        if (route_decision.branch == PREVIEW_RENDER_BRANCH_RETAINED_3D) {
            PreviewRetainedSceneRender(preview_renderer, &preview_digest, &preview_projector);
            PreviewRetainedSceneRenderLightMarker(preview_renderer,
                                                  &preview_projector,
                                                  light_point.x,
                                                  light_point.y,
                                                  light_z,
                                                  (SDL_Color){255, 255, 255, 255},
                                                  6);
        } else {
            SDL_Color path_color = {90, 120, 90, 180};
            SDL_Color camera_path_color = {60, 140, 220, 220};
            SDL_Color select_color = {255, 255, 160, 255};
            RenderBezierPathCameraStyled(preview_renderer,
                                         &sceneSettings.bezierPath,
                                         false,
                                         &sceneSettings.camera,
                                         path_color,
                                         (SDL_Color){0, 0, 0, 0},
                                         -1,
                                         select_color,
                                         3);
            RenderBezierPathCameraStyled(preview_renderer,
                                         &sceneSettings.cameraPath,
                                         false,
                                         &sceneSettings.camera,
                                         camera_path_color,
                                         (SDL_Color){0, 0, 0, 0},
                                         -1,
                                         select_color,
                                         4);
            SDL_SetRenderDrawColor(preview_renderer, 220, 220, 220, 255);
            RenderSceneObjects(preview_renderer, !AnimationUseFluidScene());
            DrawPreviewMarker(preview_renderer, light_point, (SDL_Color){255, 230, 120, 255}, 6);
            DrawPreviewMarker(preview_renderer, camera_point, (SDL_Color){120, 200, 255, 255}, 6);
        }
        DrawPreviewRouteStatus(preview_renderer, &route_decision, &playback_sample);
        DrawPreviewCloseButton(preview_renderer, close_button_rect, close_button_hovered);
        render_end_frame();
    }

    sceneSettings.camera = saved_camera;
    sceneSettings.cameraZ = saved_camera_z;
    if (standalone) {
#if USE_VULKAN
        if (preview_renderer) {
            VkRenderer* preview_vk = (VkRenderer*)preview_renderer;
            ray_tracing_text_reset_renderer(preview_renderer);
            vk_renderer_wait_idle(preview_vk);
            vk_renderer_shutdown_surface(preview_vk);
            free(preview_vk);
        }
#else
        if (preview_renderer) {
            ray_tracing_text_reset_renderer(preview_renderer);
            SDL_DestroyRenderer(preview_renderer);
        }
#endif
        if (preview_window) {
            SDL_DestroyWindow(preview_window);
        }
        PreviewSessionRestoreHostWindow(host_window);
    } else {
        SDL_CaptureMouse(SDL_FALSE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
        setRenderContext(host_renderer,
                         host_window,
                         sceneSettings.windowWidth,
                         sceneSettings.windowHeight);
        SDL_PumpEvents();
    }
    if (didInit) {
        SDL_Quit();
    }
}

void RunPreviewMode(void) {
    RunPreviewInternal(true, NULL, NULL);
}

void RunPreviewModeEmbedded(SDL_Window* host_window, SDL_Renderer* host_renderer) {
    RunPreviewInternal(false, host_window, host_renderer);
}
