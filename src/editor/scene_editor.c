// scene_editor.c  
#include "editor/scene_editor.h"
#include "editor/bezier_editor.h"
#include "editor/object_editor.h"   //  Required for object editing
#include "editor/material_editor.h"
#include "editor/object_editor_panels.h"
#include "editor/camera_editor.h"   //  Required for camera adjustments
#include "config/config_manager.h"  //  Required for loading/saving scene settings
#include "scene/object_manager.h"
#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "render/fluid/fluid_state.h"
#include "render/ray_tracing_mode_backend.h"
#include "camera/camera.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_chrome_actions.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_input_router.h"
#include "editor/scene_editor_session_runtime.h"
#include "editor/scene_editor_surface_render.h"
#include "editor/scene_editor_viewport_nav.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "render/text_upload_policy.h"
#include "render/vk_shared_device.h"
#include "ui/text_zoom_shortcuts.h"
#include "ui/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static SceneEditorPaneHost g_scenePaneHost;
static SceneEditorPaneLayout g_scenePaneLayout;
static bool g_scenePaneLayoutValid = false;
static bool g_sceneEditorPreviewOnBegin = false;

#define SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX 18.0
#define SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX 16.0
#define SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX 18.0

#if USE_VULKAN
static VkRenderer g_scene_renderer_storage;
#endif

bool sceneEditorExitFlag = false;  //  Used to signal Scene Editor should exit
static void InitializeEditorMode(SceneEditor* editor);
static void SceneEditorLayoutChrome(void);
void SceneEditorSyncWindowSize(SceneEditor* editor);
void SceneEditorRefreshPaneSplitterHover(SceneEditor* editor);
bool SceneEditorHandlePaneSplitterEvent(SceneEditor* editor, SDL_Event* event);
static void SceneEditorResumeAfterPreview(SceneEditor* editor);

#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG (-35.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG (24.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_PITCH_DEG (-80.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_PITCH_DEG (80.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM (0.03)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM (4.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_FRAME_FIT_FACTOR (0.72)

static SceneEditorDigestOverlayNavState g_viewport_nav_state = {
    false,
    0,
    0,
    SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG,
    SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG,
    1.0
};
static int g_digest_hover_object_index = -1;
static SceneEditorBezier3DGizmoState g_bezier3d_gizmo_state = {0};
static SceneEditorCamera3DGizmoState g_camera3d_gizmo_state = {0};

static bool SceneEditorResolveThemeShortcut(SDL_Keycode key,
                                            SDL_Keymod mod,
                                            bool* out_cycle_next) {
    bool ctrl_or_cmd = ((mod & (KMOD_CTRL | KMOD_GUI)) != 0);
    bool shift = ((mod & KMOD_SHIFT) != 0);
    if (out_cycle_next) {
        *out_cycle_next = false;
    }
    if (!ctrl_or_cmd) {
        return false;
    }
    if (key == SDLK_t) {
        if (out_cycle_next) {
            *out_cycle_next = true;
        }
        return true;
    }
    if (key == SDLK_u || key == SDLK_y) {
        if (out_cycle_next) {
            *out_cycle_next = false;
        }
        return true;
    }
    (void)shift;
    return false;
}

static bool SceneEditorEventMatchesEditorWindow(const SceneEditor* editor, const SDL_Event* event) {
    Uint32 editor_window_id = 0;
    Uint32 event_window_id = 0;
    if (!editor || !editor->window || !event) return true;
    editor_window_id = SDL_GetWindowID(editor->window);
    if (editor_window_id == 0) return true;
    switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            event_window_id = event->key.windowID;
            break;
        case SDL_MOUSEMOTION:
            event_window_id = event->motion.windowID;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            event_window_id = event->button.windowID;
            break;
        case SDL_MOUSEWHEEL:
            event_window_id = event->wheel.windowID;
            break;
        case SDL_TEXTINPUT:
            event_window_id = event->text.windowID;
            break;
        case SDL_TEXTEDITING:
            event_window_id = event->edit.windowID;
            break;
        case SDL_WINDOWEVENT:
            event_window_id = event->window.windowID;
            break;
        default:
            return true;
    }
    if (event_window_id == 0) return true;
    return event_window_id == editor_window_id;
}

static bool SceneEditorIsOwnWindowCloseEvent(const SceneEditor* editor, const SDL_Event* event) {
    Uint32 window_id = 0;
    if (!editor || !event || !editor->window) return false;
    if (event->type != SDL_WINDOWEVENT || event->window.event != SDL_WINDOWEVENT_CLOSE) {
        return false;
    }
    window_id = SDL_GetWindowID(editor->window);
    if (window_id == 0) return false;
    return event->window.windowID == window_id;
}

static void SceneEditorBezier3DGizmoReset(void) {
    memset(&g_bezier3d_gizmo_state, 0, sizeof(g_bezier3d_gizmo_state));
    g_bezier3d_gizmo_state.drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
}

static void SceneEditorCamera3DGizmoReset(void) {
    memset(&g_camera3d_gizmo_state, 0, sizeof(g_camera3d_gizmo_state));
    g_camera3d_gizmo_state.drag_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
}

static void SceneEditorResumeAfterPreview(SceneEditor* editor) {
    if (!editor || !editor->window || !editor->renderer) return;
    SDL_CaptureMouse(SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SceneEditorInputRouterReset();
    g_viewport_nav_state.orbit_active = false;
    g_viewport_nav_state.last_mouse_x = 0;
    g_viewport_nav_state.last_mouse_y = 0;
    SceneEditorBezier3DGizmoReset();
    SceneEditorCamera3DGizmoReset();
    SceneEditorSyncWindowSize(editor);
    SceneEditorLayoutChrome();
    InitializeEditorMode(editor);
    (void)SceneEditorViewportNavFitDigestOverlay(&g_viewport_nav_state,
                                                 g_scenePaneLayoutValid ? &g_scenePaneLayout.viewport_rect : NULL,
                                                 false);
    setRenderContext(editor->renderer,
                     editor->window,
                     sceneSettings.windowWidth,
                     sceneSettings.windowHeight);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_MOUSEMOTION, SDL_MOUSEWHEEL);
    SDL_FlushEvents(SDL_TEXTINPUT, SDL_TEXTEDITING);
}

void RenderSceneDigestOverlay(SDL_Renderer* renderer) {
    int active_mode = EditorModeRouter_ClampEditorMode(animSettings.editorMode,
                                                       SceneEditorControlSurfaceLocksObjectMode());
    int selected_object_index = ObjectEditorGetSelectedObjectIndex();
    int mouse_x = 0;
    int mouse_y = 0;
    if (!renderer || !g_scenePaneLayoutValid) {
        g_digest_hover_object_index = -1;
        return;
    }
    SDL_GetMouseState(&mouse_x, &mouse_y);
    if (active_mode == EDITOR_MODE_MATERIAL) {
        selected_object_index = MaterialEditorResolveFocusedObjectIndex();
    }
    g_digest_hover_object_index = SceneEditorDigestOverlayRender(renderer,
                                                                 &g_scenePaneLayout.viewport_rect,
                                                                 &g_viewport_nav_state,
                                                                 active_mode,
                                                                 selected_object_index,
                                                                 mouse_x,
                                                                 mouse_y,
                                                                 &g_bezier3d_gizmo_state,
                                                                 &g_camera3d_gizmo_state);
}

static bool SceneEditorHandleViewportNavigation(SceneEditor* editor,
                                                const SceneEditorPaneCommand* command,
                                                SceneEditorInputRoutingResult* result) {
    SceneEditorControlSurfaceContract contract = {0};
    SceneEditorViewportNavCommand nav_command = {0};
    bool interaction_drag = false;
    if (!editor || !command || !result) return false;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    nav_command.viewport_rect = g_scenePaneLayoutValid ? &g_scenePaneLayout.viewport_rect : NULL;
    nav_command.event = command->event;
    nav_command.viewport_canvas_region = (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_CANVAS);
    nav_command.viewport_drag_region = (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_DRAG);
    nav_command.key_frame_enabled = contract.laneKeyFrameEnabled;
    nav_command.gesture_orbit_enabled = contract.laneGestureOrbitEnabled;
    nav_command.wheel_zoom_enabled = contract.laneWheelZoomEnabled;
    nav_command.active_mode = contract.activeMode;
    nav_command.selected_object_index = (contract.activeMode == EDITOR_MODE_MATERIAL)
                                            ? MaterialEditorResolveFocusedObjectIndex()
                                            : ObjectEditorGetSelectedObjectIndex();
    result->consumed = SceneEditorViewportNavHandleCommand(&nav_command,
                                                           &g_viewport_nav_state,
                                                           &interaction_drag);
    if (!result->consumed) {
        return false;
    }

    result->target = command->target;
    result->pane_hit_region = (uint8_t)command->pane_hit_region;
    result->requested_target_invalidation = true;
    result->invalidation_reason_bits |= (SCENE_EDITOR_INVALIDATE_REASON_PANE |
                                         SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS);
    if (interaction_drag) {
        result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_INTERACTION;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_DRAG;
    } else {
        result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_PANE;
    }
    return true;
}

static SceneEditorInputTarget SceneEditorResolvePaneTarget(const SceneEditor* editor) {
    if (!editor) return SCENE_EDITOR_INPUT_TARGET_NONE;
    switch (editor->currentMode) {
        case EDITOR_MODE_PATH:
            return SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE;
        case EDITOR_MODE_OBJECT:
            return SceneEditorControlSurfaceLocksObjectMode() ? SCENE_EDITOR_INPUT_TARGET_NONE
                                                              : SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE;
        case EDITOR_MODE_CAMERA:
            return SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE;
        case EDITOR_MODE_MATERIAL:
            return SceneEditorControlSurfaceLocksObjectMode() ? SCENE_EDITOR_INPUT_TARGET_NONE
                                                              : SCENE_EDITOR_INPUT_TARGET_MATERIAL_PANE;
        default:
            return SCENE_EDITOR_INPUT_TARGET_NONE;
    }
}

static bool SceneEditorHandleSystemInput(SceneEditor* editor,
                                         SDL_Event* event,
                                         SceneEditorInputRoutingResult* result) {
    SceneEditorControlSurfaceContract contract = {0};
    if (!editor || !event || !result) return false;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    if (event->type == SDL_QUIT || SceneEditorIsOwnWindowCloseEvent(editor, event)) {
        printf("Received SDL_QUIT event. Closing Scene Editor.\n");
        editor->running = false;
        sceneEditorExitFlag = true;
        result->target = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
        result->consumed = true;
        result->invalidation_class = SCENE_EDITOR_INVALIDATION_FULL_EXIT;
        result->requested_full_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_EXIT;
        return true;
    }
    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_TAB &&
        contract.sharedKeyTabCycleEnabled &&
        contract.cycleModeEnabled) {
        editor->currentMode = EditorModeRouter_NextEditorMode(
            editor->currentMode,
            (event->key.keysym.mod & KMOD_SHIFT) != 0,
            SceneEditorControlSurfaceLocksObjectMode());
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
        printf("Changed Mode to %d via TAB\n", editor->currentMode);
        result->target = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
        result->consumed = true;
        result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_UI;
        result->requested_target_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_UI;
        return true;
    }
    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_ESCAPE &&
        contract.sharedKeyEscapeEnabled) {
        editor->running = false;
        sceneEditorExitFlag = true;
        result->target = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
        result->consumed = true;
        result->invalidation_class = SCENE_EDITOR_INVALIDATION_FULL_EXIT;
        result->requested_full_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_EXIT;
        return true;
    }
    if (event->type == SDL_KEYDOWN) {
        bool cycle_next = false;
        bool changed = false;
        int zoom_step = 0;
        int zoom_percent = 100;
        if (SceneEditorResolveThemeShortcut(event->key.keysym.sym,
                                            event->key.keysym.mod,
                                            &cycle_next)) {
            if (cycle_next) {
                ray_tracing_shared_theme_cycle_next();
            } else {
                ray_tracing_shared_theme_cycle_prev();
            }
            ray_tracing_shared_theme_save_persisted();
            ray_tracing_font_runtime_invalidate_all();
            result->target = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
            result->consumed = true;
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_UI;
            result->requested_target_invalidation = true;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_UI;
            return true;
        }
        if (ray_tracing_text_zoom_apply_shortcut(event->key.keysym.sym,
                                                 event->key.keysym.mod,
                                                 &changed,
                                                 &zoom_step,
                                                 &zoom_percent)) {
            printf("[font] text zoom %d%% step=%d%s\n",
                   zoom_percent,
                   zoom_step,
                   changed ? "" : " (clamped)");
            result->target = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
            result->consumed = true;
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_UI;
            result->requested_target_invalidation = true;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_UI;
            return true;
        }
    }
    return false;
}

static void SceneEditorApplyInputInvalidation(SceneEditor* editor,
                                              const SceneEditorInputRoutingResult* result) {
    (void)editor;
    if (!result) return;
    // IR1-S3 policy seam: invalidation classes are explicit even while render invalidation remains behavior-preserving.
}

static void SceneEditorChromeActionsInitializeMode(SceneEditor* editor) {
    InitializeEditorMode(editor);
}

static void SceneEditorChromeActionsResumePreview(SceneEditor* editor) {
    SceneEditorResumeAfterPreview(editor);
}

static SceneEditorChromeActionsEnvironment SceneEditorBuildChromeActionsEnvironment(void) {
    SceneEditorChromeActionsEnvironment env = {0};
    env.pane_layout = &g_scenePaneLayout;
    env.pane_layout_valid = g_scenePaneLayoutValid;
    env.viewport_nav_state = &g_viewport_nav_state;
    env.digest_hover_object_index = &g_digest_hover_object_index;
    env.bezier_gizmo_state = &g_bezier3d_gizmo_state;
    env.camera_gizmo_state = &g_camera3d_gizmo_state;
    env.handle_viewport_navigation = SceneEditorHandleViewportNavigation;
    env.initialize_editor_mode = SceneEditorChromeActionsInitializeMode;
    env.resume_after_preview = SceneEditorChromeActionsResumePreview;
    return env;
}

static bool SceneEditorInputRouterEventMatchesEditorWindow(void* context, const SDL_Event* event) {
    return SceneEditorEventMatchesEditorWindow((const SceneEditor*)context, event);
}

static bool SceneEditorInputRouterShouldRouteGlobalKey(void* context, const SDL_Event* event) {
    SceneEditor* editor = (SceneEditor*)context;
    SceneEditorControlSurfaceContract contract = {0};
    if (!editor || !event || event->type != SDL_KEYDOWN) {
        return false;
    }
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    if (event->key.keysym.sym == SDLK_TAB) {
        return contract.sharedKeyTabCycleEnabled && contract.cycleModeEnabled;
    }
    if (event->key.keysym.sym == SDLK_ESCAPE) {
        return contract.sharedKeyEscapeEnabled;
    }
    return false;
}

static bool SceneEditorInputRouterIsChromeHit(void* context, int mx, int my) {
    (void)context;
    return SceneEditorChromeShellIsButtonHit(mx, my);
}

static bool SceneEditorInputRouterHandleSystem(void* context,
                                               SDL_Event* event,
                                               SceneEditorInputRoutingResult* result) {
    return SceneEditorHandleSystemInput((SceneEditor*)context, event, result);
}

static SceneEditorInputTarget SceneEditorInputRouterResolvePane(void* context) {
    return SceneEditorResolvePaneTarget((const SceneEditor*)context);
}

static bool SceneEditorInputRouterResolveChrome(void* context,
                                                const SDL_Event* event,
                                                SceneEditorChromeAction* out_action) {
    (void)context;
    return SceneEditorChromeActionsResolve(event, out_action);
}

static void SceneEditorInputRouterApplyChrome(void* context, const SceneEditorChromeAction* action) {
    SceneEditorChromeActionsEnvironment env = SceneEditorBuildChromeActionsEnvironment();
    SceneEditorChromeActionsApply((SceneEditor*)context, action, &env);
}

static void SceneEditorInputRouterRoutePane(void* context,
                                            const SceneEditorPaneCommand* command,
                                            SceneEditorInputRoutingResult* result) {
    SceneEditorChromeActionsEnvironment env = SceneEditorBuildChromeActionsEnvironment();
    SceneEditorChromeActionsRoutePaneEvent((SceneEditor*)context, &env, command, result);
}

static void SceneEditorInputRouterApplyInvalidation(void* context,
                                                    const SceneEditorInputRoutingResult* result) {
    SceneEditorApplyInputInvalidation((SceneEditor*)context, result);
}

static bool SceneEditorInputRouterShouldStopProcessing(void* context) {
    const SceneEditor* editor = (const SceneEditor*)context;
    return !editor || !editor->running || sceneEditorExitFlag;
}

SceneEditorInputRouterCallbacks SceneEditorBuildInputRouterCallbacks(SceneEditor* editor) {
    SceneEditorInputRouterCallbacks callbacks = {0};
    callbacks.context = editor;
    callbacks.event_matches_editor_window = SceneEditorInputRouterEventMatchesEditorWindow;
    callbacks.should_route_global_key = SceneEditorInputRouterShouldRouteGlobalKey;
    callbacks.is_chrome_hit = SceneEditorInputRouterIsChromeHit;
    callbacks.handle_system_input = SceneEditorInputRouterHandleSystem;
    callbacks.resolve_pane_target = SceneEditorInputRouterResolvePane;
    callbacks.resolve_chrome_action = SceneEditorInputRouterResolveChrome;
    callbacks.apply_chrome_action = SceneEditorInputRouterApplyChrome;
    callbacks.route_pane_event = SceneEditorInputRouterRoutePane;
    callbacks.apply_invalidation = SceneEditorInputRouterApplyInvalidation;
    callbacks.should_stop_processing = SceneEditorInputRouterShouldStopProcessing;
    return callbacks;
}

static void SceneEditorLayoutChrome(void) {
    const SceneEditorPaneLayout* layout = NULL;
    bool pane_ok = false;

    if (!g_scenePaneHost.initialized) {
        pane_ok = scene_editor_pane_host_init(&g_scenePaneHost,
                                              sceneSettings.windowWidth,
                                              sceneSettings.windowHeight);
    } else {
        pane_ok = scene_editor_pane_host_rebuild(&g_scenePaneHost,
                                                 sceneSettings.windowWidth,
                                                 sceneSettings.windowHeight);
    }
    if (!pane_ok) {
        memset(&g_scenePaneLayout, 0, sizeof(g_scenePaneLayout));
        g_scenePaneLayoutValid = false;
        SceneEditorChromeShellLayoutFallback(sceneSettings.windowWidth, sceneSettings.windowHeight);
        return;
    }

    layout = scene_editor_pane_host_layout(&g_scenePaneHost);
    if (!layout) {
        memset(&g_scenePaneLayout, 0, sizeof(g_scenePaneLayout));
        g_scenePaneLayoutValid = false;
        SceneEditorChromeShellLayoutFallback(sceneSettings.windowWidth, sceneSettings.windowHeight);
        return;
    }
    g_scenePaneLayout = *layout;
    g_scenePaneLayoutValid = true;

    SceneEditorChromeShellLayoutFromPane(&g_scenePaneLayout);
}

void SceneEditorSyncWindowSize(SceneEditor* editor) {
    int width = 0;
    int height = 0;
    if (!editor || !editor->window) return;
    SDL_GetWindowSize(editor->window, &width, &height);
    if (width <= 0 || height <= 0) return;
    if (width != sceneSettings.windowWidth || height != sceneSettings.windowHeight) {
        sceneSettings.windowWidth = width;
        sceneSettings.windowHeight = height;
#if USE_VULKAN
        if (editor->renderer) {
            vk_renderer_set_logical_size((VkRenderer*)editor->renderer,
                                         (float)sceneSettings.windowWidth,
                                         (float)sceneSettings.windowHeight);
        }
#endif
    }
    SceneEditorLayoutChrome();
    SceneEditorRefreshPaneSplitterHover(editor);
}

void SceneEditorRefreshPaneSplitterHover(SceneEditor* editor) {
    int mouse_x = 0;
    int mouse_y = 0;

    if (!editor || !editor->window || !g_scenePaneHost.initialized) return;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    scene_editor_pane_host_update_pointer(&g_scenePaneHost, (float)mouse_x, (float)mouse_y);
}

bool SceneEditorHandlePaneSplitterEvent(SceneEditor* editor, SDL_Event* event) {
    if (!editor || !event || !g_scenePaneHost.initialized) {
        return false;
    }
    if (!SceneEditorEventMatchesEditorWindow(editor, event)) {
        return false;
    }

    switch (event->type) {
        case SDL_MOUSEMOTION:
            if (scene_editor_pane_host_splitter_drag_active(&g_scenePaneHost)) {
                if (scene_editor_pane_host_update_splitter_drag(&g_scenePaneHost,
                                                                (float)event->motion.x,
                                                                (float)event->motion.y)) {
                    SceneEditorLayoutChrome();
                }
                return true;
            }
            scene_editor_pane_host_update_pointer(&g_scenePaneHost,
                                                  (float)event->motion.x,
                                                  (float)event->motion.y);
            return false;
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button != SDL_BUTTON_LEFT) {
                return false;
            }
            scene_editor_pane_host_update_pointer(&g_scenePaneHost,
                                                  (float)event->button.x,
                                                  (float)event->button.y);
            return scene_editor_pane_host_begin_splitter_drag(&g_scenePaneHost,
                                                              (float)event->button.x,
                                                              (float)event->button.y);
        case SDL_MOUSEBUTTONUP:
            if (scene_editor_pane_host_splitter_drag_active(&g_scenePaneHost)) {
                scene_editor_pane_host_end_splitter_drag(&g_scenePaneHost);
                scene_editor_pane_host_update_pointer(&g_scenePaneHost,
                                                      (float)event->button.x,
                                                      (float)event->button.y);
                return true;
            }
            scene_editor_pane_host_update_pointer(&g_scenePaneHost,
                                                  (float)event->button.x,
                                                  (float)event->button.y);
            return false;
        default:
            return false;
    }
}

static bool SceneEditorLoadSessionState(SceneEditor* editor) {
    AnimationConfig live_config;
    bool has_live_runtime_selection = false;
    if (!editor) {
        return false;
    }
    live_config = animSettings;
    has_live_runtime_selection =
        animation_config_scene_source_clamp(live_config.sceneSource) == SCENE_SOURCE_RUNTIME_SCENE &&
        live_config.runtimeScenePath[0] != '\0';
    LoadAnimationConfig();
    LoadSceneConfig();
    if (has_live_runtime_selection) {
        animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
        animSettings.useFluidScene = false;
        animSettings.fluidManifest[0] = '\0';
        animSettings.spaceMode = live_config.spaceMode;
        snprintf(animSettings.runtimeScenePath,
                 sizeof(animSettings.runtimeScenePath),
                 "%s",
                 live_config.runtimeScenePath);
    }
    ApplyAnimationWindowSizeOverride();
    if (animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE &&
        animSettings.runtimeScenePath[0] != '\0') {
        RuntimeSceneBridgePreflight summary = {0};
        char runtime_scene_path[sizeof(animSettings.runtimeScenePath)];
        snprintf(runtime_scene_path, sizeof(runtime_scene_path), "%s", animSettings.runtimeScenePath);
        if (!runtime_scene_bridge_apply_file_defer_mesh_assets(runtime_scene_path, &summary)) {
            fprintf(stderr,
                    "[editor] failed to apply runtime scene source '%s': %s\n",
                    runtime_scene_path,
                    summary.diagnostics);
        }
    } else if (!AnimationRestoreActiveSceneSource(true)) {
        fprintf(stderr, "[editor] failed to apply active scene source; selection preserved.\n");
    }
    ApplyAnimationWindowSizeOverride();
    if (animSettings.editorMode < 0) {
        animSettings.editorMode = 0;
    }
    editor->currentMode = EditorModeRouter_ClampEditorMode(animSettings.editorMode,
                                                           SceneEditorControlSurfaceLocksObjectMode());
    return true;
}

bool SceneEditorSessionBegin(SceneEditor* editor, SDL_Renderer* renderer, SDL_Window* window) {
    int width = 0;
    int height = 0;
    if (!editor || !renderer || !window) {
        return false;
    }
    memset(editor, 0, sizeof(*editor));
    SceneEditorInputRouterReset();
    if (!SceneEditorLoadSessionState(editor)) {
        return false;
    }

    editor->window = window;
    editor->renderer = renderer;
    editor->owns_window = false;
    editor->owns_renderer = false;
    editor->running = true;
    g_viewport_nav_state.orbit_active = false;
    g_viewport_nav_state.last_mouse_x = 0;
    g_viewport_nav_state.last_mouse_y = 0;
    SceneEditorViewportNavResetDigestOverlayNavigation(&g_viewport_nav_state);
    SceneEditorBezier3DGizmoReset();
    SceneEditorCamera3DGizmoReset();

    if (!ray_tracing_font_runtime_init()) {
        fprintf(stderr, "Error: TTF_Init failed: %s\n", TTF_GetError());
        memset(editor, 0, sizeof(*editor));
        return false;
    }

    SDL_GetWindowSize(window, &width, &height);
    if (width > 0 && height > 0) {
        sceneSettings.windowWidth = width;
        sceneSettings.windowHeight = height;
    }
#if USE_VULKAN
    vk_renderer_set_logical_size((VkRenderer*)editor->renderer,
                                 (float)sceneSettings.windowWidth,
                                 (float)sceneSettings.windowHeight);
#endif
    setRenderContext(editor->renderer, editor->window,
                     sceneSettings.windowWidth, sceneSettings.windowHeight);
    ray_tracing_font_runtime_attach_renderer(editor->renderer);
    SceneEditorLayoutChrome();
    SceneEditorRefreshPaneSplitterHover(editor);
    (void)SceneEditorViewportNavFitDigestOverlay(&g_viewport_nav_state,
                                                 g_scenePaneLayoutValid ? &g_scenePaneLayout.viewport_rect : NULL,
                                                 true);
    InitializeEditorMode(editor);
    UpdateObjects();
    sceneEditorExitFlag = false;
    if (g_sceneEditorPreviewOnBegin) {
        g_sceneEditorPreviewOnBegin = false;
        RunPreviewModeEmbedded(editor->window, editor->renderer);
        SceneEditorResumeAfterPreview(editor);
    }
    printf("Scene Editor Session Begin. Host Size: %dx%d\n",
           sceneSettings.windowWidth,
           sceneSettings.windowHeight);
    return true;
}

bool InitializeSceneEditor(SceneEditor* editor) {
    if (!editor) {
        return false;
    }
    memset(editor, 0, sizeof(*editor));
    SceneEditorInputRouterReset();
    editor->running = false;
    editor->window = NULL;
    editor->renderer = NULL;
    editor->owns_window = false;
    editor->owns_renderer = false;

    if (!SceneEditorLoadSessionState(editor)) {
        return false;
    }

    //  Create the window using stored scene settings
    editor->window = SDL_CreateWindow("Scene Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      sceneSettings.windowWidth, sceneSettings.windowHeight,
                                      SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                          SDL_WINDOW_ALLOW_HIGHDPI);
    if (!editor->window) {
        fprintf(stderr, "Error: Failed to create scene window.\n");
        return false;
    }

#if USE_VULKAN
    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 0.0f;
    cfg.clear_color[1] = 0.0f;
    cfg.clear_color[2] = 0.0f;
    cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(editor->window, &cfg)) {
        fprintf(stderr, "vk_shared_device_init failed.\n");
        SDL_DestroyWindow(editor->window);
        editor->window = NULL;
        return false;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        fprintf(stderr, "vk_shared_device_get failed.\n");
        SDL_DestroyWindow(editor->window);
        editor->window = NULL;
        return false;
    }

    VkResult init = vk_renderer_init_with_device(&g_scene_renderer_storage, shared_device, editor->window, &cfg);
    if (init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed: %d\n", init);
        SDL_DestroyWindow(editor->window);
        editor->window = NULL;
        return false;
    }
    editor->renderer = (SDL_Renderer*)&g_scene_renderer_storage;
    vk_renderer_set_logical_size((VkRenderer*)editor->renderer,
                                 (float)sceneSettings.windowWidth,
                                 (float)sceneSettings.windowHeight);
#else
    //  Create the renderer
    editor->renderer = SDL_CreateRenderer(editor->window, -1, SDL_RENDERER_ACCELERATED | 
			SDL_RENDERER_PRESENTVSYNC);
    if (!editor->renderer) {
        fprintf(stderr, "Error: Failed to create scene renderer.\n");
        SDL_DestroyWindow(editor->window);
        editor->window = NULL;
        return false;
    }
#endif

    editor->owns_window = true;
    editor->owns_renderer = true;

    //  Initialize TTF for font rendering
    if (!ray_tracing_font_runtime_init()) {
        fprintf(stderr, "Error: TTF_Init failed: %s\n", TTF_GetError());
#if USE_VULKAN
        if (editor->owns_renderer && editor->renderer) {
            ray_tracing_text_reset_renderer(editor->renderer);
            vk_renderer_wait_idle((VkRenderer*)editor->renderer);
            vk_renderer_shutdown_surface((VkRenderer*)editor->renderer);
        }
#else
        if (editor->owns_renderer && editor->renderer) {
            ray_tracing_text_reset_renderer(editor->renderer);
            SDL_DestroyRenderer(editor->renderer);
        }
#endif
        editor->renderer = NULL;
        if (editor->owns_window && editor->window) {
            SDL_DestroyWindow(editor->window);
        }
        editor->window = NULL;
        editor->owns_window = false;
        editor->owns_renderer = false;
        return false;
    }
    setRenderContext(editor->renderer, editor->window,
                     sceneSettings.windowWidth, sceneSettings.windowHeight);
    ray_tracing_font_runtime_attach_renderer(editor->renderer);
    SceneEditorLayoutChrome();
    SceneEditorRefreshPaneSplitterHover(editor);

    InitializeEditorMode(editor);


    UpdateObjects();
    editor->running = true;
    printf("Scene Editor Initialized. Window Size: %dx%d\n", sceneSettings.windowWidth, 
			sceneSettings.windowHeight);
    return true;
}

void RenderSceneButtons(SDL_Renderer* renderer) {
    SceneEditorControlSurfaceContract contract = {0};
    CorePaneRect splitter_rect = {0};
    bool splitter_visible = false;
    bool splitter_hovered = false;
    bool splitter_active = false;

    if (!renderer) return;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    splitter_visible = scene_editor_pane_host_visible_splitter(&g_scenePaneHost,
                                                               &splitter_rect,
                                                               &splitter_hovered,
                                                               &splitter_active);
    SceneEditorChromeShellRender(renderer,
                                 &g_scenePaneLayout,
                                 g_scenePaneLayoutValid,
                                 &contract,
                                 splitter_visible ? &splitter_rect : NULL,
                                 splitter_hovered,
                                 splitter_active);
}

void SceneEditorLoop(SceneEditor* editor) {
    SceneEditorSessionRuntimeLoop(editor);
}

void SceneEditorSessionHandleEvent(SceneEditor* editor, SDL_Event* event) {
    if (!editor || !event || !editor->running) {
        return;
    }
    SceneEditorSessionRuntimeHandleEvent(editor, event);
}

void SceneEditorSessionRender(SceneEditor* editor) {
    SceneEditorSessionRuntimeRender(editor);
}

void SceneEditorSessionRenderWithPostDraw(SceneEditor* editor,
                                          SceneEditorSessionPostDrawFn post_draw,
                                          void* context) {
    SceneEditorSessionRuntimeRenderWithPostDraw(editor, post_draw, context);
}

bool SceneEditorSessionWantsExit(const SceneEditor* editor) {
    if (!editor) {
        return true;
    }
    return sceneEditorExitFlag || !editor->running;
}

bool SceneEditorSessionInteractionActive(const SceneEditor* editor) {
    (void)editor;
    return g_viewport_nav_state.orbit_active ||
           scene_editor_pane_host_splitter_drag_active(&g_scenePaneHost) ||
           g_bezier3d_gizmo_state.dragging ||
           g_camera3d_gizmo_state.dragging;
}

void SceneEditorSessionRequestPreviewOnBegin(void) {
    g_sceneEditorPreviewOnBegin = true;
}

void SceneEditorSessionEnd(SceneEditor* editor) {
    if (!editor) {
        return;
    }
    editor->running = false;
    editor->window = NULL;
    editor->renderer = NULL;
    editor->owns_window = false;
    editor->owns_renderer = false;
    g_viewport_nav_state.orbit_active = false;
    g_viewport_nav_state.last_mouse_x = 0;
    g_viewport_nav_state.last_mouse_y = 0;
    SceneEditorViewportNavResetDigestOverlayNavigation(&g_viewport_nav_state);
    SceneEditorBezier3DGizmoReset();
    SceneEditorCamera3DGizmoReset();
    scene_editor_pane_host_end_splitter_drag(&g_scenePaneHost);
    SceneEditorInputRouterReset();
    sceneEditorExitFlag = false;
    setRenderContext(NULL, NULL, 0, 0);
}


void HandleSceneEditorEvents(SceneEditor* editor, SDL_Event* event) {
    SceneEditorSessionRuntimeHandleEvent(editor, event);
}

bool IsClickingButtonMain(int mx, int my) {
    return SceneEditorChromeShellIsButtonHit(mx, my);
}

bool SceneEditorIsPaneToolButton(int mx, int my) {
    if ((mx >= selectButton.x && mx <= selectButton.x + selectButton.w &&
         my >= selectButton.y && my <= selectButton.y + selectButton.h) ||
        (mx >= addButton.x && mx <= addButton.x + addButton.w &&
         my >= addButton.y && my <= addButton.y + addButton.h) ||
        (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w &&
         my >= deleteButton.y && my <= deleteButton.y + deleteButton.h)) {
        return true;
    }
    return false;
}

bool SceneEditorGetPaneLayout(SceneEditorPaneLayout* out_layout) {
    if (!out_layout || !g_scenePaneLayoutValid) return false;
    *out_layout = g_scenePaneLayout;
    return true;
}

void ToggleSceneMode(SceneEditor* editor) {
    editor->currentMode = EditorModeRouter_NextEditorMode(editor->currentMode,
                                                          false,
                                                          SceneEditorControlSurfaceLocksObjectMode());
    animSettings.editorMode = editor->currentMode;
    InitializeEditorMode(editor);
    printf("Switched to mode: %d\n", editor->currentMode);
}

// Set Scene Mode
void SetSceneMode(SceneEditor* editor, int mode) {
    if (mode >= 0 && mode < EDITOR_MODE_COUNT) {
        editor->currentMode = EditorModeRouter_ClampEditorMode(mode,
                                                               SceneEditorControlSurfaceLocksObjectMode());
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
    }
}

void ResetSceneEditor(SceneEditor* editor) {
    LoadSceneConfig();  // Reload all scene settings
    ApplyAnimationWindowSizeOverride();
    editor->currentMode = 0;  // Default to Bezier Editor Mode
    animSettings.editorMode = 0;
    InitializeEditorMode(editor);
    printf("Scene Editor reset to default settings.\n");
}


void DestroySceneEditor(SceneEditor* editor) {
    if (!editor) {
        return;
    }
    editor->running = false;
    ray_tracing_font_runtime_detach_renderer(editor->renderer);
    if (editor->renderer && editor->owns_renderer) {
#if USE_VULKAN
        ray_tracing_text_reset_renderer(editor->renderer);
        vk_renderer_wait_idle((VkRenderer*)editor->renderer);
        vk_renderer_shutdown_surface((VkRenderer*)editor->renderer);
#else
        ray_tracing_text_reset_renderer(editor->renderer);
        SDL_DestroyRenderer(editor->renderer);
#endif
    }
    editor->renderer = NULL;
    if (editor->window && editor->owns_window) {
        SDL_DestroyWindow(editor->window);
    }
    editor->window = NULL;
    editor->owns_window = false;
    editor->owns_renderer = false;
    scene_editor_pane_host_end_splitter_drag(&g_scenePaneHost);
    SceneEditorInputRouterReset();
    ray_tracing_font_runtime_shutdown();
    setRenderContext(NULL, NULL, 0, 0);
    printf("Scene Editor Closed. Returning to main menu...\n");
}

static void InitializeEditorMode(SceneEditor* editor) {
    switch (editor->currentMode) {
        case EDITOR_MODE_PATH:
            InitializeBezierEditor();
            break;
        case EDITOR_MODE_OBJECT:
            InitializeObjectEditor();
            break;
        case EDITOR_MODE_CAMERA:
            InitializeCameraEditor();
            break;
        case EDITOR_MODE_MATERIAL:
            InitializeMaterialEditor();
            break;
        default:
            break;
    }
}
