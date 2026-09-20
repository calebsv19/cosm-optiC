#include "editor/material_editor_compact_response_render.h"

#include <stdbool.h>
#include <stdio.h>

#include "editor/material_editor_internal.h"
#include "editor/material_editor_layer_model.h"
#include "material/material.h"
#include "ui/shared_theme_font_adapter.h"

static int material_editor_draw_response_controls(SDL_Renderer* renderer,
                                                  SDL_Rect bounds,
                                                  int cursor_y,
                                                  int bottom_y,
                                                  const SceneObject* obj,
                                                  RayTracingThemePalette palette) {
    RuntimeMaterialTexture3DParams params;
    int pattern_w = 0;
    const char* pattern_labels[MATERIAL_EDITOR_PATTERN_BUTTON_COUNT] = {
        "Default", "Speck", "Patch", "Flow"
    };
    if (!renderer || !obj) return cursor_y;
    if (!material_editor_use_object_layer_controls(obj) &&
        material_editor_texture_kind_for_controls(obj) <= RUNTIME_MATERIAL_TEXTURE_3D_NONE) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){bounds.x, cursor_y, bounds.w, 40},
                                   "No procedural response controls for this material.",
                                   palette.text_muted);
        return cursor_y + 42;
    }
    params = material_editor_params_for_controls(obj);
    pattern_w = (bounds.w - MATERIAL_EDITOR_BUTTON_GAP * 3) / 4;
    if (material_editor_has_room_for_optional_control(cursor_y,
                                                       15 + MATERIAL_EDITOR_BUTTON_HEIGHT,
                                                       bottom_y)) {
        MATERIAL_EDITOR_SECTION_LABEL(renderer,
                                      bounds,
                                      cursor_y,
                                      MaterialEditorPanelGroupLabel(
                                          MATERIAL_EDITOR_PANEL_GROUP_PHYSICAL_RESPONSE),
                                      palette);
        cursor_y += 15;
        for (int i = 0; i < MATERIAL_EDITOR_PATTERN_BUTTON_COUNT; ++i) {
            int x = bounds.x + i * (pattern_w + MATERIAL_EDITOR_BUTTON_GAP);
            int w = (i == MATERIAL_EDITOR_PATTERN_BUTTON_COUNT - 1)
                        ? bounds.x + bounds.w - x
                        : pattern_w;
            s_pattern_rects[i] = (SDL_Rect){x, cursor_y, w, MATERIAL_EDITOR_BUTTON_HEIGHT};
            MaterialEditorDrawButton(renderer,
                                     s_pattern_rects[i],
                                     pattern_labels[i],
                                     params.patternMode == i,
                                     palette);
        }
        cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    }
    int row_h=animation_config_scale_text_point_size(&animSettings,30,30);
    for(int i=0;i<MATERIAL_EDITOR_PARAM_SLIDER_COUNT;++i) {
        if(cursor_y+row_h>bottom_y) break;
        MaterialEditorDrawParamSlider(renderer,(SDL_Rect){bounds.x,cursor_y,bounds.w,row_h-4},
            (MaterialEditorTextureParamKind)(i+1),obj,palette);
        cursor_y+=row_h;
    }
    return cursor_y;
}

static int material_editor_draw_response_readback_grid(SDL_Renderer* renderer,
                                                       SDL_Rect bounds,
                                                       int cursor_y,
                                                       int bottom_y,
                                                       RayTracingThemePalette palette) {
    MaterialEditorResponseReadback readback = {0};
    if (!renderer) return cursor_y;
    if (!MaterialEditorBuildResponseReadback(&readback)) {
        return cursor_y;
    }
    if (!material_editor_has_room_for_optional_control(cursor_y, 84, bottom_y)) {
        return cursor_y;
    }
    int row_h=animation_config_scale_text_point_size(&animSettings,30,30);
    for(int i=0;i<readback.row_count;++i) {
        const MaterialEditorResponseRow* row=&readback.rows[i];
        if(row->field==MATERIAL_EDITOR_RESPONSE_FIELD_MIRROR_DOMINANCE ||
           row->field==MATERIAL_EDITOR_RESPONSE_FIELD_MIRROR_BASE ||
           row->field==MATERIAL_EDITOR_RESPONSE_FIELD_DIFFUSE_BASE) continue;
        if(cursor_y+row_h>bottom_y) break;
        int label_w=bounds.w*45/100;
        int action_w=22;
        bool editable=row->state==MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE && row->field!=MATERIAL_EDITOR_RESPONSE_FIELD_NONE;
        MaterialEditorTextLeft(renderer,(SDL_Rect){bounds.x,cursor_y,label_w-4,row_h-4},(row->field==MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS ? "Roughness" :
            row->field==MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY ? "Reflection" :
            row->field==MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR ? "Specular" :
            row->field==MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION ? "Transmission" : row->label),palette.text_muted);
        SDL_Rect value_rect={bounds.x+label_w,cursor_y,bounds.w-label_w-(editable ? 2*action_w+4 : 0),row_h-4};
        double red,green,blue;
        if(row->field==MATERIAL_EDITOR_RESPONSE_FIELD_TINT && sscanf(row->value,"%lf %lf %lf",&red,&green,&blue)==3) {
            SDL_SetRenderDrawColor(renderer,(Uint8)(material_editor_clamp01(red)*255),
                (Uint8)(material_editor_clamp01(green)*255),(Uint8)(material_editor_clamp01(blue)*255),255);
            SDL_RenderFillRect(renderer,&value_rect);
        } else MaterialEditorTextLeft(renderer,value_rect,row->value,palette.text_primary);
        s_response_action_fields[i]=row->field;
        if(editable) for(int action=0;action<2;++action) {
            SDL_Rect rect={bounds.x+bounds.w-(2-action)*action_w,cursor_y,action_w-2,row_h-4};
            s_response_action_rects[i][action]=rect;
            MaterialEditorDrawButton(renderer,rect,action ? "+" : "-",false,palette);
        }
        cursor_y+=row_h;
    }
    return cursor_y + MATERIAL_EDITOR_BUTTON_GAP;
}

static double material_editor_layer_influence_value(
    const MaterialEditorActiveLayerReadback* layer,
    MaterialEditorLayerInfluenceKind kind) {
    if (!layer) return 0.0;
    if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_ROUGHNESS) {
        return layer->roughness_influence;
    }
    if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_REFLECTIVITY) {
        return layer->reflectivity_influence;
    }
    if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_SPECULAR) {
        return layer->specular_influence;
    }
    if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_DIFFUSE) {
        return layer->diffuse_influence;
    }
    if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_TRANSPARENCY) {
        return layer->transparency_influence;
    }
    return 0.0;
}

static int material_editor_draw_layer_influence_controls(SDL_Renderer* renderer,
                                                         SDL_Rect bounds,
                                                         int cursor_y,
                                                         int bottom_y,
                                                         RayTracingThemePalette palette) {
    static const MaterialEditorLayerInfluenceKind kinds[MATERIAL_EDITOR_LAYER_INFLUENCE_CONTROL_COUNT] = {
        MATERIAL_EDITOR_LAYER_INFLUENCE_ROUGHNESS,
        MATERIAL_EDITOR_LAYER_INFLUENCE_REFLECTIVITY,
        MATERIAL_EDITOR_LAYER_INFLUENCE_SPECULAR,
        MATERIAL_EDITOR_LAYER_INFLUENCE_DIFFUSE,
        MATERIAL_EDITOR_LAYER_INFLUENCE_TRANSPARENCY
    };
    static const char* labels[MATERIAL_EDITOR_LAYER_INFLUENCE_CONTROL_COUNT] = {
        "Rough", "Reflect", "Spec", "Diffuse", "Trans"
    };
    MaterialEditorActiveLayerReadback layer = {0};
    char line[128];
    int row_h = 24;
    int action_w = 20;
    if (!renderer) return cursor_y;
    if (!MaterialEditorBuildActiveLayerReadback(&layer)) return cursor_y;
    if (!material_editor_has_room_for_optional_control(
            cursor_y,
            15 + row_h * MATERIAL_EDITOR_LAYER_INFLUENCE_CONTROL_COUNT,
            bottom_y)) {
        return cursor_y;
    }
    MATERIAL_EDITOR_SECTION_LABEL(renderer, bounds, cursor_y, "Layer Influence", palette);
    cursor_y += 15;
    for (int i = 0; i < MATERIAL_EDITOR_LAYER_INFLUENCE_CONTROL_COUNT; ++i) {
        int y = cursor_y + i * row_h;
        double value = material_editor_layer_influence_value(&layer, kinds[i]);
        if (y + row_h > bottom_y) break;
        s_layer_influence_action_fields[i] = kinds[i];
        snprintf(line, sizeof(line), "%s %+0.2f", labels[i], value);
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){bounds.x, y + 2, bounds.w - 2 * action_w - 8, 16},
                            line,
                            palette.text_primary);
        s_layer_influence_action_rects[i][0] =
            (SDL_Rect){bounds.x + bounds.w - 2 * action_w - 2, y, action_w, 18};
        s_layer_influence_action_rects[i][1] =
            (SDL_Rect){bounds.x + bounds.w - action_w, y, action_w, 18};
        MaterialEditorDrawButton(renderer,
                                 s_layer_influence_action_rects[i][0],
                                 "-",
                                 false,
                                 palette);
        MaterialEditorDrawButton(renderer,
                                 s_layer_influence_action_rects[i][1],
                                 "+",
                                 false,
                                 palette);
    }
    return cursor_y + row_h * MATERIAL_EDITOR_LAYER_INFLUENCE_CONTROL_COUNT +
           MATERIAL_EDITOR_BUTTON_GAP;
}

static int material_editor_draw_layer_blend_controls(SDL_Renderer* renderer,
                                                     SDL_Rect bounds,
                                                     int cursor_y,
                                                     int bottom_y,
                                                     RayTracingThemePalette palette) {
    MaterialEditorActiveLayerReadback layer = {0};
    char line[128];
    int action_w = 20;
    if (!renderer) return cursor_y;
    if (!MaterialEditorBuildActiveLayerReadback(&layer)) return cursor_y;
    if (!material_editor_has_room_for_optional_control(cursor_y, 15 + 24, bottom_y)) {
        return cursor_y;
    }
    MATERIAL_EDITOR_SECTION_LABEL(renderer, bounds, cursor_y, "Layer Blend", palette);
    cursor_y += 15;
    snprintf(line, sizeof(line), "Opacity %.2f", layer.opacity);
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y + 2, bounds.w - 2 * action_w - 8, 16},
                        line,
                        palette.text_primary);
    s_layer_opacity_action_rects[0] =
        (SDL_Rect){bounds.x + bounds.w - 2 * action_w - 2, cursor_y, action_w, 18};
    s_layer_opacity_action_rects[1] =
        (SDL_Rect){bounds.x + bounds.w - action_w, cursor_y, action_w, 18};
    MaterialEditorDrawButton(renderer,
                             s_layer_opacity_action_rects[0],
                             "-",
                             false,
                             palette);
    MaterialEditorDrawButton(renderer,
                             s_layer_opacity_action_rects[1],
                             "+",
                             false,
                             palette);
    return cursor_y + 24 + MATERIAL_EDITOR_BUTTON_GAP;
}

static int material_editor_draw_glass_overlay_shortcuts(SDL_Renderer* renderer,
                                                        SDL_Rect bounds,
                                                        int cursor_y,
                                                        int bottom_y,
                                                        const SceneObject* obj,
                                                        RayTracingThemePalette palette) {
    static const RuntimeMaterialTextureLayerKind overlay_kinds[MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT] = {
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE,
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG,
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES,
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL,
        RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME
    };
    static const char* overlay_labels[MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT] = {
        "Clear", "Fog", "Scratch", "Oil", "Grime"
    };
    RuntimeMaterialTextureLayer active_layer = {0};
    int active_index = 0;
    int button_w = 0;
    if (!renderer || !obj || obj->material_id != MATERIAL_PRESET_TRANSPARENT) {
        return cursor_y;
    }
    if (!material_editor_has_room_for_optional_control(cursor_y,
                                                       15 + MATERIAL_EDITOR_BUTTON_HEIGHT + 18,
                                                       bottom_y)) {
        return cursor_y;
    }
    material_editor_get_active_layer(obj, NULL, &active_layer, &active_index);
    MATERIAL_EDITOR_SECTION_LABEL(renderer, bounds, cursor_y, "Glass Overlays", palette);
    cursor_y += 15;
    button_w = (bounds.w -
                MATERIAL_EDITOR_BUTTON_GAP * (MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT - 1)) /
               MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT;
    for (int i = 0; i < MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT; ++i) {
        int x = bounds.x + i * (button_w + MATERIAL_EDITOR_BUTTON_GAP);
        int w = i == MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT - 1
                    ? bounds.x + bounds.w - x
                    : button_w;
        bool active = false;
        s_glass_overlay_action_kinds[i] = overlay_kinds[i];
        s_glass_overlay_action_rects[i] =
            (SDL_Rect){x, cursor_y, w, MATERIAL_EDITOR_BUTTON_HEIGHT};
        if (overlay_kinds[i] == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) {
            active = active_index == 0 || active_layer.role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE;
        } else {
            active = active_layer.kind == overlay_kinds[i];
        }
        MaterialEditorDrawButton(renderer,
                                 s_glass_overlay_action_rects[i],
                                 overlay_labels[i],
                                 active,
                                 palette);
    }
    cursor_y += MATERIAL_EDITOR_BUTTON_HEIGHT + MATERIAL_EDITOR_BUTTON_GAP;
    if (cursor_y + 16 <= bottom_y) {
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                            "Select or add an overlay; Resp edits the active layer.",
                            palette.text_muted);
        cursor_y += 18;
    }
    return cursor_y + MATERIAL_EDITOR_BUTTON_GAP;
}

static int material_editor_draw_active_layer_context(SDL_Renderer* renderer,
                                                     SDL_Rect bounds,
                                                     int cursor_y,
                                                     int bottom_y,
                                                     RayTracingThemePalette palette) {
    MaterialEditorActiveLayerReadback layer = {0};
    if (!renderer) return cursor_y;
    if (!material_editor_has_room_for_optional_control(cursor_y, 112, bottom_y)) {
        return cursor_y;
    }
    if (!MaterialEditorBuildActiveLayerReadback(&layer)) {
        RenderLabelTextWrappedLeft(renderer,
                                   (SDL_Rect){bounds.x, cursor_y, bounds.w, 34},
                                   layer.detail,
                                   palette.text_muted);
        return cursor_y + 38;
    }
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        layer.title,
                        palette.text_primary);
    cursor_y += 18;
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        layer.detail,
                        palette.text_muted);
    cursor_y += 18;
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        layer.state_label,
                        layer.enabled ? palette.text_primary : palette.text_muted);
    cursor_y += 18;
    RenderLabelTextLeft(renderer,
                        (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                        layer.response_summary,
                        palette.text_muted);
    cursor_y += 18;
    if (layer.has_effective_readback) {
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                            layer.effective_summary,
                            palette.text_primary);
        cursor_y += 18;
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){bounds.x, cursor_y, bounds.w, 16},
                            layer.effective_delta_summary,
                            palette.text_muted);
        cursor_y += 18;
    }
    return cursor_y + 20;
}

int MaterialEditorDrawCompactResponsePane(SDL_Renderer* renderer,
                                          SDL_Rect bounds,
                                          int cursor_y,
                                          int bottom_y,
                                          const SceneObject* obj,
                                          RayTracingThemePalette palette) {
    (void)obj;
    cursor_y = material_editor_draw_response_readback_grid(renderer,bounds,cursor_y,bottom_y,palette);
    return cursor_y;
}

/* Deeper controls stay attached to the section whose data they edit. */
int MaterialEditorDrawPatternParameters(SDL_Renderer* renderer, SDL_Rect bounds, int y, int bottom,
                                        const SceneObject* obj, RayTracingThemePalette palette) {
    return material_editor_draw_response_controls(renderer,bounds,y,bottom,obj,palette);
}
int MaterialEditorDrawLayerComposition(SDL_Renderer* renderer, SDL_Rect bounds, int y, int bottom,
                                       RayTracingThemePalette palette) {

    y=material_editor_draw_glass_overlay_shortcuts(renderer,bounds,y,bottom,material_editor_focused_object(),palette);
    y=material_editor_draw_layer_blend_controls(renderer,bounds,y,bottom,palette);
    return material_editor_draw_layer_influence_controls(renderer,bounds,y,bottom,palette);
}

int MaterialEditorDrawLayerDiagnostics(SDL_Renderer* r, SDL_Rect b, int y, int bottom, RayTracingThemePalette p) {
    return material_editor_draw_active_layer_context(r,b,y,bottom,p);
}
