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
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "editor/scene_editor_surface_render.h"
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

// UI Button
SDL_Rect applyButton = {1000, 700, 150, 50};
SDL_Rect previewButton;
SDL_Rect changeModeButton;
SDL_Rect saveButton;
SDL_Rect backToMenuButton;
SDL_Rect addButton;  // Small square
SDL_Rect deleteButton;
SDL_Rect toggleButton;
static SDL_Rect modeSelectButtons[3];
static SceneEditorPaneHost g_scenePaneHost;
static SceneEditorPaneLayout g_scenePaneLayout;
static bool g_scenePaneLayoutValid = false;
static SDL_Rect g_sceneStatusTitleRect;
static SDL_Rect g_sceneStatusSourceRect;
static SDL_Rect g_sceneStatusPathRect;
static SDL_Rect g_sceneStatusObjectsRect;
static SDL_Rect g_sceneStatusSpaceRect;
static SDL_Rect g_sceneStatusDigestRect;
static SDL_Rect g_sceneStatusRuntimeRect;
static SDL_Rect g_sceneStatusControlsRect;
static char g_sceneActionFeedbackText[128];
static Uint64 g_sceneActionFeedbackUntilMs = 0u;
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
static int SceneEditorResolveModeButtonAtPoint(int mx, int my);
static bool SceneEditorIsChromeShellButtonHit(int mx, int my);

static bool FluidSceneLocksObjects(void) {
    return AnimationUseFluidScene();
}

static const char* SceneEditorModeLabel(int mode) {
    switch (mode) {
        case 0: return "Bezier";
        case 1: return "Objects";
        case 2: return "Camera";
        default: return "Mode";
    }
}

static const char* SceneEditorSourceLabel(void) {
    switch (animSettings.sceneSource) {
        case SCENE_SOURCE_FLUID_MANIFEST:
            return "Fluid Manifest";
        case SCENE_SOURCE_RUNTIME_SCENE:
            return "Runtime Scene";
        case SCENE_SOURCE_CONFIG_2D:
        default:
            return "2D Config";
    }
}

static const char* SceneEditorSourcePath(void) {
    if (animSettings.sceneSource == SCENE_SOURCE_FLUID_MANIFEST &&
        animSettings.fluidManifest[0]) {
        return animSettings.fluidManifest;
    }
    if (animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE &&
        animSettings.runtimeScenePath[0]) {
        return animSettings.runtimeScenePath;
    }
    return "(default)";
}

static void SceneEditorBuildControlSurfaceContract(SceneEditorControlSurfaceContract* out_contract) {
    SceneEditorControlSurfaceInput input = {0};
    int selected_object_index = ObjectEditorGetSelectedObjectIndex();
    if (!out_contract) return;
    input.requestedMode = animSettings.editorMode;
    input.lockObjectMode = FluidSceneLocksObjects();
    input.sceneSource = animSettings.sceneSource;
    input.sourceLabel = SceneEditorSourceLabel();
    input.sourcePath = SceneEditorSourcePath();
    input.objectCount = sceneSettings.objectCount;
    input.hasSelectedObject = (selected_object_index >= 0 &&
                               selected_object_index < sceneSettings.objectCount);
    input.selectedObjectIndex = selected_object_index;
    input.route = RayTracingModeBackend_ResolveRoute();
    input.digestStatus = RayTracingModeBackend_BuildSceneDigestStatus(&input.route);
    SceneEditorControlSurfaceBuild(&input, out_contract);
}

static void SceneEditorDefaultPalette(RayTracingThemePalette* out_palette) {
    if (!out_palette) return;
    out_palette->background_fill = (SDL_Color){46, 46, 52, 255};
    out_palette->panel_fill = (SDL_Color){58, 58, 68, 230};
    out_palette->panel_border = (SDL_Color){95, 95, 112, 255};
    out_palette->button_fill = (SDL_Color){180, 180, 180, 255};
    out_palette->button_active_fill = (SDL_Color){70, 140, 215, 255};
    out_palette->button_text = (SDL_Color){0, 0, 0, 255};
    out_palette->text_primary = (SDL_Color){220, 220, 230, 255};
    out_palette->text_muted = (SDL_Color){210, 210, 215, 255};
    out_palette->accent_primary = (SDL_Color){120, 200, 255, 255};
}

static RayTracingThemePalette SceneEditorResolvePalette(void) {
    RayTracingThemePalette palette = {0};
    if (!ray_tracing_shared_theme_resolve_palette(&palette)) {
        SceneEditorDefaultPalette(&palette);
    }
    return palette;
}

static Uint8 SceneEditorColorOffset(Uint8 value, int offset) {
    int out = (int)value + offset;
    if (out < 0) return 0;
    if (out > 255) return 255;
    return (Uint8)out;
}

static void SceneEditorSetActionFeedback(const char* text, Uint32 lifetime_ms) {
    if (!text || !text[0]) {
        g_sceneActionFeedbackText[0] = '\0';
        g_sceneActionFeedbackUntilMs = 0u;
        return;
    }
    snprintf(g_sceneActionFeedbackText,
             sizeof(g_sceneActionFeedbackText),
             "%s",
             text);
    g_sceneActionFeedbackUntilMs = SDL_GetTicks64() + (Uint64)lifetime_ms;
}

static SDL_Color SceneEditorResolveButtonTextColor(SDL_Color fill,
                                                   RayTracingThemePalette palette) {
    (void)fill;
    return palette.button_text;
}

static int SceneEditorResolveToneDelta(SDL_Color base_fill,
                                       int dark_theme_delta,
                                       int light_theme_delta) {
    int luminance = (int)base_fill.r * 299 + (int)base_fill.g * 587 + (int)base_fill.b * 114;
    return (luminance >= 140000) ? light_theme_delta : dark_theme_delta;
}

static SDL_Color SceneEditorResolveButtonFill(SDL_Color base_fill,
                                              SDL_Color disabled_fill,
                                              bool enabled,
                                              bool hovered,
                                              bool emphasized) {
    SDL_Color out = enabled ? base_fill : disabled_fill;
    int delta = 0;
    if (!enabled) return out;
    if (emphasized) {
        delta = SceneEditorResolveToneDelta(base_fill, hovered ? 24 : 16, hovered ? -24 : -16);
        out.r = SceneEditorColorOffset(out.r, delta);
        out.g = SceneEditorColorOffset(out.g, delta);
        out.b = SceneEditorColorOffset(out.b, delta);
        return out;
    }
    if (hovered) {
        delta = SceneEditorResolveToneDelta(base_fill, 10, -10);
        out.r = SceneEditorColorOffset(out.r, delta);
        out.g = SceneEditorColorOffset(out.g, delta);
        out.b = SceneEditorColorOffset(out.b, delta);
    }
    return out;
}

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

typedef struct SceneEditorViewportNavState {
    bool orbit_active;
    int last_mouse_x;
    int last_mouse_y;
    double orbit_yaw_deg;
    double orbit_pitch_deg;
    double overlay_zoom;
} SceneEditorViewportNavState;

typedef enum SceneEditorBezier3DGizmoAxis {
    SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE = 0,
    SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X,
    SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y,
    SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z
} SceneEditorBezier3DGizmoAxis;

typedef struct SceneEditorBezier3DGizmoState {
    bool dragging;
    bool smooth_drag;
    SceneEditorBezier3DGizmoAxis drag_axis;
    double drag_start_world_x;
    double drag_start_world_y;
    double drag_start_world_z;
    int drag_start_mouse_x;
    int drag_start_mouse_y;
} SceneEditorBezier3DGizmoState;

typedef struct SceneEditorCamera3DGizmoState {
    bool dragging;
    bool smooth_drag;
    SceneEditorBezier3DGizmoAxis drag_axis;
    double drag_start_world_x;
    double drag_start_world_y;
    double drag_start_world_z;
    int drag_start_mouse_x;
    int drag_start_mouse_y;
} SceneEditorCamera3DGizmoState;

#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG (-35.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG (24.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_PITCH_DEG (-80.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_PITCH_DEG (80.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM (0.03)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM (4.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_FRAME_FIT_FACTOR (0.72)

static SceneEditorViewportNavState g_viewport_nav_state = {
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

typedef struct SceneEditorDigestOverlayProjector {
    SDL_Rect viewport;
    double center_x;
    double center_y;
    double center_z;
    double yaw_rad;
    double pitch_rad;
    double distance;
    double scale;
    double span_max;
} SceneEditorDigestOverlayProjector;

typedef struct SceneEditorBezier3DInteractionMetrics {
    double snap_step;
    double gizmo_world_length;
    double default_handle_length;
} SceneEditorBezier3DInteractionMetrics;

static void SceneEditorDigestOverlayDrawLine3(SDL_Renderer* renderer,
                                              const SceneEditorDigestOverlayProjector* projector,
                                              double ax,
                                              double ay,
                                              double az,
                                              double bx,
                                              double by,
                                              double bz,
                                              SDL_Color color);
static bool SceneEditorBezier3DGizmoAxisLocked(SceneEditorBezier3DGizmoAxis axis);
static SceneEditorBezier3DInteractionMetrics SceneEditorDigestOverlayResolveBezierMetrics(
    const RuntimeSceneBridge3DDigestState* digest,
    const SceneEditorDigestOverlayProjector* projector);

typedef enum SceneEditorViewportRenderLane {
    SCENE_EDITOR_VIEWPORT_RENDER_LANE_PLANAR_2D = 0,
    SCENE_EDITOR_VIEWPORT_RENDER_LANE_DIGEST_3D = 1,
    SCENE_EDITOR_VIEWPORT_RENDER_LANE_NATIVE_3D_RESERVED = 2
} SceneEditorViewportRenderLane;

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

static bool SceneEditorButtonHovered(const SDL_Rect* rect) {
    int mx = 0;
    int my = 0;
    SDL_GetMouseState(&mx, &my);
    return SceneEditorPointInRect(mx, my, rect);
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

static bool SceneEditorDigestOverlayResolve(RuntimeSceneBridge3DDigestState* out_digest) {
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RuntimeSceneBridge3DDigestState digest = {0};
    if (!RayTracingModeBackend_IsCompat3DFallback(&route)) {
        if (out_digest) {
            memset(out_digest, 0, sizeof(*out_digest));
        }
        return false;
    }
    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    if (out_digest) {
        *out_digest = digest;
    }
    return digest.valid;
}

static bool SceneEditorDigestOverlayResolveExtents(const RuntimeSceneBridge3DDigestState* digest,
                                                   double* out_min_x,
                                                   double* out_min_y,
                                                   double* out_min_z,
                                                   double* out_max_x,
                                                   double* out_max_y,
                                                   double* out_max_z,
                                                   double* out_span_max) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    bool seeded = false;
    int i = 0;
    double span_x = 0.0;
    double span_y = 0.0;
    double span_z = 0.0;
    double span_max = 0.0;
    if (!digest) return false;
    if (!digest->valid) return false;

    if (digest->has_scene_bounds) {
        min_x = digest->bounds_min_x;
        min_y = digest->bounds_min_y;
        min_z = digest->bounds_min_z;
        max_x = digest->bounds_max_x;
        max_y = digest->bounds_max_y;
        max_z = digest->bounds_max_z;
        seeded = true;
    }

    for (i = 0; i < digest->primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveDigest* primitive = &digest->primitives[i];
        double half_w = primitive->has_dimensions ? fabs(primitive->width) * 0.5 : 0.0;
        double half_h = primitive->has_dimensions ? fabs(primitive->height) * 0.5 : 0.0;
        double half_d = primitive->has_dimensions ? fabs(primitive->depth) * 0.5 : 0.0;
        double p_min_x = primitive->origin_x - half_w;
        double p_max_x = primitive->origin_x + half_w;
        double p_min_y = primitive->origin_y - half_h;
        double p_max_y = primitive->origin_y + half_h;
        double p_min_z = primitive->origin_z - half_d;
        double p_max_z = primitive->origin_z + half_d;
        if (!seeded) {
            min_x = p_min_x;
            max_x = p_max_x;
            min_y = p_min_y;
            max_y = p_max_y;
            min_z = p_min_z;
            max_z = p_max_z;
            seeded = true;
        } else {
            if (p_min_x < min_x) min_x = p_min_x;
            if (p_max_x > max_x) max_x = p_max_x;
            if (p_min_y < min_y) min_y = p_min_y;
            if (p_max_y > max_y) max_y = p_max_y;
            if (p_min_z < min_z) min_z = p_min_z;
            if (p_max_z > max_z) max_z = p_max_z;
        }
    }

    if (!seeded) {
        return false;
    }

    span_x = fmax(1.0, max_x - min_x);
    span_y = fmax(1.0, max_y - min_y);
    span_z = fmax(1.0, max_z - min_z);
    span_max = fmax(span_x, fmax(span_y, span_z));

    if (out_min_x) *out_min_x = min_x;
    if (out_min_y) *out_min_y = min_y;
    if (out_min_z) *out_min_z = min_z;
    if (out_max_x) *out_max_x = max_x;
    if (out_max_y) *out_max_y = max_y;
    if (out_max_z) *out_max_z = max_z;
    if (out_span_max) *out_span_max = span_max;
    return true;
}

static bool SceneEditorDigestOverlayBuildProjectorWithView(const RuntimeSceneBridge3DDigestState* digest,
                                                           double yaw_deg,
                                                           double pitch_deg,
                                                           double zoom,
                                                           SceneEditorDigestOverlayProjector* out_projector) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    double span_max = 0.0;
    int viewport_w = 0;
    int viewport_h = 0;
    if (!digest || !out_projector) return false;
    if (!g_scenePaneLayoutValid) return false;
    if (!SceneEditorDigestOverlayResolveExtents(digest,
                                                &min_x,
                                                &min_y,
                                                &min_z,
                                                &max_x,
                                                &max_y,
                                                &max_z,
                                                &span_max)) {
        return false;
    }
    if (zoom < SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM) zoom = SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM;
    if (zoom > SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM) zoom = SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM;

    viewport_w = g_scenePaneLayout.viewport_rect.w;
    viewport_h = g_scenePaneLayout.viewport_rect.h;

    *out_projector = (SceneEditorDigestOverlayProjector){
        .viewport = g_scenePaneLayout.viewport_rect,
        .center_x = (min_x + max_x) * 0.5,
        .center_y = (min_y + max_y) * 0.5,
        .center_z = (min_z + max_z) * 0.5,
        .yaw_rad = yaw_deg * (M_PI / 180.0),
        .pitch_rad = pitch_deg * (M_PI / 180.0),
        .distance = span_max * 3.4,
        .scale = (double)fmin(viewport_w, viewport_h) * zoom,
        .span_max = span_max
    };
    if (out_projector->distance < 8.0) out_projector->distance = 8.0;
    if (viewport_w <= 0 || viewport_h <= 0) return false;
    if (out_projector->scale < 1.0) out_projector->scale = 1.0;
    return true;
}

static bool SceneEditorDigestOverlayBuildProjector(const RuntimeSceneBridge3DDigestState* digest,
                                                   SceneEditorDigestOverlayProjector* out_projector) {
    return SceneEditorDigestOverlayBuildProjectorWithView(digest,
                                                          g_viewport_nav_state.orbit_yaw_deg,
                                                          g_viewport_nav_state.orbit_pitch_deg,
                                                          g_viewport_nav_state.overlay_zoom,
                                                          out_projector);
}

static double SceneEditorResolveNiceStepFloor(double raw_step) {
    static const double buckets[] = {10.0, 5.0, 2.5, 2.0, 1.0};
    double magnitude = 1.0;
    double normalized = 0.0;
    size_t i = 0;
    if (!(raw_step > 0.0) || !isfinite(raw_step)) return 1.0;
    magnitude = pow(10.0, floor(log10(raw_step)));
    normalized = raw_step / magnitude;
    for (i = 0; i < sizeof(buckets) / sizeof(buckets[0]); ++i) {
        if (normalized >= buckets[i]) {
            return buckets[i] * magnitude;
        }
    }
    return magnitude;
}

static double SceneEditorQuantizeWorldValue(double value, double step) {
    if (!(step > 0.0) || !isfinite(step)) return value;
    return nearbyint(value / step) * step;
}

static SceneEditorBezier3DInteractionMetrics SceneEditorDigestOverlayResolveBezierMetrics(
    const RuntimeSceneBridge3DDigestState* digest,
    const SceneEditorDigestOverlayProjector* projector) {
    SceneEditorBezier3DInteractionMetrics metrics = {1.0, 6.0, 4.0};
    double span = 0.0;
    double raw_step = 0.0;
    if (projector && projector->span_max > 0.0) {
        span = projector->span_max;
    } else if (digest && digest->has_scene_bounds) {
        double span_x = fabs(digest->bounds_max_x - digest->bounds_min_x);
        double span_y = fabs(digest->bounds_max_y - digest->bounds_min_y);
        double span_z = fabs(digest->bounds_max_z - digest->bounds_min_z);
        span = fmax(span_x, fmax(span_y, span_z));
    }
    if (!(span > 0.0) || !isfinite(span)) {
        span = 10.0;
    }
    raw_step = span / 40.0;
    metrics.snap_step = SceneEditorResolveNiceStepFloor(raw_step);
    if (metrics.snap_step < 0.01) {
        metrics.snap_step = 0.01;
    }
    metrics.gizmo_world_length = fmax(metrics.snap_step * 6.0, span * 0.125);
    if (metrics.gizmo_world_length > span * 0.25) {
        metrics.gizmo_world_length = span * 0.25;
    }
    if (metrics.gizmo_world_length < metrics.snap_step * 4.0) {
        metrics.gizmo_world_length = metrics.snap_step * 4.0;
    }
    metrics.default_handle_length = fmax(metrics.snap_step * 6.0, span * 0.12);
    if (metrics.default_handle_length > span * 0.20) {
        metrics.default_handle_length = span * 0.20;
    }
    if (metrics.default_handle_length > metrics.gizmo_world_length * 1.15) {
        metrics.default_handle_length = metrics.gizmo_world_length * 1.15;
    }
    return metrics;
}

static bool SceneEditorDigestOverlayProjectPoint(const SceneEditorDigestOverlayProjector* projector,
                                                 double world_x,
                                                 double world_y,
                                                 double world_z,
                                                 int* out_x,
                                                 int* out_y) {
    double px = 0.0;
    double py = 0.0;
    double pz = 0.0;
    double yaw_x = 0.0;
    double yaw_y = 0.0;
    double yaw_z = 0.0;
    double pitch_y = 0.0;
    double screen_x = 0.0;
    double screen_y = 0.0;
    if (!projector || !out_x || !out_y) return false;
    px = world_x - projector->center_x;
    py = world_y - projector->center_y;
    pz = world_z - projector->center_z;

    yaw_x = cos(projector->yaw_rad) * px - sin(projector->yaw_rad) * py;
    yaw_y = sin(projector->yaw_rad) * px + cos(projector->yaw_rad) * py;
    yaw_z = pz;

    pitch_y = cos(projector->pitch_rad) * yaw_y - sin(projector->pitch_rad) * yaw_z;
    screen_x = (double)projector->viewport.x + (double)projector->viewport.w * 0.5 +
               yaw_x * projector->scale;
    screen_y = (double)projector->viewport.y + (double)projector->viewport.h * 0.5 +
               pitch_y * projector->scale;
    *out_x = (int)lround(screen_x);
    *out_y = (int)lround(screen_y);
    return true;
}

static void SceneEditorDigestOverlayRotateCameraToWorld(const SceneEditorDigestOverlayProjector* projector,
                                                        double cam_x,
                                                        double cam_y,
                                                        double cam_z,
                                                        double* out_world_x,
                                                        double* out_world_y,
                                                        double* out_world_z) {
    double yaw_x = 0.0;
    double yaw_y = 0.0;
    double yaw_z = 0.0;
    if (!projector || !out_world_x || !out_world_y || !out_world_z) return;
    yaw_x = cam_x;
    yaw_y = cos(projector->pitch_rad) * cam_y + sin(projector->pitch_rad) * cam_z;
    yaw_z = -sin(projector->pitch_rad) * cam_y + cos(projector->pitch_rad) * cam_z;
    *out_world_x = cos(projector->yaw_rad) * yaw_x + sin(projector->yaw_rad) * yaw_y;
    *out_world_y = -sin(projector->yaw_rad) * yaw_x + cos(projector->yaw_rad) * yaw_y;
    *out_world_z = yaw_z;
}

static double SceneEditorDigestOverlayResolveEditPlaneZ(const RuntimeSceneBridge3DDigestState* digest,
                                                        const SceneEditorDigestOverlayProjector* projector) {
    if (digest && digest->has_construction_plane) {
        return digest->construction_plane_offset;
    }
    if (projector) {
        return projector->center_z;
    }
    return 0.0;
}

static bool SceneEditorDigestOverlayScreenRayToPlanePoint(const SceneEditorDigestOverlayProjector* projector,
                                                          int screen_x,
                                                          int screen_y,
                                                          double plane_z,
                                                          double* out_world_x,
                                                          double* out_world_y,
                                                          double* out_world_z) {
    double viewport_cx = 0.0;
    double viewport_cy = 0.0;
    double cam_plane_x = 0.0;
    double cam_plane_y = 0.0;
    double origin_rel_x = 0.0;
    double origin_rel_y = 0.0;
    double origin_rel_z = 0.0;
    double dir_rel_x = 0.0;
    double dir_rel_y = 0.0;
    double dir_rel_z = 0.0;
    double origin_x = 0.0;
    double origin_y = 0.0;
    double origin_z = 0.0;
    double dir_x = 0.0;
    double dir_y = 0.0;
    double dir_z = 0.0;
    double t = 0.0;
    if (!projector || !out_world_x || !out_world_y || !out_world_z) return false;
    if (projector->scale <= 1e-6) return false;
    viewport_cx = (double)projector->viewport.x + (double)projector->viewport.w * 0.5;
    viewport_cy = (double)projector->viewport.y + (double)projector->viewport.h * 0.5;
    cam_plane_x = ((double)screen_x - viewport_cx) / projector->scale;
    cam_plane_y = ((double)screen_y - viewport_cy) / projector->scale;
    SceneEditorDigestOverlayRotateCameraToWorld(projector,
                                                cam_plane_x,
                                                cam_plane_y,
                                                projector->distance,
                                                &origin_rel_x,
                                                &origin_rel_y,
                                                &origin_rel_z);
    SceneEditorDigestOverlayRotateCameraToWorld(projector,
                                                0.0,
                                                0.0,
                                                -1.0,
                                                &dir_rel_x,
                                                &dir_rel_y,
                                                &dir_rel_z);
    origin_x = projector->center_x + origin_rel_x;
    origin_y = projector->center_y + origin_rel_y;
    origin_z = projector->center_z + origin_rel_z;
    dir_x = dir_rel_x;
    dir_y = dir_rel_y;
    dir_z = dir_rel_z;
    if (fabs(dir_z) < 1e-6) return false;
    t = (plane_z - origin_z) / dir_z;
    if (t < 0.0) return false;
    *out_world_x = origin_x + dir_x * t;
    *out_world_y = origin_y + dir_y * t;
    *out_world_z = plane_z;
    return true;
}

static int SceneEditorDigestOverlayPickBezierPointIndex(const SceneEditorDigestOverlayProjector* projector,
                                                        const Path* path,
                                                        const CameraPath3D* path3d,
                                                        double plane_z,
                                                        int screen_x,
                                                        int screen_y) {
    int pick_index = -1;
    double best_dist2 = 0.0;
    int i = 0;
    if (!projector || !path) return -1;
    for (i = 0; i < path->numPoints; ++i) {
        int px = 0;
        int py = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  path->points[i].x,
                                                  path->points[i].y,
                                                  path3d ? path3d->point_z[i] : plane_z,
                                                  &px,
                                                  &py)) {
            continue;
        }
        dx = (double)screen_x - (double)px;
        dy = (double)screen_y - (double)py;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX) {
            if (pick_index < 0 || dist2 < best_dist2) {
                pick_index = i;
                best_dist2 = dist2;
            }
        }
    }
    return pick_index;
}

static bool SceneEditorDigestOverlayBezierHandleWorldPosition(const Path* path,
                                                              const CameraPath3D* path3d,
                                                              int segment_index,
                                                              int handle_index,
                                                              double* out_x,
                                                              double* out_y,
                                                              double* out_z,
                                                              double* out_anchor_x,
                                                              double* out_anchor_y,
                                                              double* out_anchor_z,
                                                              double plane_z) {
    int point_index = 0;
    (void)plane_z;
    if (!path || !path3d) return false;
    if (segment_index < 0 || segment_index >= path->numPoints - 1) return false;
    if (!(handle_index == 0 || handle_index == 1)) return false;
    point_index = (handle_index == 0) ? segment_index : (segment_index + 1);
    if (out_anchor_x) *out_anchor_x = path->points[point_index].x;
    if (out_anchor_y) *out_anchor_y = path->points[point_index].y;
    if (out_anchor_z) *out_anchor_z = path3d->point_z[point_index];
    if (out_x) *out_x = path->points[point_index].x + path->handles[segment_index][handle_index].vx;
    if (out_y) *out_y = path->points[point_index].y + path->handles[segment_index][handle_index].vy;
    if (out_z) *out_z = path3d->point_z[point_index] + path3d->handles_vz[segment_index][handle_index];
    return true;
}

static bool SceneEditorDigestOverlayGetBezierSelectionWorldPosition(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    double* out_x,
    double* out_y,
    double* out_z) {
    double world_x = 0.0;
    double world_y = 0.0;
    double world_z = 0.0;
    if (!projector || !digest) return false;
    if (!BezierEditorGetSelectionWorldPosition3D(&world_x, &world_y, &world_z)) return false;
    if (out_x) *out_x = world_x;
    if (out_y) *out_y = world_y;
    if (out_z) *out_z = world_z;
    return true;
}

static int SceneEditorDigestOverlayPickBezierHandle(const SceneEditorDigestOverlayProjector* projector,
                                                    const Path* path,
                                                    const CameraPath3D* path3d,
                                                    double plane_z,
                                                    int screen_x,
                                                    int screen_y,
                                                    int* out_segment_index,
                                                    int* out_handle_index) {
    int picked_segment = -1;
    int picked_handle = -1;
    double best_dist2 = 0.0;
    int segment = 0;
    if (!projector || !path || !path3d) return -1;
    for (segment = 0; segment < path->numPoints - 1; ++segment) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double hx = 0.0;
            double hy = 0.0;
            double hz = 0.0;
            int px = 0;
            int py = 0;
            double dx = 0.0;
            double dy = 0.0;
            double dist2 = 0.0;
            if (!SceneEditorDigestOverlayBezierHandleWorldPosition(path,
                                                                   path3d,
                                                                   segment,
                                                                   handle_index,
                                                                   &hx,
                                                                   &hy,
                                                                   &hz,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL,
                                                                   plane_z)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector, hx, hy, hz, &px, &py)) {
                continue;
            }
            dx = (double)screen_x - (double)px;
            dy = (double)screen_y - (double)py;
            dist2 = dx * dx + dy * dy;
            if (dist2 <= SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX) {
                if (picked_segment < 0 || dist2 < best_dist2) {
                    picked_segment = segment;
                    picked_handle = handle_index;
                    best_dist2 = dist2;
                }
            }
        }
    }
    if (picked_segment >= 0) {
        if (out_segment_index) *out_segment_index = picked_segment;
        if (out_handle_index) *out_handle_index = picked_handle;
    }
    return picked_segment;
}

static bool SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(
    const SceneEditorDigestOverlayProjector* projector,
    double base_x,
    double base_y,
    double base_z,
    SceneEditorBezier3DGizmoAxis axis,
    double world_length,
    int* out_anchor_x,
    int* out_anchor_y,
    int* out_end_x,
    int* out_end_y,
    double* out_pixels_per_unit) {
    double target_x = 0.0;
    double target_y = 0.0;
    double target_z = 0.0;
    int ax = 0;
    int ay = 0;
    int bx = 0;
    int by = 0;
    double dx = 0.0;
    double dy = 0.0;
    if (!projector) return false;
    target_x = base_x;
    target_y = base_y;
    target_z = base_z;
    switch (axis) {
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X:
            target_x += world_length;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y:
            target_y += world_length;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z:
            target_z += world_length;
            break;
        default:
            return false;
    }
    if (!SceneEditorDigestOverlayProjectPoint(projector, base_x, base_y, base_z, &ax, &ay)) return false;
    if (!SceneEditorDigestOverlayProjectPoint(projector, target_x, target_y, target_z, &bx, &by)) return false;
    dx = (double)bx - (double)ax;
    dy = (double)by - (double)ay;
    if (fabs(dx) < 1e-6 && fabs(dy) < 1e-6) return false;
    if (out_anchor_x) *out_anchor_x = ax;
    if (out_anchor_y) *out_anchor_y = ay;
    if (out_end_x) *out_end_x = bx;
    if (out_end_y) *out_end_y = by;
    if (out_pixels_per_unit) {
        *out_pixels_per_unit = sqrt(dx * dx + dy * dy) / world_length;
    }
    return true;
}

static bool SceneEditorDigestOverlayProjectBezierGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    SceneEditorBezier3DGizmoAxis axis,
    double world_length,
    int* out_anchor_x,
    int* out_anchor_y,
    int* out_end_x,
    int* out_end_y,
    double* out_pixels_per_unit) {
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    if (!projector) return false;
    if (!SceneEditorDigestOverlayGetBezierSelectionWorldPosition(projector,
                                                                 digest,
                                                                 &base_x,
                                                                 &base_y,
                                                                 &base_z)) {
        return false;
    }
    return SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                                base_x,
                                                                base_y,
                                                                base_z,
                                                                axis,
                                                                world_length,
                                                                out_anchor_x,
                                                                out_anchor_y,
                                                                out_end_x,
                                                                out_end_y,
                                                                out_pixels_per_unit);
}

static SceneEditorBezier3DGizmoAxis SceneEditorDigestOverlayPickBezierGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    SceneEditorBezier3DGizmoAxis best_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    double best_dist2 = 0.0;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!projector || !digest) return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    if (BezierEditorGetSelectionKind() == BEZIER_EDITOR_SELECTION_NONE) {
        return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int gx = 0;
        int gy = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (SceneEditorBezier3DGizmoAxisLocked(axis)) {
            continue;
        }
        if (!SceneEditorDigestOverlayProjectBezierGizmoAxis(projector,
                                                            digest,
                                                            axis,
                                                            metrics.gizmo_world_length,
                                                            NULL,
                                                            NULL,
                                                            &gx,
                                                            &gy,
                                                            NULL)) {
            continue;
        }
        dx = (double)screen_x - (double)gx;
        dy = (double)screen_y - (double)gy;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX) {
            if (best_axis == SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE || dist2 < best_dist2) {
                best_axis = axis;
                best_dist2 = dist2;
            }
        }
    }
    return best_axis;
}

static bool SceneEditorBezier3DGizmoAxisLocked(SceneEditorBezier3DGizmoAxis axis) {
    (void)axis;
    return false;
}

static void SceneEditorDigestOverlayDrawBezierGizmo(SDL_Renderer* renderer,
                                                    const SceneEditorDigestOverlayProjector* projector,
                                                    const RuntimeSceneBridge3DDigestState* digest) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    int anchor_x = 0;
    int anchor_y = 0;
    int mouse_x = 0;
    int mouse_y = 0;
    SceneEditorBezier3DGizmoAxis hover_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!renderer || !projector || !digest) return;
    if (!SceneEditorDigestOverlayGetBezierSelectionWorldPosition(projector,
                                                                 digest,
                                                                 NULL,
                                                                 NULL,
                                                                 NULL)) {
        return;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    SDL_GetMouseState(&mouse_x, &mouse_y);
    hover_axis = SceneEditorDigestOverlayPickBezierGizmoAxis(projector, digest, mouse_x, mouse_y);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int end_x = 0;
        int end_y = 0;
        SDL_Color axis_color = {0, 0, 0, 255};
        SDL_Rect knob = {0, 0, 0, 0};
        int knob_half = 5;
        bool locked = SceneEditorBezier3DGizmoAxisLocked(axis);
        bool active = (g_bezier3d_gizmo_state.dragging && g_bezier3d_gizmo_state.drag_axis == axis);
        bool hovered = (hover_axis == axis);
        switch (axis) {
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X: axis_color = (SDL_Color){230, 96, 96, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y: axis_color = (SDL_Color){102, 224, 132, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z: axis_color = (SDL_Color){110, 160, 255, 255}; break;
            default: break;
        }
        if (!SceneEditorDigestOverlayProjectBezierGizmoAxis(projector,
                                                            digest,
                                                            axis,
                                                            metrics.gizmo_world_length,
                                                            &anchor_x,
                                                            &anchor_y,
                                                            &end_x,
                                                            &end_y,
                                                            NULL)) {
            continue;
        }
        if (locked) {
            axis_color.a = 110;
        } else if (active || hovered) {
            axis_color.a = 255;
        } else {
            axis_color.a = 220;
        }
        if (active) {
            knob_half = 7;
        } else if (hovered) {
            knob_half = 6;
        }
        SDL_SetRenderDrawColor(renderer, axis_color.r, axis_color.g, axis_color.b, axis_color.a);
        SDL_RenderDrawLine(renderer, anchor_x, anchor_y, end_x, end_y);
        knob = (SDL_Rect){end_x - knob_half, end_y - knob_half, knob_half * 2, knob_half * 2};
        if (locked) {
            SDL_RenderDrawRect(renderer, &knob);
        } else {
            SDL_RenderFillRect(renderer, &knob);
        }
        SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
        SDL_RenderDrawRect(renderer, &knob);
    }
}

static void SceneEditorDigestOverlayDrawPathPassive3D(SDL_Renderer* renderer,
                                                      const SceneEditorDigestOverlayProjector* projector,
                                                      const Path* path,
                                                      const CameraPath3D* path3d,
                                                      SDL_Color curve_color,
                                                      bool reverse_endpoints) {
    SDL_Color start_color = {0, 200, 0, 235};
    SDL_Color end_color = {220, 40, 40, 235};
    int i = 0;
    if (!renderer || !projector || !path || !path3d || path->numPoints < 2) return;

    {
        Point previous = GetPositionAlongPathNormalized((Path*)path, 0.0);
        double previous_z = CameraPath3D_GetPositionZNormalized(path, path3d, 0.0);
        for (i = 1; i <= 48; ++i) {
            double t = (double)i / 48.0;
            Point current = GetPositionAlongPathNormalized((Path*)path, t);
            double current_z = CameraPath3D_GetPositionZNormalized(path, path3d, t);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              previous.x,
                                              previous.y,
                                              previous_z,
                                              current.x,
                                              current.y,
                                              current_z,
                                              curve_color);
            previous = current;
            previous_z = current_z;
        }
    }

    for (i = 0; i < path->numPoints; ++i) {
        int px = 0;
        int py = 0;
        int radius = 0;
        SDL_Rect marker = {0, 0, 0, 0};
        SDL_Color color = curve_color;
        if (i != 0 && i != path->numPoints - 1) {
            continue;
        }
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  path->points[i].x,
                                                  path->points[i].y,
                                                  path3d->point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        radius = 5;
        if (reverse_endpoints) {
            color = (i == 0) ? end_color : start_color;
        } else {
            color = (i == 0) ? start_color : end_color;
        }
        marker = (SDL_Rect){px - radius, py - radius, radius * 2, radius * 2};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &marker);
        SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
        SDL_RenderDrawRect(renderer, &marker);
    }
}

static void SceneEditorDigestOverlayDrawBezier3D(SDL_Renderer* renderer,
                                                 const SceneEditorDigestOverlayProjector* projector,
                                                 const RuntimeSceneBridge3DDigestState* digest) {
    const double plane_z = SceneEditorDigestOverlayResolveEditPlaneZ(digest, projector);
    SDL_Color curve_color = {128, 214, 255, 235};
    SDL_Color point_color = {218, 232, 246, 255};
    SDL_Color selected_color = {255, 170, 82, 255};
    SDL_Color handle_color = {236, 120, 120, 210};
    SDL_Color selected_handle_color = {255, 196, 98, 255};
    SDL_Color hover_handle_color = {255, 226, 138, 255};
    SDL_Color hover_point_color = {255, 240, 176, 255};
    SDL_Color start_color = {0, 200, 0, 235};
    SDL_Color end_color = {220, 40, 40, 235};
    int selected_segment = -1;
    int selected_handle = -1;
    int hover_segment = -1;
    int hover_handle = -1;
    int hover_point = -1;
    int mouse_x = 0;
    int mouse_y = 0;
    BezierEditorSelectionKind selection_kind = BezierEditorGetSelectionKind();
    int i = 0;
    if (!renderer || !projector) return;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    hover_point = SceneEditorDigestOverlayPickBezierPointIndex(projector,
                                                               &sceneSettings.bezierPath,
                                                               &sceneSettings.bezierPath3D,
                                                               plane_z,
                                                               mouse_x,
                                                               mouse_y);
    if (SceneEditorDigestOverlayPickBezierHandle(projector,
                                                 &sceneSettings.bezierPath,
                                                 &sceneSettings.bezierPath3D,
                                                 plane_z,
                                                 mouse_x,
                                                 mouse_y,
                                                 &hover_segment,
                                                 &hover_handle) >= 0) {
        hover_point = -1;
    } else {
        hover_segment = -1;
        hover_handle = -1;
    }
    if (sceneSettings.bezierPath.numPoints >= 2) {
        Point previous = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, 0.0);
        double previous_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath,
                                                                &sceneSettings.bezierPath3D,
                                                                0.0);
        for (i = 1; i <= 48; ++i) {
            double t = (double)i / 48.0;
            Point current = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, t);
            double current_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath,
                                                                   &sceneSettings.bezierPath3D,
                                                                   t);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              previous.x,
                                              previous.y,
                                              previous_z,
                                              current.x,
                                              current.y,
                                              current_z,
                                              curve_color);
            previous = current;
            previous_z = current_z;
        }
    }
    if (BezierEditorGetSelectedHandle(&selected_segment, &selected_handle)) {
        selection_kind = BEZIER_EDITOR_SELECTION_HANDLE;
    }
    for (i = 0; i < sceneSettings.bezierPath.numPoints - 1; ++i) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double hx = 0.0;
            double hy = 0.0;
            double hz = 0.0;
            double anchor_x = 0.0;
            double anchor_y = 0.0;
            double anchor_z = 0.0;
            int px = 0;
            int py = 0;
            int ax = 0;
            int ay = 0;
            SDL_Rect knob = {0, 0, 0, 0};
            SDL_Color knob_color = handle_color;
            int knob_half = 5;
            bool selected = (selection_kind == BEZIER_EDITOR_SELECTION_HANDLE &&
                             i == selected_segment &&
                             handle_index == selected_handle);
            bool hovered = (i == hover_segment && handle_index == hover_handle);
            if (selected) {
                knob_color = selected_handle_color;
                knob_half = 6;
            } else if (hovered) {
                knob_color = hover_handle_color;
                knob_half = 6;
            }
            if (!SceneEditorDigestOverlayBezierHandleWorldPosition(&sceneSettings.bezierPath,
                                                                   &sceneSettings.bezierPath3D,
                                                                   i,
                                                                   handle_index,
                                                                   &hx,
                                                                   &hy,
                                                                   &hz,
                                                                   &anchor_x,
                                                                   &anchor_y,
                                                                   &anchor_z,
                                                                   plane_z)) {
                continue;
            }
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              anchor_x,
                                              anchor_y,
                                              anchor_z,
                                              hx,
                                              hy,
                                              hz,
                                              (SDL_Color){handle_color.r, handle_color.g, handle_color.b, 120});
            if (!SceneEditorDigestOverlayProjectPoint(projector, hx, hy, hz, &px, &py)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector, anchor_x, anchor_y, anchor_z, &ax, &ay)) {
                continue;
            }
            knob = (SDL_Rect){px - knob_half, py - knob_half, knob_half * 2, knob_half * 2};
            SDL_SetRenderDrawColor(renderer, knob_color.r, knob_color.g, knob_color.b, knob_color.a);
            SDL_RenderFillRect(renderer, &knob);
            SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
            SDL_RenderDrawRect(renderer, &knob);
        }
    }
    for (i = 0; i < sceneSettings.bezierPath.numPoints; ++i) {
        int px = 0;
        int py = 0;
        bool point_selected = (selection_kind == BEZIER_EDITOR_SELECTION_POINT &&
                               i == BezierEditorGetSelectedPointIndex());
        bool point_hovered = (i == hover_point);
        int radius = point_selected ? 6 : (point_hovered ? 5 : 4);
        SDL_Rect marker = {0, 0, 0, 0};
        SDL_Color color = point_selected ? selected_color : (point_hovered ? hover_point_color : point_color);
        if (!point_selected && !point_hovered && i == 0) {
            color = start_color;
        } else if (!point_selected && !point_hovered && i == sceneSettings.bezierPath.numPoints - 1) {
            color = end_color;
        }
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  sceneSettings.bezierPath.points[i].x,
                                                  sceneSettings.bezierPath.points[i].y,
                                                  sceneSettings.bezierPath3D.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        marker = (SDL_Rect){px - radius, py - radius, radius * 2, radius * 2};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &marker);
        SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
        SDL_RenderDrawRect(renderer, &marker);
    }
    if (selection_kind != BEZIER_EDITOR_SELECTION_NONE) {
        SceneEditorDigestOverlayDrawBezierGizmo(renderer, projector, digest);
    }
}

static int SceneEditorDigestOverlayPickCameraPointIndex(
    const SceneEditorDigestOverlayProjector* projector,
    int screen_x,
    int screen_y) {
    int picked = -1;
    double best_dist2 = 0.0;
    int i = 0;
    if (!projector) return -1;
    for (i = 0; i < sceneSettings.cameraPath.numPoints; ++i) {
        int px = 0;
        int py = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  sceneSettings.cameraPath.points[i].x,
                                                  sceneSettings.cameraPath.points[i].y,
                                                  sceneSettings.cameraPath3D.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        dx = (double)screen_x - (double)px;
        dy = (double)screen_y - (double)py;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX) {
            if (picked < 0 || dist2 < best_dist2) {
                picked = i;
                best_dist2 = dist2;
            }
        }
    }
    return picked;
}

static bool SceneEditorDigestOverlayPickCameraBezierHandle(
    const SceneEditorDigestOverlayProjector* projector,
    int screen_x,
    int screen_y,
    int* out_segment_index,
    int* out_handle_index) {
    int segment_index = 0;
    int picked_segment = -1;
    int picked_handle = -1;
    double best_dist2 = 0.0;
    if (!projector) return false;
    for (segment_index = 0; segment_index < sceneSettings.cameraPath.numPoints - 1; ++segment_index) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double handle_x = 0.0;
            double handle_y = 0.0;
            double handle_z = 0.0;
            double anchor_x = 0.0;
            double anchor_y = 0.0;
            double anchor_z = 0.0;
            int px = 0;
            int py = 0;
            double dx = 0.0;
            double dy = 0.0;
            double dist2 = 0.0;
            if (!CameraPath3D_GetHandleWorldPosition(&sceneSettings.cameraPath,
                                                     &sceneSettings.cameraPath3D,
                                                     segment_index,
                                                     handle_index,
                                                     &handle_x,
                                                     &handle_y,
                                                     &handle_z,
                                                     &anchor_x,
                                                     &anchor_y,
                                                     &anchor_z)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                      handle_x,
                                                      handle_y,
                                                      handle_z,
                                                      &px,
                                                      &py)) {
                continue;
            }
            dx = (double)screen_x - (double)px;
            dy = (double)screen_y - (double)py;
            dist2 = dx * dx + dy * dy;
            if (dist2 <= POINT_HIT_RADIUS * POINT_HIT_RADIUS) {
                if (picked_segment < 0 || dist2 < best_dist2) {
                    picked_segment = segment_index;
                    picked_handle = handle_index;
                    best_dist2 = dist2;
                }
            }
        }
    }
    if (picked_segment < 0) return false;
    if (out_segment_index) *out_segment_index = picked_segment;
    if (out_handle_index) *out_handle_index = picked_handle;
    return true;
}

static int SceneEditorDigestOverlayPickCameraRotationHandle(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    int picked = -1;
    double best_dist2 = 0.0;
    int i = 0;
    if (!projector || !digest) return -1;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    for (i = 0; i < sceneSettings.cameraPath.numPoints; ++i) {
        double rotation = CameraEditorGetPointRotation(i);
        double pitch = CameraEditorGetPointPitch(i);
        double draw_angle = rotation - (M_PI * 0.5);
        double direction_len = metrics.gizmo_world_length * 0.95;
        double horizontal_len = cos(pitch) * direction_len;
        double end_x = sceneSettings.cameraPath.points[i].x + cos(draw_angle) * horizontal_len;
        double end_y = sceneSettings.cameraPath.points[i].y + sin(draw_angle) * horizontal_len;
        double end_z = sceneSettings.cameraPath3D.point_z[i] + sin(pitch) * direction_len;
        int px = 0;
        int py = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectPoint(projector, end_x, end_y, end_z, &px, &py)) {
            continue;
        }
        dx = (double)screen_x - (double)px;
        dy = (double)screen_y - (double)py;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= POINT_HIT_RADIUS * POINT_HIT_RADIUS) {
            if (picked < 0 || dist2 < best_dist2) {
                picked = i;
                best_dist2 = dist2;
            }
        }
    }
    return picked;
}

static bool SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    double* out_x,
    double* out_y,
    double* out_z) {
    CameraEditorSelectionKind selection_kind = CAMERA_EDITOR_SELECTION_NONE;
    int point_index = -1;
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double rotation = 0.0;
    double draw_angle = 0.0;
    double direction_len = 0.0;
    if (!projector || !digest) return false;
    selection_kind = CameraEditorGetSelectionKind();
    if (selection_kind != CAMERA_EDITOR_SELECTION_ROTATION_HANDLE) {
        return CameraEditorGetSelectedGizmoWorldPosition(out_x, out_y, out_z);
    }
    point_index = CameraEditorGetSelectedPointIndex();
    if (point_index < 0 || point_index >= sceneSettings.cameraPath.numPoints) {
        return false;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    rotation = CameraEditorGetPointRotation(point_index);
    {
        double pitch = CameraEditorGetPointPitch(point_index);
        double horizontal_len = cos(pitch) * (metrics.gizmo_world_length * 0.95);
        direction_len = metrics.gizmo_world_length * 0.95;
        draw_angle = rotation - (M_PI * 0.5);
        if (out_x) {
            *out_x = sceneSettings.cameraPath.points[point_index].x + cos(draw_angle) * horizontal_len;
        }
        if (out_y) {
            *out_y = sceneSettings.cameraPath.points[point_index].y + sin(draw_angle) * horizontal_len;
        }
        if (out_z) {
            *out_z = sceneSettings.cameraPath3D.point_z[point_index] + sin(pitch) * direction_len;
        }
    }
    return true;
}

static SceneEditorBezier3DGizmoAxis SceneEditorDigestOverlayPickCameraGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    SceneEditorBezier3DGizmoAxis best_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    double best_dist2 = 0.0;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!projector || !digest) return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    if (!SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(projector,
                                                                         digest,
                                                                         &base_x,
                                                                         &base_y,
                                                                         &base_z)) {
        return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int gx = 0;
        int gy = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                                  base_x,
                                                                  base_y,
                                                                  base_z,
                                                                  axis,
                                                                  metrics.gizmo_world_length,
                                                                  NULL,
                                                                  NULL,
                                                                  &gx,
                                                                  &gy,
                                                                  NULL)) {
            continue;
        }
        dx = (double)screen_x - (double)gx;
        dy = (double)screen_y - (double)gy;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX) {
            if (best_axis == SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE || dist2 < best_dist2) {
                best_axis = axis;
                best_dist2 = dist2;
            }
        }
    }
    return best_axis;
}

static void SceneEditorDigestOverlayDrawCameraGizmo(SDL_Renderer* renderer,
                                                    const SceneEditorDigestOverlayProjector* projector,
                                                    const RuntimeSceneBridge3DDigestState* digest) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    int anchor_x = 0;
    int anchor_y = 0;
    int mouse_x = 0;
    int mouse_y = 0;
    SceneEditorBezier3DGizmoAxis hover_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!renderer || !projector || !digest) return;
    if (!SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(projector,
                                                                         digest,
                                                                         &base_x,
                                                                         &base_y,
                                                                         &base_z)) return;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    SDL_GetMouseState(&mouse_x, &mouse_y);
    hover_axis = SceneEditorDigestOverlayPickCameraGizmoAxis(projector, digest, mouse_x, mouse_y);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int end_x = 0;
        int end_y = 0;
        SDL_Color axis_color = {0, 0, 0, 255};
        SDL_Rect knob = {0, 0, 0, 0};
        int knob_half = 5;
        bool active = (g_camera3d_gizmo_state.dragging && g_camera3d_gizmo_state.drag_axis == axis);
        bool hovered = (hover_axis == axis);
        switch (axis) {
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X: axis_color = (SDL_Color){230, 96, 96, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y: axis_color = (SDL_Color){102, 224, 132, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z: axis_color = (SDL_Color){110, 160, 255, 255}; break;
            default: break;
        }
        if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                                  base_x,
                                                                  base_y,
                                                                  base_z,
                                                                  axis,
                                                                  metrics.gizmo_world_length,
                                                                  &anchor_x,
                                                                  &anchor_y,
                                                                  &end_x,
                                                                  &end_y,
                                                                  NULL)) {
            continue;
        }
        if (active || hovered) {
            axis_color.a = 255;
        } else {
            axis_color.a = 220;
        }
        if (active) {
            knob_half = 7;
        } else if (hovered) {
            knob_half = 6;
        }
        SDL_SetRenderDrawColor(renderer, axis_color.r, axis_color.g, axis_color.b, axis_color.a);
        SDL_RenderDrawLine(renderer, anchor_x, anchor_y, end_x, end_y);
        knob = (SDL_Rect){end_x - knob_half, end_y - knob_half, knob_half * 2, knob_half * 2};
        SDL_RenderFillRect(renderer, &knob);
        SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
        SDL_RenderDrawRect(renderer, &knob);
    }
}

static void SceneEditorDigestOverlayDrawCamera3D(SDL_Renderer* renderer,
                                                 const SceneEditorDigestOverlayProjector* projector,
                                                 const RuntimeSceneBridge3DDigestState* digest) {
    SDL_Color curve_color = {210, 168, 255, 230};
    SDL_Color point_color = {212, 238, 255, 255};
    SDL_Color selected_color = {255, 170, 82, 255};
    SDL_Color hover_point_color = {255, 240, 176, 255};
    SDL_Color handle_color = {255, 186, 104, 200};
    SDL_Color direction_color = {200, 170, 255, 220};
    SDL_Color start_color = {0, 200, 0, 235};
    SDL_Color end_color = {220, 40, 40, 235};
    int selected_point = CameraEditorGetSelectedPointIndex();
    int hover_point = -1;
    int mouse_x = 0;
    int mouse_y = 0;
    int i = 0;
    if (!renderer || !projector || !digest) return;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    hover_point = SceneEditorDigestOverlayPickCameraPointIndex(projector, mouse_x, mouse_y);
    for (i = 0; i < sceneSettings.cameraPath.numPoints - 1; ++i) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double anchor_x = 0.0;
            double anchor_y = 0.0;
            double anchor_z = 0.0;
            double end_x = 0.0;
            double end_y = 0.0;
            double end_z = 0.0;
            int anchor_px = 0;
            int anchor_py = 0;
            int end_px = 0;
            int end_py = 0;
            int knob_half = 4;
            SDL_Rect knob = {0, 0, 0, 0};
            bool related_selected = (selected_point == i) || (selected_point == i + 1);
            SDL_Color draw_color = handle_color;
            if (!CameraPath3D_GetHandleWorldPosition(&sceneSettings.cameraPath,
                                                     &sceneSettings.cameraPath3D,
                                                     i,
                                                     handle_index,
                                                     &end_x,
                                                     &end_y,
                                                     &end_z,
                                                     &anchor_x,
                                                     &anchor_y,
                                                     &anchor_z)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                      anchor_x,
                                                      anchor_y,
                                                      anchor_z,
                                                      &anchor_px,
                                                      &anchor_py) ||
                !SceneEditorDigestOverlayProjectPoint(projector,
                                                      end_x,
                                                      end_y,
                                                      end_z,
                                                      &end_px,
                                                      &end_py)) {
                continue;
            }
            if (related_selected) {
                draw_color.a = 255;
                knob_half = 5;
            }
            SDL_SetRenderDrawColor(renderer, draw_color.r, draw_color.g, draw_color.b, draw_color.a);
            SDL_RenderDrawLine(renderer, anchor_px, anchor_py, end_px, end_py);
            knob = (SDL_Rect){end_px - knob_half, end_py - knob_half, knob_half * 2, knob_half * 2};
            SDL_RenderFillRect(renderer, &knob);
            SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
            SDL_RenderDrawRect(renderer, &knob);
        }
    }
    if (sceneSettings.cameraPath.numPoints >= 2) {
        Point previous_xy = GetPositionAlongPathNormalized(&sceneSettings.cameraPath, 0.0);
        double previous_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath,
                                                                &sceneSettings.cameraPath3D,
                                                                0.0);
        for (i = 1; i <= 48; ++i) {
            double t = (double)i / 48.0;
            Point current_xy = GetPositionAlongPathNormalized(&sceneSettings.cameraPath, t);
            double current_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath,
                                                                   &sceneSettings.cameraPath3D,
                                                                   t);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              previous_xy.x,
                                              previous_xy.y,
                                              previous_z,
                                              current_xy.x,
                                              current_xy.y,
                                              current_z,
                                              curve_color);
            previous_xy = current_xy;
            previous_z = current_z;
        }
    }
    for (i = 0; i < sceneSettings.cameraPath.numPoints; ++i) {
        int px = 0;
        int py = 0;
        bool point_selected = (i == selected_point);
        bool point_hovered = (i == hover_point);
        int radius = point_selected ? 6 : (point_hovered ? 5 : 4);
        SDL_Rect marker = {0, 0, 0, 0};
        SDL_Color color = point_selected ? selected_color : (point_hovered ? hover_point_color : point_color);
        if (!point_selected && !point_hovered && i == 0) {
            color = end_color;
        } else if (!point_selected && !point_hovered && i == sceneSettings.cameraPath.numPoints - 1) {
            color = start_color;
        }
        double rotation = CameraEditorGetPointRotation(i);
        double pitch = CameraEditorGetPointPitch(i);
        double draw_angle = rotation - (M_PI * 0.5);
        double direction_len = 0.0;
        double horizontal_len = 0.0;
        double dir_end_x = 0.0;
        double dir_end_y = 0.0;
        double dir_end_z = 0.0;
        int dir_end_px = 0;
        int dir_end_py = 0;
        SDL_Color dir_color = direction_color;
        SDL_Rect dir_knob = {0, 0, 0, 0};
        int dir_knob_half = point_selected ? 5 : 4;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  sceneSettings.cameraPath.points[i].x,
                                                  sceneSettings.cameraPath.points[i].y,
                                                  sceneSettings.cameraPath3D.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        marker = (SDL_Rect){px - radius, py - radius, radius * 2, radius * 2};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &marker);
        SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
        SDL_RenderDrawRect(renderer, &marker);

        direction_len = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector).gizmo_world_length * 0.95;
        horizontal_len = cos(pitch) * direction_len;
        dir_end_x = sceneSettings.cameraPath.points[i].x + cos(draw_angle) * horizontal_len;
        dir_end_y = sceneSettings.cameraPath.points[i].y + sin(draw_angle) * horizontal_len;
        dir_end_z = sceneSettings.cameraPath3D.point_z[i] + sin(pitch) * direction_len;
        if (SceneEditorDigestOverlayProjectPoint(projector,
                                                 dir_end_x,
                                                 dir_end_y,
                                                 dir_end_z,
                                                 &dir_end_px,
                                                 &dir_end_py)) {
            if (point_selected) {
                dir_color.a = 255;
            }
            SDL_SetRenderDrawColor(renderer, dir_color.r, dir_color.g, dir_color.b, dir_color.a);
            SDL_RenderDrawLine(renderer, px, py, dir_end_px, dir_end_py);
            dir_knob = (SDL_Rect){dir_end_px - dir_knob_half,
                                  dir_end_py - dir_knob_half,
                                  dir_knob_half * 2,
                                  dir_knob_half * 2};
            SDL_RenderFillRect(renderer, &dir_knob);
            SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
            SDL_RenderDrawRect(renderer, &dir_knob);
        }
    }
    if (selected_point >= 0 && selected_point < sceneSettings.cameraPath.numPoints) {
        SceneEditorDigestOverlayDrawCameraGizmo(renderer, projector, digest);
    }
}

static bool SceneEditorCamera3DGizmoApplyDrag(const SceneEditorDigestOverlayProjector* projector,
                                              const RuntimeSceneBridge3DDigestState* digest,
                                              int mouse_x,
                                              int mouse_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    int axis_anchor_x = 0;
    int axis_anchor_y = 0;
    int axis_end_x = 0;
    int axis_end_y = 0;
    double pixels_per_unit = 0.0;
    double axis_dx = 0.0;
    double axis_dy = 0.0;
    double axis_len = 0.0;
    double mouse_dx = 0.0;
    double mouse_dy = 0.0;
    double projected_px = 0.0;
    double world_delta = 0.0;
    if (!projector || !digest || !g_camera3d_gizmo_state.dragging) return false;
    if (!SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(projector,
                                                                         digest,
                                                                         &base_x,
                                                                         &base_y,
                                                                         &base_z)) return false;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                              g_camera3d_gizmo_state.drag_start_world_x,
                                                              g_camera3d_gizmo_state.drag_start_world_y,
                                                              g_camera3d_gizmo_state.drag_start_world_z,
                                                              g_camera3d_gizmo_state.drag_axis,
                                                              metrics.gizmo_world_length,
                                                              &axis_anchor_x,
                                                              &axis_anchor_y,
                                                              &axis_end_x,
                                                              &axis_end_y,
                                                              &pixels_per_unit)) {
        return false;
    }
    axis_dx = (double)axis_end_x - (double)axis_anchor_x;
    axis_dy = (double)axis_end_y - (double)axis_anchor_y;
    axis_len = sqrt(axis_dx * axis_dx + axis_dy * axis_dy);
    if (axis_len <= 1e-6 || pixels_per_unit <= 1e-6) return false;
    mouse_dx = (double)mouse_x - (double)g_camera3d_gizmo_state.drag_start_mouse_x;
    mouse_dy = (double)mouse_y - (double)g_camera3d_gizmo_state.drag_start_mouse_y;
    projected_px = mouse_dx * (axis_dx / axis_len) + mouse_dy * (axis_dy / axis_len);
    world_delta = projected_px / pixels_per_unit;
    if (!g_camera3d_gizmo_state.smooth_drag) {
        world_delta = SceneEditorQuantizeWorldValue(world_delta, metrics.snap_step);
    }
    switch (g_camera3d_gizmo_state.drag_axis) {
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X:
            return CameraEditorMoveSelectedGizmoTo(g_camera3d_gizmo_state.drag_start_world_x + world_delta,
                                                   g_camera3d_gizmo_state.drag_start_world_y,
                                                   g_camera3d_gizmo_state.drag_start_world_z);
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y:
            return CameraEditorMoveSelectedGizmoTo(g_camera3d_gizmo_state.drag_start_world_x,
                                                   g_camera3d_gizmo_state.drag_start_world_y + world_delta,
                                                   g_camera3d_gizmo_state.drag_start_world_z);
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z:
            return CameraEditorMoveSelectedGizmoTo(g_camera3d_gizmo_state.drag_start_world_x,
                                                   g_camera3d_gizmo_state.drag_start_world_y,
                                                   g_camera3d_gizmo_state.drag_start_world_z + world_delta);
        default:
            return false;
    }
}

static bool SceneEditorBezier3DGizmoApplyDrag(const SceneEditorDigestOverlayProjector* projector,
                                              const RuntimeSceneBridge3DDigestState* digest,
                                              int mouse_x,
                                              int mouse_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    int axis_anchor_x = 0;
    int axis_anchor_y = 0;
    int axis_end_x = 0;
    int axis_end_y = 0;
    double pixels_per_unit = 0.0;
    double axis_dx = 0.0;
    double axis_dy = 0.0;
    double axis_len = 0.0;
    double mouse_dx = 0.0;
    double mouse_dy = 0.0;
    double projected_px = 0.0;
    double world_delta = 0.0;
    double new_x = g_bezier3d_gizmo_state.drag_start_world_x;
    double new_y = g_bezier3d_gizmo_state.drag_start_world_y;
    double new_z = g_bezier3d_gizmo_state.drag_start_world_z;
    if (!projector || !digest) return false;
    if (!g_bezier3d_gizmo_state.dragging) return false;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    if (!SceneEditorDigestOverlayProjectBezierGizmoAxis(projector,
                                                        digest,
                                                        g_bezier3d_gizmo_state.drag_axis,
                                                        metrics.gizmo_world_length,
                                                        &axis_anchor_x,
                                                        &axis_anchor_y,
                                                        &axis_end_x,
                                                        &axis_end_y,
                                                        &pixels_per_unit)) {
        return false;
    }
    axis_dx = (double)axis_end_x - (double)axis_anchor_x;
    axis_dy = (double)axis_end_y - (double)axis_anchor_y;
    axis_len = sqrt(axis_dx * axis_dx + axis_dy * axis_dy);
    if (axis_len <= 1e-6 || pixels_per_unit <= 1e-6) return false;
    mouse_dx = (double)mouse_x - (double)g_bezier3d_gizmo_state.drag_start_mouse_x;
    mouse_dy = (double)mouse_y - (double)g_bezier3d_gizmo_state.drag_start_mouse_y;
    projected_px = mouse_dx * (axis_dx / axis_len) + mouse_dy * (axis_dy / axis_len);
    world_delta = projected_px / pixels_per_unit;
    if (!g_bezier3d_gizmo_state.smooth_drag) {
        world_delta = SceneEditorQuantizeWorldValue(world_delta, metrics.snap_step);
    }
    switch (g_bezier3d_gizmo_state.drag_axis) {
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X:
            new_x += world_delta;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y:
            new_y += world_delta;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z:
            new_z += world_delta;
            break;
        default:
            return false;
    }
    return BezierEditorMoveSelectionTo3D(new_x, new_y, new_z);
}

static bool SceneEditorDigestOverlayPrimitiveScreenRect(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveDigest* primitive,
    SDL_Rect* out_rect,
    SDL_Point* out_center) {
    double corners[8][3] = {{0.0}};
    int corner_count = 0;
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    int center_x = 0;
    int center_y = 0;
    int i = 0;
    bool seeded = false;

    if (!projector || !primitive || !out_rect || !out_center) return false;

    if (!primitive->has_dimensions) {
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  primitive->origin_x,
                                                  primitive->origin_y,
                                                  primitive->origin_z,
                                                  &center_x,
                                                  &center_y)) {
            return false;
        }
        out_rect->x = center_x - 14;
        out_rect->y = center_y - 14;
        out_rect->w = 28;
        out_rect->h = 28;
        out_center->x = center_x;
        out_center->y = center_y;
        return true;
    }

    if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
        double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
        corners[0][0] = primitive->origin_x - half_w; corners[0][1] = primitive->origin_y - half_h; corners[0][2] = primitive->origin_z;
        corners[1][0] = primitive->origin_x + half_w; corners[1][1] = primitive->origin_y - half_h; corners[1][2] = primitive->origin_z;
        corners[2][0] = primitive->origin_x + half_w; corners[2][1] = primitive->origin_y + half_h; corners[2][2] = primitive->origin_z;
        corners[3][0] = primitive->origin_x - half_w; corners[3][1] = primitive->origin_y + half_h; corners[3][2] = primitive->origin_z;
        corner_count = 4;
    } else if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
               primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX) {
        double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
        double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
        double half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
        corners[0][0] = primitive->origin_x - half_w; corners[0][1] = primitive->origin_y - half_h; corners[0][2] = primitive->origin_z - half_d;
        corners[1][0] = primitive->origin_x + half_w; corners[1][1] = primitive->origin_y - half_h; corners[1][2] = primitive->origin_z - half_d;
        corners[2][0] = primitive->origin_x + half_w; corners[2][1] = primitive->origin_y + half_h; corners[2][2] = primitive->origin_z - half_d;
        corners[3][0] = primitive->origin_x - half_w; corners[3][1] = primitive->origin_y + half_h; corners[3][2] = primitive->origin_z - half_d;
        corners[4][0] = primitive->origin_x - half_w; corners[4][1] = primitive->origin_y - half_h; corners[4][2] = primitive->origin_z + half_d;
        corners[5][0] = primitive->origin_x + half_w; corners[5][1] = primitive->origin_y - half_h; corners[5][2] = primitive->origin_z + half_d;
        corners[6][0] = primitive->origin_x + half_w; corners[6][1] = primitive->origin_y + half_h; corners[6][2] = primitive->origin_z + half_d;
        corners[7][0] = primitive->origin_x - half_w; corners[7][1] = primitive->origin_y + half_h; corners[7][2] = primitive->origin_z + half_d;
        corner_count = 8;
    } else {
        return false;
    }

    if (!SceneEditorDigestOverlayProjectPoint(projector,
                                              primitive->origin_x,
                                              primitive->origin_y,
                                              primitive->origin_z,
                                              &center_x,
                                              &center_y)) {
        center_x = projector->viewport.x + projector->viewport.w / 2;
        center_y = projector->viewport.y + projector->viewport.h / 2;
    }
    for (i = 0; i < corner_count; ++i) {
        int sx = 0;
        int sy = 0;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  corners[i][0],
                                                  corners[i][1],
                                                  corners[i][2],
                                                  &sx,
                                                  &sy)) {
            continue;
        }
        if (!seeded) {
            min_x = max_x = sx;
            min_y = max_y = sy;
            seeded = true;
        } else {
            if (sx < min_x) min_x = sx;
            if (sx > max_x) max_x = sx;
            if (sy < min_y) min_y = sy;
            if (sy > max_y) max_y = sy;
        }
    }
    if (!seeded) return false;

    out_rect->x = min_x;
    out_rect->y = min_y;
    out_rect->w = (max_x - min_x) + 1;
    out_rect->h = (max_y - min_y) + 1;
    out_center->x = center_x;
    out_center->y = center_y;
    return true;
}

static int SceneEditorDigestOverlayPickObjectIndex(const SceneEditorDigestOverlayProjector* projector,
                                                   const RuntimeSceneBridge3DDigestState* digest,
                                                   int mx,
                                                   int my) {
    int pick_index = -1;
    double pick_area = 0.0;
    double fallback_dist2 = 0.0;
    int i = 0;

    if (!projector || !digest) return -1;

    for (i = 0; i < digest->primitive_count && i < sceneSettings.objectCount; ++i) {
        SDL_Rect rect = {0, 0, 0, 0};
        SDL_Point center = {0, 0};
        SDL_Rect expanded = {0, 0, 0, 0};
        double area = 0.0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        const int pad = 6;

        if (!SceneEditorDigestOverlayPrimitiveScreenRect(projector, &digest->primitives[i], &rect, &center)) {
            continue;
        }

        expanded.x = rect.x - pad;
        expanded.y = rect.y - pad;
        expanded.w = rect.w + pad * 2;
        expanded.h = rect.h + pad * 2;
        area = (double)expanded.w * (double)expanded.h;
        if (SceneEditorPointInRect(mx, my, &expanded)) {
            if (pick_index < 0 || area < pick_area) {
                pick_index = i;
                pick_area = area;
            }
            continue;
        }

        dx = (double)mx - (double)center.x;
        dy = (double)my - (double)center.y;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= 26.0 * 26.0) {
            if (pick_index < 0 || dist2 < fallback_dist2) {
                pick_index = i;
                fallback_dist2 = dist2;
            }
        }
    }

    return pick_index;
}

static SDL_Color SceneEditorColorFromPackedRGB(int packed, Uint8 alpha) {
    SDL_Color out;
    out.r = (Uint8)((packed >> 16) & 0xFF);
    out.g = (Uint8)((packed >> 8) & 0xFF);
    out.b = (Uint8)(packed & 0xFF);
    out.a = alpha;
    return out;
}

static SDL_Color SceneEditorDigestOverlayResolvePrimitiveColor(int primitive_index) {
    SDL_Color color = {220, 200, 88, 240};
    if (primitive_index >= 0 && primitive_index < sceneSettings.objectCount) {
        SceneObject* obj = &sceneSettings.sceneObjects[primitive_index];
        if (obj->color != 0) {
            color = SceneEditorColorFromPackedRGB(obj->color, 240);
        }
    }
    return color;
}

static void SceneEditorDigestOverlayDrawSelectionMarker(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveDigest* primitive,
    SDL_Color color) {
    int cx = 0;
    int cy = 0;
    const int outer_r = 5;
    const int inner_r = 2;
    SDL_Rect outer_box = {0, 0, 0, 0};
    SDL_Rect inner_box = {0, 0, 0, 0};
    if (!renderer || !projector || !primitive) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector,
                                              primitive->origin_x,
                                              primitive->origin_y,
                                              primitive->origin_z,
                                              &cx,
                                              &cy)) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    outer_box = (SDL_Rect){cx - outer_r, cy - outer_r, outer_r * 2, outer_r * 2};
    inner_box = (SDL_Rect){cx - inner_r, cy - inner_r, inner_r * 2, inner_r * 2};
    SDL_RenderDrawRect(renderer, &outer_box);
    SDL_RenderFillRect(renderer, &inner_box);
}

static void SceneEditorDigestOverlayDrawLine3(SDL_Renderer* renderer,
                                              const SceneEditorDigestOverlayProjector* projector,
                                              double ax,
                                              double ay,
                                              double az,
                                              double bx,
                                              double by,
                                              double bz,
                                              SDL_Color color) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (!renderer || !projector) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector, ax, ay, az, &x0, &y0)) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector, bx, by, bz, &x1, &y1)) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
}

static void SceneEditorDigestOverlayDrawBox(SDL_Renderer* renderer,
                                            const SceneEditorDigestOverlayProjector* projector,
                                            double min_x,
                                            double min_y,
                                            double min_z,
                                            double max_x,
                                            double max_y,
                                            double max_z,
                                            SDL_Color color) {
    static const int k_edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    double corners[8][3];
    int i = 0;
    if (!renderer || !projector) return;
    corners[0][0] = min_x; corners[0][1] = min_y; corners[0][2] = min_z;
    corners[1][0] = max_x; corners[1][1] = min_y; corners[1][2] = min_z;
    corners[2][0] = max_x; corners[2][1] = max_y; corners[2][2] = min_z;
    corners[3][0] = min_x; corners[3][1] = max_y; corners[3][2] = min_z;
    corners[4][0] = min_x; corners[4][1] = min_y; corners[4][2] = max_z;
    corners[5][0] = max_x; corners[5][1] = min_y; corners[5][2] = max_z;
    corners[6][0] = max_x; corners[6][1] = max_y; corners[6][2] = max_z;
    corners[7][0] = min_x; corners[7][1] = max_y; corners[7][2] = max_z;
    for (i = 0; i < 12; ++i) {
        int a = k_edges[i][0];
        int b = k_edges[i][1];
        SceneEditorDigestOverlayDrawLine3(renderer,
                                          projector,
                                          corners[a][0], corners[a][1], corners[a][2],
                                          corners[b][0], corners[b][1], corners[b][2],
                                          color);
    }
}

static void SceneEditorDigestOverlayDrawConstructionPlane(SDL_Renderer* renderer,
                                                          const SceneEditorDigestOverlayProjector* projector,
                                                          const RuntimeSceneBridge3DDigestState* digest,
                                                          SDL_Color color) {
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    double plane_z = 0.0;
    if (!renderer || !projector || !digest) return;
    if (!digest->has_scene_bounds || !digest->has_construction_plane) return;
    min_x = digest->bounds_min_x;
    max_x = digest->bounds_max_x;
    min_y = digest->bounds_min_y;
    max_y = digest->bounds_max_y;
    plane_z = digest->construction_plane_offset;
    SceneEditorDigestOverlayDrawLine3(renderer, projector, min_x, min_y, plane_z, max_x, min_y, plane_z, color);
    SceneEditorDigestOverlayDrawLine3(renderer, projector, max_x, min_y, plane_z, max_x, max_y, plane_z, color);
    SceneEditorDigestOverlayDrawLine3(renderer, projector, max_x, max_y, plane_z, min_x, max_y, plane_z, color);
    SceneEditorDigestOverlayDrawLine3(renderer, projector, min_x, max_y, plane_z, min_x, min_y, plane_z, color);
}

static void SceneEditorDigestOverlayDrawPrism(SDL_Renderer* renderer,
                                              const SceneEditorDigestOverlayProjector* projector,
                                              const RuntimeSceneBridgePrimitiveDigest* primitive,
                                              SDL_Color color) {
    double half_w = 0.0;
    double half_h = 0.0;
    double half_d = 0.0;
    if (!renderer || !projector || !primitive) return;
    if (!(primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
          primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX)) {
        return;
    }
    half_w = fmax(0.05, fabs(primitive->width) * 0.5);
    half_h = fmax(0.05, fabs(primitive->height) * 0.5);
    half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
    SceneEditorDigestOverlayDrawBox(renderer,
                                    projector,
                                    primitive->origin_x - half_w,
                                    primitive->origin_y - half_h,
                                    primitive->origin_z - half_d,
                                    primitive->origin_x + half_w,
                                    primitive->origin_y + half_h,
                                    primitive->origin_z + half_d,
                                    color);
}

static void RenderSceneDigestOverlay(SDL_Renderer* renderer) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    SDL_Rect previous_clip = {0, 0, 0, 0};
    SDL_bool clip_was_enabled = SDL_FALSE;
    int mx = 0;
    int my = 0;
    int i = 0;
    int active_mode = EditorModeRouter_ClampEditorMode(animSettings.editorMode, FluidSceneLocksObjects());
    if (!renderer) return;
    if (!SceneEditorDigestOverlayResolve(&digest)) return;
    if (!SceneEditorDigestOverlayBuildProjector(&digest, &projector)) return;

    clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &projector.viewport);
    g_digest_hover_object_index = -1;
    SDL_GetMouseState(&mx, &my);
    if (active_mode == 1 && SceneEditorPointInRect(mx, my, &projector.viewport)) {
        g_digest_hover_object_index = SceneEditorDigestOverlayPickObjectIndex(&projector, &digest, mx, my);
    }

    if (digest.has_scene_bounds) {
        SDL_Color bounds_color = digest.bounds_enabled
                                     ? (SDL_Color){90, 130, 190, 220}
                                     : (SDL_Color){70, 84, 102, 170};
        SceneEditorDigestOverlayDrawBox(renderer,
                                        &projector,
                                        digest.bounds_min_x,
                                        digest.bounds_min_y,
                                        digest.bounds_min_z,
                                        digest.bounds_max_x,
                                        digest.bounds_max_y,
                                        digest.bounds_max_z,
                                        bounds_color);
    }

    SceneEditorDigestOverlayDrawConstructionPlane(renderer,
                                                  &projector,
                                                  &digest,
                                                  (SDL_Color){205, 176, 106, 220});

    for (i = 0; i < digest.primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveDigest* primitive = &digest.primitives[i];
        SDL_Color primitive_color = SceneEditorDigestOverlayResolvePrimitiveColor(i);
        bool is_selected = (active_mode == 1 && ObjectEditorGetSelectedObjectIndex() == i);
        bool is_hover = (active_mode == 1 && g_digest_hover_object_index == i);
        SDL_Color highlight_color = is_selected
                                        ? (SDL_Color){255, 120, 70, 255}
                                        : (SDL_Color){84, 224, 255, 245};
        if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE && primitive->has_dimensions) {
            double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
            double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              &projector,
                                              primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                              primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                              primitive_color);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              &projector,
                                              primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                              primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                              primitive_color);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              &projector,
                                              primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                              primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                              primitive_color);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              &projector,
                                              primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                              primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                              primitive_color);
            if (is_selected || is_hover) {
                SceneEditorDigestOverlayDrawSelectionMarker(renderer, &projector, primitive, highlight_color);
            }
        } else {
            SceneEditorDigestOverlayDrawPrism(renderer,
                                              &projector,
                                              primitive,
                                              primitive_color);
            if (is_selected || is_hover) {
                SceneEditorDigestOverlayDrawSelectionMarker(renderer, &projector, primitive, highlight_color);
            }
        }
    }

    SceneEditorDigestOverlayDrawPathPassive3D(renderer,
                                              &projector,
                                              &sceneSettings.bezierPath,
                                              &sceneSettings.bezierPath3D,
                                              (SDL_Color){128, 214, 255, 210},
                                              false);
    SceneEditorDigestOverlayDrawPathPassive3D(renderer,
                                              &projector,
                                              &sceneSettings.cameraPath,
                                              &sceneSettings.cameraPath3D,
                                              (SDL_Color){210, 168, 255, 210},
                                              true);

    if (active_mode == 0) {
        SceneEditorDigestOverlayDrawBezier3D(renderer, &projector, &digest);
    } else if (active_mode == 2) {
        SceneEditorDigestOverlayDrawCamera3D(renderer, &projector, &digest);
    }

    SceneEditorDigestOverlayDrawLine3(renderer,
                                      &projector,
                                      projector.center_x,
                                      projector.center_y,
                                      projector.center_z,
                                      projector.center_x + projector.span_max * 0.15,
                                      projector.center_y,
                                      projector.center_z,
                                      (SDL_Color){230, 110, 110, 240});
    SceneEditorDigestOverlayDrawLine3(renderer,
                                      &projector,
                                      projector.center_x,
                                      projector.center_y,
                                      projector.center_z,
                                      projector.center_x,
                                      projector.center_y + projector.span_max * 0.15,
                                      projector.center_z,
                                      (SDL_Color){110, 220, 140, 240});
    SceneEditorDigestOverlayDrawLine3(renderer,
                                      &projector,
                                      projector.center_x,
                                      projector.center_y,
                                      projector.center_z,
                                      projector.center_x,
                                      projector.center_y,
                                      projector.center_z + projector.span_max * 0.15,
                                      (SDL_Color){120, 170, 240, 240});

    if (clip_was_enabled) {
        SDL_RenderSetClipRect(renderer, &previous_clip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }
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
    SceneEditorBuildControlSurfaceContract(&contract);
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
            return FluidSceneLocksObjects() ? SCENE_EDITOR_INPUT_TARGET_NONE
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
    SceneEditorBuildControlSurfaceContract(&contract);
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
            FluidSceneLocksObjects());
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
    SceneEditorBuildControlSurfaceContract(&contract);
    out_action->kind = SCENE_EDITOR_CHROME_ACTION_NONE;
    out_action->mode_index = -1;
    if (event->type != SDL_MOUSEBUTTONDOWN) return false;
    if (event->button.button != SDL_BUTTON_LEFT) return false;
    mx = event->button.x;
    my = event->button.y;
    selected_mode = SceneEditorResolveModeButtonAtPoint(mx, my);
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
    SceneEditorBuildControlSurfaceContract(&contract);
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_MODE_SELECT) {
        bool selectable = (action->mode_index >= 0 &&
                           action->mode_index < 3 &&
                           contract.modeSelectable[action->mode_index]);
        if (selectable) {
            int clamped_mode = EditorModeRouter_ClampEditorMode(action->mode_index, FluidSceneLocksObjects());
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
                                                              FluidSceneLocksObjects());
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
        printf("Changed Mode to %d\n", editor->currentMode);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_APPLY) {
        if (!contract.applyEnabled) {
            return;
        }
        SceneEditorSetActionFeedback("Scene changes applied", 1800);
        printf("Applied scene editor authoring in-app for mode %d\n", editor->currentMode);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_SAVE) {
        if (!contract.saveEnabled) {
            return;
        }
        if (!SceneEditorSaveCurrentAuthoring()) {
            SceneEditorSetActionFeedback("Scene save failed", 2200);
            return;
        }
        SceneEditorSetActionFeedback("Scene saved", 1800);
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
    if (!SceneEditorDigestOverlayBuildProjector(&digest, &projector)) return false;

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
    if (!SceneEditorDigestOverlayBuildProjector(&digest, &projector)) return false;
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
            return SceneEditorCamera3DGizmoApplyDrag(&projector,
                                                     &digest,
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
    world_x = SceneEditorQuantizeWorldValue(world_x, metrics.snap_step);
    world_y = SceneEditorQuantizeWorldValue(world_y, metrics.snap_step);
    world_z = SceneEditorQuantizeWorldValue(world_z, metrics.snap_step);
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
    if (!SceneEditorDigestOverlayBuildProjector(&digest, &projector)) return false;
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
            return SceneEditorBezier3DGizmoApplyDrag(&projector,
                                                     &digest,
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
        if (!SceneEditorBezier3DGizmoAxisLocked(axis) &&
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
    world_x = SceneEditorQuantizeWorldValue(world_x, metrics.snap_step);
    world_y = SceneEditorQuantizeWorldValue(world_y, metrics.snap_step);
    world_z = SceneEditorQuantizeWorldValue(world_z, metrics.snap_step);
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
    SceneEditorBuildControlSurfaceContract(&contract);
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
        SceneEditorBuildControlSurfaceContract(&contract);
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
        if (SceneEditorIsChromeShellButtonHit(mx, my)) {
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

static int SceneEditorMeasureButtonWidth(const char* label, int min_width) {
    int point_size = animation_config_scale_text_point_size(&animSettings, 24, 12);
    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx ? ctx->renderer : NULL;
    int text_w = 0;
    int text_h = 0;
    TTF_Font* font = NULL;
    if (!label || !label[0]) return min_width;
    point_size = ray_tracing_text_raster_point_size(renderer, point_size, 12);
    font = ray_tracing_text_font_cache_get_ui_regular(point_size);
    if (!font) return min_width;
    if (TTF_SizeUTF8(font, label, &text_w, &text_h) != 0) {
        return min_width;
    }
    text_w = ray_tracing_text_logical_pixels(renderer, text_w);
    if (text_w + 28 > min_width) {
        min_width = text_w + 28;
    }
    return min_width;
}

static int SceneEditorMeasureButtonHeight(int min_height) {
    int point_size = animation_config_scale_text_point_size(&animSettings, 24, 12);
    int target = point_size + 14;
    if (target > min_height) return target;
    return min_height;
}

static int SceneEditorResolveModeButtonAtPoint(int mx, int my) {
    for (int i = 0; i < 3; i++) {
        if (SceneEditorPointInRect(mx, my, &modeSelectButtons[i])) {
            return i;
        }
    }
    return -1;
}

static bool SceneEditorIsChromeShellButtonHit(int mx, int my) {
    if (SceneEditorResolveModeButtonAtPoint(mx, my) >= 0) {
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &previewButton)) {
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &changeModeButton)) {
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &applyButton)) {
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &saveButton)) {
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &backToMenuButton)) {
        return true;
    }
    return false;
}

static void SceneEditorLayoutFallback(void) {
    int width = sceneSettings.windowWidth;
    int height = sceneSettings.windowHeight;
    int compactButtonWidth = SceneEditorMeasureButtonWidth("Delete", 70);
    int compactButtonHeight = SceneEditorMeasureButtonHeight(40);
    int footerButtonHeight = SceneEditorMeasureButtonHeight(46);
    int buttonGap = 10;
    int footerMargin = 30;
    int contentWidth = SceneEditorMeasureButtonWidth("Back to Menu", 320);
    int pairWidth = (contentWidth - buttonGap) / 2;

    addButton = (SDL_Rect){width - compactButtonWidth - 20, 20, compactButtonWidth, compactButtonHeight};
    deleteButton = (SDL_Rect){width - compactButtonWidth - 20,
                              20 + compactButtonHeight + 20,
                              compactButtonWidth,
                              compactButtonHeight};
    toggleButton = (SDL_Rect){width - compactButtonWidth - 20,
                              20 + (compactButtonHeight + 20) * 2,
                              compactButtonWidth,
                              compactButtonHeight};

    backToMenuButton = (SDL_Rect){width - contentWidth - footerMargin,
                                  height - footerButtonHeight - footerMargin,
                                  contentWidth,
                                  footerButtonHeight};
    applyButton = (SDL_Rect){backToMenuButton.x,
                             backToMenuButton.y - footerButtonHeight - buttonGap,
                             pairWidth,
                             footerButtonHeight};
    saveButton = (SDL_Rect){applyButton.x + pairWidth + buttonGap,
                            applyButton.y,
                            contentWidth - pairWidth - buttonGap,
                            footerButtonHeight};
    changeModeButton = (SDL_Rect){backToMenuButton.x,
                                  applyButton.y - footerButtonHeight - buttonGap,
                                  pairWidth,
                                  footerButtonHeight};
    previewButton = (SDL_Rect){changeModeButton.x + pairWidth + buttonGap,
                               changeModeButton.y,
                               contentWidth - pairWidth - buttonGap,
                               footerButtonHeight};

    for (int i = 0; i < 3; i++) {
        modeSelectButtons[i] = (SDL_Rect){0, 0, 0, 0};
    }
    g_sceneStatusTitleRect = (SDL_Rect){20, 20, 0, 0};
    g_sceneStatusSourceRect = (SDL_Rect){20, 20, 0, 0};
    g_sceneStatusPathRect = (SDL_Rect){20, 20, 0, 0};
    g_sceneStatusObjectsRect = (SDL_Rect){20, 20, 0, 0};
    g_sceneStatusSpaceRect = (SDL_Rect){20, 20, 0, 0};
    g_sceneStatusDigestRect = (SDL_Rect){20, 20, 0, 0};
    g_sceneStatusRuntimeRect = (SDL_Rect){20, 20, 0, 0};
    g_sceneStatusControlsRect = (SDL_Rect){20, 20, 0, 0};
}

static void SceneEditorLayoutChrome(void) {
    const SceneEditorPaneLayout* layout = NULL;
    int compactButtonWidth = 0;
    int compactButtonHeight = 0;
    int actionButtonHeight = 0;
    int actionRowWidth = 0;
    int actionHalfWidth = 0;
    int buttonGap = 10;
    int modeGap = 4;
    SDL_Rect left = {0};
    SDL_Rect right = {0};
    SDL_Rect modeRect = {0};
    int modeButtonWidth = 0;
    int modeButtonRemain = 0;
    int modeButtonX = 0;
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
        g_scenePaneLayoutValid = false;
        SceneEditorLayoutFallback();
        return;
    }

    layout = scene_editor_pane_host_layout(&g_scenePaneHost);
    if (!layout) {
        g_scenePaneLayoutValid = false;
        SceneEditorLayoutFallback();
        return;
    }
    g_scenePaneLayout = *layout;
    g_scenePaneLayoutValid = true;

    compactButtonWidth = SceneEditorMeasureButtonWidth("Delete", 90);
    compactButtonHeight = SceneEditorMeasureButtonHeight(36);
    actionButtonHeight = SceneEditorMeasureButtonHeight(44);
    left = g_scenePaneLayout.left_content_rect;
    right = g_scenePaneLayout.right_content_rect;
    modeRect = g_scenePaneLayout.mode_router_rect;
    actionRowWidth = right.w;
    if (actionRowWidth < 160) {
        actionRowWidth = 160;
    }
    actionHalfWidth = (actionRowWidth - buttonGap) / 2;

    addButton = (SDL_Rect){left.x, left.y, compactButtonWidth, compactButtonHeight};
    deleteButton = (SDL_Rect){left.x,
                              addButton.y + compactButtonHeight + buttonGap,
                              compactButtonWidth,
                              compactButtonHeight};
    toggleButton = (SDL_Rect){left.x,
                              deleteButton.y + compactButtonHeight + buttonGap,
                              compactButtonWidth,
                              compactButtonHeight};

    backToMenuButton = (SDL_Rect){right.x,
                                  right.y + right.h - actionButtonHeight,
                                  actionRowWidth,
                                  actionButtonHeight};
    applyButton = (SDL_Rect){right.x,
                             backToMenuButton.y - actionButtonHeight - buttonGap,
                             actionHalfWidth,
                             actionButtonHeight};
    saveButton = (SDL_Rect){applyButton.x + actionHalfWidth + buttonGap,
                            applyButton.y,
                            actionRowWidth - actionHalfWidth - buttonGap,
                            actionButtonHeight};
    changeModeButton = (SDL_Rect){right.x,
                                  applyButton.y - actionButtonHeight - buttonGap,
                                  actionHalfWidth,
                                  actionButtonHeight};
    previewButton = (SDL_Rect){changeModeButton.x + actionHalfWidth + buttonGap,
                               changeModeButton.y,
                               actionRowWidth - actionHalfWidth - buttonGap,
                               actionButtonHeight};

    modeButtonWidth = (modeRect.w - modeGap * 2) / 3;
    if (modeButtonWidth < 80) modeButtonWidth = 80;
    modeButtonRemain = modeRect.w - (modeButtonWidth * 3 + modeGap * 2);
    modeButtonX = modeRect.x;
    for (int i = 0; i < 3; i++) {
        int button_w = modeButtonWidth;
        if (i == 2 && modeButtonRemain > 0) {
            button_w += modeButtonRemain;
        }
        modeSelectButtons[i] = (SDL_Rect){modeButtonX, modeRect.y, button_w, modeRect.h};
        modeButtonX += button_w + modeGap;
    }

    g_sceneStatusTitleRect = (SDL_Rect){right.x, right.y, right.w, 28};
    g_sceneStatusSourceRect = (SDL_Rect){right.x, right.y + 34, right.w, 24};
    g_sceneStatusPathRect = (SDL_Rect){right.x, right.y + 62, right.w, 24};
    g_sceneStatusObjectsRect = (SDL_Rect){right.x, right.y + 90, right.w, 24};
    g_sceneStatusSpaceRect = (SDL_Rect){right.x, right.y + 118, right.w, 24};
    g_sceneStatusDigestRect = (SDL_Rect){right.x, right.y + 146, right.w, 24};
    g_sceneStatusRuntimeRect = (SDL_Rect){right.x, right.y + 174, right.w, 24};
    g_sceneStatusControlsRect = (SDL_Rect){right.x, right.y + 202, right.w, 24};
}

static void RenderFluidBounds(SDL_Renderer* renderer) {
    if (!g_fluidGrid.valid) return;
    Camera cam = CameraBuildPreviewCamera(&sceneSettings.camera,
                                          GetCurrentMarginPixels(),
                                          sceneSettings.windowWidth,
                                          sceneSettings.windowHeight);
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(&cam,
                                                                       sceneSettings.windowWidth,
                                                                       sceneSettings.windowHeight);
    CameraPoint minS = SpaceModeAdapter_WorldToScreen(&view_ctx,
                                                       g_fluidGrid.min_x,
                                                       g_fluidGrid.min_y);
    CameraPoint maxS = SpaceModeAdapter_WorldToScreen(&view_ctx,
                                                       g_fluidGrid.max_x,
                                                       g_fluidGrid.max_y);
    int x0 = (int)lrint(fmin(minS.x, maxS.x));
    int x1 = (int)lrint(fmax(minS.x, maxS.x));
    int y0 = (int)lrint(fmin(minS.y, maxS.y));
    int y1 = (int)lrint(fmax(minS.y, maxS.y));
    SDL_Rect rect = {x0, y0, x1 - x0, y1 - y0};
    SDL_SetRenderDrawColor(renderer, 120, 200, 255, 180);
    SDL_RenderDrawRect(renderer, &rect);
}

static SceneEditorViewportRenderLane SceneEditorResolveViewportRenderLane(void) {
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    if (RayTracingModeBackend_IsCompat3DFallback(&route)) {
        return SCENE_EDITOR_VIEWPORT_RENDER_LANE_DIGEST_3D;
    }
    if (RayTracingModeBackend_IsNative3D(&route)) {
        return SCENE_EDITOR_VIEWPORT_RENDER_LANE_NATIVE_3D_RESERVED;
    }
    return SCENE_EDITOR_VIEWPORT_RENDER_LANE_PLANAR_2D;
}

static void SceneEditorRenderActiveModeLayer(SceneEditor* editor) {
    if (!editor || !editor->renderer) {
        return;
    }
    switch (editor->currentMode) {
        case 0:
            RenderBezierEditor(editor->renderer);
            break;
        case 1:
            RenderObjectEditor(editor->renderer);
            break;
        case 2:
            RenderCameraEditor(editor->renderer);
            break;
        default:
            break;
    }
}

static void SceneEditorRenderViewportLane(SceneEditor* editor,
                                          SceneEditorViewportRenderLane lane) {
    if (!editor || !editor->renderer) {
        return;
    }
    if (lane == SCENE_EDITOR_VIEWPORT_RENDER_LANE_PLANAR_2D) {
        SceneEditorRenderActiveModeLayer(editor);
        RenderFluidBounds(editor->renderer);
        return;
    }
    if (lane == SCENE_EDITOR_VIEWPORT_RENDER_LANE_DIGEST_3D) {
        RenderSceneDigestOverlay(editor->renderer);
        return;
    }
    /* Native 3D lane reserved: no viewport-owned fallback primitives in this phase. */
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
                                                           FluidSceneLocksObjects());
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
    RayTracingThemePalette palette = SceneEditorResolvePalette();
    SDL_Color paneLabelColor = palette.text_primary;
    SDL_Color statusColor = palette.text_muted;
    SDL_Color paneFill = palette.panel_fill;
    SDL_Color paneBorder = palette.panel_border;
    SDL_Color modeBarFill = (SDL_Color){
        SceneEditorColorOffset(palette.panel_fill.r, -10),
        SceneEditorColorOffset(palette.panel_fill.g, -10),
        SceneEditorColorOffset(palette.panel_fill.b, -10),
        240
    };
    SDL_Color modeInactiveFill = palette.button_fill;
    SDL_Color modeActiveFill = palette.button_fill;
    SDL_Color previewFill = palette.button_fill;
    SDL_Color cycleFill = palette.button_fill;
    SDL_Color applyFill = palette.button_fill;
    SDL_Color saveFill = palette.button_fill;
    SDL_Color backFill = palette.button_fill;
    SDL_Color disabledFill = (SDL_Color){
        SceneEditorColorOffset(palette.panel_fill.r, -6),
        SceneEditorColorOffset(palette.panel_fill.g, -6),
        SceneEditorColorOffset(palette.panel_fill.b, -6),
        255
    };
    SDL_Color borderColor = (SDL_Color){
        SceneEditorColorOffset(palette.panel_border.r, -30),
        SceneEditorColorOffset(palette.panel_border.g, -30),
        SceneEditorColorOffset(palette.panel_border.b, -30),
        255
    };
    SDL_Rect modeTitleRect = {0};
    SDL_Rect feedbackRect = {0};
    SDL_Color resolvedFill = {0};
    SDL_Color resolvedText = {0};
    bool showFeedback = false;

    if (!renderer) return;
    SceneEditorBuildControlSurfaceContract(&contract);

    if (g_scenePaneLayoutValid) {
        SDL_SetRenderDrawColor(renderer, paneFill.r, paneFill.g, paneFill.b, paneFill.a);
        SDL_RenderFillRect(renderer, &g_scenePaneLayout.left_pane_rect);
        SDL_RenderFillRect(renderer, &g_scenePaneLayout.right_pane_rect);
        SDL_SetRenderDrawColor(renderer, modeBarFill.r, modeBarFill.g, modeBarFill.b, modeBarFill.a);
        SDL_RenderFillRect(renderer, &g_scenePaneLayout.mode_router_rect);

        modeTitleRect = (SDL_Rect){
            g_scenePaneLayout.left_pane_rect.x + 10,
            g_scenePaneLayout.left_pane_rect.y + 6,
            g_scenePaneLayout.left_pane_rect.w - 20,
            20
        };
        RenderLabelText(renderer, modeTitleRect, contract.paneLeftTitle, paneLabelColor);

        modeTitleRect = (SDL_Rect){
            g_scenePaneLayout.center_pane_rect.x + 10,
            g_scenePaneLayout.center_pane_rect.y + 6,
            g_scenePaneLayout.center_pane_rect.w - 20,
            20
        };
        RenderLabelText(renderer, modeTitleRect, contract.paneCenterTitle, paneLabelColor);

        modeTitleRect = (SDL_Rect){
            g_scenePaneLayout.right_pane_rect.x + 10,
            g_scenePaneLayout.right_pane_rect.y + 6,
            g_scenePaneLayout.right_pane_rect.w - 20,
            20
        };
        RenderLabelText(renderer, modeTitleRect, contract.paneRightTitle, paneLabelColor);

        SDL_SetRenderDrawColor(renderer, paneBorder.r, paneBorder.g, paneBorder.b, paneBorder.a);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.left_pane_rect);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.center_pane_rect);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.right_pane_rect);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.mode_router_rect);
        SDL_SetRenderDrawColor(renderer, paneBorder.r, paneBorder.g, paneBorder.b, 220);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.viewport_rect);
    }

    for (int i = 0; i < 3; i++) {
        bool selectable = contract.modeSelectable[i];
        bool active = (i == contract.activeMode);
        resolvedFill = selectable
                           ? SceneEditorResolveButtonFill(active ? modeActiveFill : modeInactiveFill,
                                                          disabledFill,
                                                          true,
                                                          SceneEditorButtonHovered(&modeSelectButtons[i]),
                                                          active)
                           : disabledFill;
        SDL_SetRenderDrawColor(renderer,
                               resolvedFill.r,
                               resolvedFill.g,
                               resolvedFill.b,
                               255);
        SDL_RenderFillRect(renderer, &modeSelectButtons[i]);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
        SDL_RenderDrawRect(renderer, &modeSelectButtons[i]);
        resolvedText = SceneEditorResolveButtonTextColor(resolvedFill, palette);
        RenderButtonTextWithColor(renderer,
                                  modeSelectButtons[i],
                                  SceneEditorModeLabel(i),
                                  resolvedText);
    }

    resolvedFill = SceneEditorResolveButtonFill(applyFill,
                                                disabledFill,
                                                contract.applyEnabled,
                                                SceneEditorButtonHovered(&applyButton),
                                                false);
    SDL_SetRenderDrawColor(renderer, resolvedFill.r, resolvedFill.g, resolvedFill.b, 255);
    SDL_RenderFillRect(renderer, &applyButton);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &applyButton);
    resolvedText = SceneEditorResolveButtonTextColor(resolvedFill, palette);
    RenderButtonTextWithColor(renderer, applyButton, contract.applyLabel, resolvedText);

    resolvedFill = SceneEditorResolveButtonFill(previewFill,
                                                disabledFill,
                                                contract.previewEnabled,
                                                SceneEditorButtonHovered(&previewButton),
                                                false);
    SDL_SetRenderDrawColor(renderer, resolvedFill.r, resolvedFill.g, resolvedFill.b, 255);
    SDL_RenderFillRect(renderer, &previewButton);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &previewButton);
    resolvedText = SceneEditorResolveButtonTextColor(resolvedFill, palette);
    RenderButtonTextWithColor(renderer, previewButton, contract.previewLabel, resolvedText);

    resolvedFill = SceneEditorResolveButtonFill(cycleFill,
                                                disabledFill,
                                                contract.cycleModeEnabled,
                                                SceneEditorButtonHovered(&changeModeButton),
                                                false);
    SDL_SetRenderDrawColor(renderer, resolvedFill.r, resolvedFill.g, resolvedFill.b, 255);
    SDL_RenderFillRect(renderer, &changeModeButton);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &changeModeButton);
    resolvedText = SceneEditorResolveButtonTextColor(resolvedFill, palette);
    RenderButtonTextWithColor(renderer, changeModeButton, contract.cycleModeLabel, resolvedText);

    resolvedFill = SceneEditorResolveButtonFill(saveFill,
                                                disabledFill,
                                                contract.saveEnabled,
                                                SceneEditorButtonHovered(&saveButton),
                                                false);
    SDL_SetRenderDrawColor(renderer, resolvedFill.r, resolvedFill.g, resolvedFill.b, 255);
    SDL_RenderFillRect(renderer, &saveButton);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &saveButton);
    resolvedText = SceneEditorResolveButtonTextColor(resolvedFill, palette);
    RenderButtonTextWithColor(renderer, saveButton, contract.saveLabel, resolvedText);

    resolvedFill = SceneEditorResolveButtonFill(backFill,
                                                disabledFill,
                                                contract.backToMenuEnabled,
                                                SceneEditorButtonHovered(&backToMenuButton),
                                                false);
    SDL_SetRenderDrawColor(renderer, resolvedFill.r, resolvedFill.g, resolvedFill.b, 255);
    SDL_RenderFillRect(renderer, &backToMenuButton);
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &backToMenuButton);
    resolvedText = SceneEditorResolveButtonTextColor(resolvedFill, palette);
    RenderButtonTextWithColor(renderer, backToMenuButton, contract.backToMenuLabel, resolvedText);

    showFeedback = (g_sceneActionFeedbackText[0] &&
                    g_sceneActionFeedbackUntilMs > SDL_GetTicks64());
    if (showFeedback) {
        feedbackRect = (SDL_Rect){
            g_scenePaneLayoutValid ? g_scenePaneLayout.right_content_rect.x : backToMenuButton.x,
            previewButton.y - 30,
            g_scenePaneLayoutValid ? g_scenePaneLayout.right_content_rect.w : backToMenuButton.w,
            24
        };
        resolvedText = palette.text_primary;
        RenderLabelText(renderer, feedbackRect, g_sceneActionFeedbackText, resolvedText);
    }

    SceneEditorSurfaceRenderLeftPaneContent(renderer, &g_scenePaneLayout, &contract, paneLabelColor, statusColor);
    SceneEditorSurfaceRenderRightPaneStatus(renderer,
                                            &g_scenePaneLayout,
                                            &contract,
                                            (showFeedback ? feedbackRect.y : previewButton.y) - 10,
                                            paneLabelColor,
                                            statusColor);
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
                RayTracingThemePalette palette = SceneEditorResolvePalette();
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

            SceneEditorRenderViewportLane(editor, SceneEditorResolveViewportRenderLane());
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
        RayTracingThemePalette palette = SceneEditorResolvePalette();
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

    SceneEditorRenderViewportLane(editor, SceneEditorResolveViewportRenderLane());
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
    return SceneEditorIsChromeShellButtonHit(mx, my);
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
                                                          FluidSceneLocksObjects());
    animSettings.editorMode = editor->currentMode;
    InitializeEditorMode(editor);
    printf("Switched to mode: %d\n", editor->currentMode);
}

// Set Scene Mode
void SetSceneMode(SceneEditor* editor, int mode) {
    if (mode >= 0 && mode <= 2) {
        editor->currentMode = EditorModeRouter_ClampEditorMode(mode, FluidSceneLocksObjects());
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
