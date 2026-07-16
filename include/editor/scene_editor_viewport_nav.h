#ifndef SCENE_EDITOR_VIEWPORT_NAV_H
#define SCENE_EDITOR_VIEWPORT_NAV_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "editor/scene_editor_digest_overlay.h"

typedef struct SceneEditorViewportNavCommand {
    const SDL_Rect* viewport_rect;
    const SDL_Event* event;
    bool viewport_canvas_region;
    bool viewport_drag_region;
    bool key_frame_enabled;
    bool gesture_orbit_enabled;
    bool gesture_pan_enabled;
    bool wheel_zoom_enabled;
    int active_mode;
    int selected_object_index;
} SceneEditorViewportNavCommand;

void SceneEditorViewportNavResetDigestOverlayNavigation(SceneEditorDigestOverlayNavState* nav_state);
bool SceneEditorViewportNavFitDigestOverlay(SceneEditorDigestOverlayNavState* nav_state,
                                            const SDL_Rect* viewport_rect,
                                            bool reset_angles);
bool SceneEditorViewportNavFitDigestOverlayForTarget(SceneEditorDigestOverlayNavState* nav_state,
                                                     const SDL_Rect* viewport_rect,
                                                     bool reset_angles,
                                                     int active_mode,
                                                     int selected_object_index);
bool SceneEditorViewportNavApplyDigestWheelZoom(SceneEditorDigestOverlayNavState* nav_state,
                                                const SDL_Rect* viewport_rect,
                                                int screen_x,
                                                int screen_y,
                                                double wheel_delta,
                                                int active_mode,
                                                int selected_object_index);
bool SceneEditorViewportNavApplyDigestPan(SceneEditorDigestOverlayNavState* nav_state,
                                         const SDL_Rect* viewport_rect,
                                         int screen_dx,
                                         int screen_dy,
                                         int active_mode,
                                         int selected_object_index);
bool SceneEditorViewportNavApplyDigestResize(SceneEditorDigestOverlayNavState* nav_state);
bool SceneEditorViewportNavHandleCommand(const SceneEditorViewportNavCommand* command,
                                         SceneEditorDigestOverlayNavState* nav_state,
                                         bool* out_interaction_drag);

#endif
