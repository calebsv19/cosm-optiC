#ifndef MATERIAL_EDITOR_INTERNAL_H
#define MATERIAL_EDITOR_INTERNAL_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "config/config_manager.h"
#include "editor/material_editor.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_preview.h"
#include "render/render_helper.h"
#include "scene/object_manager.h"
#include "ui/shared_theme_font_adapter.h"

#define MATERIAL_EDITOR_BUTTON_HEIGHT 22
#define MATERIAL_EDITOR_BUTTON_GAP 5
#define MATERIAL_EDITOR_CONTROL_GAP 7
#define MATERIAL_EDITOR_SLIDER_HEIGHT 30
#define MATERIAL_EDITOR_SLIDER_TRACK_HEIGHT 7
#define MATERIAL_EDITOR_SLIDER_KNOB_WIDTH 11
#define MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES 64
#define MATERIAL_EDITOR_MAX_GROUP_ROWS 16
#define MATERIAL_EDITOR_GROUP_ROW_HEIGHT 24
#define MATERIAL_EDITOR_GROUP_ROW_GAP 2
#define MATERIAL_EDITOR_MAX_FACE_GROUPS SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES
#define MATERIAL_EDITOR_PARAM_SLIDER_COUNT 7
#define MATERIAL_EDITOR_PATTERN_BUTTON_COUNT 4
#define MATERIAL_EDITOR_MIN_GROUP_LIST_HEIGHT 150
#define MATERIAL_EDITOR_MAX_LAYER_ROWS 4
#define MATERIAL_EDITOR_LAYER_ROW_HEIGHT 22
#define MATERIAL_EDITOR_LAYER_ROW_GAP 2
#define MATERIAL_EDITOR_LAYER_ACTION_COUNT 5
#define MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT 4
#define MATERIAL_EDITOR_KNOB_HEIGHT 58
#define MATERIAL_EDITOR_SECTION_LABEL(renderer, bounds, y, label, palette) \
    RenderLabelTextLeft((renderer), (SDL_Rect){(bounds).x, (y), (bounds).w, 14}, (label), (palette).text_primary)

typedef struct MaterialEditorFaceGroupInfo {
    int face_group_index;
    int triangle_count;
    int selected_count;
    SceneEditorMaterialPreviewTriangleAddress representative;
} MaterialEditorFaceGroupInfo;

extern int s_material_editor_focused_object_index;
extern MaterialEditorSliderKind s_material_editor_active_slider;
extern MaterialEditorTextureParamKind s_material_editor_active_param_slider;
extern MaterialEditorViewMode s_material_editor_view_mode;
extern bool s_material_editor_solid_faces_enabled;
extern int s_material_editor_active_face_group_index;
extern SceneEditorMaterialPreviewTriangleAddress
    s_material_editor_selected_triangles[MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES];
extern int s_material_editor_selected_triangle_count;
extern SDL_Rect s_texture_none_rect;
extern SDL_Rect s_texture_rust_rect;
extern SDL_Rect s_texture_fog_rect;
extern SDL_Rect s_layer_kind_rects[MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT];
extern RuntimeMaterialTextureLayerKind
    s_layer_kind_rect_kinds[MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT];
extern SDL_Rect s_solid_faces_rect;
extern SDL_Rect s_reset_face_rect;
extern SDL_Rect s_copy_face_rect;
extern SDL_Rect s_clear_groups_rect;
extern SDL_Rect s_group_panel_rect;
extern SDL_Rect s_group_list_rect;
extern SDL_Rect s_group_row_rects[MATERIAL_EDITOR_MAX_GROUP_ROWS];
extern int s_group_row_face_groups[MATERIAL_EDITOR_MAX_GROUP_ROWS];
extern SceneEditorMaterialPreviewTriangleAddress
    s_group_row_addresses[MATERIAL_EDITOR_MAX_GROUP_ROWS];
extern char s_group_row_labels[MATERIAL_EDITOR_MAX_GROUP_ROWS][64];
extern char s_group_header_label[64];
extern int s_group_row_count;
extern int s_group_scroll_offset;
extern int s_group_visible_capacity;
extern int s_group_total_count;
extern SDL_Rect s_slider_sections[4];
extern SDL_Rect s_slider_tracks[4];
extern SDL_Rect s_param_sections[MATERIAL_EDITOR_PARAM_SLIDER_COUNT];
extern SDL_Rect s_pattern_rects[MATERIAL_EDITOR_PATTERN_BUTTON_COUNT];
extern SDL_Rect s_layer_panel_rect;
extern SDL_Rect s_layer_list_rect;
extern SDL_Rect s_layer_row_rects[MATERIAL_EDITOR_MAX_LAYER_ROWS];
extern SDL_Rect s_layer_toggle_rects[MATERIAL_EDITOR_MAX_LAYER_ROWS];
extern int s_layer_row_indices[MATERIAL_EDITOR_MAX_LAYER_ROWS];
extern char s_layer_row_labels[MATERIAL_EDITOR_MAX_LAYER_ROWS][72];
extern SDL_Rect s_layer_action_rects[MATERIAL_EDITOR_LAYER_ACTION_COUNT];
extern int s_layer_row_count;
extern int s_layer_scroll_offset;
extern int s_layer_visible_capacity;
extern int s_layer_total_count;
extern int s_material_editor_param_drag_start_y;
extern double s_material_editor_param_drag_start_value;

SceneEditorMaterialTextureParamField material_editor_texture_param_field(
    MaterialEditorTextureParamKind kind);
double material_editor_clamp01(double value);
int material_editor_texture_param_slot(MaterialEditorTextureParamKind kind);
SceneObject* material_editor_focused_object(void);
int material_editor_find_selected_triangle(
    const SceneEditorMaterialPreviewTriangleAddress* address);
int material_editor_collect_focused_face_groups(MaterialEditorFaceGroupInfo* out_groups,
                                                int group_capacity);
bool material_editor_selected_face_group_exists(int face_group_index);
bool material_editor_add_triangle_selection(
    const SceneEditorMaterialPreviewTriangleAddress* address);
bool material_editor_remove_face_group_selection(int scene_object_index, int face_group_index);
int material_editor_collect_face_group_triangles(
    const SceneEditorMaterialPreviewTriangleAddress* address,
    SceneEditorMaterialPreviewTriangleAddress* out_addresses,
    int address_capacity);
bool material_editor_face_group_fully_selected(
    const SceneEditorMaterialPreviewTriangleAddress* address);
bool material_editor_has_room_for_optional_control(int cursor_y,
                                                   int control_h,
                                                   int bottom_y);
bool material_editor_use_object_layer_controls(const SceneObject* obj);
bool material_editor_get_active_layer(const SceneObject* obj,
                                      RuntimeMaterialTextureStack* out_stack,
                                      RuntimeMaterialTextureLayer* out_layer,
                                      int* out_index);
const char* material_editor_short_layer_kind_label(RuntimeMaterialTextureLayerKind kind);
RayTracingThemePalette material_editor_palette(void);
int material_editor_texture_kind_for_controls(const SceneObject* obj);
RuntimeMaterialTexture3DParams material_editor_params_for_controls(const SceneObject* obj);
const char* material_editor_label_for_slider(MaterialEditorSliderKind kind);
const char* material_editor_label_for_param(const SceneObject* obj,
                                            MaterialEditorTextureParamKind kind);
double material_editor_value_for_slider(const SceneObject* obj, MaterialEditorSliderKind kind);
double material_editor_value_for_param_slider(const SceneObject* obj,
                                              MaterialEditorTextureParamKind kind);
void material_editor_format_slider_value(const SceneObject* obj,
                                         MaterialEditorSliderKind kind,
                                         char* out,
                                         size_t out_size);

void MaterialEditorResetGroupListLayout(void);
void MaterialEditorResetLayerListLayout(void);
int MaterialEditorDrawGroupList(SDL_Renderer* renderer,
                                SDL_Rect content_bounds,
                                int cursor_y,
                                int bottom_y,
                                RayTracingThemePalette palette);
int MaterialEditorDrawLayerList(SDL_Renderer* renderer,
                                SDL_Rect content_bounds,
                                int cursor_y,
                                int bottom_y,
                                const SceneObject* obj,
                                RayTracingThemePalette palette);
int MaterialEditorDrawLayerKindButtons(SDL_Renderer* renderer,
                                       SDL_Rect content_bounds,
                                       int cursor_y,
                                       int bottom_y,
                                       const SceneObject* obj,
                                       RayTracingThemePalette palette);
void MaterialEditorDrawButton(SDL_Renderer* renderer,
                              SDL_Rect rect,
                              const char* label,
                              bool active,
                              RayTracingThemePalette palette);
void MaterialEditorDrawSlider(SDL_Renderer* renderer,
                              SDL_Rect bounds,
                              MaterialEditorSliderKind kind,
                              const SceneObject* obj,
                              RayTracingThemePalette palette);
void MaterialEditorDrawParamSlider(SDL_Renderer* renderer,
                                   SDL_Rect bounds,
                                   MaterialEditorTextureParamKind kind,
                                   const SceneObject* obj,
                                   RayTracingThemePalette palette);

#endif
