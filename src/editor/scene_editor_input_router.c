#include "editor/scene_editor_input_router.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/bezier_editor.h"
#include "editor/camera_editor.h"
#include "editor/object_editor.h"

#define SCENE_EDITOR_MAX_QUEUED_CHROME_MUTATIONS 32
#define SCENE_EDITOR_MAX_QUEUED_PANE_MUTATIONS 160
#define SCENE_EDITOR_MAX_QUEUED_MUTATION_ORDER 192

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

static void scene_editor_input_routing_result_reset(SceneEditorInputRoutingResult* result) {
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->target = SCENE_EDITOR_INPUT_TARGET_NONE;
}

static void scene_editor_input_normalized_reset(SceneEditorInputNormalized* normalized) {
    if (!normalized) return;
    memset(normalized, 0, sizeof(*normalized));
    normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IGNORED;
    normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_NONE;
    normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_NONE;
    normalized->event = NULL;
}

static bool scene_editor_mutation_queue_append_ordered_ref(SceneEditorMutationLane lane,
                                                           uint16_t lane_index) {
    if (g_scene_mutation_queue.order_count >= SCENE_EDITOR_MAX_QUEUED_MUTATION_ORDER) {
        g_scene_mutation_queue.dropped_order_count += 1u;
        return false;
    }
    g_scene_mutation_queue.ordered_refs[g_scene_mutation_queue.order_count].lane = lane;
    g_scene_mutation_queue.ordered_refs[g_scene_mutation_queue.order_count].lane_index = lane_index;
    g_scene_mutation_queue.order_count += 1u;
    return true;
}

static bool scene_editor_mutation_queue_enqueue_chrome_action(const SceneEditorChromeAction* action) {
    uint16_t lane_index = 0;
    if (!action || action->kind == SCENE_EDITOR_CHROME_ACTION_NONE) return false;
    if (g_scene_mutation_queue.chrome_count >= SCENE_EDITOR_MAX_QUEUED_CHROME_MUTATIONS) {
        g_scene_mutation_queue.dropped_chrome_count += 1u;
        return false;
    }
    lane_index = g_scene_mutation_queue.chrome_count;
    g_scene_mutation_queue.chrome_actions[lane_index] = *action;
    g_scene_mutation_queue.chrome_count += 1u;
    if (!scene_editor_mutation_queue_append_ordered_ref(SCENE_EDITOR_MUTATION_LANE_CHROME, lane_index)) {
        g_scene_mutation_queue.chrome_count -= 1u;
        g_scene_mutation_queue.dropped_chrome_count += 1u;
        return false;
    }
    return true;
}

static bool scene_editor_mutation_queue_enqueue_pane_command(const SceneEditorPaneCommand* command) {
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
    if (!scene_editor_mutation_queue_append_ordered_ref(SCENE_EDITOR_MUTATION_LANE_PANE, lane_index)) {
        g_scene_mutation_queue.pane_count -= 1u;
        g_scene_mutation_queue.dropped_pane_count += 1u;
        return false;
    }
    return true;
}

static bool scene_editor_input_diag_enabled(void) {
    const char* value = getenv("RAY_TRACING_EDITOR_INPUT_DIAG");
    if (!value || !value[0]) return false;
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

static bool scene_editor_is_text_zoom_shortcut_key(SDL_Keycode key, SDL_Keymod mod) {
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

static bool scene_editor_resolve_theme_shortcut(SDL_Keycode key,
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

static void scene_editor_input_diag_maybe_emit(const SDL_Event* event,
                                               const SceneEditorInputDiagFrame* diag,
                                               const SceneEditorInputRoutingResult* route) {
    if (!event || !diag || !route) return;
    if (!scene_editor_input_diag_enabled()) return;
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

static bool scene_editor_resolve_pane_command(SceneEditorInputTarget target,
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

static SceneEditorPaneHitRegion scene_editor_resolve_pane_hit_region(SceneEditorInputTarget target,
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
        if (hit == CAMERA_EDITOR_HIT_CONTROLS) {
            return SCENE_EDITOR_PANE_HIT_CONTROLS;
        }
        return SCENE_EDITOR_PANE_HIT_CANVAS;
    }

    return SCENE_EDITOR_PANE_HIT_NONE;
}

static void scene_editor_normalize_input(const SceneEditorInputRouterCallbacks* callbacks,
                                         const SDL_Event* event,
                                         SceneEditorInputNormalized* normalized) {
    bool cycle_next = false;
    if (!callbacks || !event || !normalized) return;

    scene_editor_input_normalized_reset(normalized);
    normalized->event = event;
    if (callbacks->event_matches_editor_window &&
        !callbacks->event_matches_editor_window(callbacks->context, event)) {
        return;
    }

    if (event->type == SDL_QUIT ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE)) {
        normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
        normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
        normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
        return;
    }

    if (event->type == SDL_KEYDOWN) {
        if ((callbacks->should_route_global_key &&
             callbacks->should_route_global_key(callbacks->context, event)) ||
            scene_editor_resolve_theme_shortcut(event->key.keysym.sym,
                                                event->key.keysym.mod,
                                                &cycle_next) ||
            scene_editor_is_text_zoom_shortcut_key(event->key.keysym.sym, event->key.keysym.mod)) {
            normalized->action_class = SCENE_EDITOR_INPUT_ACTION_IMMEDIATE;
            normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_GLOBAL;
            normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_SYSTEM;
            return;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT &&
        callbacks->is_chrome_hit &&
        callbacks->is_chrome_hit(callbacks->context, event->button.x, event->button.y)) {
        normalized->action_class = SCENE_EDITOR_INPUT_ACTION_QUEUED;
        normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_CHROME;
        normalized->target_hint = SCENE_EDITOR_INPUT_TARGET_CHROME;
        return;
    }

    if (callbacks->resolve_pane_target) {
        normalized->target_hint = callbacks->resolve_pane_target(callbacks->context);
    }
    if (normalized->target_hint != SCENE_EDITOR_INPUT_TARGET_NONE &&
        (event->type == SDL_MOUSEBUTTONDOWN ||
         event->type == SDL_MOUSEBUTTONUP ||
         event->type == SDL_MOUSEMOTION ||
         event->type == SDL_MOUSEWHEEL ||
         event->type == SDL_KEYDOWN)) {
        normalized->action_class = SCENE_EDITOR_INPUT_ACTION_QUEUED;
        normalized->route_policy = SCENE_EDITOR_INPUT_ROUTE_POLICY_ACTIVE_PANE;
    }
}

void SceneEditorInputRouterReset(void) {
    memset(&g_scene_mutation_queue, 0, sizeof(g_scene_mutation_queue));
}

void SceneEditorInputRouterHandleEvent(SDL_Event* event,
                                       const SceneEditorInputRouterCallbacks* callbacks) {
    SceneEditorInputNormalized normalized;
    SceneEditorInputDiagFrame diag;
    SceneEditorInputRoutingResult route;

    if (!event || !callbacks) return;

    memset(&diag, 0, sizeof(diag));
    diag.raw_event_count = 1u;
    scene_editor_input_normalized_reset(&normalized);
    scene_editor_normalize_input(callbacks, event, &normalized);

    scene_editor_input_routing_result_reset(&route);
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
        if (callbacks->handle_system_input &&
            callbacks->handle_system_input(callbacks->context, event, &route)) {
            diag.routed_global_count += 1u;
        }
    } else if (normalized.route_policy == SCENE_EDITOR_INPUT_ROUTE_POLICY_CHROME) {
        SceneEditorChromeAction action = {0};
        if (callbacks->resolve_chrome_action &&
            callbacks->resolve_chrome_action(callbacks->context, event, &action) &&
            scene_editor_mutation_queue_enqueue_chrome_action(&action)) {
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
        SceneEditorPaneHitRegion pane_hit = scene_editor_resolve_pane_hit_region(normalized.target_hint, event);
        SceneEditorPaneCommand pane_command = {0};
        if (scene_editor_resolve_pane_command(normalized.target_hint, pane_hit, event, &pane_command) &&
            scene_editor_mutation_queue_enqueue_pane_command(&pane_command)) {
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

    if (callbacks->apply_invalidation) {
        callbacks->apply_invalidation(callbacks->context, &route);
    }
    if (route.requested_target_invalidation) {
        diag.target_invalidation_count += 1u;
    }
    if (route.requested_full_invalidation) {
        diag.full_invalidation_count += 1u;
    }
    scene_editor_input_diag_maybe_emit(event, &diag, &route);
}

void SceneEditorInputRouterDrain(const SceneEditorInputRouterCallbacks* callbacks) {
    uint16_t i = 0;
    if (!callbacks) return;
    for (i = 0; i < g_scene_mutation_queue.order_count; ++i) {
        SceneEditorQueuedMutationRef* ref = &g_scene_mutation_queue.ordered_refs[i];
        if (ref->lane == SCENE_EDITOR_MUTATION_LANE_CHROME) {
            if (ref->lane_index < g_scene_mutation_queue.chrome_count && callbacks->apply_chrome_action) {
                callbacks->apply_chrome_action(callbacks->context,
                                               &g_scene_mutation_queue.chrome_actions[ref->lane_index]);
                if (callbacks->should_stop_processing && callbacks->should_stop_processing(callbacks->context)) {
                    break;
                }
            }
            continue;
        }
        if (ref->lane == SCENE_EDITOR_MUTATION_LANE_PANE) {
            if (ref->lane_index < g_scene_mutation_queue.pane_count && callbacks->route_pane_event) {
                SceneEditorQueuedPaneCommand* queued = &g_scene_mutation_queue.pane_commands[ref->lane_index];
                SceneEditorPaneCommand command = {0};
                SceneEditorInputRoutingResult route = {0};
                command.kind = queued->kind;
                command.target = queued->target;
                command.pane_hit_region = queued->pane_hit_region;
                command.event = &queued->event_copy;
                scene_editor_input_routing_result_reset(&route);
                callbacks->route_pane_event(callbacks->context, &command, &route);
                if (callbacks->should_stop_processing && callbacks->should_stop_processing(callbacks->context)) {
                    break;
                }
            }
            continue;
        }
    }
    SceneEditorInputRouterReset();
}
