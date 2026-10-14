#include "app/animation.h"
#include "app/animation_input_helpers.h"
#include "app/evaluated_scene_service.h"
#include "app/preview_mode_route.h"
#include "app/preview_camera_sample.h"
#include "app/preview_camera_projector.h"
#include "app/preview_retained_scene_quality.h"
#include "app/preview_retained_scene_renderer.h"
#include "app/preview_retained_scene_surface.h"
#include "app/preview_timeline_inspection.h"
#include "app/preview_timeline_inspection_render.h"
#include "app/preview_workspace.h"
#include "app/preview_workspace_render.h"
#include "app/preview_status_render.h"
#include "kit_ui_window_sdl.h"
#include "app/runtime_time.h"
#include "config/config_manager.h"
#include "editor/material_editor_face_preview.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "render/ray_tracing2.h"
#include "render/render_helper.h"
#include "render/space_mode_adapter.h"
#include "render/ray_tracing_mode_backend.h"
#include "engine/Render/render_pipeline.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "import/runtime_scene_bridge.h"
#include "render/vk_shared_device.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double kPreviewBg = 60.0;

static void DrawPreviewMarker(SDL_Renderer* renderer,
                              const Camera* camera,
                              Point world,
                              SDL_Color color,
                              int radius) {
    SpaceModeViewContext view_ctx;
    CameraPoint screen;
    int dx = 0;
    int dy = 0;
    if (!renderer || !camera) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    view_ctx = SpaceModeAdapter_BuildViewContext(camera,
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

static void PreviewSessionSyncWindowSize(SDL_Window* preview_window,
                                         PreviewWorkspace* preview_workspace) {
    int width = 0;
    int height = 0;
    if (!preview_window) return;
    SDL_GetWindowSize(preview_window, &width, &height);
    if (width <= 0 || height <= 0) return;
    if (sceneSettings.windowWidth == width && sceneSettings.windowHeight == height) return;

    sceneSettings.windowWidth = width;
    sceneSettings.windowHeight = height;
    if (preview_workspace && preview_workspace->valid) {
        (void)PreviewWorkspaceResize(preview_workspace, width, height);
    }
}

static void RunPreviewInternal(bool standalone, SDL_Window* host_window, SDL_Renderer* host_renderer,
                               TimelineSample* in_out_sample) {
    bool didInit = false;
    bool didFontRuntimeInit = false;
    Uint32 preview_window_id = 0;
    SDL_Window* preview_window = NULL;
    SDL_Renderer* preview_renderer = NULL;
    uint64_t prev_ns = 0;
    bool running_preview = true;
    PreviewSharedControls shared_controls = {0};
    KitUiWindowState observed_window = {0};
    bool sample_initialized = false;
    PreviewWorkspace preview_workspace = {0};
    PreviewTimelineInspection timeline_inspection = {0};
    RuntimeSceneLightTimelineDocument timeline_document = {0};
    TimelineRate preview_rate = {0};
    TimelineRange preview_range = {0};
    PreviewRetainedSceneQuality preview_quality =
        PREVIEW_RETAINED_SCENE_QUALITY_WIREFRAME;
#if USE_VULKAN
    bool owns_shared_device = false;
#endif

    if (standalone) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL_Init Error (preview): %s\n", SDL_GetError());
            return;
        }
        didInit = true;
        if (!ray_tracing_font_runtime_init()) {
            fprintf(stderr, "TTF_Init Error (preview): %s\n", TTF_GetError());
            SDL_Quit();
            return;
        }
        didFontRuntimeInit = true;
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
            if (didFontRuntimeInit) ray_tracing_font_runtime_shutdown();
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
            owns_shared_device = (vk_shared_device_get() == NULL);
            if (!vk_shared_device_init(preview_window, &preview_cfg)) {
                fprintf(stderr, "vk_shared_device_init failed (preview).\n");
                SDL_DestroyWindow(preview_window);
                if (didInit) SDL_Quit();
                return;
            }
            shared_device = vk_shared_device_get();
            if (!shared_device) {
                fprintf(stderr, "vk_shared_device_get failed (preview).\n");
                if (owns_shared_device) vk_shared_device_shutdown();
                SDL_DestroyWindow(preview_window);
                if (didFontRuntimeInit) ray_tracing_font_runtime_shutdown();
                if (didInit) SDL_Quit();
                return;
            }
            preview_vk = (VkRenderer*)malloc(sizeof(VkRenderer));
            if (!preview_vk) {
                if (owns_shared_device) vk_shared_device_shutdown();
                SDL_DestroyWindow(preview_window);
                if (didFontRuntimeInit) ray_tracing_font_runtime_shutdown();
                if (didInit) SDL_Quit();
                return;
            }
            preview_init = vk_renderer_init_with_device(preview_vk, shared_device, preview_window, &preview_cfg);
            if (preview_init != VK_SUCCESS) {
                fprintf(stderr, "vk_renderer_init failed (preview): %d\n", preview_init);
                free(preview_vk);
                if (owns_shared_device) vk_shared_device_shutdown();
                SDL_DestroyWindow(preview_window);
                if (didFontRuntimeInit) ray_tracing_font_runtime_shutdown();
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
            if (didFontRuntimeInit) ray_tracing_font_runtime_shutdown();
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

    PreviewRetainedSceneSurfacePrepare();
    if (RayEvaluatedSceneResolveTimelineClock(
            &preview_rate, &preview_range) != TIMELINE_STATUS_OK ||
        PreviewWorkspaceInit(
            &preview_workspace, preview_rate, preview_range,
            sceneSettings.windowWidth,
            sceneSettings.windowHeight) != TIMELINE_STATUS_OK) {
        fprintf(stderr, "Preview transport initialization failed.\n");
        running_preview = false;
    }
    if(running_preview && in_out_sample &&
       PreviewWorkspaceInspectSample(&preview_workspace,*in_out_sample)!=TIMELINE_STATUS_OK) {
        fprintf(stderr,"Preview requested sample is outside the scene timeline.\n");
        running_preview=false;
    }
    sample_initialized=running_preview && in_out_sample;
    if (RuntimeSceneLightTimelineGetLast(&timeline_document) &&
        PreviewTimelineInspectionInit(&timeline_inspection,
                                      &timeline_document) !=
            TIMELINE_STATUS_OK) {
        fprintf(stderr, "Preview timeline marker projection failed.\n");
        memset(&timeline_inspection, 0, sizeof(timeline_inspection));
    }
    prev_ns = runtime_time_now_ns();
    while (running_preview) {
        SDL_Event event;
        SDL_Rect close_button_rect = PreviewSharedCloseRect(sceneSettings.windowWidth);
        RayEvaluatedSceneServiceResult evaluation = {0};
        TimelineSample selected_sample = {0};
        PreviewTransportDirection selected_direction =
            PREVIEW_TRANSPORT_DIRECTION_FORWARD;
        PreviewRetainedSceneFrame retained_frame = {0};
        Point light_point = {sceneSettings.bezierPath.points[0].x,
                             sceneSettings.bezierPath.points[0].y};
        double light_z = sceneSettings.bezierPath3D.point_z[0];
        Point camera_point = {sceneSettings.camera.x, sceneSettings.camera.y};
        PreviewCameraSample camera_sample = {0};
        Camera evaluated_view_camera = sceneSettings.camera;
        PreviewCameraProjector preview_projector = {0};
        RuntimeSceneBridge3DDigestState preview_digest = {0};
        PreviewModeRouteDecision route_decision = {0};
        RayTracingRuntimeRoute preview_route = RayTracingModeBackend_ResolveRoute();
        RayTracingSceneDigestStatus preview_digest_status = {0};
        bool projector_ready = false;
        uint64_t now_ns;
        double dt;

        uint32_t window_changes = 0;
        if (kit_ui_window_refresh_sdl(&observed_window, preview_window, &window_changes).code != CORE_OK)
            observed_window.presentable = 0;
        if (window_changes) {
            PreviewSharedControlsCancel(&shared_controls);
            PreviewWorkspaceCancelPointer(&preview_workspace);
            PreviewSessionSyncWindowSize(preview_window, &preview_workspace);
        }
        while (SDL_PollEvent(&event)) {
            Uint32 event_window = 0;
            switch (event.type) {
                case SDL_WINDOWEVENT: event_window = event.window.windowID; break;
                case SDL_KEYDOWN: case SDL_KEYUP: event_window = event.key.windowID; break;
                case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: event_window = event.button.windowID; break;
                case SDL_MOUSEMOTION: event_window = event.motion.windowID; break;
                case SDL_MOUSEWHEEL: event_window = event.wheel.windowID; break;
                default: break;
            }
            if (event_window && event_window != preview_window_id) continue;
            if (kit_ui_window_event_invalidates_sdl(&event)) {
                PreviewSharedControlsCancel(&shared_controls);
                PreviewWorkspaceCancelPointer(&preview_workspace);
                PreviewSessionSyncWindowSize(preview_window, &preview_workspace);
            }
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running_preview = false;
                continue;
            }
            if (event.type == SDL_WINDOWEVENT) continue;
            if (!observed_window.presentable ||
                ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) &&
                 !(observed_window.flags & SDL_WINDOW_INPUT_FOCUS))) continue;
            if (kit_ui_window_fullscreen_key_sdl(preview_window, &event)) {
                PreviewSharedControlsCancel(&shared_controls);
                PreviewWorkspaceCancelPointer(&preview_workspace);
                continue;
            }
            /* Until fresh geometry has been painted, neither controls nor
             * scientific pointer gestures may acquire a stale target. */
            if (!shared_controls.ready &&
                (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
                 event.type == SDL_MOUSEMOTION || event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)) continue;
            PreviewSharedAction action = PREVIEW_SHARED_NONE;
            if (!preview_workspace.scrubbing && PreviewSharedControlsRoute(&shared_controls, &event, &action)) {
                switch (action) {
                    case PREVIEW_SHARED_PLAY: (void)PreviewWorkspaceTogglePlayback(&preview_workspace); break;
                    case PREVIEW_SHARED_LOOP: (void)PreviewWorkspaceSetMode(&preview_workspace, PREVIEW_TRANSPORT_MODE_LOOP); break;
                    case PREVIEW_SHARED_BOUNCE: (void)PreviewWorkspaceSetMode(&preview_workspace, PREVIEW_TRANSPORT_MODE_BOUNCE); break;
                    case PREVIEW_SHARED_CLOSE: running_preview = false; break;
                    default: break;
                }
                continue;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                TimelineSample marker_sample = {0};
                if (PreviewTimelineInspectionSelectAt(&timeline_inspection,
                        preview_workspace.layout.slider_track, event.button.x, event.button.y, 7, &marker_sample)) {
                    (void)PreviewWorkspaceInspectSample(&preview_workspace, marker_sample);
                } else {
                    if (event.button.y >= preview_workspace.layout.slider_track.y &&
                        event.button.y <= preview_workspace.layout.slider_track.y + preview_workspace.layout.slider_track.h)
                        PreviewTimelineInspectionClearSelection(&timeline_inspection);
                    (void)PreviewWorkspacePointerDown(&preview_workspace, event.button.x, event.button.y);
                }
            }
            if (event.type == SDL_MOUSEMOTION)
                (void)PreviewWorkspacePointerMotion(&preview_workspace, event.motion.x, event.motion.y);
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
                (void)PreviewWorkspacePointerUp(&preview_workspace, event.button.x, event.button.y);
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE: (void)PreviewWorkspaceTogglePlayback(&preview_workspace); continue;
                    case SDLK_l: (void)PreviewWorkspaceSetMode(&preview_workspace, PREVIEW_TRANSPORT_MODE_LOOP); continue;
                    case SDLK_b: (void)PreviewWorkspaceSetMode(&preview_workspace, PREVIEW_TRANSPORT_MODE_BOUNCE); continue;
                    case SDLK_LEFT: (void)PreviewWorkspaceSeekFrameDelta(&preview_workspace,-1); continue;
                    case SDLK_RIGHT: (void)PreviewWorkspaceSeekFrameDelta(&preview_workspace,1); continue;
                    case SDLK_q: preview_quality = PreviewRetainedSceneQualityCycle(preview_quality); continue;
                    default: break;
                }
                if (animation_handle_text_zoom_shortcut(&event.key)) continue;
                animation_handle_fluid_overlay_key(event.key.keysym.sym);
            }
        }

        now_ns = runtime_time_now_ns();
        dt = runtime_time_diff_seconds(now_ns, prev_ns);
        prev_ns = now_ns;
        (void)PreviewWorkspaceAdvance(&preview_workspace, dt);
        (void)PreviewWorkspaceCurrentSample(
            &preview_workspace, &selected_sample, &selected_direction);

        if (RayEvaluatedSceneCaptureSampleWithPlayback(
                selected_sample,
                PreviewTransportEvaluatedPlaybackMode(
                    &preview_workspace.transport),
                selected_direction == PREVIEW_TRANSPORT_DIRECTION_REVERSE,
                false,
                &evaluation)) {
            const RayEvaluatedSceneSnapshot* snapshot = &evaluation.snapshot;
            light_point.x = snapshot->light.position.x;
            light_point.y = snapshot->light.position.y;
            light_z = snapshot->light.position.z;
            camera_point.x = snapshot->camera.position.x;
            camera_point.y = snapshot->camera.position.y;
            evaluated_view_camera.x = snapshot->camera.position.x;
            evaluated_view_camera.y = snapshot->camera.position.y;
            evaluated_view_camera.rotation = snapshot->camera.yaw_radians;
            evaluated_view_camera.zoom = snapshot->camera.zoom;
            camera_sample.valid = snapshot->camera.valid;
            camera_sample.uses_authored_path =
                snapshot->camera.uses_authored_path;
            camera_sample.position_x = snapshot->camera.position.x;
            camera_sample.position_y = snapshot->camera.position.y;
            camera_sample.position_z = snapshot->camera.position.z;
            camera_sample.yaw_radians = snapshot->camera.yaw_radians;
            camera_sample.pitch_radians = snapshot->camera.pitch_radians;
            camera_sample.has_orientation_frame=snapshot->camera.has_orientation_frame;
            camera_sample.orientation_frame=snapshot->camera.orientation_frame;
            camera_sample.orientation_fallback=snapshot->camera.orientation_fallback;
            camera_sample.fov_y_degrees = snapshot->camera.fov_y_degrees;
            camera_sample.aspect_ratio = snapshot->camera.aspect_ratio;
        } else {
            (void)PreviewCameraSampleEvaluate(
                &sceneSettings.camera,
                sceneSettings.cameraZ,
                &sceneSettings.cameraPath,
                &sceneSettings.cameraPath3D,
                0.0,
                sceneSettings.windowWidth,
                sceneSettings.windowHeight,
                &camera_sample);
        }
        PreviewTimelineInspectionUpdateReadout(
            &timeline_inspection, evaluation.snapshot.valid
                                      ? &evaluation.snapshot
                                      : NULL);
        if (preview_route.requestedMode == SPACE_MODE_3D) {
            SDL_Rect preview_viewport = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight};
            projector_ready = PreviewCameraProjectorBuild(&camera_sample, preview_viewport, &preview_projector);
            runtime_scene_bridge_get_last_3d_digest_state(&preview_digest);
            preview_digest_status = RayTracingModeBackend_BuildSceneDigestStatus(&preview_route);
        }
        (void)PreviewRetainedSceneFrameBuild(
            preview_quality, &evaluation.snapshot, &retained_frame);
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
        if (standalone) {
            ray_tracing_font_runtime_attach_renderer(preview_renderer);
        }
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
            if (PreviewRetainedSceneQualityUsesSurface(preview_quality)) {
                (void)PreviewRetainedSceneSurfaceRender(preview_renderer,
                                                        &preview_projector,
                                                        &retained_frame,
                                                        NULL);
            }
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
                                         &evaluated_view_camera,
                                         path_color,
                                         (SDL_Color){0, 0, 0, 0},
                                         -1,
                                         select_color,
                                         3);
            RenderBezierPathCameraStyled(preview_renderer,
                                         &sceneSettings.cameraPath,
                                         false,
                                         &evaluated_view_camera,
                                         camera_path_color,
                                         (SDL_Color){0, 0, 0, 0},
                                         -1,
                                         select_color,
                                         4);
            SDL_SetRenderDrawColor(preview_renderer, 220, 220, 220, 255);
            RenderSceneObjectsWithCamera(preview_renderer,
                                         !AnimationUseFluidScene(),
                                         &evaluated_view_camera);
            DrawPreviewMarker(preview_renderer,
                              &evaluated_view_camera,
                              light_point,
                              (SDL_Color){255, 230, 120, 255},
                              6);
            DrawPreviewMarker(preview_renderer,
                              &evaluated_view_camera,
                              camera_point,
                              (SDL_Color){120, 200, 255, 255},
                              6);
        }
        PreviewStatusRender(preview_renderer, sceneSettings.windowWidth, preview_workspace.layout.panel.y, timeline_inspection.has_selection,
                               &route_decision,
                               &evaluation,
                               preview_quality);
        PreviewSharedControlsPublish(&shared_controls, &preview_workspace);
        PreviewWorkspaceRender(preview_renderer, &preview_workspace, &shared_controls);
        PreviewTimelineInspectionRender(preview_renderer, &preview_workspace,
                                        &timeline_inspection);
        close_button_rect = PreviewSharedCloseRect(sceneSettings.windowWidth);
        PreviewSharedButtonRender(preview_renderer, &shared_controls, PREVIEW_SHARED_CLOSE,
                                  close_button_rect, "Close Preview", false);
        render_end_frame();
    }

    PreviewRetainedSceneSurfaceReset(preview_renderer);
    PreviewTimelineInspectionReset(&timeline_inspection);
    if (standalone) {
        ray_tracing_font_runtime_detach_renderer(preview_renderer);
#if USE_VULKAN
        if (preview_renderer) {
            VkRenderer* preview_vk = (VkRenderer*)preview_renderer;
            ray_tracing_text_reset_renderer(preview_renderer);
            vk_renderer_wait_idle(preview_vk);
            MaterialEditorFacePreviewDetachRenderer(preview_renderer);
            vk_renderer_shutdown_surface(preview_vk);
            free(preview_vk);
        }
#else
        if (preview_renderer) {
            ray_tracing_text_reset_renderer(preview_renderer);
            MaterialEditorFacePreviewDetachRenderer(preview_renderer);
            SDL_DestroyRenderer(preview_renderer);
        }
#endif
        if (preview_window) {
            SDL_DestroyWindow(preview_window);
        }
        if (owns_shared_device) {
            vk_shared_device_shutdown();
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
    if(sample_initialized && preview_workspace.valid) {
        PreviewTransportDirection direction;
        PreviewWorkspaceCurrentSample(&preview_workspace,in_out_sample,&direction);
    }
    if (didInit) {
        ray_tracing_font_runtime_shutdown();
        SDL_Quit();
    }
}

void RunPreviewMode(void) {
    RunPreviewInternal(true, NULL, NULL, NULL);
}

void RunPreviewModeEmbedded(SDL_Window* host_window, SDL_Renderer* host_renderer) {
    RunPreviewInternal(false, host_window, host_renderer, NULL);
}
void RunPreviewModeEmbeddedAtSample(SDL_Window* host_window, SDL_Renderer* host_renderer,
                                    TimelineSample* in_out_sample) {
    RunPreviewInternal(false,host_window,host_renderer,in_out_sample);
}
