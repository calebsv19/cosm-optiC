// scene_editor.c  
#include "editor/scene_editor.h"
#include "editor/bezier_editor.h"
#include "editor/object_editor.h"   //  Required for object editing
#include "editor/object_editor_panels.h"
#include "editor/camera_editor.h"   //  Required for camera adjustments
#include "config/config_manager.h"  //  Required for loading/saving scene settings
#include "scene/object_manager.h"
#include "app/animation.h"
#include "app/scene_loop_diag.h"
#include "app/scene_loop_policy.h"
#include "import/runtime_scene_bridge.h"
#include "render/fluid/fluid_state.h"
#include "render/ray_tracing_mode_backend.h"
#include "camera/camera.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "editor/scene_editor_surface_render.h"
#include "editor/scene_editor_viewport_render.h"
#include "engine/Render/render_pipeline.h"
#include "render/text_font_cache.h"
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
#include <stdint.h>

static SceneEditorPaneHost g_scenePaneHost;
static SceneEditorPaneLayout g_scenePaneLayout;
static bool g_scenePaneLayoutValid = false;
static bool g_sceneEditorPreviewOnBegin = false;

#define SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX 18.0
#define SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX 16.0
#define SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX 18.0
#define SCENE_EDITOR_IDLE_HEARTBEAT_MS 250u

#if USE_VULKAN
static VkRenderer g_scene_renderer_storage;
#endif

bool sceneEditorExitFlag = false;  //  Used to signal Scene Editor should exit
static void InitializeEditorMode(SceneEditor* editor);
static void SceneEditorLayoutChrome(void);
static void SceneEditorSyncWindowSize(SceneEditor* editor);
static void SceneEditorResumeAfterPreview(SceneEditor* editor);
static bool SceneEditorViewportFitDigestOverlay(bool reset_angles);

typedef enum SceneEditorInputTarget {
    SCENE_EDITOR_INPUT_TARGET_NONE = 0,
    SCENE_EDITOR_INPUT_TARGET_SYSTEM,
    SCENE_EDITOR_INPUT_TARGET_CHROME,
    SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE,
    SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE,
    SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE
} SceneEditorInputTarget;

typedef enum SceneEditorInputActionClass {
    SCENE_EDITOR_INPUT_ACTION_IGNORED = 0,
    SCENE_EDITOR_INPUT_ACTION_IMMEDIATE,
    SCENE_EDITOR_INPUT_ACTION_QUEUED
} SceneEditorInputActionClass;

typedef enum SceneEditorInputRoutePolicy {
    SCENE_EDITOR_INPUT_ROUTE_POLICY_NONE = 0,
    SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL,
    SCENE_EDITOR_INPUT_ROUTE_POLICY_CHROME,
    SCENE_EDITOR_INPUT_ROUTE_POLICY_ACTIVE_PANE
} SceneEditorInputRoutePolicy;

typedef struct SceneEditorInputRoutingResult {
    SceneEditorInputTarget target;
    bool consumed;
    bool requested_target_invalidation;
    bool requested_full_invalidation;
    uint8_t pane_hit_region;
    uint8_t invalidation_class;
    uint32_t invalidation_reason_bits;
} SceneEditorInputRoutingResult;

typedef struct SceneEditorInputNormalized {
    SceneEditorInputActionClass action_class;
    SceneEditorInputRoutePolicy route_policy;
    SceneEditorInputTarget target_hint;
    const SDL_Event* event;
} SceneEditorInputNormalized;

typedef struct SceneEditorInputDiagFrame {
    uint32_t raw_event_count;
    uint32_t normalized_count;
    uint32_t ignored_count;
    uint32_t immediate_count;
    uint32_t queued_count;
    uint32_t routed_global_count;
    uint32_t routed_chrome_count;
    uint32_t routed_pane_count;
    uint32_t pane_controls_count;
    uint32_t pane_canvas_count;
    uint32_t pane_drag_count;
    uint32_t target_invalidation_count;
    uint32_t full_invalidation_count;
} SceneEditorInputDiagFrame;

enum {
    SCENE_EDITOR_INVALIDATE_REASON_UI = 1u << 0,
    SCENE_EDITOR_INVALIDATE_REASON_PANE = 1u << 1,
    SCENE_EDITOR_INVALIDATE_REASON_EXIT = 1u << 2,
    SCENE_EDITOR_INVALIDATE_REASON_PANE_CONTROLS = 1u << 3,
    SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS = 1u << 4,
    SCENE_EDITOR_INVALIDATE_REASON_PANE_DRAG = 1u << 5
};

typedef enum SceneEditorPaneHitRegion {
    SCENE_EDITOR_PANE_HIT_NONE = 0,
    SCENE_EDITOR_PANE_HIT_CONTROLS,
    SCENE_EDITOR_PANE_HIT_LIST_PANEL,
    SCENE_EDITOR_PANE_HIT_CANVAS,
    SCENE_EDITOR_PANE_HIT_DRAG
} SceneEditorPaneHitRegion;

typedef enum SceneEditorInvalidationClass {
    SCENE_EDITOR_INVALIDATION_NONE = 0,
    SCENE_EDITOR_INVALIDATION_TARGET_UI,
    SCENE_EDITOR_INVALIDATION_TARGET_PANE,
    SCENE_EDITOR_INVALIDATION_TARGET_INTERACTION,
    SCENE_EDITOR_INVALIDATION_FULL_EXIT
} SceneEditorInvalidationClass;

typedef enum SceneEditorPaneCommandKind {
    SCENE_EDITOR_PANE_COMMAND_NONE = 0,
    SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN,
    SCENE_EDITOR_PANE_COMMAND_POINTER_UP,
    SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG,
    SCENE_EDITOR_PANE_COMMAND_WHEEL,
    SCENE_EDITOR_PANE_COMMAND_KEY
} SceneEditorPaneCommandKind;

typedef enum SceneEditorChromeActionKind {
    SCENE_EDITOR_CHROME_ACTION_NONE = 0,
    SCENE_EDITOR_CHROME_ACTION_MODE_SELECT,
    SCENE_EDITOR_CHROME_ACTION_PREVIEW,
    SCENE_EDITOR_CHROME_ACTION_CYCLE_MODE,
    SCENE_EDITOR_CHROME_ACTION_APPLY,
    SCENE_EDITOR_CHROME_ACTION_SAVE,
    SCENE_EDITOR_CHROME_ACTION_BACK_TO_MENU
} SceneEditorChromeActionKind;

typedef struct SceneEditorPaneCommand {
    SceneEditorPaneCommandKind kind;
    SceneEditorInputTarget target;
    SceneEditorPaneHitRegion pane_hit_region;
    SDL_Event* event;
} SceneEditorPaneCommand;

typedef struct SceneEditorChromeAction {
    SceneEditorChromeActionKind kind;
    int mode_index;
} SceneEditorChromeAction;

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

typedef enum SceneEditorMutationLane {
    SCENE_EDITOR_MUTATION_LANE_NONE = 0,
    SCENE_EDITOR_MUTATION_LANE_CHROME,
    SCENE_EDITOR_MUTATION_LANE_PANE
} SceneEditorMutationLane;

typedef struct SceneEditorQueuedPaneCommand {
    SceneEditorPaneCommandKind kind;
    SceneEditorInputTarget target;
    SceneEditorPaneHitRegion pane_hit_region;
    SDL_Event event_copy;
} SceneEditorQueuedPaneCommand;

typedef struct SceneEditorQueuedMutationRef {
    SceneEditorMutationLane lane;
    uint16_t lane_index;
} SceneEditorQueuedMutationRef;

enum {
    SCENE_EDITOR_MAX_QUEUED_CHROME_MUTATIONS = 32,
    SCENE_EDITOR_MAX_QUEUED_PANE_MUTATIONS = 160,
    SCENE_EDITOR_MAX_QUEUED_MUTATION_ORDER = 192
};

typedef struct SceneEditorMutationQueue {
    SceneEditorChromeAction chrome_actions[SCENE_EDITOR_MAX_QUEUED_CHROME_MUTATIONS];
    uint16_t chrome_count;
    SceneEditorQueuedPaneCommand pane_commands[SCENE_EDITOR_MAX_QUEUED_PANE_MUTATIONS];
    uint16_t pane_count;
    SceneEditorQueuedMutationRef ordered_refs[SCENE_EDITOR_MAX_QUEUED_MUTATION_ORDER];
    uint16_t order_count;
    uint32_t dropped_chrome_count;
    uint32_t dropped_pane_count;
    uint32_t dropped_order_count;
} SceneEditorMutationQueue;

static SceneEditorMutationQueue g_scene_mutation_queue;

static void SceneEditorInputRoutingResult_Reset(SceneEditorInputRoutingResult* result) {
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->target = SCENE_EDITOR_INPUT_TARGET_NONE;
}

static void SceneEditorInputNormalized_Reset(SceneEditorInputNormalized* normalized) {
    if (!normalized) return;
    memset(normalized, 0, sizeof(*normalized));
    normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IGNORED;
    normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_NONE;
    normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_NONE;
    normalized->event = NULL;
}

static void SceneEditorMutationQueue_Reset(void) {
    memset(&g_scene_mutation_queue, 0, sizeof(g_scene_mutation_queue));
}

static bool SceneEditorMutationQueue_AppendOrderedRef(SceneEditorMutationLane lane, uint16_t lane_index) {
    if (g_scene_mutation_queue.order_count >= SCENE_EDITOR_MAX_QUEUED_MUTATION_ORDER) {
        g_scene_mutation_queue.dropped_order_count += 1u;
        return false;
    }
    g_scene_mutation_queue.ordered_refs[g_scene_mutation_queue.order_count].lane = lane;
    g_scene_mutation_queue.ordered_refs[g_scene_mutation_queue.order_count].lane_index = lane_index;
    g_scene_mutation_queue.order_count += 1u;
    return true;
}

static bool SceneEditorMutationQueue_EnqueueChromeAction(const SceneEditorChromeAction* action) {
    uint16_t lane_index = 0;
    if (!action || action->kind == SCENE_EDITOR_CHROME_ACTION_NONE) return false;
    if (g_scene_mutation_queue.chrome_count >= SCENE_EDITOR_MAX_QUEUED_CHROME_MUTATIONS) {
        g_scene_mutation_queue.dropped_chrome_count += 1u;
        return false;
    }
    lane_index = g_scene_mutation_queue.chrome_count;
    g_scene_mutation_queue.chrome_actions[lane_index] = *action;
    g_scene_mutation_queue.chrome_count += 1u;
    if (!SceneEditorMutationQueue_AppendOrderedRef(SCENE_EDITOR_MUTATION_LANE_CHROME, lane_index)) {
        g_scene_mutation_queue.chrome_count -= 1u;
        g_scene_mutation_queue.dropped_chrome_count += 1u;
        return false;
    }
    return true;
}

static bool SceneEditorMutationQueue_EnqueuePaneCommand(const SceneEditorPaneCommand* command) {
    SceneEditorQueuedPaneCommand* queued = NULL;
    uint16_t lane_index = 0;
    if (!command || !command->event || command->kind == SCENE_EDITOR_PANE_COMMAND_NONE) return false;
    if (g_scene_mutation_queue.pane_count >= SCENE_EDITOR_MAX_QUEUED_PANE_MUTATIONS) {
        g_scene_mutation_queue.dropped_pane_count += 1u;
        return false;
    }
    lane_index = g_scene_mutation_queue.pane_count;
    queued = &g_scene_mutation_queue.pane_commands[lane_index];
    queued->kind = command->kind;
    queued->target = command->target;
    queued->pane_hit_region = command->pane_hit_region;
    queued->event_copy = *command->event;
    g_scene_mutation_queue.pane_count += 1u;
    if (!SceneEditorMutationQueue_AppendOrderedRef(SCENE_EDITOR_MUTATION_LANE_PANE, lane_index)) {
        g_scene_mutation_queue.pane_count -= 1u;
        g_scene_mutation_queue.dropped_pane_count += 1u;
        return false;
    }
    return true;
}

static bool SceneEditorInputDiagEnabled(void) {
    const char* value = getenv("RAY_TRACING_EDITOR_INPUT_DIAG");
    if (!value || !value[0]) return false;
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

static bool SceneEditorIsTextZoomShortcutKey(SDL_Keycode key, SDL_Keymod mod) {
    if ((mod & (KMOD_CTRL | KMOD_GUI)) == 0) {
        return false;
    }
    return key == SDLK_0 ||
           key == SDLK_KP_0 ||
           key == SDLK_EQUALS ||
           key == SDLK_PLUS ||
           key == SDLK_KP_PLUS ||
           key == SDLK_MINUS ||
           key == SDLK_UNDERSCORE ||
           key == SDLK_KP_MINUS;
}

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

static void SceneEditorInputDiagMaybeEmit(const SDL_Event* event,
                                          const SceneEditorInputDiagFrame* diag,
                                          const SceneEditorInputRoutingResult* route) {
    if (!event || !diag || !route) return;
    if (!SceneEditorInputDiagEnabled()) return;
    if (!(event->type == SDL_QUIT ||
          event->type == SDL_KEYDOWN ||
          event->type == SDL_MOUSEBUTTONDOWN ||
          event->type == SDL_WINDOWEVENT)) {
        return;
    }

    printf("[ir1] scene_editor_input raw=%u normalized=%u ignored=%u immediate=%u queued=%u "
           "route(g=%u c=%u p=%u) pane_hit(ctrl=%u canvas=%u drag=%u) "
           "invalidate(target=%u full=%u class=%u) consumed=%d target=%d reasons=0x%x\n",
           diag->raw_event_count,
           diag->normalized_count,
           diag->ignored_count,
           diag->immediate_count,
           diag->queued_count,
           diag->routed_global_count,
           diag->routed_chrome_count,
           diag->routed_pane_count,
           diag->pane_controls_count,
           diag->pane_canvas_count,
           diag->pane_drag_count,
           diag->target_invalidation_count,
           diag->full_invalidation_count,
           (unsigned int)route->invalidation_class,
           route->consumed ? 1 : 0,
           (int)route->target,
           (unsigned int)route->invalidation_reason_bits);
}

static bool SceneEditorPointInRect(int x, int y, const SDL_Rect* rect) {
    if (!rect) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
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

static bool SceneEditorViewportRectContainsEventPoint(const SDL_Event* event) {
    int mx = 0;
    int my = 0;
    if (!event) return false;
    if (!g_scenePaneLayoutValid) return true;
    if (event->type == SDL_MOUSEMOTION) {
        mx = event->motion.x;
        my = event->motion.y;
    } else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        mx = event->button.x;
        my = event->button.y;
    } else if (event->type == SDL_MOUSEWHEEL) {
        SDL_GetMouseState(&mx, &my);
    } else {
        return false;
    }
    return SceneEditorPointInRect(mx, my, &g_scenePaneLayout.viewport_rect);
}

static void SceneEditorViewportResetDigestOverlayNavigation(void) {
    g_viewport_nav_state.orbit_yaw_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG;
    g_viewport_nav_state.orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG;
    g_viewport_nav_state.overlay_zoom = 1.0;
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
    SceneEditorMutationQueue_Reset();
    g_viewport_nav_state.orbit_active = false;
    g_viewport_nav_state.last_mouse_x = 0;
    g_viewport_nav_state.last_mouse_y = 0;
    SceneEditorBezier3DGizmoReset();
    SceneEditorCamera3DGizmoReset();
    SceneEditorSyncWindowSize(editor);
    SceneEditorLayoutChrome();
    InitializeEditorMode(editor);
    (void)SceneEditorViewportFitDigestOverlay(false);
    setRenderContext(editor->renderer,
                     editor->window,
                     sceneSettings.windowWidth,
                     sceneSettings.windowHeight);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_MOUSEMOTION, SDL_MOUSEWHEEL);
    SDL_FlushEvents(SDL_TEXTINPUT, SDL_TEXTEDITING);
}

static void RenderSceneDigestOverlay(SDL_Renderer* renderer) {
    int active_mode = EditorModeRouter_ClampEditorMode(animSettings.editorMode,
                                                       SceneEditorControlSurfaceLocksObjectMode());
    int mouse_x = 0;
    int mouse_y = 0;
    if (!renderer || !g_scenePaneLayoutValid) {
        g_digest_hover_object_index = -1;
        return;
    }
    SDL_GetMouseState(&mouse_x, &mouse_y);
    g_digest_hover_object_index = SceneEditorDigestOverlayRender(renderer,
                                                                 &g_scenePaneLayout.viewport_rect,
                                                                 &g_viewport_nav_state,
                                                                 active_mode,
                                                                 ObjectEditorGetSelectedObjectIndex(),
                                                                 mouse_x,
                                                                 mouse_y,
                                                                 &g_bezier3d_gizmo_state,
                                                                 &g_camera3d_gizmo_state);
}

static CameraPoint SceneEditorViewportScreenToWorldPoint(int screen_x, int screen_y) {
    Camera preview = CameraBuildPreviewCamera(&sceneSettings.camera,
                                              GetCurrentMarginPixels(),
                                              sceneSettings.windowWidth,
                                              sceneSettings.windowHeight);
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(&preview,
                                                                       sceneSettings.windowWidth,
                                                                       sceneSettings.windowHeight);
    return SpaceModeAdapter_ScreenToWorld(&view_ctx, screen_x, screen_y);
}

static void SceneEditorViewportZoomTowardScreenPoint(int screen_x, int screen_y, int wheel_y) {
    CameraPoint before = {0.0, 0.0};
    CameraPoint after = {0.0, 0.0};
    double delta = 0.0;
    if (wheel_y == 0) return;
    before = SceneEditorViewportScreenToWorldPoint(screen_x, screen_y);
    delta = sceneSettings.camera.zoom * 0.10 * (double)wheel_y;
    CameraZoom(&sceneSettings.camera, delta, 0.05, 200.0);
    after = SceneEditorViewportScreenToWorldPoint(screen_x, screen_y);
    CameraPan(&sceneSettings.camera, before.x - after.x, before.y - after.y);
}

static void SceneEditorViewportOrbitByMouseDelta(int dx, int dy) {
    RuntimeSceneBridge3DDigestState digest = {0};
    if (SceneEditorDigestOverlayResolve(&digest)) {
        g_viewport_nav_state.orbit_yaw_deg += (double)dx * 0.45;
        g_viewport_nav_state.orbit_pitch_deg += (double)dy * 0.35;
        if (g_viewport_nav_state.orbit_pitch_deg < SCENE_EDITOR_DIGEST_OVERLAY_MIN_PITCH_DEG) {
            g_viewport_nav_state.orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_MIN_PITCH_DEG;
        }
        if (g_viewport_nav_state.orbit_pitch_deg > SCENE_EDITOR_DIGEST_OVERLAY_MAX_PITCH_DEG) {
            g_viewport_nav_state.orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_MAX_PITCH_DEG;
        }
        return;
    }
    {
        double orbit_delta = (double)dx * 0.010 + (double)dy * 0.003;
        if (fabs(orbit_delta) <= 1e-9) {
            return;
        }
        CameraRotate(&sceneSettings.camera, orbit_delta);
    }
}

static bool SceneEditorViewportDigestZoomByWheel(int wheel_y) {
    RuntimeSceneBridge3DDigestState digest = {0};
    if (wheel_y == 0) {
        return false;
    }
    if (!SceneEditorDigestOverlayResolve(&digest)) {
        return false;
    }
    if (wheel_y > 0) {
        g_viewport_nav_state.overlay_zoom *= 1.12;
    } else {
        g_viewport_nav_state.overlay_zoom *= 0.90;
    }
    if (g_viewport_nav_state.overlay_zoom < SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM) {
        g_viewport_nav_state.overlay_zoom = SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM;
    }
    if (g_viewport_nav_state.overlay_zoom > SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM) {
        g_viewport_nav_state.overlay_zoom = SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM;
    }
    return true;
}

static bool SceneEditorViewportFrameToScene(void) {
    bool any = false;
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    int i = 0;
    const double margin_world = 18.0;
    for (i = 0; i < sceneSettings.objectCount; ++i) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];
        double radius = obj->radius * obj->scale;
        if (radius <= 0.0 && obj->numPoints > 0) {
            radius = 6.0;
        }
        if (!any) {
            min_x = obj->x - radius;
            max_x = obj->x + radius;
            min_y = obj->y - radius;
            max_y = obj->y + radius;
            any = true;
        } else {
            if (obj->x - radius < min_x) min_x = obj->x - radius;
            if (obj->x + radius > max_x) max_x = obj->x + radius;
            if (obj->y - radius < min_y) min_y = obj->y - radius;
            if (obj->y + radius > max_y) max_y = obj->y + radius;
        }
    }
    if (!any) {
        return false;
    }
    min_x -= margin_world;
    max_x += margin_world;
    min_y -= margin_world;
    max_y += margin_world;
    {
        double span_x = max_x - min_x;
        double span_y = max_y - min_y;
        double zoom_x = (span_x > 1e-6) ? ((double)sceneSettings.windowWidth / span_x) : sceneSettings.camera.zoom;
        double zoom_y = (span_y > 1e-6) ? ((double)sceneSettings.windowHeight / span_y) : sceneSettings.camera.zoom;
        double fit_zoom = fmin(zoom_x, zoom_y);
        if (fit_zoom < 0.05) fit_zoom = 0.05;
        if (fit_zoom > 200.0) fit_zoom = 200.0;
        CameraSetPosition(&sceneSettings.camera, (min_x + max_x) * 0.5, (min_y + max_y) * 0.5);
        CameraSetZoom(&sceneSettings.camera, fit_zoom);
    }
    return true;
}

static bool SceneEditorViewportFitDigestOverlay(bool reset_angles) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    double span_max = 0.0;
    double projected_min_x = 0.0;
    double projected_min_y = 0.0;
    double projected_max_x = 0.0;
    double projected_max_y = 0.0;
    double corners[8][3];
    double available_w = 0.0;
    double available_h = 0.0;
    double projected_w = 0.0;
    double projected_h = 0.0;
    double fit_zoom = 1.0;
    int sx = 0;
    int sy = 0;
    int i = 0;
    if (!SceneEditorDigestOverlayResolve(&digest)) {
        return false;
    }
    if (reset_angles) {
        g_viewport_nav_state.orbit_yaw_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG;
        g_viewport_nav_state.orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG;
    }
    if (!SceneEditorDigestOverlayResolveExtents(&digest,
                                                &min_x,
                                                &min_y,
                                                &min_z,
                                                &max_x,
                                                &max_y,
                                                &max_z,
                                                &span_max)) {
        SceneEditorViewportResetDigestOverlayNavigation();
        return true;
    }

    if (!SceneEditorDigestOverlayBuildProjectorWithView(&digest,
                                                        &g_scenePaneLayout.viewport_rect,
                                                        g_viewport_nav_state.orbit_yaw_deg,
                                                        g_viewport_nav_state.orbit_pitch_deg,
                                                        1.0,
                                                        &projector)) {
        return false;
    }

    corners[0][0] = min_x; corners[0][1] = min_y; corners[0][2] = min_z;
    corners[1][0] = min_x; corners[1][1] = min_y; corners[1][2] = max_z;
    corners[2][0] = min_x; corners[2][1] = max_y; corners[2][2] = min_z;
    corners[3][0] = min_x; corners[3][1] = max_y; corners[3][2] = max_z;
    corners[4][0] = max_x; corners[4][1] = min_y; corners[4][2] = min_z;
    corners[5][0] = max_x; corners[5][1] = min_y; corners[5][2] = max_z;
    corners[6][0] = max_x; corners[6][1] = max_y; corners[6][2] = min_z;
    corners[7][0] = max_x; corners[7][1] = max_y; corners[7][2] = max_z;

    for (i = 0; i < 8; ++i) {
        if (!SceneEditorDigestOverlayProjectPoint(&projector,
                                                  corners[i][0],
                                                  corners[i][1],
                                                  corners[i][2],
                                                  &sx,
                                                  &sy)) {
            continue;
        }
        if (i == 0) {
            projected_min_x = projected_max_x = (double)sx;
            projected_min_y = projected_max_y = (double)sy;
        } else {
            if ((double)sx < projected_min_x) projected_min_x = (double)sx;
            if ((double)sx > projected_max_x) projected_max_x = (double)sx;
            if ((double)sy < projected_min_y) projected_min_y = (double)sy;
            if ((double)sy > projected_max_y) projected_max_y = (double)sy;
        }
    }

    available_w = (double)projector.viewport.w * SCENE_EDITOR_DIGEST_OVERLAY_FRAME_FIT_FACTOR;
    available_h = (double)projector.viewport.h * SCENE_EDITOR_DIGEST_OVERLAY_FRAME_FIT_FACTOR;
    projected_w = fmax(1.0, projected_max_x - projected_min_x);
    projected_h = fmax(1.0, projected_max_y - projected_min_y);
    if (available_w > 1.0 && available_h > 1.0) {
        double zoom_x = available_w / projected_w;
        double zoom_y = available_h / projected_h;
        fit_zoom = fmin(zoom_x, zoom_y);
    }
    if (fit_zoom < SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM) {
        fit_zoom = SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM;
    }
    if (fit_zoom > SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM) {
        fit_zoom = SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM;
    }
    g_viewport_nav_state.overlay_zoom = fit_zoom;
    return true;
}

static bool SceneEditorViewportFrameToDigestOverlay(void) {
    return SceneEditorViewportFitDigestOverlay(true);
}

static bool SceneEditorHandleViewportNavigation(SceneEditor* editor,
                                                const SceneEditorPaneCommand* command,
                                                SceneEditorInputRoutingResult* result) {
    SDL_Event* event = NULL;
    SceneEditorControlSurfaceContract contract = {0};
    if (!editor || !command || !result) return false;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    event = command->event;
    if (!event) return false;
    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_f &&
        contract.laneKeyFrameEnabled) {
        if (SceneEditorViewportFrameToDigestOverlay()) {
            result->consumed = true;
        } else {
            result->consumed = SceneEditorViewportFrameToScene();
        }
    }
    if (result->consumed) {
        result->target = command->target;
        result->pane_hit_region = (uint8_t)command->pane_hit_region;
        result->requested_target_invalidation = true;
        result->invalidation_reason_bits |= (SCENE_EDITOR_INVALIDATE_REASON_PANE |
                                             SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS);
        result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_PANE;
        return true;
    }
    if (command->pane_hit_region != SCENE_EDITOR_PANE_HIT_CANVAS &&
        command->pane_hit_region != SCENE_EDITOR_PANE_HIT_DRAG) {
        return false;
    }

    if (event->type == SDL_MOUSEMOTION &&
        SceneEditorViewportRectContainsEventPoint(event)) {
        SDL_Keymod mods = SDL_GetModState();
        bool alt_down = ((mods & KMOD_ALT) != 0);
        if (alt_down && contract.laneGestureOrbitEnabled) {
            SceneEditorViewportOrbitByMouseDelta(event->motion.xrel, event->motion.yrel);
            g_viewport_nav_state.orbit_active = true;
            g_viewport_nav_state.last_mouse_x = event->motion.x;
            g_viewport_nav_state.last_mouse_y = event->motion.y;
            result->consumed = true;
        } else {
            g_viewport_nav_state.orbit_active = false;
        }
    } else if (event->type == SDL_MOUSEWHEEL &&
               SceneEditorViewportRectContainsEventPoint(event)) {
        int mx = 0;
        int my = 0;
        int wheel_y = event->wheel.y;
        if (wheel_y != 0 && contract.laneWheelZoomEnabled) {
            if (SceneEditorViewportDigestZoomByWheel(wheel_y)) {
                result->consumed = true;
            } else {
                SDL_GetMouseState(&mx, &my);
                SceneEditorViewportZoomTowardScreenPoint(mx, my, wheel_y);
                result->consumed = true;
            }
        }
    }

    if (!result->consumed) {
        return false;
    }

    result->target = command->target;
    result->pane_hit_region = (uint8_t)command->pane_hit_region;
    result->requested_target_invalidation = true;
    result->invalidation_reason_bits |= (SCENE_EDITOR_INVALIDATE_REASON_PANE |
                                         SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS);
    if (event->type == SDL_MOUSEMOTION && g_viewport_nav_state.orbit_active) {
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
        case 0:
            return SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE;
        case 1:
            return SceneEditorControlSurfaceLocksObjectMode() ? SCENE_EDITOR_INPUT_TARGET_NONE
                                                              : SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE;
        case 2:
            return SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE;
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
            ray_tracing_text_font_cache_shutdown();
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

static bool SceneEditorResolveChromeAction(const SDL_Event* event, SceneEditorChromeAction* out_action) {
    int selected_mode = 0;
    int mx = 0;
    int my = 0;
    SceneEditorControlSurfaceContract contract = {0};
    if (!event || !out_action) return false;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    out_action->kind = SCENE_EDITOR_CHROME_ACTION_NONE;
    out_action->mode_index = -1;
    if (event->type != SDL_MOUSEBUTTONDOWN) return false;
    if (event->button.button != SDL_BUTTON_LEFT) return false;
    mx = event->button.x;
    my = event->button.y;
    selected_mode = SceneEditorChromeShellResolveModeButtonAtPoint(mx, my);
    if (selected_mode >= 0) {
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_MODE_SELECT;
        out_action->mode_index = selected_mode;
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &previewButton)) {
        if (!contract.previewEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_PREVIEW;
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &changeModeButton)) {
        if (!contract.cycleModeEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_CYCLE_MODE;
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &applyButton)) {
        if (!contract.applyEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_APPLY;
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &saveButton)) {
        if (!contract.saveEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_SAVE;
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &backToMenuButton)) {
        if (!contract.backToMenuEnabled) return false;
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_BACK_TO_MENU;
        return true;
    }
    return false;
}

static bool SceneEditorSaveCurrentAuthoring(void) {
    char diagnostics[256];

    if (animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE &&
        animSettings.runtimeScenePath[0] != '\0') {
        if (!SceneEditorRuntimeScenePersistAuthoring(diagnostics, sizeof(diagnostics))) {
            fprintf(stderr,
                    "[editor] failed to persist runtime scene authoring '%s': %s\n",
                    animSettings.runtimeScenePath,
                    diagnostics);
            return false;
        }
    }
    SaveAllSettings();
    return true;
}

static void SceneEditorApplyChromeAction(SceneEditor* editor, const SceneEditorChromeAction* action) {
    SceneEditorControlSurfaceContract contract = {0};
    if (!editor || !action || action->kind == SCENE_EDITOR_CHROME_ACTION_NONE) return;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_MODE_SELECT) {
        bool selectable = (action->mode_index >= 0 &&
                           action->mode_index < 3 &&
                           contract.modeSelectable[action->mode_index]);
        if (selectable) {
            int clamped_mode = EditorModeRouter_ClampEditorMode(action->mode_index,
                                                                SceneEditorControlSurfaceLocksObjectMode());
            editor->currentMode = clamped_mode;
            animSettings.editorMode = clamped_mode;
            InitializeEditorMode(editor);
            printf("Changed Mode to %d via mode router\n", editor->currentMode);
        } else {
            printf("Mode %d currently unavailable in this scene source.\n", action->mode_index);
        }
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_PREVIEW) {
        if (!contract.previewEnabled) {
            return;
        }
        RunPreviewModeEmbedded(editor->window, editor->renderer);
        SceneEditorResumeAfterPreview(editor);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_CYCLE_MODE) {
        if (!contract.cycleModeEnabled) {
            return;
        }
        editor->currentMode = EditorModeRouter_NextEditorMode(editor->currentMode,
                                                              false,
                                                              SceneEditorControlSurfaceLocksObjectMode());
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
        printf("Changed Mode to %d\n", editor->currentMode);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_APPLY) {
        if (!contract.applyEnabled) {
            return;
        }
        SceneEditorChromeShellSetActionFeedback("Scene changes applied", 1800);
        printf("Applied scene editor authoring in-app for mode %d\n", editor->currentMode);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_SAVE) {
        if (!contract.saveEnabled) {
            return;
        }
        if (!SceneEditorSaveCurrentAuthoring()) {
            SceneEditorChromeShellSetActionFeedback("Scene save failed", 2200);
            return;
        }
        SceneEditorChromeShellSetActionFeedback("Scene saved", 1800);
        printf("Saved scene editor authoring in mode %d\n", editor->currentMode);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_BACK_TO_MENU) {
        if (!contract.backToMenuEnabled) {
            return;
        }
        editor->running = false;
        sceneEditorExitFlag = true;
        printf("Exited scene editor from mode %d\n", editor->currentMode);
    }
}

static bool SceneEditorDispatchBezierPaneCommand(const SceneEditorPaneCommand* command) {
    if (!command || !command->event) return false;
    switch (command->kind) {
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_UP:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG:
        case SCENE_EDITOR_PANE_COMMAND_KEY:
            HandleBezierEditorEvents(command->event, &draggingPoint, &draggingVelocity);
            return true;
        default:
            return false;
    }
}

static bool SceneEditorDispatchObjectPaneCommand(const SceneEditorPaneCommand* command) {
    if (!command || !command->event) return false;
    switch (command->kind) {
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_UP:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG:
        case SCENE_EDITOR_PANE_COMMAND_WHEEL:
        case SCENE_EDITOR_PANE_COMMAND_KEY:
            HandleObjectEditorEvents(command->event);
            return true;
        default:
            return false;
    }
}

static bool SceneEditorDispatchCameraPaneCommand(const SceneEditorPaneCommand* command) {
    if (!command || !command->event) return false;
    switch (command->kind) {
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_UP:
        case SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG:
        case SCENE_EDITOR_PANE_COMMAND_WHEEL:
        case SCENE_EDITOR_PANE_COMMAND_KEY:
            HandleCameraEditorEvents(command->event);
            return true;
        default:
            return false;
    }
}

static bool SceneEditorContractCanvasAllowedForTarget(
    const SceneEditorControlSurfaceContract* contract,
    SceneEditorInputTarget target) {
    if (!contract) return false;
    switch (target) {
        case SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE:
            return contract->laneBezierCanvasEditEnabled;
        case SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE:
            return contract->laneObjectCanvasEditEnabled;
        case SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE:
            return contract->laneCameraCanvasEditEnabled;
        default:
            return false;
    }
}

static bool SceneEditorDispatchControlled3DObjectCanvasCommand(const SceneEditorPaneCommand* command) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    int pick = -1;
    if (!command || !command->event) return false;
    if (command->kind != SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN) return false;
    if (command->event->type != SDL_MOUSEBUTTONDOWN ||
        command->event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }
    if (!SceneEditorViewportRectContainsEventPoint(command->event)) return false;
    if (!SceneEditorDigestOverlayResolve(&digest)) return false;
    if (!SceneEditorDigestOverlayBuildProjector(&digest,
                                                &g_scenePaneLayout.viewport_rect,
                                                &g_viewport_nav_state,
                                                &projector)) return false;

    pick = SceneEditorDigestOverlayPickObjectIndex(&projector,
                                                   &digest,
                                                   command->event->button.x,
                                                   command->event->button.y);
    if (pick < 0) {
        pick = g_digest_hover_object_index;
    }
    if (pick < 0) {
        return false;
    }
    ObjectEditorSetSelectedObjectIndex(pick);
    return true;
}

static bool SceneEditorDispatchControlled3DCameraCanvasCommand(const SceneEditorPaneCommand* command) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double plane_z = 0.0;
    SDL_Keymod mods = SDL_GetModState();
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    int pick = -1;
    int handle_segment = -1;
    int handle_index = -1;
    int rotation_pick = -1;
    double world_x = 0.0;
    double world_y = 0.0;
    double world_z = 0.0;
    if (!command || !command->event) return false;
    if (!SceneEditorViewportRectContainsEventPoint(command->event)) return false;
    if (!SceneEditorDigestOverlayResolve(&digest)) return false;
    if (!SceneEditorDigestOverlayBuildProjector(&digest,
                                                &g_scenePaneLayout.viewport_rect,
                                                &g_viewport_nav_state,
                                                &projector)) return false;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(&digest, &projector);
    plane_z = SceneEditorDigestOverlayResolveEditPlaneZ(&digest, &projector);

    if (command->kind == SCENE_EDITOR_PANE_COMMAND_POINTER_UP) {
        if (command->event->type == SDL_MOUSEBUTTONUP &&
            command->event->button.button == SDL_BUTTON_LEFT &&
            g_camera3d_gizmo_state.dragging) {
            SceneEditorCamera3DGizmoReset();
            return true;
        }
        return false;
    }
    if (command->kind == SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG) {
        if (command->event->type == SDL_MOUSEMOTION &&
            g_camera3d_gizmo_state.dragging &&
            (command->event->motion.state & SDL_BUTTON_LMASK)) {
            return SceneEditorDigestOverlayApplyCameraGizmoDrag(&projector,
                                                                &digest,
                                                                &g_camera3d_gizmo_state,
                                                                command->event->motion.x,
                                                                command->event->motion.y);
        }
        return false;
    }
    if (command->kind != SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN) return false;
    if (command->event->type != SDL_MOUSEBUTTONDOWN ||
        command->event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }

    axis = SceneEditorDigestOverlayPickCameraGizmoAxis(&projector,
                                                       &digest,
                                                       command->event->button.x,
                                                       command->event->button.y);
    if (axis != SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE) {
        if (SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(&projector,
                                                                            &digest,
                                                                            &g_camera3d_gizmo_state.drag_start_world_x,
                                                                            &g_camera3d_gizmo_state.drag_start_world_y,
                                                                            &g_camera3d_gizmo_state.drag_start_world_z)) {
            g_camera3d_gizmo_state.dragging = true;
            g_camera3d_gizmo_state.drag_axis = axis;
            g_camera3d_gizmo_state.smooth_drag = ((mods & (KMOD_GUI | KMOD_CTRL)) != 0);
            g_camera3d_gizmo_state.drag_start_mouse_x = command->event->button.x;
            g_camera3d_gizmo_state.drag_start_mouse_y = command->event->button.y;
            return true;
        }
        return false;
    }

    if (SceneEditorDigestOverlayPickCameraBezierHandle(&projector,
                                                       command->event->button.x,
                                                       command->event->button.y,
                                                       &handle_segment,
                                                       &handle_index)) {
        SceneEditorCamera3DGizmoReset();
        return CameraEditorSelectBezierHandle(handle_segment, handle_index);
    }

    rotation_pick = SceneEditorDigestOverlayPickCameraRotationHandle(&projector,
                                                                     &digest,
                                                                     command->event->button.x,
                                                                     command->event->button.y);
    if (rotation_pick >= 0) {
        SceneEditorCamera3DGizmoReset();
        return CameraEditorSelectRotationHandle(rotation_pick);
    }

    pick = SceneEditorDigestOverlayPickCameraPointIndex(&projector,
                                                        command->event->button.x,
                                                        command->event->button.y);
    if (pick >= 0) {
        SceneEditorCamera3DGizmoReset();
        CameraEditorSetSelectedPointIndex(pick);
        return true;
    }

    if ((mods & KMOD_SHIFT) == 0) {
        SceneEditorCamera3DGizmoReset();
        CameraEditorClearSelection();
        return true;
    }

    if (!SceneEditorDigestOverlayScreenRayToPlanePoint(&projector,
                                                       command->event->button.x,
                                                       command->event->button.y,
                                                       plane_z,
                                                       &world_x,
                                                       &world_y,
                                                       &world_z)) {
        return false;
    }
    world_x = SceneEditorDigestOverlayQuantizeWorldValue(world_x, metrics.snap_step);
    world_y = SceneEditorDigestOverlayQuantizeWorldValue(world_y, metrics.snap_step);
    world_z = SceneEditorDigestOverlayQuantizeWorldValue(world_z, metrics.snap_step);
    if (!CameraPath3D_InsertPoint(&sceneSettings.cameraPath3D,
                                  &sceneSettings.cameraPath,
                                  world_x,
                                  world_y,
                                  world_z,
                                  metrics.default_handle_length)) {
        return false;
    }
    if (sceneSettings.cameraPath.numPoints > 0) {
        int new_index = sceneSettings.cameraPath.numPoints - 1;
        double seed_rot = (new_index > 0)
                              ? sceneSettings.cameraPath.rotations[new_index - 1]
                              : sceneSettings.camera.rotation;
        sceneSettings.cameraPath.rotations[new_index] = seed_rot;
        sceneSettings.cameraPath.rotationSet[new_index] = true;
        CameraEditorSetSelectedPointIndex(new_index);
        return true;
    }
    return false;
}

static bool SceneEditorDispatchControlled3DBezierCanvasCommand(const SceneEditorPaneCommand* command) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double plane_z = 0.0;
    SDL_Keymod mods = SDL_GetModState();
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    int handle_segment = -1;
    int handle_index = -1;
    int pick = -1;
    double world_x = 0.0;
    double world_y = 0.0;
    double world_z = 0.0;
    int previous_point_count = 0;
    if (!command || !command->event) return false;
    if (!SceneEditorViewportRectContainsEventPoint(command->event)) return false;
    if (!SceneEditorDigestOverlayResolve(&digest)) return false;
    if (!SceneEditorDigestOverlayBuildProjector(&digest,
                                                &g_scenePaneLayout.viewport_rect,
                                                &g_viewport_nav_state,
                                                &projector)) return false;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(&digest, &projector);
    plane_z = SceneEditorDigestOverlayResolveEditPlaneZ(&digest, &projector);
    if (command->kind == SCENE_EDITOR_PANE_COMMAND_POINTER_UP) {
        if (command->event->type == SDL_MOUSEBUTTONUP &&
            command->event->button.button == SDL_BUTTON_LEFT &&
            g_bezier3d_gizmo_state.dragging) {
            SceneEditorBezier3DGizmoReset();
            return true;
        }
        return false;
    }
    if (command->kind == SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG) {
        if (command->event->type == SDL_MOUSEMOTION &&
            g_bezier3d_gizmo_state.dragging &&
            (command->event->motion.state & SDL_BUTTON_LMASK)) {
            return SceneEditorDigestOverlayApplyBezierGizmoDrag(&projector,
                                                                &digest,
                                                                &g_bezier3d_gizmo_state,
                                                                command->event->motion.x,
                                                                command->event->motion.y);
        }
        return false;
    }
    if (command->kind != SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN) return false;
    if (command->event->type != SDL_MOUSEBUTTONDOWN ||
        command->event->button.button != SDL_BUTTON_LEFT) {
        return false;
    }
    axis = SceneEditorDigestOverlayPickBezierGizmoAxis(&projector,
                                                       &digest,
                                                       command->event->button.x,
                                                       command->event->button.y);
    if (axis != SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE) {
        if (!SceneEditorDigestOverlayBezierGizmoAxisLocked(axis) &&
            BezierEditorGetSelectionWorldPosition3D(&g_bezier3d_gizmo_state.drag_start_world_x,
                                                    &g_bezier3d_gizmo_state.drag_start_world_y,
                                                    &g_bezier3d_gizmo_state.drag_start_world_z)) {
            g_bezier3d_gizmo_state.dragging = true;
            g_bezier3d_gizmo_state.drag_axis = axis;
            g_bezier3d_gizmo_state.smooth_drag = ((mods & (KMOD_GUI | KMOD_CTRL)) != 0);
            g_bezier3d_gizmo_state.drag_start_mouse_x = command->event->button.x;
            g_bezier3d_gizmo_state.drag_start_mouse_y = command->event->button.y;
            return true;
        }
        return false;
    }
    if (SceneEditorDigestOverlayPickBezierHandle(&projector,
                                                 &sceneSettings.bezierPath,
                                                 &sceneSettings.bezierPath3D,
                                                 plane_z,
                                                 command->event->button.x,
                                                 command->event->button.y,
                                                 &handle_segment,
                                                 &handle_index) >= 0) {
        SceneEditorBezier3DGizmoReset();
        BezierEditorSelectHandle(handle_segment, handle_index);
        return true;
    }
    pick = SceneEditorDigestOverlayPickBezierPointIndex(&projector,
                                                        &sceneSettings.bezierPath,
                                                        &sceneSettings.bezierPath3D,
                                                        plane_z,
                                                        command->event->button.x,
                                                        command->event->button.y);
    if (pick >= 0) {
        SceneEditorBezier3DGizmoReset();
        BezierEditorSetSelectedPointIndex(pick);
        return true;
    }
    if ((mods & KMOD_SHIFT) == 0) {
        SceneEditorBezier3DGizmoReset();
        BezierEditorClearSelection();
        return true;
    }
    if (!SceneEditorDigestOverlayScreenRayToPlanePoint(&projector,
                                                       command->event->button.x,
                                                       command->event->button.y,
                                                       plane_z,
                                                       &world_x,
                                                       &world_y,
                                                       &world_z)) {
        return false;
    }
    previous_point_count = sceneSettings.bezierPath.numPoints;
    world_x = SceneEditorDigestOverlayQuantizeWorldValue(world_x, metrics.snap_step);
    world_y = SceneEditorDigestOverlayQuantizeWorldValue(world_y, metrics.snap_step);
    world_z = SceneEditorDigestOverlayQuantizeWorldValue(world_z, metrics.snap_step);
    CameraPath3D_InsertPoint(&sceneSettings.bezierPath3D,
                             &sceneSettings.bezierPath,
                             world_x,
                             world_y,
                             world_z,
                             metrics.default_handle_length);
    if (sceneSettings.bezierPath.numPoints > previous_point_count) {
        SceneEditorBezier3DGizmoReset();
        BezierEditorSetSelectedPointIndex(sceneSettings.bezierPath.numPoints - 1);
        return true;
    }
    return false;
}

static void SceneEditorRoutePaneEvent(SceneEditor* editor,
                                      const SceneEditorPaneCommand* command,
                                      SceneEditorInputRoutingResult* result) {
    SceneEditorControlSurfaceContract contract = {0};
    bool canvas_allowed_for_target = false;
    bool controlled_3d_viewport_command_region = false;
    if (!editor || !command || !command->event || !result) return;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    if (SceneEditorHandleViewportNavigation(editor, command, result)) {
        return;
    }
    if (contract.lane == SCENE_EDITOR_CONTROL_SURFACE_LANE_NATIVE_3D_RESERVED) {
        return;
    }
    controlled_3d_viewport_command_region =
        (contract.laneViewportObjectPickEnabled ||
         contract.laneViewportBezierPlacementEnabled ||
         contract.laneViewportCameraPlacementEnabled) &&
        (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_CANVAS ||
         command->pane_hit_region == SCENE_EDITOR_PANE_HIT_DRAG);
    if (controlled_3d_viewport_command_region) {
        if (contract.laneViewportBezierPlacementEnabled &&
            command->target == SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE) {
            result->consumed = SceneEditorDispatchControlled3DBezierCanvasCommand(command);
        } else if (contract.laneViewportObjectPickEnabled &&
                   command->target == SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE) {
            result->consumed = SceneEditorDispatchControlled3DObjectCanvasCommand(command);
        } else if (contract.laneViewportCameraPlacementEnabled &&
                   command->target == SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE) {
            result->consumed = SceneEditorDispatchControlled3DCameraCanvasCommand(command);
        } else {
            result->consumed = false;
        }
        if (!result->consumed) {
            return;
        }
        result->target = command->target;
        result->pane_hit_region = (uint8_t)command->pane_hit_region;
        result->requested_target_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE;
        if (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_DRAG) {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_INTERACTION;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_DRAG;
        } else {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_PANE;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS;
        }
        return;
    }
    canvas_allowed_for_target = SceneEditorContractCanvasAllowedForTarget(&contract, command->target);
    if (command->kind != SCENE_EDITOR_PANE_COMMAND_KEY &&
        command->pane_hit_region != SCENE_EDITOR_PANE_HIT_CONTROLS &&
        command->pane_hit_region != SCENE_EDITOR_PANE_HIT_LIST_PANEL &&
        !canvas_allowed_for_target) {
        return;
    }
    switch (command->target) {
        case SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE:
            result->consumed = SceneEditorDispatchBezierPaneCommand(command);
            break;
        case SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE:
            result->consumed = SceneEditorDispatchObjectPaneCommand(command);
            break;
        case SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE:
            result->consumed = SceneEditorDispatchCameraPaneCommand(command);
            break;
        default:
            result->consumed = false;
            break;
    }
    if (result->consumed) {
        result->target = command->target;
        result->pane_hit_region = (uint8_t)command->pane_hit_region;
        result->requested_target_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE;
        if (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_CONTROLS ||
            command->pane_hit_region == SCENE_EDITOR_PANE_HIT_LIST_PANEL) {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_UI;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_CONTROLS;
        } else if (command->pane_hit_region == SCENE_EDITOR_PANE_HIT_DRAG) {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_INTERACTION;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_DRAG;
        } else {
            result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_PANE;
            result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS;
        }
    }
}

static void SceneEditorDrainMutationQueue(SceneEditor* editor) {
    uint16_t i = 0;
    if (!editor) return;
    for (i = 0; i < g_scene_mutation_queue.order_count; ++i) {
        SceneEditorQueuedMutationRef* ref = &g_scene_mutation_queue.ordered_refs[i];
        if (ref->lane == SCENE_EDITOR_MUTATION_LANE_CHROME) {
            if (ref->lane_index < g_scene_mutation_queue.chrome_count) {
                SceneEditorApplyChromeAction(editor,
                                             &g_scene_mutation_queue.chrome_actions[ref->lane_index]);
                if (!editor->running || sceneEditorExitFlag) {
                    break;
                }
            }
            continue;
        }
        if (ref->lane == SCENE_EDITOR_MUTATION_LANE_PANE) {
            if (ref->lane_index < g_scene_mutation_queue.pane_count) {
                SceneEditorQueuedPaneCommand* queued = &g_scene_mutation_queue.pane_commands[ref->lane_index];
                SceneEditorPaneCommand command = {0};
                SceneEditorInputRoutingResult route = {0};
                command.kind = queued->kind;
                command.target = queued->target;
                command.pane_hit_region = queued->pane_hit_region;
                command.event = &queued->event_copy;
                SceneEditorInputRoutingResult_Reset(&route);
                SceneEditorRoutePaneEvent(editor, &command, &route);
                if (!editor->running || sceneEditorExitFlag) {
                    break;
                }
            }
            continue;
        }
    }
    SceneEditorMutationQueue_Reset();
}

static bool SceneEditorResolvePaneCommand(SceneEditorInputTarget target,
                                          SceneEditorPaneHitRegion pane_hit_region,
                                          SDL_Event* event,
                                          SceneEditorPaneCommand* command) {
    if (!event || !command) return false;
    memset(command, 0, sizeof(*command));
    command->target = target;
    command->pane_hit_region = pane_hit_region;
    command->event = event;
    command->kind = SCENE_EDITOR_PANE_COMMAND_NONE;

    if (target != SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE &&
        target != SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE &&
        target != SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE) {
        return false;
    }

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            command->kind = SCENE_EDITOR_PANE_COMMAND_POINTER_DOWN;
            break;
        case SDL_MOUSEBUTTONUP:
            command->kind = SCENE_EDITOR_PANE_COMMAND_POINTER_UP;
            break;
        case SDL_MOUSEMOTION:
            command->kind = SCENE_EDITOR_PANE_COMMAND_POINTER_DRAG;
            break;
        case SDL_MOUSEWHEEL:
            command->kind = SCENE_EDITOR_PANE_COMMAND_WHEEL;
            break;
        case SDL_KEYDOWN:
            command->kind = SCENE_EDITOR_PANE_COMMAND_KEY;
            break;
        default:
            command->kind = SCENE_EDITOR_PANE_COMMAND_NONE;
            break;
    }

    return command->kind != SCENE_EDITOR_PANE_COMMAND_NONE;
}

static SceneEditorPaneHitRegion SceneEditorResolvePaneHitRegion(SceneEditorInputTarget target,
                                                                const SDL_Event* event) {
    int mx = 0;
    int my = 0;
    if (!event) return SCENE_EDITOR_PANE_HIT_NONE;

    if (event->type == SDL_MOUSEMOTION) {
        mx = event->motion.x;
        my = event->motion.y;
        if (event->motion.state & SDL_BUTTON_LMASK) {
            return SCENE_EDITOR_PANE_HIT_DRAG;
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        mx = event->button.x;
        my = event->button.y;
    } else if (event->type == SDL_MOUSEWHEEL) {
        SDL_GetMouseState(&mx, &my);
    } else {
        return SCENE_EDITOR_PANE_HIT_NONE;
    }

    if (target == SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE) {
        BezierEditorHitRegion hit = BezierEditorHitRegionAtPoint(mx, my);
        if (hit == BEZIER_EDITOR_HIT_CONTROLS) return SCENE_EDITOR_PANE_HIT_CONTROLS;
        return SCENE_EDITOR_PANE_HIT_CANVAS;
    }
    if (target == SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE) {
        ObjectEditorHitRegion hit = ObjectEditorHitRegionAtPoint(mx, my);
        if (hit == OBJECT_EDITOR_HIT_CONTROLS) return SCENE_EDITOR_PANE_HIT_CONTROLS;
        if (hit == OBJECT_EDITOR_HIT_ASSET_PANEL || hit == OBJECT_EDITOR_HIT_MATERIAL_PANEL) {
            return SCENE_EDITOR_PANE_HIT_LIST_PANEL;
        }
        return SCENE_EDITOR_PANE_HIT_CANVAS;
    }
    if (target == SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE) {
        CameraEditorHitRegion hit = CameraEditorHitRegionAtPoint(mx, my);
        if (hit == CAMERA_EDITOR_HIT_CONTROLS || hit == CAMERA_EDITOR_HIT_SLIDER) {
            return SCENE_EDITOR_PANE_HIT_CONTROLS;
        }
        return SCENE_EDITOR_PANE_HIT_CANVAS;
    }

    return SCENE_EDITOR_PANE_HIT_NONE;
}

static void SceneEditorNormalizeInput(SceneEditor* editor,
                                      const SDL_Event* event,
                                      SceneEditorInputNormalized* normalized) {
    if (!editor || !event || !normalized) return;

    SceneEditorInputNormalized_Reset(normalized);
    normalized->event = event;
    if (!SceneEditorEventMatchesEditorWindow(editor, event)) {
        normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IGNORED;
        normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_NONE;
        normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_NONE;
        return;
    }

    if (event->type == SDL_QUIT || SceneEditorIsOwnWindowCloseEvent(editor, event)) {
        normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
        normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
        normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
        return;
    }

    if (event->type == SDL_KEYDOWN) {
        bool cycle_next = false;
        SceneEditorControlSurfaceContract contract = {0};
        SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
        if (event->key.keysym.sym == SDLK_TAB) {
            if (contract.sharedKeyTabCycleEnabled && contract.cycleModeEnabled) {
                normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
                normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
                normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
                return;
            }
        }
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            if (contract.sharedKeyEscapeEnabled) {
                normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
                normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
                normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
                return;
            }
        }
        if (SceneEditorResolveThemeShortcut(event->key.keysym.sym,
                                            event->key.keysym.mod,
                                            &cycle_next)) {
            normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
            normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
            normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
            return;
        }
        if (SceneEditorIsTextZoomShortcutKey(event->key.keysym.sym, event->key.keysym.mod)) {
            normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
            normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
            normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
            return;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        const int mx = event->button.x;
        const int my = event->button.y;
        if (SceneEditorChromeShellIsButtonHit(mx, my)) {
            normalized->action_class = SCENE_EDITOR_INPUT_ACTION_QUEUED;
            normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_CHROME;
            normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_CHROME;
            return;
        }
    }

    normalized->target_hint = SceneEditorResolvePaneTarget(editor);
    if (normalized->target_hint != SCENE_EDITOR_INPUT_TARGET_NONE &&
        (event->type == SDL_MOUSEBUTTONDOWN ||
         event->type == SDL_MOUSEBUTTONUP ||
         event->type == SDL_MOUSEMOTION ||
         event->type == SDL_MOUSEWHEEL ||
         event->type == SDL_KEYDOWN)) {
        normalized->action_class = SCENE_EDITOR_INPUT_ACTION_QUEUED;
        normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_ACTIVE_PANE;
        return;
    }

    normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IGNORED;
    normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_NONE;
}

static void SceneEditorApplyInputInvalidation(SceneEditor* editor,
                                              const SceneEditorInputRoutingResult* result) {
    (void)editor;
    if (!result) return;
    // IR1-S3 policy seam: invalidation classes are explicit even while render invalidation remains behavior-preserving.
}

static void SceneEditorHandleInputRouted(SceneEditor* editor, SDL_Event* event) {
    SceneEditorInputNormalized normalized;
    SceneEditorInputDiagFrame diag;
    SceneEditorInputRoutingResult route;

    memset(&diag, 0, sizeof(diag));
    diag.raw_event_count = 1u;
    SceneEditorInputNormalized_Reset(&normalized);
    SceneEditorNormalizeInput(editor, event, &normalized);

    SceneEditorInputRoutingResult_Reset(&route);
    if (normalized.action_class == SCENE_EDITOR_INPUT_ACTION_IMMEDIATE ||
        normalized.action_class == SCENE_EDITOR_INPUT_ACTION_QUEUED) {
        diag.normalized_count += 1u;
    }
    if (normalized.action_class == SCENE_EDITOR_INPUT_ACTION_IGNORED) {
        diag.ignored_count += 1u;
    } else if (normalized.action_class == SCENE_EDITOR_INPUT_ACTION_IMMEDIATE) {
        diag.immediate_count += 1u;
    } else if (normalized.action_class == SCENE_EDITOR_INPUT_ACTION_QUEUED) {
        diag.queued_count += 1u;
    }

    if (normalized.route_policy == SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL) {
        if (SceneEditorHandleSystemInput(editor, event, &route)) {
            diag.routed_global_count += 1u;
        }
    } else if (normalized.route_policy == SCENE_EDITOR_INPUT_ROUTE_POLICY_CHROME) {
        SceneEditorChromeAction action = {0};
        if (SceneEditorResolveChromeAction(event, &action) &&
            SceneEditorMutationQueue_EnqueueChromeAction(&action)) {
            route.target = SCENE_EDITOR_INPUT_TARGET_CHROME;
            route.consumed = true;
            if (action.kind == SCENE_EDITOR_CHROME_ACTION_APPLY) {
                route.invalidation_class = SCENE_EDITOR_INVALIDATION_FULL_EXIT;
                route.requested_full_invalidation = true;
                route.invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_EXIT;
            } else {
                route.invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_UI;
                route.requested_target_invalidation = true;
                route.invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_UI;
            }
            diag.routed_chrome_count += 1u;
        }
    } else if (normalized.route_policy == SCENE_EDITOR_INPUT_ROUTE_POLICY_ACTIVE_PANE) {
        SceneEditorPaneHitRegion pane_hit = SceneEditorResolvePaneHitRegion(normalized.target_hint, event);
        SceneEditorPaneCommand pane_command;
        if (SceneEditorResolvePaneCommand(normalized.target_hint, pane_hit, event, &pane_command) &&
            SceneEditorMutationQueue_EnqueuePaneCommand(&pane_command)) {
            route.target = normalized.target_hint;
            route.pane_hit_region = (uint8_t)pane_hit;
            route.consumed = true;
            route.requested_target_invalidation = true;
            route.invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE;
            if (pane_hit == SCENE_EDITOR_PANE_HIT_CONTROLS ||
                pane_hit == SCENE_EDITOR_PANE_HIT_LIST_PANEL) {
                route.invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_UI;
                route.invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_CONTROLS;
            } else if (pane_hit == SCENE_EDITOR_PANE_HIT_DRAG) {
                route.invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_INTERACTION;
                route.invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_DRAG;
            } else {
                route.invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_PANE;
                route.invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS;
            }
        }
        if (route.consumed) {
            diag.routed_pane_count += 1u;
            if (pane_hit == SCENE_EDITOR_PANE_HIT_CONTROLS || pane_hit == SCENE_EDITOR_PANE_HIT_LIST_PANEL) {
                diag.pane_controls_count += 1u;
            } else if (pane_hit == SCENE_EDITOR_PANE_HIT_DRAG) {
                diag.pane_drag_count += 1u;
            } else {
                diag.pane_canvas_count += 1u;
            }
        }
    }

    SceneEditorApplyInputInvalidation(editor, &route);
    if (route.requested_target_invalidation) {
        diag.target_invalidation_count += 1u;
    }
    if (route.requested_full_invalidation) {
        diag.full_invalidation_count += 1u;
    }
    SceneEditorInputDiagMaybeEmit(event, &diag, &route);
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

static void SceneEditorSyncWindowSize(SceneEditor* editor) {
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
}

static bool SceneEditorLoadSessionState(SceneEditor* editor) {
    if (!editor) {
        return false;
    }
    LoadAnimationConfig();
    LoadSceneConfig();
    if (!AnimationRestoreActiveSceneSource(true)) {
        fprintf(stderr, "[editor] failed to apply active scene source; fallback persisted.\n");
    }
    if (animSettings.editorMode < 0) {
        animSettings.editorMode = 0;
    }
    editor->currentMode = EditorModeRouter_ClampEditorMode(animSettings.editorMode % 3,
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
    SceneEditorMutationQueue_Reset();
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
    SceneEditorViewportResetDigestOverlayNavigation();
    SceneEditorBezier3DGizmoReset();
    SceneEditorCamera3DGizmoReset();

    if (TTF_WasInit() == 0 && TTF_Init() == -1) {
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
    SceneEditorLayoutChrome();
    (void)SceneEditorViewportFitDigestOverlay(true);
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
    SceneEditorMutationQueue_Reset();
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
                                      SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
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
    if (TTF_Init() == -1) {
        fprintf(stderr, "Error: TTF_Init failed: %s\n", TTF_GetError());
#if USE_VULKAN
        if (editor->owns_renderer && editor->renderer) {
            vk_renderer_wait_idle((VkRenderer*)editor->renderer);
            vk_renderer_shutdown_surface((VkRenderer*)editor->renderer);
        }
#else
        if (editor->owns_renderer && editor->renderer) {
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
    SceneEditorLayoutChrome();

    InitializeEditorMode(editor);


    UpdateObjects();
    editor->running = true;
    printf("Scene Editor Initialized. Window Size: %dx%d\n", sceneSettings.windowWidth, 
			sceneSettings.windowHeight);
    return true;
}

void RenderSceneButtons(SDL_Renderer* renderer) {
    SceneEditorControlSurfaceContract contract = {0};

    if (!renderer) return;
    SceneEditorControlSurfaceBuildCurrent(ObjectEditorGetSelectedObjectIndex(), &contract);
    SceneEditorChromeShellRender(renderer,
                                 &g_scenePaneLayout,
                                 g_scenePaneLayoutValid,
                                 &contract);
}

void SceneEditorLoop(SceneEditor* editor) {
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
                        HandleSceneEditorEvents(editor, &event);
                    }
                }
            }

            SceneEditorSyncWindowSize(editor);
            while (SDL_PollEvent(&event)) {
                frame_dirty = true;
                HandleSceneEditorEvents(editor, &event);
                if (sceneEditorExitFlag) {
                    break;
                }
            }
            SceneEditorDrainMutationQueue(editor);
            if (sceneEditorExitFlag || !editor->running) {
                if (perf_freq > 0u) {
                    double frame_elapsed_sec =
                        (double)(SDL_GetPerformanceCounter() - frame_begin_counter) / (double)perf_freq;
                    scene_loop_diag_tick(frame_elapsed_sec, wait_blocked_ms, wait_call_count);
                }
                break;
            }

            heartbeat_due = (last_render_ms == 0u) ||
                            ((Uint32)(SDL_GetTicks() - last_render_ms) >= SCENE_EDITOR_IDLE_HEARTBEAT_MS);
            if (!frame_dirty && !heartbeat_due) {
                if (perf_freq > 0u) {
                    double frame_elapsed_sec =
                        (double)(SDL_GetPerformanceCounter() - frame_begin_counter) / (double)perf_freq;
                    scene_loop_diag_tick(frame_elapsed_sec, wait_blocked_ms, wait_call_count);
                }
                continue;
            }

            // **Check for Dirty Objects and Update Them**
            for (int i = 0; i < sceneSettings.objectCount; i++) {
                SceneObject* obj = &sceneSettings.sceneObjects[i];
                if (IsObjectDirty(obj)) {
                    UpdateObject(obj);
                }
            }

            setRenderContext(editor->renderer, editor->window,
                             sceneSettings.windowWidth, sceneSettings.windowHeight);
            {
                RayTracingThemePalette palette = SceneEditorChromeShellResolvePalette();
                render_set_clear_color(editor->renderer,
                                       palette.background_fill.r,
                                       palette.background_fill.g,
                                       palette.background_fill.b,
                                       255);
            }
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

void SceneEditorSessionHandleEvent(SceneEditor* editor, SDL_Event* event) {
    if (!editor || !event || !editor->running) {
        return;
    }
    HandleSceneEditorEvents(editor, event);
}

void SceneEditorSessionRender(SceneEditor* editor) {
    if (!editor || !editor->running || !editor->window || !editor->renderer) {
        if (editor) {
            editor->running = false;
        }
        sceneEditorExitFlag = true;
        return;
    }

    SceneEditorDrainMutationQueue(editor);
    if (!editor->running || sceneEditorExitFlag) {
        return;
    }

    SceneEditorSyncWindowSize(editor);
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];
        if (IsObjectDirty(obj)) {
            UpdateObject(obj);
        }
    }

    setRenderContext(editor->renderer, editor->window,
                     sceneSettings.windowWidth, sceneSettings.windowHeight);
    {
        RayTracingThemePalette palette = SceneEditorChromeShellResolvePalette();
        render_set_clear_color(editor->renderer,
                               palette.background_fill.r,
                               palette.background_fill.g,
                               palette.background_fill.b,
                               255);
    }
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

bool SceneEditorSessionWantsExit(const SceneEditor* editor) {
    if (!editor) {
        return true;
    }
    return sceneEditorExitFlag || !editor->running;
}

bool SceneEditorSessionInteractionActive(const SceneEditor* editor) {
    (void)editor;
    return g_viewport_nav_state.orbit_active ||
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
    SceneEditorViewportResetDigestOverlayNavigation();
    SceneEditorBezier3DGizmoReset();
    SceneEditorCamera3DGizmoReset();
    SceneEditorMutationQueue_Reset();
    sceneEditorExitFlag = false;
    setRenderContext(NULL, NULL, 0, 0);
}


void HandleSceneEditorEvents(SceneEditor* editor, SDL_Event* event) {
    SceneEditorHandleInputRouted(editor, event);
}

bool IsClickingButtonMain(int mx, int my) {
    return SceneEditorChromeShellIsButtonHit(mx, my);
}

bool SceneEditorIsPaneToolButton(int mx, int my) {
    if ((mx >= addButton.x && mx <= addButton.x + addButton.w &&
         my >= addButton.y && my <= addButton.y + addButton.h) ||
        (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w &&
         my >= deleteButton.y && my <= deleteButton.y + deleteButton.h) ||
        (mx >= toggleButton.x && mx <= toggleButton.x + toggleButton.w &&
         my >= toggleButton.y && my <= toggleButton.y + toggleButton.h)) {
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
    if (mode >= 0 && mode <= 2) {
        editor->currentMode = EditorModeRouter_ClampEditorMode(mode,
                                                               SceneEditorControlSurfaceLocksObjectMode());
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
    }
}

void ResetSceneEditor(SceneEditor* editor) {
    LoadSceneConfig();  // Reload all scene settings
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
    if (editor->renderer && editor->owns_renderer) {
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)editor->renderer);
        vk_renderer_shutdown_surface((VkRenderer*)editor->renderer);
#else
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
    SceneEditorMutationQueue_Reset();
    setRenderContext(NULL, NULL, 0, 0);
    printf("Scene Editor Closed. Returning to main menu...\n");
}

static void InitializeEditorMode(SceneEditor* editor) {
    switch (editor->currentMode) {
        case 0:
            InitializeBezierEditor();
            break;
        case 1:
            InitializeObjectEditor();
            break;
        case 2:
            InitializeCameraEditor();
            break;
        default:
            break;
    }
}
