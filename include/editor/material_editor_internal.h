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
#define MATERIAL_EDITOR_GRAPH_ACTION_COUNT 4
#define MATERIAL_EDITOR_RECIPE_ACTION_COUNT 3
#define MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS 8
#define MATERIAL_EDITOR_KNOB_HEIGHT 58
#define MATERIAL_EDITOR_SUBPANE_COUNT 6
#define MATERIAL_EDITOR_RESPONSE_ACTION_COUNT 2
#define MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT 5
#define MATERIAL_EDITOR_COMPACT_HEADER_HEIGHT 24
#define MATERIAL_EDITOR_COMPACT_TAB_HEIGHT 20
#define MATERIAL_EDITOR_COMPACT_MIN_TAB_WIDTH 42
#define MATERIAL_EDITOR_IDENTITY_POPOVER_HEIGHT 154
#define MATERIAL_EDITOR_PROOF_SCHEMA "ray_tracing_material_proof_package_request_v1"
#define MATERIAL_EDITOR_PROOF_SUMMARY_SCHEMA "ray_tracing_material_proof_summary_v1"
#define MATERIAL_EDITOR_PROOF_TEXT_CAPACITY 160
#define MATERIAL_EDITOR_RESPONSE_MAX_ROWS 8
#define MATERIAL_EDITOR_SECTION_LABEL(renderer, bounds, y, label, palette) \
    RenderLabelTextLeft((renderer), (SDL_Rect){(bounds).x, (y), (bounds).w, 14}, (label), (palette).text_primary)

typedef struct MaterialEditorFaceGroupInfo {
    int face_group_index;
    int triangle_count;
    int selected_count;
    SceneEditorMaterialPreviewTriangleAddress representative;
} MaterialEditorFaceGroupInfo;

typedef enum MaterialEditorMutationDestination {
    MATERIAL_EDITOR_MUTATION_DESTINATION_NONE = 0,
    MATERIAL_EDITOR_MUTATION_DESTINATION_OBJECT_ASSIGNMENT,
    MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK,
    MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE,
    MATERIAL_EDITOR_MUTATION_DESTINATION_LEGACY_OBJECT_TEXTURE_FALLBACK,
    MATERIAL_EDITOR_MUTATION_DESTINATION_AUTHORED_TEXTURE_BINDING
} MaterialEditorMutationDestination;

typedef enum MaterialEditorPanelGroup {
    MATERIAL_EDITOR_PANEL_GROUP_NONE = 0,
    MATERIAL_EDITOR_PANEL_GROUP_BASE_LAYER,
    MATERIAL_EDITOR_PANEL_GROUP_PHYSICAL_RESPONSE,
    MATERIAL_EDITOR_PANEL_GROUP_TEXTURE_BINDING,
    MATERIAL_EDITOR_PANEL_GROUP_FACE_OVERRIDE,
    MATERIAL_EDITOR_PANEL_GROUP_PREVIEW_READBACK
} MaterialEditorPanelGroup;

typedef enum MaterialEditorSubPane {
    MATERIAL_EDITOR_SUBPANE_STACK = 0,
    MATERIAL_EDITOR_SUBPANE_RESPONSE,
    MATERIAL_EDITOR_SUBPANE_TEXTURES,
    MATERIAL_EDITOR_SUBPANE_FACE,
    MATERIAL_EDITOR_SUBPANE_GRAPH,
    MATERIAL_EDITOR_SUBPANE_PROOF
} MaterialEditorSubPane;

typedef enum MaterialEditorRecipeAxis {
    MATERIAL_EDITOR_RECIPE_AXIS_NONE = -1,
    MATERIAL_EDITOR_RECIPE_AXIS_FAMILY = 0,
    MATERIAL_EDITOR_RECIPE_AXIS_SURFACE = 1,
    MATERIAL_EDITOR_RECIPE_AXIS_FINISH = 2
} MaterialEditorRecipeAxis;

typedef struct MaterialEditorCompactLayoutMetrics {
    int pad_x;
    int pad_y;
    int gap;
    int header_h;
    int tab_h;
    int tab_gap;
    int min_tab_w;
    int popover_h;
} MaterialEditorCompactLayoutMetrics;

typedef struct MaterialEditorCompactLayoutRects {
    SDL_Rect identity_header;
    SDL_Rect identity_disclosure;
    SDL_Rect identity_popover;
    SDL_Rect tab_row;
    SDL_Rect tab_rects[MATERIAL_EDITOR_SUBPANE_COUNT];
    SDL_Rect content;
    bool identity_popover_visible;
} MaterialEditorCompactLayoutRects;

typedef struct MaterialEditorProofReadback {
    char request_schema[64];
    char summary_schema[64];
    char proof_id[96];
    char phase[24];
    char route_primary[64];
    char route_status[96];
    char request_path[96];
    char summary_path[96];
    char index_path[96];
    char image_path[96];
    char image_status[96];
    char row_id[96];
    char label[128];
    char material_family[96];
    char source_material[128];
    char expected_behavior[160];
    char deferred_status[160];
    char glass_proof_case[96];
    char glass_proof_package[128];
    char glass_proof_coverage[192];
    char glass_missing_proof[192];
    char mirror_proof_case[96];
    char mirror_proof_package[128];
    char mirror_proof_coverage[192];
    char mirror_missing_proof[192];
    char destination_label[64];
    char panel_group_label[64];
    bool m4_request_compatible;
    bool launch_deferred;
    bool glass_proof_readback;
    bool mirror_proof_readback;
} MaterialEditorProofReadback;

typedef struct MaterialEditorGraphReadback {
    char phase[24];
    char graph_id[64];
    char authoring_state[64];
    char evaluator_route[64];
    char integration_status[96];
    int scene_object_index;
    int graph_node_count;
    int compiled_stack_layer_count;
    int channel_ref_count;
    bool has_graph;
    bool has_compiled_stack_fallback;
    bool visual_node_ui_deferred;
    bool visual_graph_mvp_available;
} MaterialEditorGraphReadback;

typedef struct MaterialEditorMaterialReadback {
    char preset_label[64];
    char state_label[64];
    char source_label[64];
    char save_request_label[96];
    int scene_object_index;
    int material_id;
    int stack_layer_count;
    bool preset_valid;
    bool custom_stack;
    bool authored_texture_bound;
    bool graph_backed;
    bool save_request_deferred;
} MaterialEditorMaterialReadback;

typedef struct MaterialEditorRecipeReadback {
    char header_label[160];
    char family_label[64];
    char surface_label[64];
    char finish_label[64];
    char detail_label[160];
    int material_id;
    RuntimeMaterialTextureLayerKind surface_kind;
    RuntimeMaterialTextureLayerKind finish_kind;
    bool has_custom_stack;
    bool has_finish_overlay;
    bool graph_backed;
} MaterialEditorRecipeReadback;

typedef struct MaterialEditorRecipeOption {
    char label[64];
    int material_id;
    RuntimeMaterialTextureLayerKind layer_kind;
    bool selected;
    bool compatible;
} MaterialEditorRecipeOption;

typedef struct MaterialEditorActiveLayerReadback {
    char title[96];
    char detail[160];
    char layer_id[48];
    int active_index;
    int layer_count;
    bool has_layer;
    bool enabled;
    bool base_layer;
} MaterialEditorActiveLayerReadback;

typedef struct MaterialEditorTextureChannelReadback {
    char visual_channels[128];
    char physical_channels[160];
    char future_channels[128];
    char deferred_channels[128];
    char procedural_source[160];
    char glass_authored_mapping[320];
    char glass_procedural_mapping[192];
    char glass_deferred_mapping[192];
    int visual_count;
    int physical_count;
    int future_count;
    bool has_authored_channels;
    bool has_procedural_source;
    bool has_glass_mapping;
} MaterialEditorTextureChannelReadback;

typedef struct MaterialEditorFaceRegionReadback {
    char active_label[96];
    char selection_label[128];
    char layer_label[128];
    char override_label[128];
    int focused_object_index;
    int active_face_group_index;
    int focused_face_group_count;
    int selected_face_group_count;
    bool has_active_face_group;
    bool has_active_layer;
    bool has_layer_specific_override;
    bool has_legacy_override;
    bool can_reset;
    bool can_copy_to_selected;
} MaterialEditorFaceRegionReadback;

typedef enum MaterialEditorResponseFamily {
    MATERIAL_EDITOR_RESPONSE_FAMILY_GENERIC = 0,
    MATERIAL_EDITOR_RESPONSE_FAMILY_GLASS,
    MATERIAL_EDITOR_RESPONSE_FAMILY_MIRROR,
    MATERIAL_EDITOR_RESPONSE_FAMILY_METAL,
    MATERIAL_EDITOR_RESPONSE_FAMILY_EMISSIVE
} MaterialEditorResponseFamily;

typedef enum MaterialEditorResponseFieldState {
    MATERIAL_EDITOR_RESPONSE_FIELD_READBACK = 0,
    MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE,
    MATERIAL_EDITOR_RESPONSE_FIELD_GUARDED
} MaterialEditorResponseFieldState;

typedef struct MaterialEditorResponseRow {
    MaterialEditorResponseField field;
    char label[24];
    char value[48];
    char note[80];
    MaterialEditorResponseFieldState state;
} MaterialEditorResponseRow;

typedef struct MaterialEditorResponseReadback {
    MaterialEditorResponseFamily family;
    char title[64];
    char subtitle[128];
    char route_label[96];
    int row_count;
    MaterialEditorResponseRow rows[MATERIAL_EDITOR_RESPONSE_MAX_ROWS];
    bool family_specific;
    bool has_guarded_fields;
} MaterialEditorResponseReadback;

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
extern SDL_Rect s_proof_readback_rect;
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
extern SDL_Rect s_graph_action_rects[MATERIAL_EDITOR_GRAPH_ACTION_COUNT];
extern SDL_Rect s_recipe_action_rects[MATERIAL_EDITOR_RECIPE_ACTION_COUNT];
extern SDL_Rect s_recipe_menu_item_rects[MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS];
extern SDL_Rect s_response_action_rects[MATERIAL_EDITOR_RESPONSE_MAX_ROWS]
                                      [MATERIAL_EDITOR_RESPONSE_ACTION_COUNT];
extern MaterialEditorResponseField
    s_response_action_fields[MATERIAL_EDITOR_RESPONSE_MAX_ROWS];
extern SDL_Rect s_glass_overlay_action_rects[MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT];
extern RuntimeMaterialTextureLayerKind
    s_glass_overlay_action_kinds[MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT];
extern int s_material_editor_param_drag_start_y;
extern double s_material_editor_param_drag_start_value;
extern MaterialEditorProofReadback s_material_editor_proof_readback;
extern bool s_material_editor_proof_readback_valid;
extern char s_material_editor_proof_readback_status[MATERIAL_EDITOR_PROOF_TEXT_CAPACITY];
extern MaterialEditorSubPane s_material_editor_active_subpane;
extern bool s_material_editor_identity_popover_open;
extern MaterialEditorRecipeAxis s_material_editor_recipe_menu_axis;
extern MaterialEditorCompactLayoutRects s_material_editor_compact_layout_rects;

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
const char* MaterialEditorMutationDestinationLabel(
    MaterialEditorMutationDestination destination);
MaterialEditorMutationDestination
MaterialEditorMutationDestinationForFocusedTextureControls(void);
const char* MaterialEditorPanelGroupLabel(MaterialEditorPanelGroup group);
MaterialEditorPanelGroup MaterialEditorPanelGroupForMutationDestination(
    MaterialEditorMutationDestination destination);
const char* MaterialEditorSubPaneLabel(MaterialEditorSubPane pane);
const char* MaterialEditorSubPaneCompactLabel(MaterialEditorSubPane pane);
MaterialEditorSubPane MaterialEditorSubPaneClamp(MaterialEditorSubPane pane);
MaterialEditorSubPane MaterialEditorGetActiveSubPane(void);
void MaterialEditorSetActiveSubPane(MaterialEditorSubPane pane);
bool MaterialEditorIdentityPopoverOpen(void);
void MaterialEditorSetIdentityPopoverOpen(bool open);
bool MaterialEditorToggleIdentityPopover(void);
MaterialEditorCompactLayoutMetrics MaterialEditorCompactLayoutDefaultMetrics(void);
MaterialEditorCompactLayoutRects MaterialEditorCompactLayoutBuild(
    SDL_Rect content_bounds,
    bool identity_popover_open);
int MaterialEditorRenderCompactPaneControls(SDL_Renderer* renderer,
                                            SDL_Rect content_bounds,
                                            int top_y,
                                            int bottom_y);
bool MaterialEditorBuildFocusedProofReadback(MaterialEditorProofReadback* out_readback);
bool MaterialEditorPrimeProofReadbackForFocused(void);
void MaterialEditorFormatProofReadbackStatus(const MaterialEditorProofReadback* readback,
                                             char* out,
                                             size_t out_size);
bool MaterialEditorBuildFocusedGraphReadback(MaterialEditorGraphReadback* out_readback);
bool MaterialEditorEnsureGraphForFocused(void);
bool MaterialEditorAddGraphLayerNodeForFocused(void);
bool MaterialEditorAddGraphChannelNodeForFocused(void);
bool MaterialEditorClearGraphForFocused(void);
bool MaterialEditorBuildMaterialReadback(MaterialEditorMaterialReadback* out_readback);
bool MaterialEditorBuildRecipeReadback(MaterialEditorRecipeReadback* out_readback);
bool MaterialEditorCycleRecipeFamilyForFocused(void);
bool MaterialEditorCycleRecipeSurfaceForFocused(void);
bool MaterialEditorCycleRecipeFinishForFocused(void);
const char* MaterialEditorRecipeAxisLabel(MaterialEditorRecipeAxis axis);
int MaterialEditorBuildRecipeOptions(MaterialEditorRecipeAxis axis,
                                     MaterialEditorRecipeOption* out_options,
                                     int option_capacity);
void MaterialEditorSetRecipeMenuAxis(MaterialEditorRecipeAxis axis);
MaterialEditorRecipeAxis MaterialEditorGetRecipeMenuAxis(void);
bool MaterialEditorToggleRecipeMenuAxis(MaterialEditorRecipeAxis axis);
bool MaterialEditorApplyRecipeOptionForFocused(MaterialEditorRecipeAxis axis, int option_index);
bool MaterialEditorBuildActiveLayerReadback(MaterialEditorActiveLayerReadback* out_readback);
bool MaterialEditorBuildTextureChannelReadback(int focused_object_index,
                                               MaterialEditorTextureChannelReadback* out_readback);
bool MaterialEditorBuildFaceRegionReadback(MaterialEditorFaceRegionReadback* out_readback);
bool MaterialEditorBuildResponseReadback(MaterialEditorResponseReadback* out_readback);
const char* MaterialEditorResponseFamilyLabel(MaterialEditorResponseFamily family);
const char* MaterialEditorResponseFieldStateLabel(MaterialEditorResponseFieldState state);
const char* MaterialEditorResponseFieldLabel(MaterialEditorResponseField field);

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
