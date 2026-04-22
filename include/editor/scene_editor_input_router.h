#ifndef SCENE_EDITOR_INPUT_ROUTER_H
#define SCENE_EDITOR_INPUT_ROUTER_H

#include <stdbool.h>
#include <stdint.h>

#include <SDL2/SDL.h>

typedef enum SceneEditorInputTarget {
    SCENE_EDITOR_INPUT_TARGET_NONE = 0,
    SCENE_EDITOR_INPUT_TARGET_SYSTEM,
    SCENE_EDITOR_INPUT_TARGET_CHROME,
    SCENE_EDITOR_INPUT_TARGET_BEZIER_PANE,
    SCENE_EDITOR_INPUT_TARGET_OBJECT_PANE,
    SCENE_EDITOR_INPUT_TARGET_CAMERA_PANE
} SceneEditorInputTarget;

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

enum {
    SCENE_EDITOR_INVALIDATE_REASON_UI = 1u << 0,
    SCENE_EDITOR_INVALIDATE_REASON_PANE = 1u << 1,
    SCENE_EDITOR_INVALIDATE_REASON_EXIT = 1u << 2,
    SCENE_EDITOR_INVALIDATE_REASON_PANE_CONTROLS = 1u << 3,
    SCENE_EDITOR_INVALIDATE_REASON_PANE_CANVAS = 1u << 4,
    SCENE_EDITOR_INVALIDATE_REASON_PANE_DRAG = 1u << 5
};

typedef struct SceneEditorInputRoutingResult {
    SceneEditorInputTarget target;
    bool consumed;
    bool requested_target_invalidation;
    bool requested_full_invalidation;
    uint8_t pane_hit_region;
    uint8_t invalidation_class;
    uint32_t invalidation_reason_bits;
} SceneEditorInputRoutingResult;

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

typedef struct SceneEditorInputRouterCallbacks {
    void* context;
    bool (*event_matches_editor_window)(void* context, const SDL_Event* event);
    bool (*should_route_global_key)(void* context, const SDL_Event* event);
    bool (*is_chrome_hit)(void* context, int mx, int my);
    bool (*handle_system_input)(void* context, SDL_Event* event, SceneEditorInputRoutingResult* result);
    SceneEditorInputTarget (*resolve_pane_target)(void* context);
    bool (*resolve_chrome_action)(void* context, const SDL_Event* event, SceneEditorChromeAction* out_action);
    void (*apply_chrome_action)(void* context, const SceneEditorChromeAction* action);
    void (*route_pane_event)(void* context, const SceneEditorPaneCommand* command, SceneEditorInputRoutingResult* result);
    void (*apply_invalidation)(void* context, const SceneEditorInputRoutingResult* result);
    bool (*should_stop_processing)(void* context);
} SceneEditorInputRouterCallbacks;

void SceneEditorInputRouterReset(void);
void SceneEditorInputRouterHandleEvent(SDL_Event* event,
                                       const SceneEditorInputRouterCallbacks* callbacks);
void SceneEditorInputRouterDrain(const SceneEditorInputRouterCallbacks* callbacks);

#endif
