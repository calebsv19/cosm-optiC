#include "editor/material_editor.h"
#include "editor/material_editor_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "editor/material_editor_face_preview.h"
#include "editor/material_editor_authored_texture_binding.h"
#include "camera/camera.h"
#include "config/config_manager.h"
#include "editor/material_editor_knob_control.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/material_editor_layer_model.h"
#include "editor/scene_editor.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "render/render_helper.h"
#include "scene/object_manager.h"
#include "ui/shared_theme_font_adapter.h"

int s_material_editor_focused_object_index = -1;
MaterialEditorSliderKind s_material_editor_active_slider = MATERIAL_EDITOR_SLIDER_NONE;
MaterialEditorTextureParamKind s_material_editor_active_param_slider =
    MATERIAL_EDITOR_TEXTURE_PARAM_NONE;
MaterialEditorViewMode s_material_editor_view_mode = MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN;
bool s_material_editor_solid_faces_enabled = true;
int s_material_editor_active_face_group_index = -1;
SceneEditorMaterialPreviewTriangleAddress
    s_material_editor_selected_triangles[MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES];
int s_material_editor_selected_triangle_count = 0;
SDL_Rect s_texture_none_rect = {0, 0, 0, 0};
SDL_Rect s_texture_rust_rect = {0, 0, 0, 0};
SDL_Rect s_texture_fog_rect = {0, 0, 0, 0};
SDL_Rect s_layer_kind_rects[MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT];
RuntimeMaterialTextureLayerKind
    s_layer_kind_rect_kinds[MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT];
SDL_Rect s_solid_faces_rect = {0, 0, 0, 0};
SDL_Rect s_reset_face_rect = {0, 0, 0, 0};
SDL_Rect s_copy_face_rect = {0, 0, 0, 0};
SDL_Rect s_proof_readback_rect = {0, 0, 0, 0};
SDL_Rect s_clear_groups_rect = {0, 0, 0, 0};
SDL_Rect s_group_panel_rect = {0, 0, 0, 0};
SDL_Rect s_group_list_rect = {0, 0, 0, 0};
SDL_Rect s_group_row_rects[MATERIAL_EDITOR_MAX_GROUP_ROWS];
int s_group_row_face_groups[MATERIAL_EDITOR_MAX_GROUP_ROWS];
SceneEditorMaterialPreviewTriangleAddress s_group_row_addresses[MATERIAL_EDITOR_MAX_GROUP_ROWS];
char s_group_row_labels[MATERIAL_EDITOR_MAX_GROUP_ROWS][64];
char s_group_header_label[64];
int s_group_row_count = 0;
int s_group_scroll_offset = 0;
int s_group_visible_capacity = 0;
int s_group_total_count = 0;
SDL_Rect s_slider_sections[4];
SDL_Rect s_slider_tracks[4];
SDL_Rect s_param_sections[MATERIAL_EDITOR_PARAM_SLIDER_COUNT];
SDL_Rect s_pattern_rects[MATERIAL_EDITOR_PATTERN_BUTTON_COUNT];
SDL_Rect s_layer_panel_rect = {0, 0, 0, 0};
SDL_Rect s_layer_list_rect = {0, 0, 0, 0};
SDL_Rect s_layer_row_rects[MATERIAL_EDITOR_MAX_LAYER_ROWS];
SDL_Rect s_layer_toggle_rects[MATERIAL_EDITOR_MAX_LAYER_ROWS];
int s_layer_row_indices[MATERIAL_EDITOR_MAX_LAYER_ROWS];
char s_layer_row_labels[MATERIAL_EDITOR_MAX_LAYER_ROWS][72];
SDL_Rect s_layer_action_rects[MATERIAL_EDITOR_LAYER_ACTION_COUNT];
int s_layer_row_count = 0;
int s_layer_scroll_offset = 0;
int s_layer_visible_capacity = 0;
int s_layer_total_count = 0;
SDL_Rect s_graph_action_rects[MATERIAL_EDITOR_GRAPH_ACTION_COUNT];
SDL_Rect s_recipe_action_rects[MATERIAL_EDITOR_RECIPE_ACTION_COUNT];
SDL_Rect s_recipe_menu_item_rects[MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS];
int s_material_editor_param_drag_start_y = 0;
double s_material_editor_param_drag_start_value = 0.0;
MaterialEditorProofReadback s_material_editor_proof_readback;
bool s_material_editor_proof_readback_valid = false;
char s_material_editor_proof_readback_status[MATERIAL_EDITOR_PROOF_TEXT_CAPACITY];
MaterialEditorSubPane s_material_editor_active_subpane = MATERIAL_EDITOR_SUBPANE_STACK;
bool s_material_editor_identity_popover_open = false;
MaterialEditorRecipeAxis s_material_editor_recipe_menu_axis = MATERIAL_EDITOR_RECIPE_AXIS_NONE;

SceneEditorMaterialTextureParamField material_editor_texture_param_field(
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

int material_editor_texture_param_slot(MaterialEditorTextureParamKind kind) {
    int slot = (int)kind - 1;
    if (slot < 0 || slot >= MATERIAL_EDITOR_PARAM_SLIDER_COUNT) return -1;
    return slot;
}

double material_editor_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool material_editor_resolve_controls_face_placement(
    const SceneObject* obj,
    int focused_object_index,
    int face_group_index,
    SceneEditorMaterialFacePlacement* out_placement) {
    SceneEditorMaterialFacePlacement placement = {0};
    RuntimeMaterialTextureLayer layer = {0};
    int layer_index = -1;
    if (!obj || focused_object_index < 0 || face_group_index < 0 || !out_placement) {
        return false;
    }
    if (SceneEditorMaterialStackHasObjectStack(focused_object_index) &&
        material_editor_get_active_layer(obj, NULL, &layer, &layer_index)) {
        layer = RuntimeMaterialTextureLayerNormalize(layer);
        placement = SceneEditorMaterialFacePlacementGetEffectiveForLayer(
            obj,
            focused_object_index,
            face_group_index,
            layer.layerId);
        if (SceneEditorMaterialFacePlacementHasOverrideForLayer(focused_object_index,
                                                                face_group_index,
                                                                layer.layerId)) {
            *out_placement = placement;
            return true;
        }
        placement.layerIndex = layer_index;
        snprintf(placement.layerId, sizeof(placement.layerId), "%s", layer.layerId);
        placement.textureId = layer.placement.textureId;
        placement.offsetU = layer.placement.offsetU;
        placement.offsetV = layer.placement.offsetV;
        placement.scale = layer.placement.scale;
        placement.strength = layer.placement.strength;
        placement.rotation = layer.placement.rotation;
        placement.params = layer.params;
        *out_placement = placement;
        return true;
    }
    placement = SceneEditorMaterialFacePlacementGetEffective(obj,
                                                             focused_object_index,
                                                             face_group_index);
    if (SceneEditorMaterialFacePlacementHasOverride(focused_object_index, face_group_index)) {
        *out_placement = placement;
        return true;
    }
    *out_placement = placement;
    return true;
}

static double material_editor_normalized_value_from_placement(
    const SceneEditorMaterialFacePlacement* placement,
    MaterialEditorSliderKind kind) {
    double value = 0.0;
    if (!placement) return 0.0;
    if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) {
        return material_editor_clamp01(placement->strength);
    }
    if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
        value = placement->scale;
        if (value < 0.25) value = 0.25;
        if (value > 8.0) value = 8.0;
        return (value - 0.25) / 7.75;
    }
    if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) {
        return material_editor_clamp01(placement->offsetU);
    }
    if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) {
        return material_editor_clamp01(placement->offsetV);
    }
    return 0.0;
}

bool material_editor_has_room_for_optional_control(int cursor_y,
                                                   int control_h,
                                                   int bottom_y) {
    return cursor_y + control_h + MATERIAL_EDITOR_MIN_GROUP_LIST_HEIGHT <= bottom_y;
}

const char* material_editor_short_layer_kind_label(RuntimeMaterialTextureLayerKind kind) {
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL) return "Metal";
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE) return "Concrete";
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR) return "Wear";
    return RuntimeMaterialTextureLayerKindDisplayName(kind);
}

RayTracingThemePalette material_editor_palette(void) {
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

double material_editor_value_for_slider(const SceneObject* obj, MaterialEditorSliderKind kind) {
    RuntimeMaterialTextureLayer layer;
    if (!obj) return 0.0;
    if (s_material_editor_active_face_group_index >= 0) {
        int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
        SceneEditorMaterialFacePlacement placement = {0};
        if (material_editor_resolve_controls_face_placement(obj,
                                                            focused_object_index,
                                                            s_material_editor_active_face_group_index,
                                                            &placement)) {
            return material_editor_normalized_value_from_placement(&placement, kind);
        }
    }
    if (material_editor_use_object_layer_controls(obj) &&
        material_editor_get_active_layer(obj, NULL, &layer, NULL)) {
        if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) return material_editor_clamp01(layer.placement.strength);
        if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
            double value = layer.placement.scale;
            if (value < 0.25) value = 0.25;
            if (value > 8.0) value = 8.0;
            return (value - 0.25) / 7.75;
        }
        if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) return material_editor_clamp01(layer.placement.offsetU);
        if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) return material_editor_clamp01(layer.placement.offsetV);
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

int material_editor_texture_kind_for_controls(const SceneObject* obj) {
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTextureLayer layer;
    if (!obj) return 0;
    if (s_material_editor_active_face_group_index >= 0) {
        SceneEditorMaterialFacePlacement placement = {0};
        material_editor_resolve_controls_face_placement(obj,
                                                        focused_object_index,
                                                        s_material_editor_active_face_group_index,
                                                        &placement);
        return placement.textureId;
    }
    if (material_editor_use_object_layer_controls(obj) &&
        material_editor_get_active_layer(obj, NULL, &layer, NULL)) {
        if (layer.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST) {
            return RUNTIME_MATERIAL_TEXTURE_3D_RUST;
        }
        if (layer.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG) {
            return RUNTIME_MATERIAL_TEXTURE_3D_FOG;
        }
        return RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    }
    return obj->textureId;
}

RuntimeMaterialTexture3DParams material_editor_params_for_controls(const SceneObject* obj) {
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTextureLayer layer;
    if (!obj) return RuntimeMaterialTexture3DDefaultParams();
    if (s_material_editor_active_face_group_index >= 0) {
        SceneEditorMaterialFacePlacement placement = {0};
        material_editor_resolve_controls_face_placement(obj,
                                                        focused_object_index,
                                                        s_material_editor_active_face_group_index,
                                                        &placement);
        return RuntimeMaterialTexture3DNormalizeParams(placement.params);
    }
    if (material_editor_use_object_layer_controls(obj) &&
        material_editor_get_active_layer(obj, NULL, &layer, NULL)) {
        return RuntimeMaterialTexture3DNormalizeParams(layer.params);
    }
    return RuntimeMaterialTexture3DParamsFromObject(obj);
}

const char* material_editor_label_for_slider(MaterialEditorSliderKind kind) {
    if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) return "Strength";
    if (kind == MATERIAL_EDITOR_SLIDER_SCALE) return "Scale";
    if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) return "Offset U";
    if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) return "Offset V";
    return "";
}

static RuntimeMaterialTextureLayerKind material_editor_active_layer_kind_for_labels(
    const SceneObject* obj) {
    RuntimeMaterialTextureLayer layer;
    int texture_id = 0;
    if (!obj) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    if (material_editor_use_object_layer_controls(obj) && material_editor_get_active_layer(obj, NULL, &layer, NULL)) return layer.kind;
    texture_id = material_editor_texture_kind_for_controls(obj);
    if (texture_id == RUNTIME_MATERIAL_TEXTURE_3D_RUST) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
    if (texture_id == RUNTIME_MATERIAL_TEXTURE_3D_FOG) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG;
    return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
}

const char* material_editor_label_for_param(const SceneObject* obj,
                                            MaterialEditorTextureParamKind kind) {
    static const char* generic_labels[] = {"Coverage", "Grain", "Edge", "Contrast", "Flow", "Color", "Damage"};
    static const char* fog_labels[] = {"Density", "Drift", "Soft", "Fade", "Flow", "Tint", "Haze"};
    static const char* grime_labels[] = {"Cover", "Streak", "Edge", "Dark", "Run", "Tint", "Dirt"};
    static const char* oil_labels[] = {"Film", "Gloss", "Edge", "Sheen", "Smear", "Tint", "Break"};
    RuntimeMaterialTextureLayerKind layer_kind = material_editor_active_layer_kind_for_labels(obj);
    int slot = material_editor_texture_param_slot(kind);
    if (slot < 0) return "";
    if (layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG) return fog_labels[slot];
    if (layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME) return grime_labels[slot];
    if (layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) return oil_labels[slot];
    return generic_labels[slot];
}

double material_editor_value_for_param_slider(const SceneObject* obj,
                                              MaterialEditorTextureParamKind kind) {
    RuntimeMaterialTexture3DParams params = material_editor_params_for_controls(obj);
    if (!obj || material_editor_texture_param_slot(kind) < 0) return 0.0;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE) return params.coverage;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_GRAIN) return params.grain;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_EDGE_SOFTNESS) return params.edgeSoftness;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_CONTRAST) return params.contrast;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_FLOW) return params.flow;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COLOR_DEPTH) return params.colorDepth;
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_SURFACE_DAMAGE) return params.surfaceDamage;
    return 0.0;
}

void material_editor_format_slider_value(const SceneObject* obj,
                                         MaterialEditorSliderKind kind,
                                         char* out,
                                         size_t out_size) {
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    SceneEditorMaterialFacePlacement placement;
    RuntimeMaterialTextureLayer layer;
    if (!out || out_size == 0) return;
    if (!obj) {
        snprintf(out, out_size, "--");
        return;
    }
    if (s_material_editor_active_face_group_index >= 0) {
        material_editor_resolve_controls_face_placement(obj,
                                                        focused_object_index,
                                                        s_material_editor_active_face_group_index,
                                                        &placement);
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
    if (material_editor_use_object_layer_controls(obj) &&
        material_editor_get_active_layer(obj, NULL, &layer, NULL)) {
        if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) {
            snprintf(out, out_size, "%.2f", layer.placement.strength);
        } else if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
            snprintf(out, out_size, "%.2f", layer.placement.scale);
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) {
            snprintf(out, out_size, "%.2f", layer.placement.offsetU);
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) {
            snprintf(out, out_size, "%.2f", layer.placement.offsetV);
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

void InitializeMaterialEditor(void) {
    SceneObject* obj = material_editor_focused_object();
    if (!obj && sceneSettings.objectCount > 0) {
        s_material_editor_focused_object_index = 0;
    }
    s_material_editor_active_slider = MATERIAL_EDITOR_SLIDER_NONE;
    s_material_editor_active_param_slider = MATERIAL_EDITOR_TEXTURE_PARAM_NONE;
    s_group_scroll_offset = 0;
    s_layer_scroll_offset = 0;
    s_material_editor_active_subpane =
        MaterialEditorSubPaneClamp(s_material_editor_active_subpane);
    s_material_editor_identity_popover_open = false;
    s_material_editor_recipe_menu_axis = MATERIAL_EDITOR_RECIPE_AXIS_NONE;
    memset(&s_material_editor_proof_readback, 0, sizeof(s_material_editor_proof_readback));
    s_material_editor_proof_readback_valid = false;
    s_material_editor_proof_readback_status[0] = '\0';
    MaterialEditorLayerModelReset();
    MaterialEditorAuthoredTextureBindingReset();
    MaterialEditorFacePreviewReset();
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
    return MaterialEditorRenderCompactPaneControls(renderer, content_bounds, top_y, bottom_y);
    memset(s_slider_sections, 0, sizeof(s_slider_sections));
    memset(s_slider_tracks, 0, sizeof(s_slider_tracks));
    memset(s_param_sections, 0, sizeof(s_param_sections));
    memset(s_pattern_rects, 0, sizeof(s_pattern_rects));
    memset(s_layer_kind_rects, 0, sizeof(s_layer_kind_rects));
    memset(s_layer_kind_rect_kinds, 0, sizeof(s_layer_kind_rect_kinds));
    MaterialEditorResetLayerListLayout();
    s_texture_none_rect = (SDL_Rect){0, 0, 0, 0};
    s_texture_rust_rect = (SDL_Rect){0, 0, 0, 0};
    s_texture_fog_rect = (SDL_Rect){0, 0, 0, 0};
    s_solid_faces_rect = (SDL_Rect){0, 0, 0, 0};
    s_reset_face_rect = (SDL_Rect){0, 0, 0, 0};
    s_copy_face_rect = (SDL_Rect){0, 0, 0, 0};
    s_proof_readback_rect = (SDL_Rect){0, 0, 0, 0};
    MaterialEditorResetGroupListLayout();
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
        RuntimeMaterialTextureLayer active_layer = {0};
        bool has_override = false;
        if (SceneEditorMaterialStackHasObjectStack(focused_index) &&
            material_editor_get_active_layer(obj, NULL, &active_layer, NULL)) {
            has_override =
                SceneEditorMaterialFacePlacementHasOverrideForLayer(
                    focused_index,
                    s_material_editor_active_face_group_index,
                    active_layer.layerId);
        } else {
            has_override = SceneEditorMaterialFacePlacementHasOverride(
                focused_index,
                s_material_editor_active_face_group_index);
        }
        snprintf(edit_text, sizeof(edit_text), "Face #%d %s", s_material_editor_active_face_group_index, has_override ? "override" : "default");
    } else {
        snprintf(edit_text, sizeof(edit_text), "Object defaults");
    }
    snprintf(line, sizeof(line), "Material Obj #%d  mat=%d", focused_index, obj->material_id);
    RenderLabelTextLeft(renderer, (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 16}, line, palette.text_primary);
    cursor_y += 20;

    snprintf(line, sizeof(line), "Faces %d/%d selected | %s", selected_faces, focused_faces, edit_text);
    RenderLabelTextLeft(renderer, (SDL_Rect){content_bounds.x, cursor_y, content_bounds.w, 16}, line, palette.text_muted);
    cursor_y += 22;

    cursor_y = MaterialEditorAuthoredTextureBindingRenderPaneControls(renderer,
                                                                      content_bounds,
                                                                      cursor_y,
                                                                      bottom_y,
                                                                      focused_index,
                                                                      palette);

    cursor_y = MaterialEditorDrawLayerList(renderer, content_bounds, cursor_y, bottom_y, obj, palette);

    if (material_editor_use_object_layer_controls(obj)) {
        cursor_y = MaterialEditorDrawLayerKindButtons(renderer, content_bounds, cursor_y, bottom_y, obj, palette);
    } else {
        third_w = (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP * 2) / 3;
        if (cursor_y + 15 + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
            MATERIAL_EDITOR_SECTION_LABEL(
                renderer,
                content_bounds,
                cursor_y,
                MaterialEditorPanelGroupLabel(MATERIAL_EDITOR_PANEL_GROUP_TEXTURE_BINDING),
                palette);
            cursor_y += 15;
            s_texture_none_rect = (SDL_Rect){content_bounds.x, cursor_y, third_w, MATERIAL_EDITOR_BUTTON_HEIGHT};
            s_texture_rust_rect = (SDL_Rect){s_texture_none_rect.x + third_w + MATERIAL_EDITOR_BUTTON_GAP, cursor_y, third_w, MATERIAL_EDITOR_BUTTON_HEIGHT};
            s_texture_fog_rect = (SDL_Rect){s_texture_rust_rect.x + third_w + MATERIAL_EDITOR_BUTTON_GAP, cursor_y, content_bounds.w - third_w * 2 - MATERIAL_EDITOR_BUTTON_GAP * 2, MATERIAL_EDITOR_BUTTON_HEIGHT};
            int active_texture = material_editor_texture_kind_for_controls(obj);
            MaterialEditorDrawButton(renderer, s_texture_none_rect, "None", active_texture == 0, palette);
            MaterialEditorDrawButton(renderer, s_texture_rust_rect, "Rust", active_texture == 1, palette);
            MaterialEditorDrawButton(renderer, s_texture_fog_rect, "Fog", active_texture == 2, palette);
            cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
        }
    }

    {
        int grid_y = cursor_y;
        int drawn_rows = 0;
        int col_w = (content_bounds.w - MATERIAL_EDITOR_CONTROL_GAP) / 2;
        if (material_editor_has_room_for_optional_control(
                cursor_y,
                15 + MATERIAL_EDITOR_SLIDER_HEIGHT * 2 + MATERIAL_EDITOR_CONTROL_GAP,
                bottom_y)) {
            MATERIAL_EDITOR_SECTION_LABEL(
                renderer,
                content_bounds,
                cursor_y,
                "Texture Placement",
                palette);
            cursor_y += 15;
            grid_y = cursor_y;
        }
        for (int i = 0; i < 4; ++i) {
            MaterialEditorSliderKind kind = (MaterialEditorSliderKind)(i + 1);
            int col = i % 2;
            int row = i / 2;
            int x = content_bounds.x + col * (col_w + MATERIAL_EDITOR_CONTROL_GAP);
            int y = grid_y + row * (MATERIAL_EDITOR_SLIDER_HEIGHT + MATERIAL_EDITOR_CONTROL_GAP);
            int w = (col == 1) ? content_bounds.x + content_bounds.w - x : col_w;
            if (y + MATERIAL_EDITOR_SLIDER_HEIGHT > bottom_y) break;
            MaterialEditorDrawSlider(renderer, (SDL_Rect){x, y, w, MATERIAL_EDITOR_SLIDER_HEIGHT}, kind, obj, palette);
            if (drawn_rows < row + 1) drawn_rows = row + 1;
        }
        cursor_y += drawn_rows * (MATERIAL_EDITOR_SLIDER_HEIGHT + MATERIAL_EDITOR_CONTROL_GAP);
    }

    if (material_editor_use_object_layer_controls(obj) ||
        material_editor_texture_kind_for_controls(obj) > RUNTIME_MATERIAL_TEXTURE_3D_NONE) {
        RuntimeMaterialTexture3DParams params = material_editor_params_for_controls(obj);
        int pattern_w = (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP * 3) / 4;
        const char* pattern_labels[MATERIAL_EDITOR_PATTERN_BUTTON_COUNT] = {
            "Default", "Speck", "Patch", "Flow"
        };
        if (material_editor_has_room_for_optional_control(cursor_y, 15 + MATERIAL_EDITOR_BUTTON_HEIGHT, bottom_y)) {
            MATERIAL_EDITOR_SECTION_LABEL(
                renderer,
                content_bounds,
                cursor_y,
                MaterialEditorPanelGroupLabel(MATERIAL_EDITOR_PANEL_GROUP_PHYSICAL_RESPONSE),
                palette);
            cursor_y += 15;
            for (int i = 0; i < MATERIAL_EDITOR_PATTERN_BUTTON_COUNT; ++i) {
                int x = content_bounds.x + i * (pattern_w + MATERIAL_EDITOR_BUTTON_GAP);
                int w = (i == MATERIAL_EDITOR_PATTERN_BUTTON_COUNT - 1)
                            ? content_bounds.x + content_bounds.w - x
                            : pattern_w;
                s_pattern_rects[i] = (SDL_Rect){x, cursor_y, w, MATERIAL_EDITOR_BUTTON_HEIGHT};
                MaterialEditorDrawButton(renderer, s_pattern_rects[i], pattern_labels[i], params.patternMode == i, palette);
            }
            cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
        }
        {
            int grid_y = cursor_y;
            int drawn_rows = 0;
            int col_count = 4;
            int col_w = (content_bounds.w - MATERIAL_EDITOR_CONTROL_GAP * (col_count - 1)) / col_count;
            if (material_editor_has_room_for_optional_control(cursor_y, 15 + MATERIAL_EDITOR_KNOB_HEIGHT, bottom_y)) {
                MATERIAL_EDITOR_SECTION_LABEL(renderer, content_bounds, cursor_y, "Response Parameters", palette);
                cursor_y += 15;
                grid_y = cursor_y;
            }
            for (int i = 0; i < MATERIAL_EDITOR_PARAM_SLIDER_COUNT; ++i) {
                MaterialEditorTextureParamKind kind = (MaterialEditorTextureParamKind)(i + 1);
                int col = i % col_count;
                int row = i / col_count;
                int x = content_bounds.x + col * (col_w + MATERIAL_EDITOR_CONTROL_GAP);
                int y = grid_y + row * (MATERIAL_EDITOR_KNOB_HEIGHT + MATERIAL_EDITOR_CONTROL_GAP);
                int w = (col == col_count - 1) ? content_bounds.x + content_bounds.w - x : col_w;
                if (!material_editor_has_room_for_optional_control(y, MATERIAL_EDITOR_KNOB_HEIGHT, bottom_y)) {
                    break;
                }
                MaterialEditorDrawParamSlider(renderer, (SDL_Rect){x, y, w, MATERIAL_EDITOR_KNOB_HEIGHT}, kind, obj, palette);
                if (drawn_rows < row + 1) drawn_rows = row + 1;
            }
            cursor_y += drawn_rows * (MATERIAL_EDITOR_KNOB_HEIGHT + MATERIAL_EDITOR_CONTROL_GAP);
        }
    }

    if (cursor_y + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
        RuntimeMaterialTextureLayer active_layer = {0};
        bool can_reset = false;
        if (s_material_editor_active_face_group_index >= 0) {
            if (SceneEditorMaterialStackHasObjectStack(focused_index) &&
                material_editor_get_active_layer(obj, NULL, &active_layer, NULL)) {
                can_reset = SceneEditorMaterialFacePlacementHasOverrideForLayer(
                    focused_index,
                    s_material_editor_active_face_group_index,
                    active_layer.layerId);
            } else {
                can_reset = SceneEditorMaterialFacePlacementHasOverride(
                    focused_index,
                    s_material_editor_active_face_group_index);
            }
        }
        int reset_w = can_reset ? (content_bounds.w - MATERIAL_EDITOR_BUTTON_GAP) / 2 : 0;
        if (cursor_y + 15 + MATERIAL_EDITOR_BUTTON_HEIGHT <= bottom_y) {
            MATERIAL_EDITOR_SECTION_LABEL(
                renderer,
                content_bounds,
                cursor_y,
                MaterialEditorPanelGroupLabel(can_reset ? MATERIAL_EDITOR_PANEL_GROUP_FACE_OVERRIDE
                                                        : MATERIAL_EDITOR_PANEL_GROUP_PREVIEW_READBACK),
                palette);
            cursor_y += 15;
        }
        s_solid_faces_rect = (SDL_Rect){content_bounds.x, cursor_y, reset_w > 0 ? reset_w : content_bounds.w, MATERIAL_EDITOR_BUTTON_HEIGHT};
        MaterialEditorDrawButton(renderer, s_solid_faces_rect, "Solid Preview", s_material_editor_solid_faces_enabled, palette);
        if (reset_w > 0) {
            s_reset_face_rect = (SDL_Rect){s_solid_faces_rect.x + s_solid_faces_rect.w +
                                              MATERIAL_EDITOR_BUTTON_GAP,
                                          cursor_y,
                                          content_bounds.w - s_solid_faces_rect.w -
                                              MATERIAL_EDITOR_BUTTON_GAP,
                                          MATERIAL_EDITOR_BUTTON_HEIGHT};
            MaterialEditorDrawButton(renderer, s_reset_face_rect, "Reset Face", false, palette);
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
        MaterialEditorDrawButton(renderer, s_copy_face_rect, "Copy to Selected", false, palette);
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    }
    if (material_editor_has_room_for_optional_control(cursor_y,
                                                       15 + MATERIAL_EDITOR_BUTTON_HEIGHT,
                                                       bottom_y)) {
        MATERIAL_EDITOR_SECTION_LABEL(
            renderer,
            content_bounds,
            cursor_y,
            MaterialEditorPanelGroupLabel(MATERIAL_EDITOR_PANEL_GROUP_PREVIEW_READBACK),
            palette);
        cursor_y += 15;
        s_proof_readback_rect = (SDL_Rect){content_bounds.x,
                                           cursor_y,
                                           content_bounds.w,
                                           MATERIAL_EDITOR_BUTTON_HEIGHT};
        MaterialEditorDrawButton(renderer,
                                 s_proof_readback_rect,
                                 "M4 Proof Readback",
                                 s_material_editor_proof_readback_valid,
                                 palette);
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
        if (s_material_editor_proof_readback_valid &&
            material_editor_has_room_for_optional_control(cursor_y, 18, bottom_y)) {
            RenderLabelTextLeft(renderer,
                                (SDL_Rect){content_bounds.x,
                                           cursor_y,
                                           content_bounds.w,
                                           16},
                                s_material_editor_proof_readback_status,
                                palette.text_muted);
            cursor_y += 20;
        }
    }
    cursor_y = MaterialEditorDrawGroupList(renderer, content_bounds, cursor_y, bottom_y, palette);
    return cursor_y;
}

int MaterialEditorRenderRightPanePreview(SDL_Renderer* renderer,
                                         SDL_Rect content_bounds,
                                         int top_y,
                                         int bottom_y) {
    SceneObject* obj = material_editor_focused_object();
    RayTracingThemePalette palette = material_editor_palette();
    int focused_index = -1;
    int needed_h = 0;
    SceneEditorMaterialPreviewTriangleAddress active_face_address = {0};
    bool has_active_face_address = false;
    if (!renderer || content_bounds.w <= 0 || top_y >= bottom_y) return top_y;
    if (!obj) {
        SDL_Rect label = {content_bounds.x, top_y, content_bounds.w, bottom_y - top_y};
        RenderLabelTextWrappedLeft(renderer,
                                   label,
                                   "No object selected. Select an object in Objects mode first.",
                                   palette.text_muted);
        return bottom_y;
    }
    focused_index = MaterialEditorResolveFocusedObjectIndex();
    has_active_face_address = MaterialEditorGetActiveFaceAddress(&active_face_address);
    if (!has_active_face_address) {
        active_face_address.sceneObjectIndex = focused_index;
        active_face_address.primitiveIndex = -1;
        active_face_address.faceGroupIndex = s_material_editor_active_face_group_index;
    }
    needed_h = MaterialEditorFacePreviewPreferredHeightForAddress(obj,
                                                                  &active_face_address,
                                                                  content_bounds.w);
    if (!material_editor_has_room_for_optional_control(top_y, needed_h, bottom_y)) {
        return top_y;
    }
    return MaterialEditorFacePreviewRenderPaneForAddress(renderer,
                                                         content_bounds,
                                                         top_y,
                                                         obj,
                                                         &active_face_address,
                                                         palette);
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

bool MaterialEditorBindAuthoredTextureManifestForFocused(const char* manifest_path) {
    return MaterialEditorAuthoredTextureBindingBindForFocused(MaterialEditorResolveFocusedObjectIndex(),
                                                             manifest_path);
}

bool MaterialEditorClearAuthoredTextureBindingForFocused(void) {
    return MaterialEditorAuthoredTextureBindingClearForFocused(MaterialEditorResolveFocusedObjectIndex());
}

bool MaterialEditorGetAuthoredTextureBindingSummary(char* out_manifest_path,
                                                    size_t out_manifest_path_size,
                                                    char* out_binding_mode,
                                                    size_t out_binding_mode_size,
                                                    int* out_face_count) {
    return MaterialEditorAuthoredTextureBindingGetSummary(MaterialEditorResolveFocusedObjectIndex(),
                                                          out_manifest_path,
                                                          out_manifest_path_size,
                                                          out_binding_mode,
                                                          out_binding_mode_size,
                                                          out_face_count);
}

bool MaterialEditorGetAuthoredTextureInvalidSummary(char* out_manifest_path,
                                                    size_t out_manifest_path_size,
                                                    char* out_binding_mode,
                                                    size_t out_binding_mode_size,
                                                    char* out_reason,
                                                    size_t out_reason_size) {
    return MaterialEditorAuthoredTextureBindingGetInvalidSummary(
        MaterialEditorResolveFocusedObjectIndex(),
        out_manifest_path,
        out_manifest_path_size,
        out_binding_mode,
        out_binding_mode_size,
        out_reason,
        out_reason_size);
}
