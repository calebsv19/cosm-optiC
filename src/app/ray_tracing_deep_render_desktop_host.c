#include "app/ray_tracing_deep_render_desktop_host.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "app/data_paths.h"
#include "app/ray_tracing_deep_render_cancellation.h"
#include "app/ray_tracing_deep_render_completion.h"
#include "app/ray_tracing_deep_render_desktop_render_internal.h"
#include "config/config_manager.h"
#include "engine/Render/render_pipeline.h"
#include "render/pipeline/ray_tracing2_preview_present.h"
#include "render/ray_tracing2.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_native_3d_progress_hud.h"
#include "render/timer_hud_api.h"

typedef struct RayTracingDeepRenderDesktopHostState {
    RayTracingDeepRenderSession session;
    RayTracingDeepRenderListener listener;
    RayTracingDeepRenderCancellation cancellation;
    RayTracingDeepRenderDesktopRenderUnit renderUnit;
    RuntimeNative3DAsyncRenderJob* job;
    RayTracingDeepRenderDesktopSelection selection;
    bool initialized;
    bool fallbackLogged;
    bool failureLogged;
} RayTracingDeepRenderDesktopHostState;

typedef struct RayTracingDeepRenderDesktopPresentContext {
    SDL_Renderer* renderer;
    SDL_Rect destination;
    bool drewImage;
} RayTracingDeepRenderDesktopPresentContext;

static RayTracingDeepRenderDesktopHostState s_deep_render_desktop_host;

RayTracingDeepRenderDesktopSelection
RayTracingDeepRenderDesktopHost_AssessSelection(bool deep_render,
                                                bool async_deep_render,
                                                bool native_3d,
                                                bool tiled,
                                                bool dynamic_dependency) {
    if (!deep_render || !async_deep_render) {
        return RAY_TRACING_DEEP_RENDER_DESKTOP_NOT_REQUESTED;
    }
    if (!native_3d || !tiled) {
        return RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_ROUTE;
    }
    if (dynamic_dependency) {
        return RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_DYNAMIC_DEPENDENCY;
    }
    return RAY_TRACING_DEEP_RENDER_DESKTOP_SELECTED;
}

const char* RayTracingDeepRenderDesktopSelection_Name(
    RayTracingDeepRenderDesktopSelection selection) {
    switch (selection) {
        case RAY_TRACING_DEEP_RENDER_DESKTOP_NOT_REQUESTED:
            return "not_requested";
        case RAY_TRACING_DEEP_RENDER_DESKTOP_SELECTED: return "selected";
        case RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_ROUTE:
            return "fallback_route";
        case RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_DYNAMIC_DEPENDENCY:
            return "fallback_dynamic_dependency";
    }
    return "unknown";
}

static bool deep_render_desktop_active_state(
    RayTracingDeepRenderSessionState state) {
    return state == RAY_TRACING_DEEP_RENDER_SESSION_PREPARING ||
           state == RAY_TRACING_DEEP_RENDER_SESSION_RENDERING ||
           state == RAY_TRACING_DEEP_RENDER_SESSION_SAVING ||
           state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELING;
}

static bool deep_render_desktop_present(
    const RayTracingDeepRenderPresentationView* view,
    void* user_data) {
    RayTracingDeepRenderDesktopPresentContext* context =
        (RayTracingDeepRenderDesktopPresentContext*)user_data;
    if (!view || !view->valid || !view->pixels || !context ||
        !context->renderer) {
        return false;
    }
    RayTracing2PreviewPresent_DrawABGRBufferToRectFiltered(
        context->renderer,
        view->pixels,
        view->hostWidth,
        view->hostHeight,
        context->destination,
        animSettings.upscaleMode3D == RUNTIME_3D_UPSCALE_MODE_BILINEAR);
    context->drewImage = true;
    return true;
}

static bool deep_render_desktop_descendant_path(const char* parent,
                                                const char* child) {
    size_t parent_length = 0u;
    if (!parent || !parent[0] || !child || !child[0] || strstr(parent, "..") ||
        strstr(child, "..")) {
        return false;
    }
    parent_length = strlen(parent);
    while (parent_length > 1u && parent[parent_length - 1u] == '/') {
        parent_length -= 1u;
    }
    return strncmp(parent, child, parent_length) == 0 &&
           child[parent_length] == '/';
}

static bool deep_render_desktop_resolve_output(int absolute_frame_index,
                                               char* output_root,
                                               size_t output_root_size,
                                               char* frame_directory,
                                               size_t frame_directory_size,
                                               char* final_path,
                                               size_t final_path_size) {
    const char* configured_root = animSettings.outputRoot[0]
                                      ? animSettings.outputRoot
                                      : ray_tracing_default_output_root();
    if (!output_root || !frame_directory || !final_path ||
        snprintf(output_root, output_root_size, "%s", configured_root) >=
            (int)output_root_size ||
        !ray_tracing_resolve_frame_output_dir(
            animSettings.frameDir, frame_directory, frame_directory_size) ||
        !deep_render_desktop_descendant_path(output_root, frame_directory) ||
        snprintf(final_path,
                 final_path_size,
                 "%s/frame_%04d.bmp",
                 frame_directory,
                 absolute_frame_index) >= (int)final_path_size) {
        return false;
    }
    return true;
}

static void deep_render_desktop_mark_failed(
    RayTracingDeepRenderDesktopHostState* state,
    const char* reason,
    bool* running) {
    RuntimeNative3DAsyncRenderJobStatus job_status =
        RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE;
    if (!state) return;
    if (state->job) {
        job_status = RuntimeNative3DAsyncRenderJob_GetStatus(state->job);
        if (job_status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING) {
            RuntimeNative3DAsyncRenderJob_RequestCancel(state->job);
        }
        if (job_status != RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE) {
            (void)RuntimeNative3DAsyncRenderJob_Join(state->job);
        }
    }
    if (deep_render_desktop_active_state(state->session.state)) {
        (void)RayTracingDeepRenderSession_MarkFailed(
            &state->session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER);
    }
    if (!state->failureLogged) {
        fprintf(stderr,
                "[deep_render_async] stopped: %s\n",
                reason ? reason : "unknown failure");
        state->failureLogged = true;
    }
    if (running) *running = false;
}

bool RayTracingDeepRenderDesktopHost_BeginRun(int start_frame_index,
                                              int frame_count) {
    RayTracingDeepRenderDesktopHostState* state = &s_deep_render_desktop_host;
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RayTracingDeepRenderSessionDesc session_desc = {
        .startFrameIndex = start_frame_index,
        .frameCount = frame_count,
        .initialGeneration = 1u,
    };

    RayTracingDeepRenderDesktopHost_Shutdown();
    state->selection = RayTracingDeepRenderDesktopHost_AssessSelection(
        animSettings.deepRenderMode,
        animSettings.asyncDeepRender,
        RayTracingModeBackend_IsNative3D(&route),
        route.useTiles,
        animSettings.volumeInteractionEnabled);
    if (state->selection != RAY_TRACING_DEEP_RENDER_DESKTOP_SELECTED) {
        if (state->selection != RAY_TRACING_DEEP_RENDER_DESKTOP_NOT_REQUESTED) {
            fprintf(stderr,
                    "[deep_render_async] synchronous fallback: %s\n",
                    RayTracingDeepRenderDesktopSelection_Name(state->selection));
            state->fallbackLogged = true;
        }
        return false;
    }
    if (start_frame_index < 0 || frame_count <= 0) {
        state->selection = RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_ROUTE;
        return false;
    }
    RayTracingDeepRenderSession_Init(&state->session);
    RayTracingDeepRenderListener_Init(&state->listener);
    RayTracingDeepRenderCancellation_Init(&state->cancellation);
    RayTracingDeepRenderDesktopRenderUnit_Init(&state->renderUnit);
    state->job = RuntimeNative3DAsyncRenderJob_Create();
    if (!state->job ||
        !RayTracingDeepRenderSession_Begin(&state->session, &session_desc)) {
        RayTracingDeepRenderDesktopHost_Shutdown();
        return false;
    }
    state->initialized = true;
    fprintf(stderr,
            "[deep_render_async] selected start=%d frames=%d\n",
            start_frame_index,
            frame_count);
    return true;
}

static bool deep_render_desktop_start_current_frame(
    RayTracingDeepRenderDesktopHostState* state,
    double light_x,
    double light_y,
    bool* running) {
    RayTracingDeepRenderDesktopStartDesc desc = {0};
    RayTracingDeepRenderDesktopStartStatus start_status;
    const char* reason = NULL;
    char output_root[PATH_MAX];
    char frame_directory[PATH_MAX];
    char final_path[PATH_MAX];
    if (!state || state->session.state != RAY_TRACING_DEEP_RENDER_SESSION_PREPARING) {
        return false;
    }
    if (!deep_render_desktop_resolve_output(
            state->session.currentAbsoluteFrameIndex,
            output_root,
            sizeof(output_root),
            frame_directory,
            sizeof(frame_directory),
            final_path,
            sizeof(final_path))) {
        deep_render_desktop_mark_failed(state, "invalid output identity", running);
        return false;
    }
    desc.generation = state->session.generation;
    desc.localFrameIndex = state->session.currentLocalFrameIndex;
    desc.absoluteFrameIndex = state->session.currentAbsoluteFrameIndex;
    desc.frameCount = state->session.frameCount;
    desc.frameDurationSeconds = animSettings.frameDuration;
    desc.animationTimeSeconds =
        (double)desc.absoluteFrameIndex * animSettings.frameDuration;
    desc.normalizedT = AnimationCurrentNormalizedT();
    desc.lightX = light_x;
    desc.lightY = light_y;
    desc.outputWidth = sceneSettings.windowWidth;
    desc.outputHeight = sceneSettings.windowHeight;
    desc.outputRoot = output_root;
    desc.frameDirectory = frame_directory;
    desc.finalFramePath = final_path;
    start_status = RayTracingDeepRenderDesktopRenderUnit_Start(
        &state->renderUnit, &state->session, state->job, &desc, &reason);
    if (start_status == RAY_TRACING_DEEP_RENDER_DESKTOP_START_READY) {
        RayTracingDeepRenderCancellation_Reset(&state->cancellation);
        return true;
    }
    if (start_status == RAY_TRACING_DEEP_RENDER_DESKTOP_START_UNSUPPORTED &&
        state->session.completedFrameCount == 0) {
        fprintf(stderr,
                "[deep_render_async] synchronous fallback: %s\n",
                reason ? reason : "unsupported request");
        state->selection = RAY_TRACING_DEEP_RENDER_DESKTOP_FALLBACK_ROUTE;
        state->fallbackLogged = true;
        state->initialized = false;
        return false;
    }
    deep_render_desktop_mark_failed(state, reason, running);
    return false;
}

static void deep_render_desktop_request_cancel(
    RayTracingDeepRenderDesktopHostState* state,
    bool* running) {
    RayTracingDeepRenderCancellationResult result;
    RuntimeNative3DAsyncRenderJob* job = NULL;
    if (!state || !deep_render_desktop_active_state(state->session.state) ||
        state->session.state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELING) {
        return;
    }
    if (state->session.state != RAY_TRACING_DEEP_RENDER_SESSION_PREPARING) {
        job = state->job;
    }
    if (!RayTracingDeepRenderCancellation_Request(
            &state->cancellation, &state->session, job, &result)) {
        deep_render_desktop_mark_failed(state, "cancel request failed", running);
    }
}

static void deep_render_desktop_consume_terminal(
    RayTracingDeepRenderDesktopHostState* state,
    const RayTracingDeepRenderListenerPollResult* poll,
    int* frame_counter,
    bool* running) {
    if (!state || !poll || !poll->terminalObserved) return;
    if (state->session.state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELING) {
        RayTracingDeepRenderCancellationResult cancel_result;
        if (!RayTracingDeepRenderCancellation_Poll(&state->cancellation,
                                                   &state->session,
                                                   state->job,
                                                   poll,
                                                   &cancel_result)) {
            deep_render_desktop_mark_failed(
                state,
                RayTracingDeepRenderCancellationStatus_Name(cancel_result.status),
                running);
        }
        return;
    }
    {
        RayTracingDeepRenderCompletionResult completion;
        RayTracingDeepRenderPresentationView view =
            RayTracingDeepRenderListener_GetView(&state->listener);
        if (!RayTracingDeepRenderCompletion_Process(&state->session,
                                                    state->job,
                                                    poll,
                                                    &view,
                                                    NULL,
                                                    &completion)) {
            deep_render_desktop_mark_failed(
                state,
                RayTracingDeepRenderCompletionStatus_Name(completion.status),
                running);
            return;
        }
        if (frame_counter) {
            *frame_counter = state->session.completedFrameCount;
        }
        if (state->session.state == RAY_TRACING_DEEP_RENDER_SESSION_COMPLETED) {
            printf("Deep render mode complete. Final frame saved.\n");
            if (running) *running = false;
        }
    }
}

bool RayTracingDeepRenderDesktopHost_SubmitFrame(SDL_Window* window,
                                                SDL_Renderer* renderer,
                                                double light_x,
                                                double light_y,
                                                int* frame_counter,
                                                bool* running) {
    RayTracingDeepRenderDesktopHostState* state = &s_deep_render_desktop_host;
    RayTracingDeepRenderListenerPollResult poll = {0};
    RayTracingDeepRenderDesktopPresentContext present = {
        .renderer = renderer,
        .destination = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight},
    };
    TimerHUDSession* timer_hud = timer_hud_session();
    bool frame_presented = false;

    if (state->selection != RAY_TRACING_DEEP_RENDER_DESKTOP_SELECTED ||
        !state->initialized) {
        return false;
    }
    if (!window || !renderer || !frame_counter || !running) return true;
    setRenderContext(renderer,
                     window,
                     sceneSettings.windowWidth,
                     sceneSettings.windowHeight);
    SetLightPosition(light_x, light_y);
    if (!*running) deep_render_desktop_request_cancel(state, running);
    if (state->session.state == RAY_TRACING_DEEP_RENDER_SESSION_PREPARING &&
        *running &&
        !deep_render_desktop_start_current_frame(state, light_x, light_y, running)) {
        if (state->selection != RAY_TRACING_DEEP_RENDER_DESKTOP_SELECTED) {
            return false;
        }
    }

    if (timer_hud) {
        ts_session_frame_start(timer_hud);
        ts_session_start_timer(timer_hud, "Render Scene Frame");
    }
    render_set_clear_color(renderer, 0, 0, 0, 255);
    if (!render_begin_frame()) {
        if (render_device_lost()) *running = false;
        if (timer_hud) {
            ts_session_stop_timer(timer_hud, "Render Scene Frame");
            ts_session_frame_end(timer_hud);
        }
        return true;
    }

    if (state->session.state == RAY_TRACING_DEEP_RENDER_SESSION_RENDERING ||
        state->session.state == RAY_TRACING_DEEP_RENDER_SESSION_CANCELING) {
        if (!RayTracingDeepRenderListener_Poll(&state->listener,
                                               &state->session,
                                               state->renderUnit.progress,
                                               state->job,
                                               deep_render_desktop_present,
                                               &present,
                                               &poll)) {
            deep_render_desktop_mark_failed(
                state,
                RayTracingDeepRenderListenerStatus_Name(poll.status),
                running);
        } else {
            deep_render_desktop_consume_terminal(
                state, &poll, frame_counter, running);
        }
    }
    if (timer_hud) {
        ts_session_stop_timer(timer_hud, "Render Scene Frame");
        ts_session_render(timer_hud);
    }
    RuntimeNative3DProgressHUD_Draw(renderer);
    frame_presented = render_end_frame();
    if (!frame_presented && render_device_lost()) *running = false;
    if (timer_hud) ts_session_frame_end(timer_hud);
    return true;
}

bool RayTracingDeepRenderDesktopHost_HasActiveWork(void) {
    const RayTracingDeepRenderDesktopHostState* state =
        &s_deep_render_desktop_host;
    return state->initialized &&
           state->selection == RAY_TRACING_DEEP_RENDER_DESKTOP_SELECTED &&
           deep_render_desktop_active_state(state->session.state);
}

bool RayTracingDeepRenderDesktopHost_IsSelected(void) {
    return s_deep_render_desktop_host.initialized &&
           s_deep_render_desktop_host.selection ==
               RAY_TRACING_DEEP_RENDER_DESKTOP_SELECTED;
}

bool RayTracingDeepRenderDesktopHost_CompletedSuccessfully(void) {
    return RayTracingDeepRenderDesktopHost_IsSelected() &&
           s_deep_render_desktop_host.session.state ==
               RAY_TRACING_DEEP_RENDER_SESSION_COMPLETED;
}

void RayTracingDeepRenderDesktopHost_Shutdown(void) {
    RayTracingDeepRenderDesktopHostState* state = &s_deep_render_desktop_host;
    if (state->job) {
        RuntimeNative3DAsyncRenderJob_Destroy(state->job);
        state->job = NULL;
    }
    RayTracingDeepRenderDesktopRenderUnit_Destroy(&state->renderUnit);
    RayTracingDeepRenderListener_Destroy(&state->listener);
    RayTracingDeepRenderSession_Reset(&state->session);
    memset(state, 0, sizeof(*state));
}
