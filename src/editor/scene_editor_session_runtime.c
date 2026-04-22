#include "editor/scene_editor_session_runtime.h"

#include <stdint.h>

#include "app/scene_loop_diag.h"
#include "app/scene_loop_policy.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_internal.h"
#include "editor/scene_editor_viewport_render.h"
#include "engine/Render/render_pipeline.h"
#include "scene/object_manager.h"

static void scene_editor_session_runtime_drain_input(SceneEditor* editor) {
    SceneEditorInputRouterCallbacks callbacks = {0};
    callbacks = SceneEditorBuildInputRouterCallbacks(editor);
    SceneEditorInputRouterDrain(&callbacks);
}

static void scene_editor_session_runtime_update_dirty_objects(void) {
    for (int i = 0; i < sceneSettings.objectCount; ++i) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];
        if (IsObjectDirty(obj)) {
            UpdateObject(obj);
        }
    }
}

static void scene_editor_session_runtime_prepare_frame(SceneEditor* editor) {
    RayTracingThemePalette palette = {0};
    setRenderContext(editor->renderer, editor->window,
                     sceneSettings.windowWidth, sceneSettings.windowHeight);
    palette = SceneEditorChromeShellResolvePalette();
    render_set_clear_color(editor->renderer,
                           palette.background_fill.r,
                           palette.background_fill.g,
                           palette.background_fill.b,
                           255);
}

void SceneEditorSessionRuntimeHandleEvent(SceneEditor* editor, SDL_Event* event) {
    SceneEditorInputRouterCallbacks callbacks = {0};
    if (!editor || !event) {
        return;
    }
    callbacks = SceneEditorBuildInputRouterCallbacks(editor);
    SceneEditorInputRouterHandleEvent(event, &callbacks);
}

void SceneEditorSessionRuntimeRender(SceneEditor* editor) {
    if (!editor || !editor->running || !editor->window || !editor->renderer) {
        if (editor) {
            editor->running = false;
        }
        sceneEditorExitFlag = true;
        return;
    }

    scene_editor_session_runtime_drain_input(editor);
    if (!editor->running || sceneEditorExitFlag) {
        return;
    }

    SceneEditorSyncWindowSize(editor);
    scene_editor_session_runtime_update_dirty_objects();
    scene_editor_session_runtime_prepare_frame(editor);
    if (!render_begin_frame()) {
        if (render_device_lost()) {
            editor->running = false;
            sceneEditorExitFlag = true;
        }
        return;
    }

    SceneEditorViewportRenderDraw(editor->renderer,
                                  editor->currentMode,
                                  RenderSceneDigestOverlay);
    RenderSceneButtons(editor->renderer);
    render_end_frame();
}

void SceneEditorSessionRuntimeLoop(SceneEditor* editor) {
    SDL_Event event;
    uint64_t perf_freq = SDL_GetPerformanceFrequency();
    bool frame_dirty = true;
    Uint32 last_render_ms = 0u;

    if (!editor || !editor->window || !editor->renderer) {
        return;
    }
    sceneEditorExitFlag = false;

    while (editor->running && !sceneEditorExitFlag) {
        if (!editor->window || !editor->renderer) {
            editor->running = false;
            sceneEditorExitFlag = true;
            break;
        }
        {
            uint64_t frame_begin_counter = SDL_GetPerformanceCounter();
            uint32_t wait_blocked_ms = 0u;
            uint32_t wait_call_count = 0u;
            bool heartbeat_due = false;

            if (!frame_dirty) {
                SceneLoopWaitPolicyInput wait_input = {
                    .high_intensity_mode = false,
                    .interaction_active = SceneEditorSessionInteractionActive(editor),
                    .background_busy = false,
                    .resize_pending = false,
                };
                int wait_timeout_ms = scene_loop_compute_wait_timeout_ms(&wait_input);
                if (wait_timeout_ms > 0) {
                    uint64_t wait_start = SDL_GetPerformanceCounter();
                    int wait_result = SDL_WaitEventTimeout(&event, wait_timeout_ms);
                    uint64_t wait_end = SDL_GetPerformanceCounter();
                    if (perf_freq > 0u && wait_end >= wait_start) {
                        uint64_t blocked_ms = ((wait_end - wait_start) * 1000u) / perf_freq;
                        if (blocked_ms > (uint64_t)UINT32_MAX) {
                            blocked_ms = (uint64_t)UINT32_MAX;
                        }
                        wait_blocked_ms += (uint32_t)blocked_ms;
                    }
                    wait_call_count += 1u;
                    if (wait_result == 1) {
                        frame_dirty = true;
                        SceneEditorSessionRuntimeHandleEvent(editor, &event);
                    }
                }
            }

            SceneEditorSyncWindowSize(editor);
            while (SDL_PollEvent(&event)) {
                frame_dirty = true;
                SceneEditorSessionRuntimeHandleEvent(editor, &event);
                if (sceneEditorExitFlag) {
                    break;
                }
            }
            scene_editor_session_runtime_drain_input(editor);
            if (sceneEditorExitFlag || !editor->running) {
                if (perf_freq > 0u) {
                    double frame_elapsed_sec =
                        (double)(SDL_GetPerformanceCounter() - frame_begin_counter) / (double)perf_freq;
                    scene_loop_diag_tick(frame_elapsed_sec, wait_blocked_ms, wait_call_count);
                }
                break;
            }

            heartbeat_due = (last_render_ms == 0u) ||
                            ((Uint32)(SDL_GetTicks() - last_render_ms) >= 250u);
            if (!frame_dirty && !heartbeat_due) {
                if (perf_freq > 0u) {
                    double frame_elapsed_sec =
                        (double)(SDL_GetPerformanceCounter() - frame_begin_counter) / (double)perf_freq;
                    scene_loop_diag_tick(frame_elapsed_sec, wait_blocked_ms, wait_call_count);
                }
                continue;
            }

            scene_editor_session_runtime_update_dirty_objects();
            scene_editor_session_runtime_prepare_frame(editor);
            if (!render_begin_frame()) {
                if (render_device_lost()) {
                    editor->running = false;
                    sceneEditorExitFlag = true;
                }
                frame_dirty = true;
                if (perf_freq > 0u) {
                    double frame_elapsed_sec =
                        (double)(SDL_GetPerformanceCounter() - frame_begin_counter) / (double)perf_freq;
                    scene_loop_diag_tick(frame_elapsed_sec, wait_blocked_ms, wait_call_count);
                }
                continue;
            }

            SceneEditorViewportRenderDraw(editor->renderer,
                                          editor->currentMode,
                                          RenderSceneDigestOverlay);
            RenderSceneButtons(editor->renderer);

            render_end_frame();
            frame_dirty = false;
            last_render_ms = SDL_GetTicks();
            if (perf_freq > 0u) {
                double frame_elapsed_sec =
                    (double)(SDL_GetPerformanceCounter() - frame_begin_counter) / (double)perf_freq;
                scene_loop_diag_tick(frame_elapsed_sec, wait_blocked_ms, wait_call_count);
            }
        }
    }

    DestroySceneEditor(editor);
}
