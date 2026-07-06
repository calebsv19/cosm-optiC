#include "editor/material_editor_internal.h"

#include "editor/material_editor_authored_texture_binding.h"
#include "editor/material_editor_face_preview.h"
#include "editor/material_editor_knob_control.h"

static bool material_editor_point_in_rect(int x, int y, const SDL_Rect* rect) {
    return rect && rect->w > 0 && rect->h > 0 &&
           x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static bool material_editor_scroll_group_list(int wheel_y) {
    int max_offset = s_group_total_count - s_group_visible_capacity;
    if (wheel_y == 0 || max_offset <= 0) return false;
    s_group_scroll_offset -= wheel_y;
    if (s_group_scroll_offset < 0) s_group_scroll_offset = 0;
    if (s_group_scroll_offset > max_offset) s_group_scroll_offset = max_offset;
    return true;
}

static bool material_editor_scroll_layer_list(int wheel_y) {
    int max_offset = s_layer_total_count - s_layer_visible_capacity;
    if (wheel_y == 0 || max_offset <= 0) return false;
    s_layer_scroll_offset -= wheel_y;
    if (s_layer_scroll_offset < 0) s_layer_scroll_offset = 0;
    if (s_layer_scroll_offset > max_offset) s_layer_scroll_offset = max_offset;
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

void HandleMaterialEditorEvents(SDL_Event* event) {
    if (!event) return;
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        if (MaterialEditorFacePreviewHandleEvent(event)) {
            return;
        }
        for (int i = 0; i < MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_recipe_menu_item_rects[i])) {
                MaterialEditorApplyRecipeOptionForFocused(MaterialEditorGetRecipeMenuAxis(), i);
                return;
            }
        }
        if (material_editor_point_in_rect(mx,
                                          my,
                                          &s_material_editor_compact_layout_rects.identity_disclosure)) {
            MaterialEditorSetRecipeMenuAxis(MATERIAL_EDITOR_RECIPE_AXIS_NONE);
            MaterialEditorToggleIdentityPopover();
            return;
        }
        for (int i = 0; i < MATERIAL_EDITOR_RECIPE_ACTION_COUNT; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_recipe_action_rects[i])) {
                MaterialEditorToggleRecipeMenuAxis((MaterialEditorRecipeAxis)i);
                return;
            }
        }
        if (MaterialEditorGetRecipeMenuAxis() != MATERIAL_EDITOR_RECIPE_AXIS_NONE) {
            MaterialEditorSetRecipeMenuAxis(MATERIAL_EDITOR_RECIPE_AXIS_NONE);
        }
        if (material_editor_point_in_rect(mx,
                                          my,
                                          &s_material_editor_compact_layout_rects.identity_header)) {
            MaterialEditorToggleIdentityPopover();
            return;
        }
        if (s_material_editor_compact_layout_rects.identity_popover_visible) {
            if (material_editor_point_in_rect(mx,
                                              my,
                                              &s_material_editor_compact_layout_rects.identity_popover)) {
                return;
            }
            MaterialEditorSetIdentityPopoverOpen(false);
            return;
        }
        for (int i = 0; i < MATERIAL_EDITOR_SUBPANE_COUNT; ++i) {
            if (material_editor_point_in_rect(mx,
                                              my,
                                              &s_material_editor_compact_layout_rects.tab_rects[i])) {
                MaterialEditorSetActiveSubPane((MaterialEditorSubPane)i);
                MaterialEditorSetIdentityPopoverOpen(false);
                return;
            }
        }
        if (MaterialEditorGetActiveSubPane() == MATERIAL_EDITOR_SUBPANE_TEXTURES &&
            MaterialEditorAuthoredTextureBindingHandleEvent(
                event,
                MaterialEditorResolveFocusedObjectIndex())) {
            return;
        }
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
        if (material_editor_point_in_rect(mx, my, &s_proof_readback_rect)) {
            MaterialEditorPrimeProofReadbackForFocused();
            return;
        }
        for (int i = 0; i < MATERIAL_EDITOR_GRAPH_ACTION_COUNT; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_graph_action_rects[i])) {
                if (i == 0) {
                    MaterialEditorEnsureGraphForFocused();
                } else if (i == 1) {
                    MaterialEditorAddGraphLayerNodeForFocused();
                } else if (i == 2) {
                    MaterialEditorAddGraphChannelNodeForFocused();
                } else if (i == 3) {
                    MaterialEditorClearGraphForFocused();
                }
                return;
            }
        }
        for (int i = 0; i < MATERIAL_EDITOR_LAYER_ACTION_COUNT; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_layer_action_rects[i])) {
                if (i == 0) {
                    MaterialEditorAddOverlayLayerToFocused();
                } else if (i == 1) {
                    MaterialEditorToggleActiveLayerEnabled();
                } else if (i == 2) {
                    MaterialEditorMoveActiveLayer(-1);
                } else if (i == 3) {
                    MaterialEditorMoveActiveLayer(1);
                } else if (i == 4) {
                    MaterialEditorDeleteActiveLayer();
                }
                return;
            }
        }
        for (int i = 0; i < s_layer_row_count; ++i) {
            for (int action_index = 0;
                 action_index < MATERIAL_EDITOR_LAYER_ROW_ACTION_COUNT;
                 ++action_index) {
                if (material_editor_point_in_rect(mx,
                                                  my,
                                                  &s_layer_row_action_rects[i][action_index])) {
                    MaterialEditorSetActiveLayerIndex(s_layer_row_indices[i]);
                    if (action_index == 0) {
                        MaterialEditorMoveActiveLayer(-1);
                    } else if (action_index == 1) {
                        MaterialEditorMoveActiveLayer(1);
                    } else if (action_index == 2) {
                        MaterialEditorDeleteActiveLayer();
                    }
                    return;
                }
            }
            if (material_editor_point_in_rect(mx, my, &s_layer_toggle_rects[i])) {
                MaterialEditorSetActiveLayerIndex(s_layer_row_indices[i]);
                MaterialEditorToggleActiveLayerEnabled();
                return;
            }
            if (material_editor_point_in_rect(mx, my, &s_layer_row_rects[i])) {
                MaterialEditorSetActiveLayerIndex(s_layer_row_indices[i]);
                return;
            }
        }
        for (int i = 0; i < MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_layer_kind_rects[i])) {
                MaterialEditorApplyLayerKindToFocused(s_layer_kind_rect_kinds[i]);
                return;
            }
        }
        for (int i = 0; i < MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_glass_overlay_action_rects[i])) {
                MaterialEditorApplyGlassOverlayForFocused(s_glass_overlay_action_kinds[i]);
                return;
            }
        }
        if (material_editor_point_in_rect(mx, my, &s_layer_opacity_action_rects[0])) {
            MaterialEditorApplyLayerOpacityStepToFocused(-0.05);
            return;
        }
        if (material_editor_point_in_rect(mx, my, &s_layer_opacity_action_rects[1])) {
            MaterialEditorApplyLayerOpacityStepToFocused(0.05);
            return;
        }
        for (int i = 0; i < MATERIAL_EDITOR_LAYER_INFLUENCE_CONTROL_COUNT; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_layer_influence_action_rects[i][0])) {
                MaterialEditorApplyLayerInfluenceStepToFocused(
                    s_layer_influence_action_fields[i],
                    -0.05);
                return;
            }
            if (material_editor_point_in_rect(mx, my, &s_layer_influence_action_rects[i][1])) {
                MaterialEditorApplyLayerInfluenceStepToFocused(
                    s_layer_influence_action_fields[i],
                    0.05);
                return;
            }
        }
        for (int i = 0; i < MATERIAL_EDITOR_RESPONSE_MAX_ROWS; ++i) {
            if (material_editor_point_in_rect(mx, my, &s_response_action_rects[i][0])) {
                MaterialEditorApplyResponseStepToFocused(s_response_action_fields[i], -0.05);
                return;
            }
            if (material_editor_point_in_rect(mx, my, &s_response_action_rects[i][1])) {
                MaterialEditorApplyResponseStepToFocused(s_response_action_fields[i], 0.05);
                return;
            }
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
                s_material_editor_param_drag_start_y = my;
                s_material_editor_param_drag_start_value =
                    material_editor_value_for_param_slider(material_editor_focused_object(),
                                                           s_material_editor_active_param_slider);
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
                MaterialEditorKnobValueFromDrag(s_material_editor_param_drag_start_value,
                                                s_material_editor_param_drag_start_y,
                                                event->motion.y));
        }
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        s_material_editor_active_slider = MATERIAL_EDITOR_SLIDER_NONE;
        s_material_editor_active_param_slider = MATERIAL_EDITOR_TEXTURE_PARAM_NONE;
    } else if (event->type == SDL_MOUSEWHEEL) {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        if (material_editor_point_in_rect(mx, my, &s_layer_panel_rect) ||
            material_editor_point_in_rect(mx, my, &s_layer_list_rect)) {
            material_editor_scroll_layer_list(event->wheel.y);
        } else if (material_editor_point_in_rect(mx, my, &s_group_panel_rect) ||
                   material_editor_point_in_rect(mx, my, &s_group_list_rect)) {
            material_editor_scroll_group_list(event->wheel.y);
        }
    }
}

MaterialEditorHitRegion MaterialEditorHitRegionAtPoint(int mx, int my) {
    if (MaterialEditorFacePreviewHitTest(mx, my)) {
        return MATERIAL_EDITOR_HIT_CONTROLS;
    }
    for (int i = 0; i < MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_recipe_menu_item_rects[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    if (material_editor_point_in_rect(mx, my, &s_material_editor_compact_layout_rects.identity_header) ||
        material_editor_point_in_rect(mx, my, &s_material_editor_compact_layout_rects.identity_disclosure) ||
        material_editor_point_in_rect(mx, my, &s_material_editor_compact_layout_rects.identity_popover)) {
        return MATERIAL_EDITOR_HIT_CONTROLS;
    }
    for (int i = 0; i < MATERIAL_EDITOR_RECIPE_ACTION_COUNT; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_recipe_action_rects[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    for (int i = 0; i < MATERIAL_EDITOR_SUBPANE_COUNT; ++i) {
        if (material_editor_point_in_rect(mx,
                                          my,
                                          &s_material_editor_compact_layout_rects.tab_rects[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    if (material_editor_point_in_rect(mx, my, &s_texture_none_rect) ||
        material_editor_point_in_rect(mx, my, &s_texture_rust_rect) ||
        material_editor_point_in_rect(mx, my, &s_texture_fog_rect) ||
        material_editor_point_in_rect(mx, my, &s_solid_faces_rect) ||
        material_editor_point_in_rect(mx, my, &s_reset_face_rect) ||
        material_editor_point_in_rect(mx, my, &s_copy_face_rect) ||
        material_editor_point_in_rect(mx, my, &s_proof_readback_rect) ||
        material_editor_point_in_rect(mx, my, &s_clear_groups_rect)) {
        return MATERIAL_EDITOR_HIT_CONTROLS;
    }
    for (int i = 0; i < MATERIAL_EDITOR_GRAPH_ACTION_COUNT; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_graph_action_rects[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    for (int i = 0; i < MATERIAL_EDITOR_LAYER_ACTION_COUNT; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_layer_action_rects[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    for (int i = 0; i < s_layer_row_count; ++i) {
        for (int action_index = 0;
             action_index < MATERIAL_EDITOR_LAYER_ROW_ACTION_COUNT;
             ++action_index) {
            if (material_editor_point_in_rect(mx,
                                              my,
                                              &s_layer_row_action_rects[i][action_index])) {
                return MATERIAL_EDITOR_HIT_CONTROLS;
            }
        }
        if (material_editor_point_in_rect(mx, my, &s_layer_toggle_rects[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
        if (material_editor_point_in_rect(mx, my, &s_layer_row_rects[i])) {
            return MATERIAL_EDITOR_HIT_LIST_PANEL;
        }
    }
    if (material_editor_point_in_rect(mx, my, &s_layer_panel_rect) ||
        material_editor_point_in_rect(mx, my, &s_layer_list_rect)) {
        return MATERIAL_EDITOR_HIT_LIST_PANEL;
    }
    for (int i = 0; i < MATERIAL_EDITOR_LAYER_KIND_BUTTON_COUNT; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_layer_kind_rects[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    for (int i = 0; i < MATERIAL_EDITOR_GLASS_OVERLAY_ACTION_COUNT; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_glass_overlay_action_rects[i])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    if (material_editor_point_in_rect(mx, my, &s_layer_opacity_action_rects[0]) ||
        material_editor_point_in_rect(mx, my, &s_layer_opacity_action_rects[1])) {
        return MATERIAL_EDITOR_HIT_CONTROLS;
    }
    for (int i = 0; i < MATERIAL_EDITOR_LAYER_INFLUENCE_CONTROL_COUNT; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_layer_influence_action_rects[i][0]) ||
            material_editor_point_in_rect(mx, my, &s_layer_influence_action_rects[i][1])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
    }
    for (int i = 0; i < MATERIAL_EDITOR_RESPONSE_MAX_ROWS; ++i) {
        if (material_editor_point_in_rect(mx, my, &s_response_action_rects[i][0]) ||
            material_editor_point_in_rect(mx, my, &s_response_action_rects[i][1])) {
            return MATERIAL_EDITOR_HIT_CONTROLS;
        }
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
