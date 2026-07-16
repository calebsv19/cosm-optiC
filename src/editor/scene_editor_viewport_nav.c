#include "editor/scene_editor_viewport_nav.h"

#include <math.h>

#include "camera/camera.h"
#include "config/config_manager.h"
#include "editor/camera_editor.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_viewport3d_bridge.h"
#include "render/space_mode_adapter.h"
#include "scene/object_manager.h"

#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG (-35.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG (24.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_PITCH_DEG (-80.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_PITCH_DEG (80.0)

static bool scene_editor_viewport_nav_point_in_rect(int x, int y, const SDL_Rect* rect) {
    if (!rect) return true;
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w && y < rect->y + rect->h;
}

static bool scene_editor_viewport_nav_rect_contains_event_point(const SDL_Rect* viewport_rect,
                                                                const SDL_Event* event) {
    int mx = 0;
    int my = 0;
    if (!event) return false;
    if (!viewport_rect) return true;
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
    return scene_editor_viewport_nav_point_in_rect(mx, my, viewport_rect);
}

static CameraPoint scene_editor_viewport_nav_screen_to_world_point(int screen_x, int screen_y) {
    Camera preview = CameraBuildPreviewCamera(&sceneSettings.camera,
                                              GetCurrentMarginPixels(),
                                              sceneSettings.windowWidth,
                                              sceneSettings.windowHeight);
    SpaceModeViewContext view_ctx = EditorModeRouter_BuildViewContext(&preview,
                                                                      sceneSettings.windowWidth,
                                                                      sceneSettings.windowHeight);
    return SpaceModeAdapter_ScreenToWorld(&view_ctx, screen_x, screen_y);
}

static void scene_editor_viewport_nav_zoom_toward_screen_point(int screen_x,
                                                               int screen_y,
                                                               int wheel_y) {
    CameraPoint before = {0.0, 0.0};
    CameraPoint after = {0.0, 0.0};
    double delta = 0.0;
    if (wheel_y == 0) return;
    before = scene_editor_viewport_nav_screen_to_world_point(screen_x, screen_y);
    delta = sceneSettings.camera.zoom * 0.10 * (double)wheel_y;
    CameraZoom(&sceneSettings.camera, delta, 0.05, 200.0);
    after = scene_editor_viewport_nav_screen_to_world_point(screen_x, screen_y);
    CameraPan(&sceneSettings.camera, before.x - after.x, before.y - after.y);
}

static void scene_editor_viewport_nav_orbit_by_mouse_delta(SceneEditorDigestOverlayNavState* nav_state,
                                                           int dx,
                                                           int dy) {
    RuntimeSceneBridge3DDigestState digest = {0};
    if (!nav_state) return;
    if (SceneEditorDigestOverlayResolve(&digest)) {
        const double degrees_to_radians = 3.14159265358979323846 / 180.0;
        const double radians_to_degrees = 180.0 / 3.14159265358979323846;
        double next_yaw_rad = 0.0;
        double next_pitch_rad = 0.0;
        if (!SceneEditorViewport3DBridgeApplyOrbit(
                nav_state->orbit_yaw_deg * degrees_to_radians,
                nav_state->orbit_pitch_deg * degrees_to_radians,
                (double)dx * 0.45 * degrees_to_radians,
                (double)dy * 0.35 * degrees_to_radians,
                &next_yaw_rad,
                &next_pitch_rad)) {
            return;
        }
        nav_state->orbit_yaw_deg = next_yaw_rad * radians_to_degrees;
        nav_state->orbit_pitch_deg = next_pitch_rad * radians_to_degrees;
        nav_state->orbit_yaw_deg = fmod(nav_state->orbit_yaw_deg, 360.0);
        if (nav_state->orbit_yaw_deg < 0.0) nav_state->orbit_yaw_deg += 360.0;
        if (nav_state->orbit_pitch_deg < SCENE_EDITOR_DIGEST_OVERLAY_MIN_PITCH_DEG) {
            nav_state->orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_MIN_PITCH_DEG;
        }
        if (nav_state->orbit_pitch_deg > SCENE_EDITOR_DIGEST_OVERLAY_MAX_PITCH_DEG) {
            nav_state->orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_MAX_PITCH_DEG;
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

static bool scene_editor_viewport_nav_frame_to_scene(void) {
    bool any = false;
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    int i = 0;
    const double margin_world = 18.0;
    for (i = 0; i < sceneSettings.objectCount; ++i) {
        SceneObject* obj = &sceneSettings.sceneObjects[i];
        if (SceneObjectIsGuideOnly(obj)) continue;
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

void SceneEditorViewportNavResetDigestOverlayNavigation(SceneEditorDigestOverlayNavState* nav_state) {
    if (!nav_state) return;
    nav_state->orbit_active = false;
    nav_state->pan_active = false;
    nav_state->target_valid = false;
    nav_state->zoom_limits_valid = false;
    nav_state->zoom_limits_material_focus = false;
    nav_state->last_mouse_x = 0;
    nav_state->last_mouse_y = 0;
    nav_state->orbit_yaw_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG;
    nav_state->orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG;
    nav_state->overlay_zoom = 1.0;
    nav_state->zoom_min = 0.0;
    nav_state->zoom_max = 0.0;
    nav_state->target_x = 0.0;
    nav_state->target_y = 0.0;
    nav_state->target_z = 0.0;
}

bool SceneEditorViewportNavFitDigestOverlay(SceneEditorDigestOverlayNavState* nav_state,
                                            const SDL_Rect* viewport_rect,
                                            bool reset_angles) {
    return SceneEditorViewportNavFitDigestOverlayForTarget(nav_state,
                                                           viewport_rect,
                                                           reset_angles,
                                                           EDITOR_MODE_PATH,
                                                           -1);
}

bool SceneEditorViewportNavHandleCommand(const SceneEditorViewportNavCommand* command,
                                         SceneEditorDigestOverlayNavState* nav_state,
                                         bool* out_interaction_drag) {
    const SDL_Event* event = NULL;
    bool consumed = false;
    if (out_interaction_drag) {
        *out_interaction_drag = false;
    }
    if (!command || !command->event || !nav_state) {
        return false;
    }
    event = command->event;
    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_f &&
        command->key_frame_enabled) {
        if (SceneEditorViewportNavFitDigestOverlayForTarget(nav_state,
                                                            command->viewport_rect,
                                                            true,
                                                            command->active_mode,
                                                            command->selected_object_index)) {
            consumed = true;
        } else {
            consumed = scene_editor_viewport_nav_frame_to_scene();
        }
    }
    if (consumed) {
        return true;
    }
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_MIDDLE &&
        nav_state->pan_active) {
        nav_state->pan_active = false;
        return true;
    }
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        nav_state->orbit_active = false;
    }
    if (!command->viewport_canvas_region && !command->viewport_drag_region) {
        return false;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_MIDDLE &&
        command->gesture_pan_enabled &&
        scene_editor_viewport_nav_rect_contains_event_point(command->viewport_rect, event) &&
        SceneEditorViewportNavApplyDigestPan(nav_state,
                                             command->viewport_rect,
                                             0,
                                             0,
                                             command->active_mode,
                                             command->selected_object_index)) {
        nav_state->pan_active = true;
        nav_state->orbit_active = false;
        nav_state->last_mouse_x = event->button.x;
        nav_state->last_mouse_y = event->button.y;
        consumed = true;
    } else if (event->type == SDL_MOUSEMOTION &&
        scene_editor_viewport_nav_rect_contains_event_point(command->viewport_rect, event)) {
        SDL_Keymod mods = SDL_GetModState();
        bool alt_down = ((mods & KMOD_ALT) != 0);
        bool left_down = ((event->motion.state & SDL_BUTTON_LMASK) != 0);
        bool middle_down = ((event->motion.state & SDL_BUTTON_MMASK) != 0);
        if (nav_state->pan_active && middle_down && command->gesture_pan_enabled) {
            if (SceneEditorViewportNavApplyDigestPan(nav_state,
                                                     command->viewport_rect,
                                                     event->motion.xrel,
                                                     event->motion.yrel,
                                                     command->active_mode,
                                                     command->selected_object_index)) {
                nav_state->last_mouse_x = event->motion.x;
                nav_state->last_mouse_y = event->motion.y;
                consumed = true;
            }
        } else if (alt_down && left_down && command->gesture_orbit_enabled) {
            scene_editor_viewport_nav_orbit_by_mouse_delta(nav_state,
                                                           event->motion.xrel,
                                                           event->motion.yrel);
            nav_state->orbit_active = true;
            nav_state->last_mouse_x = event->motion.x;
            nav_state->last_mouse_y = event->motion.y;
            consumed = true;
        } else {
            if (!middle_down) nav_state->pan_active = false;
            nav_state->orbit_active = false;
        }
    } else if (event->type == SDL_MOUSEWHEEL &&
               scene_editor_viewport_nav_rect_contains_event_point(command->viewport_rect, event)) {
        int mx = 0;
        int my = 0;
        double wheel_delta = event->wheel.preciseY != 0.0f
                                 ? (double)event->wheel.preciseY
                                 : (double)event->wheel.y;
        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            wheel_delta = -wheel_delta;
        }
        if (wheel_delta != 0.0 && command->wheel_zoom_enabled) {
#if SDL_VERSION_ATLEAST(2, 26, 0)
            mx = event->wheel.mouseX;
            my = event->wheel.mouseY;
#else
            SDL_GetMouseState(&mx, &my);
#endif
            if (SceneEditorViewportNavApplyDigestWheelZoom(nav_state,
                                                           command->viewport_rect,
                                                           mx,
                                                           my,
                                                           wheel_delta,
                                                           command->active_mode,
                                                           command->selected_object_index)) {
                consumed = true;
            } else {
                scene_editor_viewport_nav_zoom_toward_screen_point(
                    mx,
                    my,
                    wheel_delta > 0.0 ? 1 : -1);
                consumed = true;
            }
        }
    }

    if (!consumed) {
        return false;
    }
    if (out_interaction_drag) {
        *out_interaction_drag = (event->type == SDL_MOUSEMOTION &&
                                 (nav_state->orbit_active || nav_state->pan_active));
    }
    return true;
}
