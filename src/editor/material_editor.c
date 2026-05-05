#include "editor/material_editor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "camera/camera.h"
#include "config/config_manager.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor.h"
#include "editor/scene_editor_material_face_placement.h"
#include "render/render_helper.h"
#include "scene/object_manager.h"
#include "ui/shared_theme_font_adapter.h"

#define MATERIAL_EDITOR_BUTTON_HEIGHT 24
#define MATERIAL_EDITOR_BUTTON_GAP 5
#define MATERIAL_EDITOR_SLIDER_HEIGHT 24
#define MATERIAL_EDITOR_PARAM_SLIDER_HEIGHT 24
#define MATERIAL_EDITOR_SLIDER_TRACK_HEIGHT 5
#define MATERIAL_EDITOR_SLIDER_KNOB_WIDTH 9
#define MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES 64
#define MATERIAL_EDITOR_MAX_GROUP_ROWS 16
#define MATERIAL_EDITOR_GROUP_ROW_HEIGHT 24
#define MATERIAL_EDITOR_GROUP_ROW_GAP 2
#define MATERIAL_EDITOR_MAX_FACE_GROUPS SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES
#define MATERIAL_EDITOR_PARAM_SLIDER_COUNT 7
#define MATERIAL_EDITOR_PATTERN_BUTTON_COUNT 4
#define MATERIAL_EDITOR_MIN_GROUP_LIST_HEIGHT 96

typedef struct MaterialEditorFaceGroupInfo {
    int face_group_index;
    int triangle_count;
    int selected_count;
    SceneEditorMaterialPreviewTriangleAddress representative;
} MaterialEditorFaceGroupInfo;

static int s_material_editor_focused_object_index = -1;
static MaterialEditorSliderKind s_material_editor_active_slider = MATERIAL_EDITOR_SLIDER_NONE;
static MaterialEditorTextureParamKind s_material_editor_active_param_slider =
    MATERIAL_EDITOR_TEXTURE_PARAM_NONE;
static MaterialEditorViewMode s_material_editor_view_mode = MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN;
static bool s_material_editor_solid_faces_enabled = true;
static int s_material_editor_active_face_group_index = -1;
static SceneEditorMaterialPreviewTriangleAddress
    s_material_editor_selected_triangles[MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES];
static int s_material_editor_selected_triangle_count = 0;
static SDL_Rect s_texture_none_rect = {0, 0, 0, 0};
static SDL_Rect s_texture_rust_rect = {0, 0, 0, 0};
static SDL_Rect s_texture_fog_rect = {0, 0, 0, 0};
static SDL_Rect s_solid_faces_rect = {0, 0, 0, 0};
static SDL_Rect s_reset_face_rect = {0, 0, 0, 0};
static SDL_Rect s_copy_face_rect = {0, 0, 0, 0};
static SDL_Rect s_clear_groups_rect = {0, 0, 0, 0};
static SDL_Rect s_group_panel_rect = {0, 0, 0, 0};
static SDL_Rect s_group_list_rect = {0, 0, 0, 0};
static SDL_Rect s_group_row_rects[MATERIAL_EDITOR_MAX_GROUP_ROWS];
static int s_group_row_face_groups[MATERIAL_EDITOR_MAX_GROUP_ROWS];
static SceneEditorMaterialPreviewTriangleAddress s_group_row_addresses[MATERIAL_EDITOR_MAX_GROUP_ROWS];
static char s_group_row_labels[MATERIAL_EDITOR_MAX_GROUP_ROWS][64];
static char s_group_header_label[64];
static int s_group_row_count = 0;
static int s_group_scroll_offset = 0;
static int s_group_visible_capacity = 0;
static int s_group_total_count = 0;
static SDL_Rect s_slider_sections[4];
static SDL_Rect s_slider_tracks[4];
static SDL_Rect s_param_sections[MATERIAL_EDITOR_PARAM_SLIDER_COUNT];
static SDL_Rect s_param_tracks[MATERIAL_EDITOR_PARAM_SLIDER_COUNT];
static SDL_Rect s_pattern_rects[MATERIAL_EDITOR_PATTERN_BUTTON_COUNT];

static void material_editor_draw_button(SDL_Renderer* renderer,
                                        SDL_Rect rect,
                                        const char* label,
                                        bool active,
                                        RayTracingThemePalette palette);

static SceneEditorMaterialTextureParamField material_editor_texture_param_field(
    MaterialEditorTextureParamKind kind) {
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE) {
        return SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_COVERAGE;
    }
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_GRAIN) {
        return SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_GRAIN;
    }
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_EDGE_SOFTNESS) {
        return SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_EDGE_SOFTNESS;
    }
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_CONTRAST) {
        return SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_CONTRAST;
    }
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_FLOW) {
        return SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_FLOW;
    }
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COLOR_DEPTH) {
        return SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_COLOR_DEPTH;
    }
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_SURFACE_DAMAGE) {
        return SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_SURFACE_DAMAGE;
    }
    return SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_COVERAGE;
}

static int material_editor_texture_param_slot(MaterialEditorTextureParamKind kind) {
    int slot = (int)kind - 1;
    if (slot < 0 || slot >= MATERIAL_EDITOR_PARAM_SLIDER_COUNT) return -1;
    return slot;
}

static bool material_editor_point_in_rect(int x, int y, const SDL_Rect* rect) {
    return rect && rect->w > 0 && rect->h > 0 &&
           x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static double material_editor_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool material_editor_triangle_address_equal(
    const SceneEditorMaterialPreviewTriangleAddress* lhs,
    const SceneEditorMaterialPreviewTriangleAddress* rhs) {
    if (!lhs || !rhs) return false;
    return lhs->sceneObjectIndex == rhs->sceneObjectIndex &&
           lhs->primitiveIndex == rhs->primitiveIndex &&
           lhs->triangleIndex == rhs->triangleIndex &&
           lhs->localTriangleIndex == rhs->localTriangleIndex &&
           lhs->faceGroupIndex == rhs->faceGroupIndex;
}

static int material_editor_find_selected_triangle(
    const SceneEditorMaterialPreviewTriangleAddress* address) {
    if (!address) return -1;
    for (int i = 0; i < s_material_editor_selected_triangle_count; ++i) {
        if (material_editor_triangle_address_equal(&s_material_editor_selected_triangles[i],
                                                   address)) {
            return i;
        }
    }
    return -1;
}

static bool material_editor_selected_face_group_seen(int face_group_index, int limit) {
    if (face_group_index < 0) return false;
    for (int i = 0; i < limit && i < s_material_editor_selected_triangle_count; ++i) {
        if (s_material_editor_selected_triangles[i].faceGroupIndex == face_group_index) {
            return true;
        }
    }
    return false;
}

static bool material_editor_selected_face_group_exists(int face_group_index) {
    if (face_group_index < 0) return false;
    for (int i = 0; i < s_material_editor_selected_triangle_count; ++i) {
        if (s_material_editor_selected_triangles[i].faceGroupIndex == face_group_index) {
            return true;
        }
    }
    return false;
}

static int material_editor_find_face_group_info(const MaterialEditorFaceGroupInfo* groups,
                                                int group_count,
                                                int face_group_index) {
    if (!groups || face_group_index < 0) return -1;
    for (int i = 0; i < group_count; ++i) {
        if (groups[i].face_group_index == face_group_index) return i;
    }
    return -1;
}

static int material_editor_collect_focused_face_groups(MaterialEditorFaceGroupInfo* out_groups,
                                                       int group_capacity) {
    SceneEditorMaterialPreviewTriangleAddress addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    SceneEditorMaterialPreviewStats stats = {0};
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    int group_count = 0;
    if (!out_groups || group_capacity <= 0 || focused_object_index < 0) return 0;
    memset(out_groups, 0, sizeof(MaterialEditorFaceGroupInfo) * (size_t)group_capacity);
    memset(addresses, 0, sizeof(addresses));
    if (!SceneEditorMaterialPreviewResolveFocusedTriangles(focused_object_index,
                                                          NULL,
                                                          addresses,
                                                          SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES,
                                                          &stats)) {
        return 0;
    }
    for (int i = 0; i < stats.triangleCount; ++i) {
        int face_group = addresses[i].faceGroupIndex;
        int index = material_editor_find_face_group_info(out_groups, group_count, face_group);
        if (face_group < 0) continue;
        if (index < 0) {
            if (group_count >= group_capacity) break;
            index = group_count;
            out_groups[index].face_group_index = face_group;
            out_groups[index].representative = addresses[i];
            group_count += 1;
        }
        out_groups[index].triangle_count += 1;
        if (material_editor_find_selected_triangle(&addresses[i]) >= 0) {
            out_groups[index].selected_count += 1;
        }
    }
    return group_count;
}

static void material_editor_reset_group_list_layout(void) {
    memset(s_group_row_rects, 0, sizeof(s_group_row_rects));
    memset(s_group_row_addresses, 0, sizeof(s_group_row_addresses));
    memset(s_group_row_labels, 0, sizeof(s_group_row_labels));
    memset(s_group_header_label, 0, sizeof(s_group_header_label));
    for (int i = 0; i < MATERIAL_EDITOR_MAX_GROUP_ROWS; ++i) {
        s_group_row_face_groups[i] = -1;
    }
    s_group_row_count = 0;
    s_clear_groups_rect = (SDL_Rect){0, 0, 0, 0};
    s_group_panel_rect = (SDL_Rect){0, 0, 0, 0};
    s_group_list_rect = (SDL_Rect){0, 0, 0, 0};
    s_group_visible_capacity = 0;
    s_group_total_count = 0;
}

static bool material_editor_add_triangle_selection(
    const SceneEditorMaterialPreviewTriangleAddress* address) {
    if (!address) return false;
    if (material_editor_find_selected_triangle(address) >= 0) return true;
    if (s_material_editor_selected_triangle_count >= MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES) {
        return false;
    }
    s_material_editor_selected_triangles[s_material_editor_selected_triangle_count] = *address;
    s_material_editor_selected_triangle_count += 1;
    return true;
}

static bool material_editor_remove_face_group_selection(int scene_object_index, int face_group_index) {
    bool removed = false;
    if (face_group_index < 0) return false;
    for (int i = 0; i < s_material_editor_selected_triangle_count;) {
        if (s_material_editor_selected_triangles[i].sceneObjectIndex == scene_object_index &&
            s_material_editor_selected_triangles[i].faceGroupIndex == face_group_index) {
            for (int j = i; j + 1 < s_material_editor_selected_triangle_count; ++j) {
                s_material_editor_selected_triangles[j] = s_material_editor_selected_triangles[j + 1];
            }
            s_material_editor_selected_triangle_count -= 1;
            removed = true;
            continue;
        }
        i += 1;
    }
    return removed;
}

static int material_editor_collect_face_group_triangles(
    const SceneEditorMaterialPreviewTriangleAddress* address,
    SceneEditorMaterialPreviewTriangleAddress* out_addresses,
    int address_capacity) {
    SceneEditorMaterialPreviewTriangleAddress all_addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    SceneEditorMaterialPreviewStats stats = {0};
    int count = 0;
    if (!address || !out_addresses || address_capacity <= 0) return 0;
    memset(all_addresses, 0, sizeof(all_addresses));
    if (!SceneEditorMaterialPreviewResolveFocusedTriangles(address->sceneObjectIndex,
                                                          NULL,
                                                          all_addresses,
                                                          SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES,
                                                          &stats)) {
        return 0;
    }
    for (int i = 0; i < stats.triangleCount && count < address_capacity; ++i) {
        if (all_addresses[i].sceneObjectIndex == address->sceneObjectIndex &&
            all_addresses[i].faceGroupIndex == address->faceGroupIndex) {
            out_addresses[count] = all_addresses[i];
            count += 1;
        }
    }
    return count;
}

static bool material_editor_face_group_fully_selected(
    const SceneEditorMaterialPreviewTriangleAddress* address) {
    SceneEditorMaterialPreviewTriangleAddress group_addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    int group_count = material_editor_collect_face_group_triangles(address,
                                                                   group_addresses,
                                                                   SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES);
    if (!address || group_count <= 0) return false;
    for (int i = 0; i < group_count; ++i) {
        if (material_editor_find_selected_triangle(&group_addresses[i]) < 0) {
            return false;
        }
    }
    return true;
}

static int material_editor_draw_group_list(SDL_Renderer* renderer,
                                           SDL_Rect content_bounds,
                                           int cursor_y,
                                           int bottom_y,
                                           RayTracingThemePalette palette) {
    MaterialEditorFaceGroupInfo groups[MATERIAL_EDITOR_MAX_FACE_GROUPS];
    int group_count = 0;
    int selected_count = MaterialEditorSelectedFaceGroupCount();
    int header_h = 24;
    int row_stride = MATERIAL_EDITOR_GROUP_ROW_HEIGHT + MATERIAL_EDITOR_GROUP_ROW_GAP;
    int list_h = 0;
    int visible_capacity = 0;
    int scrollbar_w = 0;
    SDL_Rect header_rect = {0, 0, 0, 0};
    SDL_Rect list_rect = {0, 0, 0, 0};
    SDL_Rect clear_rect = {0, 0, 0, 0};
    if (!renderer || content_bounds.w <= 0 || cursor_y >= bottom_y) return cursor_y;
    material_editor_reset_group_list_layout();
    group_count = material_editor_collect_focused_face_groups(groups, MATERIAL_EDITOR_MAX_FACE_GROUPS);
    s_group_total_count = group_count;
    s_group_panel_rect = (SDL_Rect){content_bounds.x,
                                    cursor_y,
                                    content_bounds.w,
                                    bottom_y - cursor_y};
    if (cursor_y + header_h > bottom_y) return cursor_y;
    header_rect = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 18};
    snprintf(s_group_header_label,
             sizeof(s_group_header_label),
             "Face Groups %d/%d selected",
             selected_count,
             group_count);
    if (selected_count > 0 && content_bounds.w >= 180) {
        clear_rect = (SDL_Rect){content_bounds.x + content_bounds.w - 112,
                                cursor_y - 2,
                                112,
                                22};
        header_rect.w -= 120;
        s_clear_groups_rect = clear_rect;
    }
    RenderLabelTextLeft(renderer, header_rect, s_group_header_label, palette.text_primary);
    if (s_clear_groups_rect.w > 0) {
        material_editor_draw_button(renderer, clear_rect, "Clear", false, palette);
    }
    cursor_y += header_h;
    list_h = bottom_y - cursor_y;
    if (group_count <= 0 || list_h < MATERIAL_EDITOR_GROUP_ROW_HEIGHT) {
        if (list_h > 18) {
            RenderLabelTextWrappedLeft(renderer,
                                       (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, list_h},
                                       "No focused-object faces available.",
                                       palette.text_muted);
        }
        return bottom_y;
    }
    visible_capacity = list_h / row_stride;
    if (visible_capacity < 1) visible_capacity = 1;
    if (visible_capacity > MATERIAL_EDITOR_MAX_GROUP_ROWS) visible_capacity = MATERIAL_EDITOR_MAX_GROUP_ROWS;
    if (visible_capacity > group_count) visible_capacity = group_count;
    if (s_group_scroll_offset > group_count - visible_capacity) {
        s_group_scroll_offset = group_count - visible_capacity;
    }
    if (s_group_scroll_offset < 0) s_group_scroll_offset = 0;
    s_group_visible_capacity = visible_capacity;
    scrollbar_w = group_count > visible_capacity ? 8 : 0;
    list_rect = (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, list_h};
    s_group_list_rect = list_rect;
    for (int i = 0; i < visible_capacity; ++i) {
        int group_index = i + s_group_scroll_offset;
        int face_group = groups[group_index].face_group_index;
        SDL_Rect row = {content_bounds.x,
                        cursor_y + i * row_stride,
                        content_bounds.w - scrollbar_w - 4,
                        MATERIAL_EDITOR_GROUP_ROW_HEIGHT};
        bool selected = groups[group_index].selected_count > 0;
        bool active = selected && face_group == s_material_editor_active_face_group_index;
        SDL_Color fill = active ? ray_tracing_theme_resolve_button_active_fill(palette)
                                : palette.panel_fill;
        SDL_Color text = active ? ray_tracing_theme_choose_button_text(fill, palette)
                                : selected ? palette.text_primary : palette.text_muted;
        s_group_row_rects[i] = row;
        s_group_row_face_groups[i] = face_group;
        s_group_row_addresses[i] = groups[group_index].representative;
        snprintf(s_group_row_labels[i],
                 sizeof(s_group_row_labels[i]),
                 "Face #%d  %d/%d tris",
                 face_group,
                 groups[group_index].selected_count,
                 groups[group_index].triangle_count);
        s_group_row_count += 1;
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, active ? 255 : 210);
        SDL_RenderFillRect(renderer, &row);
        SDL_SetRenderDrawColor(renderer,
                               selected ? palette.accent_primary.r : palette.panel_border.r,
                               selected ? palette.accent_primary.g : palette.panel_border.g,
                               selected ? palette.accent_primary.b : palette.panel_border.b,
                               255);
        SDL_RenderDrawRect(renderer, &row);
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){row.x + 8, row.y + 4, row.w - 16, 16},
                            s_group_row_labels[i],
                            text);
    }
    if (scrollbar_w > 0) {
        int track_h = visible_capacity * row_stride - MATERIAL_EDITOR_GROUP_ROW_GAP;
        int thumb_h = (track_h * visible_capacity) / group_count;
        int max_offset = group_count - visible_capacity;
        int thumb_travel = 0;
        SDL_Rect track = {content_bounds.x + content_bounds.w - scrollbar_w,
                          cursor_y,
                          4,
                          track_h};
        SDL_Rect thumb = track;
        if (thumb_h < 18) thumb_h = 18;
        if (thumb_h > track_h) thumb_h = track_h;
        thumb_travel = track_h - thumb_h;
        thumb.h = thumb_h;
        thumb.y = track.y + (max_offset > 0 ? (thumb_travel * s_group_scroll_offset) / max_offset : 0);
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r,
                               palette.panel_border.g,
                               palette.panel_border.b,
                               180);
        SDL_RenderFillRect(renderer, &track);
        SDL_SetRenderDrawColor(renderer,
                               palette.accent_primary.r,
                               palette.accent_primary.g,
                               palette.accent_primary.b,
                               255);
        SDL_RenderFillRect(renderer, &thumb);
    }
    return bottom_y;
}

static bool material_editor_scroll_group_list(int wheel_y) {
    int max_offset = s_group_total_count - s_group_visible_capacity;
    if (wheel_y == 0 || max_offset <= 0) return false;
    s_group_scroll_offset -= wheel_y;
    if (s_group_scroll_offset < 0) s_group_scroll_offset = 0;
    if (s_group_scroll_offset > max_offset) s_group_scroll_offset = max_offset;
    return true;
}

static double material_editor_slider_value_from_x(const SDL_Rect* track, int mx) {
    double denom = 0.0;
    double raw = 0.0;
    if (!track || track->w <= MATERIAL_EDITOR_SLIDER_KNOB_WIDTH) return 0.0;
    denom = (double)(track->w - MATERIAL_EDITOR_SLIDER_KNOB_WIDTH);
    raw = ((double)mx - (double)track->x - (MATERIAL_EDITOR_SLIDER_KNOB_WIDTH * 0.5)) / denom;
    return material_editor_clamp01(raw);
}

static bool material_editor_has_room_for_optional_control(int cursor_y,
                                                          int control_h,
                                                          int bottom_y) {
    return cursor_y + control_h + MATERIAL_EDITOR_MIN_GROUP_LIST_HEIGHT <= bottom_y;
}

static SceneObject* material_editor_focused_object(void) {
    int object_count = sceneSettings.objectCount;
    int selected = ObjectEditorSelectionTrackerCurrent(object_count);
    if (selected >= 0 && selected < object_count) {
        s_material_editor_focused_object_index = selected;
        return &sceneSettings.sceneObjects[selected];
    }
    if (s_material_editor_focused_object_index < 0 ||
        s_material_editor_focused_object_index >= object_count) {
        s_material_editor_focused_object_index = ObjectEditorSelectionTrackerLast(object_count);
    }
    if (s_material_editor_focused_object_index >= 0 &&
        s_material_editor_focused_object_index < object_count) {
        return &sceneSettings.sceneObjects[s_material_editor_focused_object_index];
    }
    return NULL;
}

static RayTracingThemePalette material_editor_palette(void) {
    RayTracingThemePalette palette = {0};
    if (!ray_tracing_shared_theme_resolve_palette(&palette)) {
        palette.background_fill = (SDL_Color){46, 46, 52, 255};
        palette.panel_fill = (SDL_Color){58, 58, 68, 230};
        palette.panel_border = (SDL_Color){95, 95, 112, 255};
        palette.button_fill = (SDL_Color){180, 180, 180, 255};
        palette.button_active_fill = (SDL_Color){70, 140, 215, 255};
        palette.button_text = (SDL_Color){0, 0, 0, 255};
        palette.text_primary = (SDL_Color){220, 220, 230, 255};
        palette.text_muted = (SDL_Color){210, 210, 215, 255};
        palette.accent_primary = (SDL_Color){120, 200, 255, 255};
    }
    return palette;
}

static void material_editor_draw_button(SDL_Renderer* renderer,
                                        SDL_Rect rect,
                                        const char* label,
                                        bool active,
                                        RayTracingThemePalette palette) {
    SDL_Color fill = active ? ray_tracing_theme_resolve_button_active_fill(palette)
                            : palette.button_fill;
    SDL_Color text = ray_tracing_theme_choose_button_text(fill, palette);
    if (!renderer || !label || rect.w <= 0 || rect.h <= 0) return;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           palette.panel_border.a);
    SDL_RenderDrawRect(renderer, &rect);
    RenderButtonTextWithColor(renderer, rect, label, text);
}

static double material_editor_value_for_slider(const SceneObject* obj, MaterialEditorSliderKind kind) {
    if (!obj) return 0.0;
    if (s_material_editor_active_face_group_index >= 0) {
        int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
        SceneEditorMaterialFacePlacementField field =
            SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH;
        if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_SCALE;
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_U;
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_V;
        } else {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH;
        }
        return SceneEditorMaterialFacePlacementGetNormalizedValue(obj,
                                                                  focused_object_index,
                                                                  s_material_editor_active_face_group_index,
                                                                  field);
    }
    if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) return material_editor_clamp01(obj->textureStrength);
    if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
        double value = obj->textureScale;
        if (value < 0.25) value = 0.25;
        if (value > 8.0) value = 8.0;
        return (value - 0.25) / 7.75;
    }
    if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) return material_editor_clamp01(obj->textureOffsetU);
    if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) return material_editor_clamp01(obj->textureOffsetV);
    return 0.0;
}

static int material_editor_texture_kind_for_controls(const SceneObject* obj) {
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj) return 0;
    if (s_material_editor_active_face_group_index >= 0) {
        SceneEditorMaterialFacePlacement placement =
            SceneEditorMaterialFacePlacementGetEffective(obj,
                                                         focused_object_index,
                                                         s_material_editor_active_face_group_index);
        return placement.textureId;
    }
    return obj->textureId;
}

static RuntimeMaterialTexture3DParams material_editor_params_for_controls(const SceneObject* obj) {
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj) return RuntimeMaterialTexture3DDefaultParams();
    if (s_material_editor_active_face_group_index >= 0) {
        SceneEditorMaterialFacePlacement placement =
            SceneEditorMaterialFacePlacementGetEffective(obj,
                                                         focused_object_index,
                                                         s_material_editor_active_face_group_index);
        return RuntimeMaterialTexture3DNormalizeParams(placement.params);
    }
    return RuntimeMaterialTexture3DParamsFromObject(obj);
}

static void material_editor_assign_object_params(SceneObject* obj,
                                                 RuntimeMaterialTexture3DParams params) {
    if (!obj) return;
    params = RuntimeMaterialTexture3DNormalizeParams(params);
    obj->texturePatternMode = params.patternMode;
    obj->textureCoverage = params.coverage;
    obj->textureGrain = params.grain;
    obj->textureEdgeSoftness = params.edgeSoftness;
    obj->textureContrast = params.contrast;
    obj->textureFlow = params.flow;
    obj->textureColorDepth = params.colorDepth;
    obj->textureSurfaceDamage = params.surfaceDamage;
    obj->textureSeed = params.seed;
}

static const char* material_editor_label_for_slider(MaterialEditorSliderKind kind) {
    if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) return "Strength";
    if (kind == MATERIAL_EDITOR_SLIDER_SCALE) return "Scale";
    if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) return "Offset U";
    if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) return "Offset V";
    return "";
}

static const char* material_editor_label_for_param(MaterialEditorTextureParamKind kind) {
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE) return "Coverage";
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_GRAIN) return "Grain";
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_EDGE_SOFTNESS) return "Edge";
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_CONTRAST) return "Contrast";
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_FLOW) return "Flow";
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COLOR_DEPTH) return "Color";
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_SURFACE_DAMAGE) return "Damage";
    return "";
}

static double material_editor_value_for_param_slider(const SceneObject* obj,
                                                     MaterialEditorTextureParamKind kind) {
    RuntimeMaterialTexture3DParams params = material_editor_params_for_controls(obj);
    if (!obj || material_editor_texture_param_slot(kind) < 0) return 0.0;
    if (s_material_editor_active_face_group_index >= 0) {
        return SceneEditorMaterialFacePlacementGetTextureParamNormalizedValue(
            obj,
            MaterialEditorResolveFocusedObjectIndex(),
            s_material_editor_active_face_group_index,
            material_editor_texture_param_field(kind));
    }
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE) return params.coverage;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_GRAIN) return params.grain;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_EDGE_SOFTNESS) return params.edgeSoftness;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_CONTRAST) return params.contrast;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_FLOW) return params.flow;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COLOR_DEPTH) return params.colorDepth;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_SURFACE_DAMAGE) return params.surfaceDamage;
    return 0.0;
}

static void material_editor_format_param_value(const SceneObject* obj,
                                               MaterialEditorTextureParamKind kind,
                                               char* out,
                                               size_t out_size) {
    if (!out || out_size == 0) return;
    snprintf(out, out_size, "%.2f", material_editor_value_for_param_slider(obj, kind));
}

static void material_editor_format_slider_value(const SceneObject* obj,
                                                MaterialEditorSliderKind kind,
                                                char* out,
                                                size_t out_size) {
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    SceneEditorMaterialFacePlacement placement;
    if (!out || out_size == 0) return;
    if (!obj) {
        snprintf(out, out_size, "--");
        return;
    }
    if (s_material_editor_active_face_group_index >= 0) {
        placement = SceneEditorMaterialFacePlacementGetEffective(obj,
                                                                 focused_object_index,
                                                                 s_material_editor_active_face_group_index);
        if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) {
            snprintf(out, out_size, "%.2f", placement.strength);
        } else if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
            snprintf(out, out_size, "%.2f", placement.scale);
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) {
            snprintf(out, out_size, "%.2f", placement.offsetU);
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) {
            snprintf(out, out_size, "%.2f", placement.offsetV);
        } else {
            snprintf(out, out_size, "--");
        }
        return;
    }
    if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) {
        snprintf(out, out_size, "%.2f", obj->textureStrength);
    } else if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
        snprintf(out, out_size, "%.2f", obj->textureScale);
    } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) {
        snprintf(out, out_size, "%.2f", obj->textureOffsetU);
    } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) {
        snprintf(out, out_size, "%.2f", obj->textureOffsetV);
    } else {
        snprintf(out, out_size, "--");
    }
}

static void material_editor_draw_slider(SDL_Renderer* renderer,
                                        SDL_Rect bounds,
                                        MaterialEditorSliderKind kind,
                                        const SceneObject* obj,
                                        RayTracingThemePalette palette) {
    double normalized = material_editor_value_for_slider(obj, kind);
    SDL_Rect label_rect = {bounds.x, bounds.y, bounds.w / 2, 14};
    SDL_Rect value_rect = {bounds.x + bounds.w / 2, bounds.y, bounds.w / 2, 14};
    SDL_Rect track = {bounds.x + 2, bounds.y + 16, bounds.w - 4, MATERIAL_EDITOR_SLIDER_TRACK_HEIGHT};
    SDL_Rect knob = {0, 0, MATERIAL_EDITOR_SLIDER_KNOB_WIDTH, track.h + 7};
    char value_text[32];
    int slot = (int)kind - 1;
    if (!renderer || slot < 0 || slot >= 4) return;
    s_slider_sections[slot] = bounds;
    s_slider_tracks[slot] = track;
    material_editor_format_slider_value(obj, kind, value_text, sizeof(value_text));
    RenderLabelTextLeft(renderer, label_rect, material_editor_label_for_slider(kind), palette.text_primary);
    RenderLabelTextLeft(renderer, value_rect, value_text, palette.text_muted);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           palette.panel_border.a);
    SDL_RenderFillRect(renderer, &track);
    knob.x = track.x + (int)lround(normalized * (double)(track.w - MATERIAL_EDITOR_SLIDER_KNOB_WIDTH));
    knob.y = track.y - 3;
    SDL_SetRenderDrawColor(renderer,
                           palette.accent_primary.r,
                           palette.accent_primary.g,
                           palette.accent_primary.b,
                           255);
    SDL_RenderFillRect(renderer, &knob);
}

static void material_editor_draw_param_slider(SDL_Renderer* renderer,
                                              SDL_Rect bounds,
                                              MaterialEditorTextureParamKind kind,
                                              const SceneObject* obj,
                                              RayTracingThemePalette palette) {
    double normalized = material_editor_value_for_param_slider(obj, kind);
    SDL_Rect label_rect = {bounds.x, bounds.y, bounds.w / 2, 13};
    SDL_Rect value_rect = {bounds.x + bounds.w / 2, bounds.y, bounds.w / 2, 13};
    SDL_Rect track = {bounds.x + 2, bounds.y + 16, bounds.w - 4, MATERIAL_EDITOR_SLIDER_TRACK_HEIGHT};
    SDL_Rect knob = {0, 0, MATERIAL_EDITOR_SLIDER_KNOB_WIDTH, track.h + 6};
    char value_text[32];
    int slot = material_editor_texture_param_slot(kind);
    if (!renderer || slot < 0) return;
    s_param_sections[slot] = bounds;
    s_param_tracks[slot] = track;
    material_editor_format_param_value(obj, kind, value_text, sizeof(value_text));
    RenderLabelTextLeft(renderer, label_rect, material_editor_label_for_param(kind), palette.text_primary);
    RenderLabelTextLeft(renderer, value_rect, value_text, palette.text_muted);
    SDL_SetRenderDrawColor(renderer,
                           palette.panel_border.r,
                           palette.panel_border.g,
                           palette.panel_border.b,
                           palette.panel_border.a);
    SDL_RenderFillRect(renderer, &track);
    knob.x = track.x + (int)lround(normalized * (double)(track.w - MATERIAL_EDITOR_SLIDER_KNOB_WIDTH));
    knob.y = track.y - 3;
    SDL_SetRenderDrawColor(renderer,
                           palette.accent_primary.r,
                           palette.accent_primary.g,
                           palette.accent_primary.b,
                           255);
    SDL_RenderFillRect(renderer, &knob);
}

void InitializeMaterialEditor(void) {
    SceneObject* obj = material_editor_focused_object();
    if (!obj && sceneSettings.objectCount > 0) {
        s_material_editor_focused_object_index = 0;
    }
    s_material_editor_active_slider = MATERIAL_EDITOR_SLIDER_NONE;
    s_material_editor_active_param_slider = MATERIAL_EDITOR_TEXTURE_PARAM_NONE;
    s_group_scroll_offset = 0;
    MaterialEditorClearTriangleSelection();
}

void RenderMaterialEditor(SDL_Renderer* renderer) {
    SceneObject* obj = material_editor_focused_object();
    Camera original = sceneSettings.camera;
    Camera focused = original;
    RayTracingThemePalette palette = material_editor_palette();
    SDL_Rect bg = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight};
    if (!renderer) return;

    SDL_SetRenderDrawColor(renderer,
                           palette.background_fill.r,
                           palette.background_fill.g,
                           palette.background_fill.b,
                           255);
    SDL_RenderFillRect(renderer, &bg);
    if (!obj) return;

    focused.x = obj->x;
    focused.y = obj->y;
    focused.rotation = 0.0;
    focused.zoom = 2.0;
    if (obj->radius > 1.0) {
        double span = fmin((double)sceneSettings.windowWidth, (double)sceneSettings.windowHeight);
        focused.zoom = fmax(0.1, span * 0.32 / obj->radius);
    }
    sceneSettings.camera = focused;
    SDL_SetRenderDrawColor(renderer,
                           (Uint8)((obj->color >> 16) & 0xFF),
                           (Uint8)((obj->color >> 8) & 0xFF),
                           (Uint8)(obj->color & 0xFF),
                           255);
    RenderSceneObject(renderer, obj, true);
    SDL_SetRenderDrawColor(renderer, 255, 120, 70, 255);
    RenderSceneObject(renderer, obj, false);
    sceneSettings.camera = original;
}

int MaterialEditorRenderPaneControls(SDL_Renderer* renderer,
                                     SDL_Rect content_bounds,
                                     int top_y,
                                     int bottom_y) {
    SceneObject* obj = material_editor_focused_object();
    RayTracingThemePalette palette = material_editor_palette();
    int cursor_y = top_y;
    int third_w = 0;
    int focused_index = -1;
    int selected_faces = 0;
    int focused_faces = 0;
    char line[128];
    char edit_text[48];
    memset(s_slider_sections, 0, sizeof(s_slider_sections));
    memset(s_slider_tracks, 0, sizeof(s_slider_tracks));
    memset(s_param_sections, 0, sizeof(s_param_sections));
    memset(s_param_tracks, 0, sizeof(s_param_tracks));
    memset(s_pattern_rects, 0, sizeof(s_pattern_rects));
    s_texture_none_rect = (SDL_Rect){0, 0, 0, 0};
    s_texture_rust_rect = (SDL_Rect){0, 0, 0, 0};
    s_texture_fog_rect = (SDL_Rect){0, 0, 0, 0};
    s_solid_faces_rect = (SDL_Rect){0, 0, 0, 0};
    s_reset_face_rect = (SDL_Rect){0, 0, 0, 0};
    s_copy_face_rect = (SDL_Rect){0, 0, 0, 0};
    material_editor_reset_group_list_layout();
    if (!renderer || content_bounds.w <= 0 || cursor_y >= bottom_y) return cursor_y;

    if (!obj) {
        SDL_Rect label = {content_bounds.x, cursor_y, content_bounds.w, bottom_y - cursor_y};
        RenderLabelTextWrappedLeft(renderer, label, "No object selected. Select an object in Objects mode first.", palette.text_muted);
        return bottom_y;
    }

    focused_index = MaterialEditorResolveFocusedObjectIndex();
    selected_faces = MaterialEditorSelectedFaceGroupCount();
    focused_faces = MaterialEditorFocusedFaceGroupCount();
    if (s_material_editor_active_face_group_index >= 0) {
        bool has_override =
            SceneEditorMaterialFacePlacementHasOverride(focused_index,
                                                        s_material_editor_active_face_group_index);
        snprintf(edit_text,
                 sizeof(edit_text),
                 "Face #%d %s",
                 s_material_editor_active_face_group_index,
                 has_override ? "override" : "default");
    } else {
        snprintf(edit_text, sizeof(edit_text), "Object defaults");
    }
    snprintf(line,
             sizeof(line),
             "Obj #%d  mat=%d tex=%d",
             focused_index,
             obj->material_id,
             material_editor_texture_kind_for_controls(obj));
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 16},
                        line,
                        palette.text_primary);
    cursor_y += 20;

    snprintf(line,
             sizeof(line),
             "Faces %d/%d selected  Edit %s",
             selected_faces,
             focused_faces,
             edit_text);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 16},
                        line,
                        palette.text_muted);
    cursor_y += 22;

    third_w = (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP * 2) / 3;
    if (cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
        s_texture_none_rect = (SDL_Rect){content_bounds.x, cursor_y, third_w, MATERIAL_EDITOR_BUTTON_HEIGHT};
        s_texture_rust_rect = (SDL_Rect){s_texture_none_rect.x + third_w + MATERIAL_EDITOR_BUTTON_GAP,
                                         cursor_y,
                                         third_w,
                                         MATERIAL_EDITOR_BUTTON_HEIGHT};
        s_texture_fog_rect = (SDL_Rect){s_texture_rust_rect.x + third_w + MATERIAL_EDITOR_BUTTON_GAP,
                                        cursor_y,
                                        content_bounds.w - third_w * 2 - MATERIAL_EDITOR_BUTTON_GAP * 2,
                                        MATERIAL_EDITOR_BUTTON_HEIGHT};
        int active_texture = material_editor_texture_kind_for_controls(obj);
        material_editor_draw_button(renderer, s_texture_none_rect, "None", active_texture == 0, palette);
        material_editor_draw_button(renderer, s_texture_rust_rect, "Rust", active_texture == 1, palette);
        material_editor_draw_button(renderer, s_texture_fog_rect, "Fog", active_texture == 2, palette);
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    }

    {
        int grid_y = cursor_y;
        int drawn_rows = 0;
        int col_w = (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP) / 2;
        for (int i = 0; i < 4; ++i) {
            MaterialEditorSliderKind kind = (MaterialEditorSliderKind)(i + 1);
            int col = i % 2;
            int row = i / 2;
            int x = content_bounds.x + col * (col_w + MATERIAL_EDITOR_BUTTON_GAP);
            int y = grid_y + row * (MATERIAL_EDITOR_SLIDER_HEIGHT + 5);
            int w = (col == 1) ? content_bounds.x + content_bounds.w - x : col_w;
            if (y + MATERIAL_EDITOR_SLIDER_HEIGHT > bottom_y) break;
            material_editor_draw_slider(renderer,
                                        (SDL_Rect){x, y, w, MATERIAL_EDITOR_SLIDER_HEIGHT},
                                        kind,
                                        obj,
                                        palette);
            if (drawn_rows < row + 1) drawn_rows = row + 1;
        }
        cursor_y += drawn_rows * (MATERIAL_EDITOR_SLIDER_HEIGHT + 5);
    }

    if (material_editor_texture_kind_for_controls(obj) > RUNTIME_MATERIAL_TEXTURE_3D_NONE) {
        RuntimeMaterialTexture3DParams params = material_editor_params_for_controls(obj);
        int pattern_w = (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP * 3) / 4;
        const char* pattern_labels[MATERIAL_EDITOR_PATTERN_BUTTON_COUNT] = {
            "Default", "Speck", "Patch", "Flow"
        };
        if (material_editor_has_room_for_optional_control(cursor_y,
                                                          MATERIAL_EDITOR_BUTTON_HEIGHT,
                                                          bottom_y)) {
            for (int i = 0; i < MATERIAL_EDITOR_PATTERN_BUTTON_COUNT; ++i) {
                int x = content_bounds.x + i * (pattern_w + MATERIAL_EDITOR_BUTTON_GAP);
                int w = (i == MATERIAL_EDITOR_PATTERN_BUTTON_COUNT - 1)
                            ? content_bounds.x + content_bounds.w - x
                            : pattern_w;
                s_pattern_rects[i] = (SDL_Rect){x, cursor_y, w, MATERIAL_EDITOR_BUTTON_HEIGHT};
                material_editor_draw_button(renderer,
                                            s_pattern_rects[i],
                                            pattern_labels[i],
                                            params.patternMode == i,
                                            palette);
            }
            cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
        }
        {
            int grid_y = cursor_y;
            int drawn_rows = 0;
            int col_w = (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP) / 2;
            for (int i = 0; i < MATERIAL_EDITOR_PARAM_SLIDER_COUNT; ++i) {
                MaterialEditorTextureParamKind kind = (MaterialEditorTextureParamKind)(i + 1);
                int col = i % 2;
                int row = i / 2;
                int x = content_bounds.x + col * (col_w + MATERIAL_EDITOR_BUTTON_GAP);
                int y = grid_y + row * (MATERIAL_EDITOR_PARAM_SLIDER_HEIGHT + 4);
                int w = (col == 1) ? content_bounds.x + content_bounds.w - x : col_w;
                if (!material_editor_has_room_for_optional_control(y,
                                                                   MATERIAL_EDITOR_PARAM_SLIDER_HEIGHT,
                                                                   bottom_y)) {
                    break;
                }
                material_editor_draw_param_slider(renderer,
                                                  (SDL_Rect){x, y, w, MATERIAL_EDITOR_PARAM_SLIDER_HEIGHT},
                                                  kind,
                                                  obj,
                                                  palette);
                if (drawn_rows < row + 1) drawn_rows = row + 1;
            }
            cursor_y += drawn_rows * (MATERIAL_EDITOR_PARAM_SLIDER_HEIGHT + 4);
        }
    }

    if (cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
        int reset_w = (s_material_editor_active_face_group_index >= 0 &&
                       SceneEditorMaterialFacePlacementHasOverride(
                           focused_index,
                           s_material_editor_active_face_group_index))
                          ? (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP) / 2
                          : 0;
        s_solid_faces_rect = (SDL_Rect){content_bounds.x,
                                        cursor_y,
                                        reset_w > 0 ? reset_w : content_bounds.w,
                                        MATERIAL_EDITOR_BUTTON_HEIGHT};
        material_editor_draw_button(renderer,
                                    s_solid_faces_rect,
                                    "Solid Faces",
                                    s_material_editor_solid_faces_enabled,
                                    palette);
        if (reset_w > 0) {
            s_reset_face_rect = (SDL_Rect){s_solid_faces_rect.x + s_solid_faces_rect.w +
                                              MATERIAL_EDITOR_BUTTON_GAP,
                                          cursor_y,
                                          content_bounds.w - s_solid_faces_rect.w -
                                              MATERIAL_EDITOR_BUTTON_GAP,
                                          MATERIAL_EDITOR_BUTTON_HEIGHT};
            material_editor_draw_button(renderer, s_reset_face_rect, "Reset Face", false, palette);
        }
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    }
    if (s_material_editor_active_face_group_index >= 0 &&
        selected_faces > 1 &&
        cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
        s_copy_face_rect = (SDL_Rect){content_bounds.x,
                                      cursor_y,
                                      content_bounds.w,
                                      MATERIAL_EDITOR_BUTTON_HEIGHT};
        material_editor_draw_button(renderer, s_copy_face_rect, "Copy to Selected", false, palette);
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    }
    cursor_y = material_editor_draw_group_list(renderer, content_bounds, cursor_y, bottom_y, palette);
    return cursor_y;
}

void HandleMaterialEditorEvents(SDL_Event* event) {
    if (!event) return;
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        if (material_editor_point_in_rect(mx, my, &s_texture_none_rect)) {
            MaterialEditorApplyTextureKindToFocused(0);
            return;
        }
        if (material_editor_point_in_rect(mx, my, &s_texture_rust_rect)) {
            MaterialEditorApplyTextureKindToFocused(1);
            return;
        }
        if (material_editor_point_in_rect(mx, my, &s_texture_fog_rect)) {
            MaterialEditorApplyTextureKindToFocused(2);
            return;
        }
        if (material_editor_point_in_rect(mx, my, &s_solid_faces_rect)) {
            MaterialEditorToggleSolidFaces();
            return;
        }
        if (material_editor_point_in_rect(mx, my, &s_reset_face_rect)) {
            MaterialEditorResetActiveFacePlacement();
            return;
        }
        if (material_editor_point_in_rect(mx, my, &s_copy_face_rect)) {
            MaterialEditorCopyActiveFacePlacementToSelected();
            return;
        }
        for (int i = 0; i < MATERIAL_EDITOR_PATTERN_BUTTON_COUNT; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_pattern_rects[i])) {
                MaterialEditorApplyTexturePatternToFocused(i);
                return;
            }
        }
        if (material_editor_point_in_rect(mx, my, &s_clear_groups_rect)) {
            MaterialEditorClearTriangleSelection();
            return;
        }
        for (int i = 0; i < s_group_row_count; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_group_row_rects[i])) {
                MaterialEditorSetFaceGroupSelection(&s_group_row_addresses[i]);
                return;
            }
        }
        for (int i = 0; i < 4; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_slider_sections[i])) {
                s_material_editor_active_slider = (MaterialEditorSliderKind)(i + 1);
                MaterialEditorApplySliderValueToFocused(s_material_editor_active_slider,
                                                        material_editor_slider_value_from_x(&s_slider_tracks[i], mx));
                return;
            }
        }
        for (int i = 0; i < MATERIAL_EDITOR_PARAM_SLIDER_COUNT; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_param_sections[i])) {
                s_material_editor_active_param_slider = (MaterialEditorTextureParamKind)(i + 1);
                MaterialEditorApplyTextureParamValueToFocused(
                    s_material_editor_active_param_slider,
                    material_editor_slider_value_from_x(&s_param_tracks[i], mx));
                return;
            }
        }
    } else if (event->type == SDL_MOUSEMOTION &&
               (event->motion.state & SDL_BUTTON_LMASK) &&
               s_material_editor_active_slider != MATERIAL_EDITOR_SLIDER_NONE) {
        int slot = (int)s_material_editor_active_slider - 1;
        if (slot >= 0 && slot < 4) {
            MaterialEditorApplySliderValueToFocused(s_material_editor_active_slider,
                                                    material_editor_slider_value_from_x(&s_slider_tracks[slot],
                                                                                       event->motion.x));
        }
    } else if (event->type == SDL_MOUSEMOTION &&
               (event->motion.state & SDL_BUTTON_LMASK) &&
               s_material_editor_active_param_slider != MATERIAL_EDITOR_TEXTURE_PARAM_NONE) {
        int slot = material_editor_texture_param_slot(s_material_editor_active_param_slider);
        if (slot >= 0 && slot < MATERIAL_EDITOR_PARAM_SLIDER_COUNT) {
            MaterialEditorApplyTextureParamValueToFocused(
                s_material_editor_active_param_slider,
                material_editor_slider_value_from_x(&s_param_tracks[slot], event->motion.x));
        }
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        s_material_editor_active_slider = MATERIAL_EDITOR_SLIDER_NONE;
        s_material_editor_active_param_slider = MATERIAL_EDITOR_TEXTURE_PARAM_NONE;
    } else if (event->type == SDL_MOUSEWHEEL) {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        if (material_editor_point_in_rect(mx, my, &s_group_panel_rect) ||
            material_editor_point_in_rect(mx, my, &s_group_list_rect)) {
            material_editor_scroll_group_list(event->wheel.y);
        }
    }
}

bool MaterialEditorHandleCanvasPointerDown(const SceneEditorDigestOverlayProjector* projector,
                                           int mx,
                                           int my,
                                           bool additive) {
    SceneEditorMaterialPreviewTriangleAddress picked = {0};
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!projector || focused_object_index < 0) return false;
    if (SceneEditorMaterialPreviewPickTriangle(focused_object_index,
                                               projector,
                                               mx,
                                               my,
                                               &picked)) {
        if (additive) {
            return MaterialEditorToggleFaceGroupSelection(&picked);
        }
        return MaterialEditorSetFaceGroupSelection(&picked);
    }
    if (!additive) {
        MaterialEditorClearTriangleSelection();
        return true;
    }
    return false;
}

MaterialEditorHitRegion MaterialEditorHitRegionAtPoint(int mx, int my) {
    if (material_editor_point_in_rect(mx, my, &s_texture_none_rect) ||
        material_editor_point_in_rect(mx, my, &s_texture_rust_rect) ||
        material_editor_point_in_rect(mx, my, &s_texture_fog_rect) ||
        material_editor_point_in_rect(mx, my, &s_solid_faces_rect) ||
        material_editor_point_in_rect(mx, my, &s_reset_face_rect) ||
        material_editor_point_in_rect(mx, my, &s_copy_face_rect) ||
        material_editor_point_in_rect(mx, my, &s_clear_groups_rect)) {
        return MATERIAL_EDITOR_HIT_CONTROLS;
    }
    for (int i = 0; i < MATERIAL_EDITOR_PATTERN_BUTTON_COUNT; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_pattern_rects[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    for (int i = 0; i < s_group_row_count; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_group_row_rects[i])) {
            return MATERIAL_EDITOR_HIT_LIST_PANEL;
        }
    }
    if (material_editor_point_in_rect(mx, my, &s_group_panel_rect) ||
        material_editor_point_in_rect(mx, my, &s_group_list_rect)) {
        return MATERIAL_EDITOR_HIT_LIST_PANEL;
    }
    for (int i = 0; i < 4; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_slider_sections[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    for (int i = 0; i < MATERIAL_EDITOR_PARAM_SLIDER_COUNT; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_param_sections[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    return MATERIAL_EDITOR_HIT_CANVAS;
}

int MaterialEditorResolveFocusedObjectIndex(void) {
    SceneObject* obj = material_editor_focused_object();
    if (!obj) return -1;
    return s_material_editor_focused_object_index;
}

void MaterialEditorSetFocusedObjectIndex(int index) {
    if (index >= 0 && index < sceneSettings.objectCount) {
        if (s_material_editor_focused_object_index != index) {
            MaterialEditorClearTriangleSelection();
            s_group_scroll_offset = 0;
        }
        s_material_editor_focused_object_index = index;
        ObjectEditorSelectionTrackerSetCurrent(index, sceneSettings.objectCount);
    }
}

void MaterialEditorClearTriangleSelection(void) {
    memset(s_material_editor_selected_triangles, 0, sizeof(s_material_editor_selected_triangles));
    s_material_editor_selected_triangle_count = 0;
    s_material_editor_active_face_group_index = -1;
    material_editor_reset_group_list_layout();
}

int MaterialEditorSelectedTriangleCount(void) {
    return s_material_editor_selected_triangle_count;
}

int MaterialEditorSelectedFaceGroupCount(void) {
    int count = 0;
    for (int i = 0; i < s_material_editor_selected_triangle_count; ++i) {
        int face_group = s_material_editor_selected_triangles[i].faceGroupIndex;
        if (face_group >= 0 && !material_editor_selected_face_group_seen(face_group, i)) {
            count += 1;
        }
    }
    return count;
}

int MaterialEditorFocusedFaceGroupCount(void) {
    MaterialEditorFaceGroupInfo groups[MATERIAL_EDITOR_MAX_FACE_GROUPS];
    return material_editor_collect_focused_face_groups(groups, MATERIAL_EDITOR_MAX_FACE_GROUPS);
}

int MaterialEditorGetActiveFaceGroupIndex(void) {
    return s_material_editor_active_face_group_index;
}

bool MaterialEditorSetActiveFaceGroupIndex(int face_group_index) {
    if (!material_editor_selected_face_group_exists(face_group_index)) return false;
    s_material_editor_active_face_group_index = face_group_index;
    return true;
}

bool MaterialEditorSetFaceGroupSelectionByIndex(int face_group_index) {
    MaterialEditorFaceGroupInfo groups[MATERIAL_EDITOR_MAX_FACE_GROUPS];
    int group_count = material_editor_collect_focused_face_groups(groups, MATERIAL_EDITOR_MAX_FACE_GROUPS);
    int index = material_editor_find_face_group_info(groups, group_count, face_group_index);
    if (index < 0) return false;
    return MaterialEditorSetFaceGroupSelection(&groups[index].representative);
}

bool MaterialEditorGetSelectedTriangle(int index, SceneEditorMaterialPreviewTriangleAddress* out_address) {
    if (!out_address) return false;
    if (index < 0 || index >= s_material_editor_selected_triangle_count) return false;
    *out_address = s_material_editor_selected_triangles[index];
    return true;
}

bool MaterialEditorSetTriangleSelection(const SceneEditorMaterialPreviewTriangleAddress* address) {
    if (!address) return false;
    s_material_editor_selected_triangles[0] = *address;
    s_material_editor_selected_triangle_count = 1;
    s_material_editor_active_face_group_index = address->faceGroupIndex;
    return true;
}

bool MaterialEditorToggleTriangleSelection(const SceneEditorMaterialPreviewTriangleAddress* address) {
    int existing = material_editor_find_selected_triangle(address);
    if (!address) return false;
    if (existing >= 0) {
        int removed_face_group = s_material_editor_selected_triangles[existing].faceGroupIndex;
        for (int i = existing; i + 1 < s_material_editor_selected_triangle_count; ++i) {
            s_material_editor_selected_triangles[i] = s_material_editor_selected_triangles[i + 1];
        }
        s_material_editor_selected_triangle_count -= 1;
        if (s_material_editor_selected_triangle_count < 0) {
            s_material_editor_selected_triangle_count = 0;
        }
        if (!material_editor_selected_face_group_exists(removed_face_group) &&
            s_material_editor_active_face_group_index == removed_face_group) {
            s_material_editor_active_face_group_index = -1;
        }
        return true;
    }
    if (!material_editor_add_triangle_selection(address)) return false;
    s_material_editor_active_face_group_index = address->faceGroupIndex;
    return true;
}

bool MaterialEditorSetFaceGroupSelection(const SceneEditorMaterialPreviewTriangleAddress* address) {
    SceneEditorMaterialPreviewTriangleAddress group_addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    int group_count = material_editor_collect_face_group_triangles(address,
                                                                   group_addresses,
                                                                   SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES);
    if (!address || group_count <= 0) return false;
    MaterialEditorClearTriangleSelection();
    for (int i = 0; i < group_count; ++i) {
        if (!material_editor_add_triangle_selection(&group_addresses[i])) return false;
    }
    s_material_editor_active_face_group_index = address->faceGroupIndex;
    return true;
}

bool MaterialEditorToggleFaceGroupSelection(const SceneEditorMaterialPreviewTriangleAddress* address) {
    SceneEditorMaterialPreviewTriangleAddress group_addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    int group_count = 0;
    if (!address) return false;
    if (material_editor_face_group_fully_selected(address)) {
        bool removed = material_editor_remove_face_group_selection(address->sceneObjectIndex,
                                                                   address->faceGroupIndex);
        if (removed && s_material_editor_active_face_group_index == address->faceGroupIndex) {
            s_material_editor_active_face_group_index = -1;
        }
        return removed;
    }
    group_count = material_editor_collect_face_group_triangles(address,
                                                              group_addresses,
                                                              SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES);
    if (group_count <= 0) return false;
    for (int i = 0; i < group_count; ++i) {
        if (!material_editor_add_triangle_selection(&group_addresses[i])) return false;
    }
    s_material_editor_active_face_group_index = address->faceGroupIndex;
    return true;
}

MaterialEditorViewMode MaterialEditorGetViewMode(void) {
    return s_material_editor_view_mode;
}

void MaterialEditorSetViewMode(MaterialEditorViewMode mode) {
    if (mode == MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN ||
        mode == MATERIAL_EDITOR_VIEW_SCENE_PLACEMENT) {
        s_material_editor_view_mode = mode;
    }
}

bool MaterialEditorGetSolidFacesEnabled(void) {
    return s_material_editor_solid_faces_enabled;
}

void MaterialEditorSetSolidFacesEnabled(bool enabled) {
    s_material_editor_solid_faces_enabled = enabled;
}

bool MaterialEditorToggleSolidFaces(void) {
    s_material_editor_solid_faces_enabled = !s_material_editor_solid_faces_enabled;
    return s_material_editor_solid_faces_enabled;
}

bool MaterialEditorApplyTextureKindToFocused(int texture_id) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj) return false;
    if (texture_id < 0 || texture_id > 2) return false;
    if (s_material_editor_active_face_group_index >= 0) {
        if (!SceneEditorMaterialFacePlacementApplyTextureKind(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index,
                texture_id)) {
            return false;
        }
        MarkObjectDirty(obj);
        return true;
    }
    obj->textureId = texture_id;
    MarkObjectDirty(obj);
    return true;
}

bool MaterialEditorApplySliderValueToFocused(MaterialEditorSliderKind kind, double value) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    SceneEditorMaterialFacePlacementField field =
        SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH;
    if (!obj) return false;
    value = material_editor_clamp01(value);
    if (s_material_editor_active_face_group_index >= 0) {
        if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH;
        } else if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_SCALE;
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_U;
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_V;
        } else {
            return false;
        }
        if (!SceneEditorMaterialFacePlacementApplyNormalizedValue(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index,
                field,
                value)) {
            return false;
        }
        MarkObjectDirty(obj);
        return true;
    }
    if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) {
        obj->textureStrength = value;
    } else if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
        obj->textureScale = 0.25 + value * 7.75;
    } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) {
        obj->textureOffsetU = value;
    } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) {
        obj->textureOffsetV = value;
    } else {
        return false;
    }
    MarkObjectDirty(obj);
    return true;
}

bool MaterialEditorApplyTexturePatternToFocused(int pattern_mode) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTexture3DParams params;
    if (!obj) return false;
    if (s_material_editor_active_face_group_index >= 0) {
        if (!SceneEditorMaterialFacePlacementApplyTextureParamPatternMode(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index,
                pattern_mode)) {
            return false;
        }
        MarkObjectDirty(obj);
        return true;
    }
    params = RuntimeMaterialTexture3DParamsFromObject(obj);
    params.patternMode = pattern_mode;
    material_editor_assign_object_params(obj, params);
    MarkObjectDirty(obj);
    return true;
}

bool MaterialEditorApplyTextureParamValueToFocused(MaterialEditorTextureParamKind kind, double value) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTexture3DParams params;
    if (!obj) return false;
    if (material_editor_texture_param_slot(kind) < 0) return false;
    value = material_editor_clamp01(value);
    if (s_material_editor_active_face_group_index >= 0) {
        if (!SceneEditorMaterialFacePlacementApplyTextureParamNormalizedValue(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index,
                material_editor_texture_param_field(kind),
                value)) {
            return false;
        }
        MarkObjectDirty(obj);
        return true;
    }
    params = RuntimeMaterialTexture3DParamsFromObject(obj);
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE) {
        params.coverage = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_GRAIN) {
        params.grain = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_EDGE_SOFTNESS) {
        params.edgeSoftness = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_CONTRAST) {
        params.contrast = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_FLOW) {
        params.flow = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COLOR_DEPTH) {
        params.colorDepth = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_SURFACE_DAMAGE) {
        params.surfaceDamage = value;
    } else {
        return false;
    }
    material_editor_assign_object_params(obj, params);
    MarkObjectDirty(obj);
    return true;
}

bool MaterialEditorResetActiveFacePlacement(void) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0 || s_material_editor_active_face_group_index < 0) {
        return false;
    }
    if (!SceneEditorMaterialFacePlacementResetFace(focused_object_index,
                                                  s_material_editor_active_face_group_index)) {
        return false;
    }
    MarkObjectDirty(obj);
    return true;
}

bool MaterialEditorCopyActiveFacePlacementToSelected(void) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    SceneEditorMaterialFacePlacement active;
    int copied = 0;
    int seen[MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES];
    int seen_count = 0;
    if (!obj || focused_object_index < 0 || s_material_editor_active_face_group_index < 0) {
        return false;
    }
    active = SceneEditorMaterialFacePlacementGetEffective(obj,
                                                          focused_object_index,
                                                          s_material_editor_active_face_group_index);
    for (int i = 0; i < MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES; ++i) {
        seen[i] = -1;
    }
    for (int i = 0; i < s_material_editor_selected_triangle_count; ++i) {
        int face_group = s_material_editor_selected_triangles[i].faceGroupIndex;
        bool already_seen = false;
        SceneEditorMaterialFacePlacement target = active;
        if (s_material_editor_selected_triangles[i].sceneObjectIndex != focused_object_index ||
            face_group < 0 ||
            face_group == s_material_editor_active_face_group_index) {
            continue;
        }
        for (int j = 0; j < seen_count; ++j) {
            if (seen[j] == face_group) {
                already_seen = true;
                break;
            }
        }
        if (already_seen) continue;
        if (seen_count < MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES) {
            seen[seen_count] = face_group;
            seen_count += 1;
        }
        target.sceneObjectIndex = focused_object_index;
        target.faceGroupIndex = face_group;
        target.hasOverride = true;
        if (SceneEditorMaterialFacePlacementSetOverride(&target)) {
            copied += 1;
        }
    }
    if (copied <= 0) return false;
    MarkObjectDirty(obj);
    return true;
}
