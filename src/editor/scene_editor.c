// scene_editor.c  
#include "editor/scene_editor.h"
#include "editor/bezier_editor.h"
#include "editor/object_editor.h"   //  Required for object editing
#include "editor/camera_editor.h"   //  Required for camera adjustments
#include "config/config_manager.h"  //  Required for loading/saving scene settings
#include "scene/object_manager.h"
#include "app/animation.h"
#include "render/fluid_state.h"
#include "camera/camera.h"
#include "editor/editor_mode_router.h"
#include "engine/Render/render_pipeline.h"
#include "render/vk_shared_device.h"
#include "ui/text_zoom_shortcuts.h"

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

#if USE_VULKAN
static VkRenderer g_scene_renderer_storage;
#endif

bool sceneEditorExitFlag = false;  //  Used to signal Scene Editor should exit
static void InitializeEditorMode(SceneEditor* editor);

static bool FluidSceneLocksObjects(void) {
    return AnimationUseFluidScene();
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

typedef struct SceneEditorPaneCommand {
    SceneEditorPaneCommandKind kind;
    SceneEditorInputTarget target;
    SceneEditorPaneHitRegion pane_hit_region;
    SDL_Event* event;
} SceneEditorPaneCommand;

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
    if (event->type == SDL_QUIT ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE)) {
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
    if (event->type == SDL_KEYDOWN) {
        bool changed = false;
        int zoom_step = 0;
        int zoom_percent = 100;
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

static bool SceneEditorHandleChromeInput(SceneEditor* editor,
                                         SDL_Event* event,
                                         SceneEditorInputRoutingResult* result) {
    if (!editor || !event || !result) return false;
    if (event->type != SDL_MOUSEBUTTONDOWN) return false;

    const int mx = event->button.x;
    const int my = event->button.y;

    if (SceneEditorPointInRect(mx, my, &previewButton)) {
        RunPreviewModeEmbedded();
        result->target = SCENE_EDITOR_INPUT_TARGET_CHROME;
        result->consumed = true;
        result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_UI;
        result->requested_target_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_UI;
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &changeModeButton)) {
        editor->currentMode = EditorModeRouter_NextEditorMode(editor->currentMode,
                                                              false,
                                                              FluidSceneLocksObjects());
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
        SaveAllSettings();
        LoadAllSettings();
        printf("Changed Mode to %d\n", editor->currentMode);
        result->target = SCENE_EDITOR_INPUT_TARGET_CHROME;
        result->consumed = true;
        result->invalidation_class = SCENE_EDITOR_INVALIDATION_TARGET_UI;
        result->requested_target_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_UI;
        return true;
    }
    if (SceneEditorPointInRect(mx, my, &applyButton)) {
        SaveAllSettings();
        editor->running = false;
        sceneEditorExitFlag = true;
        printf("Changed Mode to %d\n", editor->currentMode);
        result->target = SCENE_EDITOR_INPUT_TARGET_CHROME;
        result->consumed = true;
        result->invalidation_class = SCENE_EDITOR_INVALIDATION_FULL_EXIT;
        result->requested_full_invalidation = true;
        result->invalidation_reason_bits |= SCENE_EDITOR_INVALIDATE_REASON_EXIT;
        return true;
    }
    return false;
}

static void SceneEditorRoutePaneEvent(SceneEditor* editor,
                                      const SceneEditorPaneCommand* command,
                                      SceneEditorInputRoutingResult* result) {
    if (!editor || !command || !command->event || !result) return;
    switch (command->target) {
        case SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE:
            HandleBezierEditorEvents(command->event, &draggingPoint, &draggingVelocity);
            result->consumed = true;
            break;
        case SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE:
            HandleObjectEditorEvents(command->event);
            result->consumed = true;
            break;
        case SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE:
            HandleCameraEditorEvents(command->event);
            result->consumed = true;
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

    if (event->type == SDL_QUIT ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE)) {
        normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
        normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
        normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
        return;
    }

    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_TAB) {
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

    if (event->type == SDL_MOUSEBUTTONDOWN) {
        const int mx = event->button.x;
        const int my = event->button.y;
        if (SceneEditorPointInRect(mx, my, &previewButton) ||
            SceneEditorPointInRect(mx, my, &changeModeButton) ||
            SceneEditorPointInRect(mx, my, &applyButton)) {
            normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
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
        normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
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
        if (SceneEditorHandleChromeInput(editor, event, &route)) {
            diag.routed_chrome_count += 1u;
        }
    } else if (normalized.route_policy == SCENE_EDITOR_INPUT_ROUTE_POLICY_ACTIVE_PANE) {
        SceneEditorPaneHitRegion pane_hit = SceneEditorResolvePaneHitRegion(normalized.target_hint, event);
        SceneEditorPaneCommand pane_command;
        if (SceneEditorResolvePaneCommand(normalized.target_hint, pane_hit, event, &pane_command)) {
            SceneEditorRoutePaneEvent(editor, &pane_command, &route);
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
    const char* font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
    int point_size = animation_config_scale_text_point_size(&animSettings, 24, 12);
    int text_w = 0;
    int text_h = 0;
    TTF_Font* font = NULL;
    if (!label || !label[0]) return min_width;
    font = TTF_OpenFont(font_path, point_size);
    if (!font) return min_width;
    if (TTF_SizeUTF8(font, label, &text_w, &text_h) != 0) {
        TTF_CloseFont(font);
        return min_width;
    }
    TTF_CloseFont(font);
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

void InitializeSceneEditor(SceneEditor* editor) {
    LoadAnimationConfig();
    //  Load all scene configurations (window size, objects, paths)
    LoadSceneConfig();
    if (animSettings.useFluidScene && animSettings.fluidManifest[0]) {
        if (!AnimationApplyFluidScene(animSettings.fluidManifest)) {
            fprintf(stderr, "[editor] failed to apply fluid scene: %s\n", animSettings.fluidManifest);
        }
    } else {
        AnimationClearFluidGrid();
    }
    if (animSettings.editorMode < 0)
        animSettings.editorMode = 0;
    editor->currentMode = EditorModeRouter_ClampEditorMode(animSettings.editorMode % 3,
                                                           FluidSceneLocksObjects());

    //  Create the window using stored scene settings
    editor->window = SDL_CreateWindow("Scene Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      sceneSettings.windowWidth, sceneSettings.windowHeight,
                                      SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!editor->window) {
        fprintf(stderr, "Error: Failed to create scene window.\n");
        return;
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
        return;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        fprintf(stderr, "vk_shared_device_get failed.\n");
        SDL_DestroyWindow(editor->window);
        return;
    }

    VkResult init = vk_renderer_init_with_device(&g_scene_renderer_storage, shared_device, editor->window, &cfg);
    if (init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed: %d\n", init);
        SDL_DestroyWindow(editor->window);
        return;
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
        return;
    }
#endif

    //  Initialize TTF for font rendering
    if (TTF_Init() == -1) {
        fprintf(stderr, "Error: TTF_Init failed: %s\n", TTF_GetError());
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)editor->renderer);
        vk_renderer_shutdown_surface((VkRenderer*)editor->renderer);
#else
        SDL_DestroyRenderer(editor->renderer);
#endif
        SDL_DestroyWindow(editor->window);
        return;
    }
    // **Initialize Button Positions Based on Window Size**
    int width = sceneSettings.windowWidth;
    int height = sceneSettings.windowHeight;
    int compactButtonWidth = 70;
    int compactButtonHeight = SceneEditorMeasureButtonHeight(40);
    int footerButtonHeight = SceneEditorMeasureButtonHeight(50);
    int applyWidth = SceneEditorMeasureButtonWidth("Apply", 150);
    int previewWidth = SceneEditorMeasureButtonWidth("Preview", 150);
    int changeModeWidth = SceneEditorMeasureButtonWidth("Change Mode", 130);
    addButton = (SDL_Rect){width - compactButtonWidth - 20, 20, compactButtonWidth, compactButtonHeight};
    deleteButton = (SDL_Rect){width - compactButtonWidth - 20, 20 + compactButtonHeight + 20,
                              compactButtonWidth, compactButtonHeight};
    toggleButton = (SDL_Rect){width - compactButtonWidth - 20, 20 + (compactButtonHeight + 20) * 2,
                              compactButtonWidth, compactButtonHeight};
    
    applyButton = (SDL_Rect){width - applyWidth - 30, height - footerButtonHeight - 30,
                             applyWidth, footerButtonHeight};  // Bottom-right apply button
    previewButton = (SDL_Rect){applyButton.x - previewWidth - 20, applyButton.y,
                               previewWidth, footerButtonHeight}; // Left of apply
    changeModeButton = (SDL_Rect){width - changeModeWidth - 30,
                                  applyButton.y - footerButtonHeight - 12,
                                  changeModeWidth, footerButtonHeight};

    InitializeEditorMode(editor);


    UpdateObjects();
    printf("Scene Editor Initialized. Window Size: %dx%d\n", sceneSettings.windowWidth, 
		sceneSettings.windowHeight);
}

void RenderSceneButtons(SDL_Renderer* renderer) {
    //Individual Buttons
    SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
    SDL_RenderFillRect(renderer, &applyButton);
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderFillRect(renderer, &previewButton);
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_RenderFillRect(renderer, &changeModeButton);
    
    // Outlines and Text
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &applyButton);
    RenderButtonText(renderer, applyButton, "Apply");
    SDL_RenderDrawRect(renderer, &previewButton);
    RenderButtonText(renderer, previewButton, "Preview");
    SDL_RenderDrawRect(renderer, &changeModeButton);
    RenderButtonText(renderer, changeModeButton, "Change Mode");

    if (EditorModeRouter_IsControlled3D()) {
        SDL_Rect hintRect = {
            20,
            changeModeButton.y - 28,
            sceneSettings.windowWidth - 40,
            22
        };
        SDL_Color hintColor = {255, 220, 140, 255};
        SDL_SetRenderDrawColor(renderer, 30, 30, 38, 220);
        SDL_RenderFillRect(renderer, &hintRect);
        SDL_SetRenderDrawColor(renderer, 90, 90, 110, 255);
        SDL_RenderDrawRect(renderer, &hintRect);
        RenderLabelText(renderer, hintRect, EditorModeRouter_RuntimeHintLabel(), hintColor);
    }
}

void SceneEditorLoop(SceneEditor* editor) {
    SDL_Event event;
    sceneEditorExitFlag = false;

    while (editor->running && !sceneEditorExitFlag) {
        while (SDL_PollEvent(&event)) {
            HandleSceneEditorEvents(editor, &event);
            if (sceneEditorExitFlag)
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
        render_set_clear_color(editor->renderer, 0, 0, 0, 255);
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


void HandleSceneEditorEvents(SceneEditor* editor, SDL_Event* event) {
    SceneEditorHandleInputRouted(editor, event);
}

bool IsClickingButtonMain(int mx, int my) {
    // Check if click is within main buttons
    if ((mx >= applyButton.x && mx <= applyButton.x + applyButton.w && my >= applyButton.y 
                && my <= applyButton.y + applyButton.h) ||
        (mx >= previewButton.x && mx <= previewButton.x + previewButton.w && my >= previewButton.y
                && my <= previewButton.y + previewButton.h) ||
	(mx >= changeModeButton.x && mx <= changeModeButton.x + changeModeButton.w &&
            my >= changeModeButton.y && my <= changeModeButton.y + changeModeButton.h)) {
	return true;  // Click is inside a UI button
    }


    if ((mx >= addButton.x && mx <= addButton.x + addButton.w && my >= addButton.y 
		&& my <= addButton.y + addButton.h) ||
        (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w && my >= deleteButton.y 
		&& my <= deleteButton.y + deleteButton.h) ||  
        (mx >= toggleButton.x && mx <= toggleButton.x + toggleButton.w && my >= toggleButton.y 
		&& my <= toggleButton.y+ toggleButton.h)) {
        return true;  // Click is inside a UI button
    }
         
    return false;  // Click is not inside a UI button
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
    if (editor->renderer) {
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)editor->renderer);
        vk_renderer_shutdown_surface((VkRenderer*)editor->renderer);
#else
        SDL_DestroyRenderer(editor->renderer);
#endif
        editor->renderer = NULL;
    }
    if (editor->window) {
        SDL_DestroyWindow(editor->window);
        editor->window = NULL;
    }
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
