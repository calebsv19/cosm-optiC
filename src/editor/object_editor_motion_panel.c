#include "editor/object_editor_panels_internal.h"

#include "editor/object_editor_motion.h"
#include "motion/runtime_motion_track_3d.h"

#include <stdio.h>
#include <string.h>

static SDL_Color object_editor_motion_panel_disabled_color(SDL_Color color) {
    color.r = (Uint8)((int)color.r * 2 / 3);
    color.g = (Uint8)((int)color.g * 2 / 3);
    color.b = (Uint8)((int)color.b * 2 / 3);
    color.a = 150;
    return color;
}

static bool object_editor_motion_panel_selected_object_id(char* out_object_id,
                                                          size_t out_object_id_size) {
    int selected_index = ObjectEditorGetSelectedObjectIndex();
    return ObjectEditorMotionObjectIdForSceneIndex(selected_index,
                                                   out_object_id,
                                                   out_object_id_size);
}

static ObjectEditorPanelMotionAction object_editor_motion_panel_active_action(
    const RuntimeMotionTrack3D* track) {
    if (!track || !track->enabled) {
        return OBJECT_EDITOR_PANEL_MOTION_ACTION_STATIC;
    }
    if (track->mode == RUNTIME_MOTION_TRACK_3D_MODE_PHYSICS) {
        return OBJECT_EDITOR_PANEL_MOTION_ACTION_PHYSICS_RESERVED;
    }
    return OBJECT_EDITOR_PANEL_MOTION_ACTION_AUTHORED;
}

static SDL_Color object_editor_motion_panel_status_color(
    ObjectEditorPanelMotionAction action,
    bool has_object_id,
    const RayTracingThemePalette* palette) {
    if (!has_object_id) {
        return (SDL_Color){160, 160, 170, 210};
    }
    if (action == OBJECT_EDITOR_PANEL_MOTION_ACTION_AUTHORED) {
        return palette ? palette->accent_primary : (SDL_Color){120, 220, 215, 255};
    }
    if (action == OBJECT_EDITOR_PANEL_MOTION_ACTION_PHYSICS_RESERVED) {
        return (SDL_Color){255, 190, 96, 235};
    }
    return (SDL_Color){170, 180, 190, 220};
}

static const char* object_editor_motion_panel_status_label(
    ObjectEditorPanelMotionAction action,
    bool has_object_id) {
    if (!has_object_id) {
        return "No runtime id";
    }
    if (action == OBJECT_EDITOR_PANEL_MOTION_ACTION_AUTHORED) {
        return "Authored path";
    }
    if (action == OBJECT_EDITOR_PANEL_MOTION_ACTION_PHYSICS_RESERVED) {
        return "Physics reserved";
    }
    return "Static object";
}

static void object_editor_motion_panel_draw_button(SDL_Renderer* renderer,
                                                   const SDL_Rect* rect,
                                                   const char* label,
                                                   bool active,
                                                   bool enabled,
                                                   const RayTracingThemePalette* palette,
                                                   SDL_Color text_color,
                                                   SDL_Color border_color) {
    SDL_Color fill = ObjectEditorPanels_PanelButtonFill(palette, active);
    if (!enabled) {
        fill = object_editor_motion_panel_disabled_color(fill);
    }
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, enabled ? 230 : 150);
    SDL_RenderDrawRect(renderer, rect);
    RenderLabelText(renderer,
                    *rect,
                    label,
                    enabled ? text_color : object_editor_motion_panel_disabled_color(text_color));
}

void ObjectEditorPanels_DrawMotionSection(SDL_Renderer* renderer,
                                          const RayTracingThemePalette* palette) {
    SDL_Rect section = {0};
    SDL_Rect label = {0};
    SDL_Rect static_button = {0};
    SDL_Rect authored_button = {0};
    SDL_Rect physics_button = {0};
    SDL_Rect status_label = {0};
    SDL_Rect status_swatch = {0};
    SDL_Color text_color = ObjectEditorPanels_PanelTextColor(palette);
    SDL_Color muted_color = ObjectEditorPanels_PanelMutedTextColor(palette);
    SDL_Color inactive_fill = ObjectEditorPanels_PanelInactiveRowFill(palette);
    SDL_Color border_color = ObjectEditorPanels_PanelBorderColor(palette);
    char object_id[RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE];
    const RuntimeMotionTrack3D* track = NULL;
    ObjectEditorPanelMotionAction active_action = OBJECT_EDITOR_PANEL_MOTION_ACTION_STATIC;
    bool has_object_id = false;
    SDL_Color status_color = {0};

    if (!renderer || ObjectEditorPanels_MotionSectionHeight() <= 0) {
        return;
    }
    memset(object_id, 0, sizeof(object_id));
    has_object_id = object_editor_motion_panel_selected_object_id(object_id, sizeof(object_id));
    if (has_object_id) {
        track = ObjectEditorMotionFindTrack(object_id);
    }
    active_action = object_editor_motion_panel_active_action(track);

    ObjectEditorPanels_ResolveMotionSectionMetrics(&section,
                                                   &label,
                                                   &static_button,
                                                   &authored_button,
                                                   &physics_button,
                                                   &status_label,
                                                   &status_swatch);
    SDL_SetRenderDrawColor(renderer, inactive_fill.r, inactive_fill.g, inactive_fill.b, 170);
    SDL_RenderFillRect(renderer, &section);
    SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, 170);
    SDL_RenderDrawRect(renderer, &section);

    RenderLabelTextLeft(renderer, label, "Motion", muted_color);
    object_editor_motion_panel_draw_button(renderer,
                                           &static_button,
                                           "Static",
                                           active_action == OBJECT_EDITOR_PANEL_MOTION_ACTION_STATIC,
                                           has_object_id,
                                           palette,
                                           text_color,
                                           border_color);
    object_editor_motion_panel_draw_button(renderer,
                                           &authored_button,
                                           "Path",
                                           active_action == OBJECT_EDITOR_PANEL_MOTION_ACTION_AUTHORED,
                                           has_object_id,
                                           palette,
                                           text_color,
                                           border_color);
    object_editor_motion_panel_draw_button(renderer,
                                           &physics_button,
                                           "Phys",
                                           active_action ==
                                               OBJECT_EDITOR_PANEL_MOTION_ACTION_PHYSICS_RESERVED,
                                           false,
                                           palette,
                                           text_color,
                                           border_color);

    status_color = object_editor_motion_panel_status_color(active_action,
                                                           has_object_id,
                                                           palette);
    SDL_SetRenderDrawColor(renderer,
                           status_color.r,
                           status_color.g,
                           status_color.b,
                           status_color.a);
    SDL_RenderFillRect(renderer, &status_swatch);
    SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, 220);
    SDL_RenderDrawRect(renderer, &status_swatch);
    RenderLabelTextLeft(renderer,
                        status_label,
                        object_editor_motion_panel_status_label(active_action, has_object_id),
                        has_object_id ? text_color : muted_color);
}
