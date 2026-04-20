// scene_editor.c  
#include "editor/scene_editor.h"
#include "editor/bezier_editor.h"
#include "editor/object_editor.h"   //  Required for object editing
#include "editor/camera_editor.h"   //  Required for camera adjustments
#include "config/config_manager.h"  //  Required for loading/saving scene settings
#include "scene/object_manager.h"
#include "app/animation.h"
#include "render/fluid/fluid_state.h"
#include "render/ray_tracing_mode_backend.h"
#include "camera/camera.h"
#include "editor/editor_mode_router.h"
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

#if USE_VULKAN
static VkRenderer g_scene_renderer_storage;
#endif

bool sceneEditorExitFlag = false;  //  Used to signal Scene Editor should exit
static void InitializeEditorMode(SceneEditor* editor);
static void SceneEditorLayoutChrome(void);
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
    SCENE_EDITOR_CHROME_ACTION_APPLY
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
} SceneEditorViewportNavState;

static SceneEditorViewportNavState g_viewport_nav_state = {false, 0, 0};

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

static bool SceneEditorModeSelectable(int mode_index) {
    if (mode_index == 1 && FluidSceneLocksObjects()) {
        return false;
    }
    return mode_index >= 0 && mode_index <= 2;
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
    double orbit_delta = (double)dx * 0.010 + (double)dy * 0.003;
    if (fabs(orbit_delta) <= 1e-9) {
        return;
    }
    CameraRotate(&sceneSettings.camera, orbit_delta);
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

static bool SceneEditorHandleViewportNavigation(SceneEditor* editor,
                                                const SceneEditorPaneCommand* command,
                                                SceneEditorInputRoutingResult* result) {
    SDL_Event* event = NULL;
    if (!editor || !command || !result) return false;
    event = command->event;
    if (!event) return false;
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_f) {
        result->consumed = SceneEditorViewportFrameToScene();
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
        if (alt_down) {
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
        if (wheel_y != 0) {
            SDL_GetMouseState(&mx, &my);
            SceneEditorViewportZoomTowardScreenPoint(mx, my, wheel_y);
            result->consumed = true;
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
    if (!editor || !event || !result) return false;
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
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_TAB) {
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
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
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
    if (!event || !out_action) return false;
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
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_PREVIEW;
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &changeModeButton)) {
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_CYCLE_MODE;
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &applyButton)) {
        out_action->kind = SCENE_EDITOR_CHROME_ACTION_APPLY;
        return true;
    }
    return false;
}

static void SceneEditorApplyChromeAction(SceneEditor* editor, const SceneEditorChromeAction* action) {
    if (!editor || !action || action->kind == SCENE_EDITOR_CHROME_ACTION_NONE) return;
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_MODE_SELECT) {
        if (SceneEditorModeSelectable(action->mode_index)) {
            int clamped_mode = EditorModeRouter_ClampEditorMode(action->mode_index, FluidSceneLocksObjects());
            editor->currentMode = clamped_mode;
            animSettings.editorMode = clamped_mode;
            InitializeEditorMode(editor);
            SaveAllSettings();
            LoadAllSettings();
            printf("Changed Mode to %d via mode router\n", editor->currentMode);
        } else {
            printf("Mode %d currently unavailable in this scene source.\n", action->mode_index);
        }
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_PREVIEW) {
        RunPreviewModeEmbedded();
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_CYCLE_MODE) {
        editor->currentMode = EditorModeRouter_NextEditorMode(editor->currentMode,
                                                              false,
                                                              FluidSceneLocksObjects());
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
        SaveAllSettings();
        LoadAllSettings();
        printf("Changed Mode to %d\n", editor->currentMode);
        return;
    }
    if (action->kind == SCENE_EDITOR_CHROME_ACTION_APPLY) {
        SaveAllSettings();
        editor->running = false;
        sceneEditorExitFlag = true;
        printf("Changed Mode to %d\n", editor->currentMode);
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

static void SceneEditorRoutePaneEvent(SceneEditor* editor,
                                      const SceneEditorPaneCommand* command,
                                      SceneEditorInputRoutingResult* result) {
    if (!editor || !command || !command->event || !result) return;
    if (SceneEditorHandleViewportNavigation(editor, command, result)) {
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

    if (event->type == SDL_QUIT || SceneEditorIsOwnWindowCloseEvent(editor, event)) {
        normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
        normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
        normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
        return;
    }

    if (event->type == SDL_KEYDOWN) {
        bool cycle_next = false;
        if (event->key.keysym.sym == SDLK_TAB) {
            normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
            normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
            normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
            return;
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
    return false;
}

static void SceneEditorLayoutFallback(void) {
    int width = sceneSettings.windowWidth;
    int height = sceneSettings.windowHeight;
    int compactButtonWidth = SceneEditorMeasureButtonWidth("Delete", 70);
    int compactButtonHeight = SceneEditorMeasureButtonHeight(40);
    int footerButtonHeight = SceneEditorMeasureButtonHeight(50);
    int applyWidth = SceneEditorMeasureButtonWidth("Apply", 150);
    int previewWidth = SceneEditorMeasureButtonWidth("Preview", 150);
    int changeModeWidth = SceneEditorMeasureButtonWidth("Change Mode", 130);

    addButton = (SDL_Rect){width - compactButtonWidth - 20, 20, compactButtonWidth, compactButtonHeight};
    deleteButton = (SDL_Rect){width - compactButtonWidth - 20,
                              20 + compactButtonHeight + 20,
                              compactButtonWidth,
                              compactButtonHeight};
    toggleButton = (SDL_Rect){width - compactButtonWidth - 20,
                              20 + (compactButtonHeight + 20) * 2,
                              compactButtonWidth,
                              compactButtonHeight};

    applyButton = (SDL_Rect){width - applyWidth - 30,
                             height - footerButtonHeight - 30,
                             applyWidth,
                             footerButtonHeight};
    previewButton = (SDL_Rect){applyButton.x - previewWidth - 20,
                               applyButton.y,
                               previewWidth,
                               footerButtonHeight};
    changeModeButton = (SDL_Rect){width - changeModeWidth - 30,
                                  applyButton.y - footerButtonHeight - 12,
                                  changeModeWidth,
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
}

static void SceneEditorLayoutChrome(void) {
    const SceneEditorPaneLayout* layout = NULL;
    int compactButtonWidth = 0;
    int compactButtonHeight = 0;
    int actionButtonHeight = 0;
    int actionButtonWidth = 0;
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
    actionButtonWidth = SceneEditorMeasureButtonWidth("Change Mode", 146);

    left = g_scenePaneLayout.left_content_rect;
    right = g_scenePaneLayout.right_content_rect;
    modeRect = g_scenePaneLayout.mode_router_rect;

    addButton = (SDL_Rect){left.x, left.y, compactButtonWidth, compactButtonHeight};
    deleteButton = (SDL_Rect){left.x,
                              addButton.y + compactButtonHeight + buttonGap,
                              compactButtonWidth,
                              compactButtonHeight};
    toggleButton = (SDL_Rect){left.x,
                              deleteButton.y + compactButtonHeight + buttonGap,
                              compactButtonWidth,
                              compactButtonHeight};

    applyButton = (SDL_Rect){right.x + right.w - actionButtonWidth,
                             right.y + right.h - actionButtonHeight,
                             actionButtonWidth,
                             actionButtonHeight};
    previewButton = (SDL_Rect){applyButton.x,
                               applyButton.y - actionButtonHeight - buttonGap,
                               actionButtonWidth,
                               actionButtonHeight};
    changeModeButton = (SDL_Rect){applyButton.x,
                                  previewButton.y - actionButtonHeight - buttonGap,
                                  actionButtonWidth,
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
    InitializeEditorMode(editor);
    UpdateObjects();
    sceneEditorExitFlag = false;
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
    char statusSource[128];
    char statusPath[256];
    char statusObjects[128];
    char statusSpace[128];
    char statusDigest[256];
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
    SDL_Color modeActiveFill = palette.button_active_fill;
    SDL_Color previewFill = palette.button_fill;
    SDL_Color cycleFill = palette.button_active_fill;
    SDL_Color applyFill = palette.accent_primary;
    SDL_Color borderColor = (SDL_Color){
        SceneEditorColorOffset(palette.panel_border.r, -30),
        SceneEditorColorOffset(palette.panel_border.g, -30),
        SceneEditorColorOffset(palette.panel_border.b, -30),
        255
    };
    SDL_Rect modeTitleRect = {0};
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RayTracingSceneDigestStatus digestStatus = RayTracingModeBackend_BuildSceneDigestStatus(&route);

    if (!renderer) return;

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
        RenderLabelText(renderer, modeTitleRect, "Left Pane: Editor Controls", paneLabelColor);

        modeTitleRect = (SDL_Rect){
            g_scenePaneLayout.center_pane_rect.x + 10,
            g_scenePaneLayout.center_pane_rect.y + 6,
            g_scenePaneLayout.center_pane_rect.w - 20,
            20
        };
        RenderLabelText(renderer, modeTitleRect, "Center Pane: Viewport", paneLabelColor);

        modeTitleRect = (SDL_Rect){
            g_scenePaneLayout.right_pane_rect.x + 10,
            g_scenePaneLayout.right_pane_rect.y + 6,
            g_scenePaneLayout.right_pane_rect.w - 20,
            20
        };
        RenderLabelText(renderer, modeTitleRect, "Right Pane: Program / Scene", paneLabelColor);

        SDL_SetRenderDrawColor(renderer, paneBorder.r, paneBorder.g, paneBorder.b, paneBorder.a);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.left_pane_rect);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.center_pane_rect);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.right_pane_rect);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.mode_router_rect);
        SDL_SetRenderDrawColor(renderer, paneBorder.r, paneBorder.g, paneBorder.b, 220);
        SDL_RenderDrawRect(renderer, &g_scenePaneLayout.viewport_rect);
    }

    for (int i = 0; i < 3; i++) {
        bool selectable = SceneEditorModeSelectable(i);
        bool active = (i == EditorModeRouter_ClampEditorMode(animSettings.editorMode, FluidSceneLocksObjects()));
        SDL_SetRenderDrawColor(renderer,
                               selectable ? (active ? modeActiveFill.r : modeInactiveFill.r) : paneFill.r,
                               selectable ? (active ? modeActiveFill.g : modeInactiveFill.g) : paneFill.g,
                               selectable ? (active ? modeActiveFill.b : modeInactiveFill.b) : paneFill.b,
                               255);
        SDL_RenderFillRect(renderer, &modeSelectButtons[i]);
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
        SDL_RenderDrawRect(renderer, &modeSelectButtons[i]);
        RenderButtonText(renderer, modeSelectButtons[i], SceneEditorModeLabel(i));
    }

    SDL_SetRenderDrawColor(renderer, applyFill.r, applyFill.g, applyFill.b, 255);
    SDL_RenderFillRect(renderer, &applyButton);
    SDL_SetRenderDrawColor(renderer, previewFill.r, previewFill.g, previewFill.b, 255);
    SDL_RenderFillRect(renderer, &previewButton);
    SDL_SetRenderDrawColor(renderer, cycleFill.r, cycleFill.g, cycleFill.b, 255);
    SDL_RenderFillRect(renderer, &changeModeButton);

    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &applyButton);
    RenderButtonText(renderer, applyButton, "Apply");
    SDL_RenderDrawRect(renderer, &previewButton);
    RenderButtonText(renderer, previewButton, "Preview");
    SDL_RenderDrawRect(renderer, &changeModeButton);
    RenderButtonText(renderer, changeModeButton, "Cycle Mode");

    snprintf(statusSource, sizeof(statusSource), "Source: %s", SceneEditorSourceLabel());
    snprintf(statusPath, sizeof(statusPath), "Path: %s", SceneEditorSourcePath());
    snprintf(statusObjects, sizeof(statusObjects), "Objects: %d  Active: %s",
             sceneSettings.objectCount,
             SceneEditorModeLabel(EditorModeRouter_ClampEditorMode(animSettings.editorMode,
                                                                   FluidSceneLocksObjects())));
    snprintf(statusSpace, sizeof(statusSpace), "Route: %s", RayTracingModeBackend_Name(&route));
    if (!RayTracingModeBackend_IsCompat3DFallback(&route)) {
        snprintf(statusDigest, sizeof(statusDigest), "Digest: n/a (2D lane)");
    } else if (!digestStatus.valid) {
        snprintf(statusDigest, sizeof(statusDigest), "Digest: pending runtime 3D payload");
    } else {
        const char *bounds_state = (digestStatus.hasSceneBounds && digestStatus.boundsEnabled) ? "on" : "off";
        const char *plane_axis = digestStatus.constructionPlaneAxis[0] ?
            digestStatus.constructionPlaneAxis : "?";
        snprintf(statusDigest,
                 sizeof(statusDigest),
                 "Digest: prim=%d p=%d r=%d b=%s plane=%s@%.2f scaf=%d",
                 digestStatus.digestPrimitiveCount,
                 digestStatus.planePrimitiveCount,
                 digestStatus.rectPrismPrimitiveCount,
                 bounds_state,
                 plane_axis,
                 digestStatus.constructionPlaneOffset,
                 digestStatus.scaffoldPrimitiveCount);
    }

    RenderLabelText(renderer, g_sceneStatusTitleRect, "ray_tracing Scene Editor", paneLabelColor);
    RenderLabelText(renderer, g_sceneStatusSourceRect, statusSource, statusColor);
    RenderLabelText(renderer, g_sceneStatusPathRect, statusPath, statusColor);
    RenderLabelText(renderer, g_sceneStatusObjectsRect, statusObjects, statusColor);
    RenderLabelText(renderer, g_sceneStatusSpaceRect, statusSpace, statusColor);
    RenderLabelText(renderer, g_sceneStatusDigestRect, statusDigest, statusColor);
}

void SceneEditorLoop(SceneEditor* editor) {
    SDL_Event event;

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
        SceneEditorSyncWindowSize(editor);
        while (SDL_PollEvent(&event)) {
            HandleSceneEditorEvents(editor, &event);
            if (sceneEditorExitFlag)
                break;
        }
        SceneEditorDrainMutationQueue(editor);
        if (sceneEditorExitFlag || !editor->running) {
            break;
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
            SDL_Delay(10);
            continue;
        }

        // **Render Active Editor Mode**
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
        }
        RenderFluidBounds(editor->renderer);
        RenderSceneButtons(editor->renderer);

        render_end_frame();
        SDL_Delay(16);  // Maintain ~60 FPS
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
    RenderFluidBounds(editor->renderer);
    RenderSceneButtons(editor->renderer);
    render_end_frame();
}

bool SceneEditorSessionWantsExit(const SceneEditor* editor) {
    if (!editor) {
        return true;
    }
    return sceneEditorExitFlag || !editor->running;
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
    SceneEditorMutationQueue_Reset();
    sceneEditorExitFlag = false;
    setRenderContext(NULL, NULL, 0, 0);
    SaveAllSettings();
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
