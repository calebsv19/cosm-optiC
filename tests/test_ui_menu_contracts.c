#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/animation.h"
#include "app/data_paths.h"
#include "config/config_manager.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/render_helper.h"
#include "test_support.h"
#include "ui/menu_batch_panel.h"
#include "ui/menu_layout.h"
#include "ui/menu_panel_chrome.h"
#include "ui/scene_source_catalog.h"
#include "ui/scene_source_ui_labels.h"
#include "ui/sdl_menu_render.h"
#include "ui/sdl_menu_state.h"
#include "ui/volume_source_catalog.h"
#include "ui/volume_source_ui_labels.h"

static bool test_volume_catalog_entry_path(const VolumeSourceCatalogEntry* entries,
                                           size_t count,
                                           const char* path) {
    char resolved_target[PATH_MAX];
    const char* use_target = path;
    if (!entries || !path) return false;
    if (realpath(path, resolved_target)) {
        use_target = resolved_target;
    }
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].path, use_target) == 0) {
            return true;
        }
    }
    return false;
}

static int test_menu_batch_panel_click_starts_frame_dir_edit(void) {
    MenuRuntimeState state;
    MenuBatchPanelLayout layout;
    SDL_Event event;
    memset(&state, 0, sizeof(state));
    memset(&layout, 0, sizeof(layout));
    memset(&event, 0, sizeof(event));

    strncpy(animSettings.frameDir, "/tmp/ray_tracing_menu_frames", sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    layout.frameDirValueRect = (SDL_Rect){100, 120, 220, 34};

    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.x = 110;
    event.button.y = 130;
    event.button.clicks = 1;

    assert_true("menu_batch_frame_edit_click_consumed",
                menu_batch_panel_handle_click(&event, NULL, NULL, &state, &layout));
    assert_true("menu_batch_frame_edit_active", state.editingFrameDir);
    assert_true("menu_batch_frame_edit_buffer",
                strcmp(state.pathInputBuffer, animSettings.frameDir) == 0);
    return 0;
}

static int test_menu_batch_panel_clear_button_updates_frame_count(void) {
    char tmp_template[] = "/tmp/ray_tracing_menu_batch_clear_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char frame0[PATH_MAX];
    char frame1[PATH_MAX];
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    MenuRuntimeState state;
    MenuBatchPanelLayout layout;
    SDL_Event event;

    assert_true("menu_batch_clear_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(frame0, sizeof(frame0), "%s/frame_0000.bmp", tmp_root);
    snprintf(frame1, sizeof(frame1), "%s/frame_0001.bmp", tmp_root);
    assert_true("menu_batch_clear_write_frame0", write_text_file(frame0, "a"));
    assert_true("menu_batch_clear_write_frame1", write_text_file(frame1, "b"));

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    memset(&state, 0, sizeof(state));
    memset(&layout, 0, sizeof(layout));
    memset(&event, 0, sizeof(event));

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    menu_batch_panel_refresh(&state);

    layout.clearFramesRect = (SDL_Rect){200, 200, 118, 34};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.x = 210;
    event.button.y = 210;
    event.button.clicks = 1;

    assert_true("menu_batch_clear_click_consumed",
                menu_batch_panel_handle_click(&event, NULL, NULL, &state, &layout));
    assert_true("menu_batch_clear_removed_frame0", !path_exists(frame0));
    assert_true("menu_batch_clear_removed_frame1", !path_exists(frame1));
    assert_true("menu_batch_clear_count_zero", state.exportBatchStatus.frame_count == 0u);
    assert_true("menu_batch_clear_status_label",
                strcmp(state.statusLabel, "Cleared 2 frames") == 0);

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    rmdir(tmp_root);
    return 0;
}

static int test_menu_layout_builds_non_overlapping_primary_zones(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_layout_build_base(NULL, &state, &screen);

    assert_true("menu_layout_left_panel_has_width", screen.leftPanelRect.w >= 350);
    assert_true("menu_layout_slider_panel_has_width", screen.sliderPanelRect.w >= 300);
    assert_true("menu_layout_center_panel_has_width", screen.centerBatchRect.w >= 300);
    assert_true("menu_layout_left_before_center",
                screen.leftPanelRect.x + screen.leftPanelRect.w < screen.centerBatchRect.x);
    assert_true("menu_layout_center_before_slider",
                screen.centerBatchRect.x + screen.centerBatchRect.w < screen.sliderPanelRect.x);
    assert_true("menu_layout_center_above_footer",
                screen.centerBatchRect.y + screen.centerBatchRect.h <= screen.bottomActionRowRect.y);
    animSettings = saved_anim;
    return 0;
}

static int test_menu_layout_keeps_manifest_dropdown_inside_left_panel(void) {
    MenuRuntimeState state;
    MenuButtonLayout buttons;
    MenuScreenLayout screen;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&buttons, 0, sizeof(buttons));
    memset(&screen, 0, sizeof(screen));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    state.manifestDropdownOpen = true;
    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);

    menu_layout_finalize_with_buttons(&screen, &buttons, &state);

    assert_true("menu_layout_manifest_overlay_visible", screen.manifestReserveRect.w > 0 && screen.manifestReserveRect.h > 0);
    assert_true("menu_layout_manifest_overlay_below_input_row",
                screen.manifestReserveRect.y >= buttons.inputRootValueRect.y + buttons.inputRootValueRect.h);
    assert_true("menu_layout_manifest_overlay_inside_left_panel_left",
                screen.manifestReserveRect.x >= screen.leftPanelRect.x);
    assert_true("menu_layout_manifest_overlay_inside_left_panel_right",
                test_rect_right(&screen.manifestReserveRect) <= test_rect_right(&screen.leftPanelRect));
    assert_true("menu_layout_manifest_overlay_inside_left_panel_bottom",
                test_rect_bottom(&screen.manifestReserveRect) <= test_rect_bottom(&screen.leftPanelRect));
    assert_true("menu_layout_batch_y_unchanged_when_manifest_opens",
                screen.centerBatchRect.y == screen.centerControlsRect.y + screen.centerControlsRect.h + 18);
    assert_true("menu_layout_batch_above_footer",
                screen.centerBatchRect.y + screen.centerBatchRect.h <= screen.bottomActionRowRect.y);
    animSettings = saved_anim;
    return 0;
}

static int test_menu_button_layout_respects_owned_screen_zones(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    MenuButtonLayout buttons;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&buttons, 0, sizeof(buttons));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.editorMode = 0;

    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    menu_layout_finalize_with_buttons(&screen, &buttons, &state);

    assert_true("menu_buttons_left_controls_inside_left_panel",
                buttons.loadSceneRect.x >= screen.leftPanelRect.x &&
                test_rect_right(&buttons.inputRootApplyRect) <= test_rect_right(&screen.leftPanelRect));
    assert_true("menu_buttons_center_controls_inside_center_zone",
                buttons.falloffRect.x >= screen.centerControlsRect.x &&
                test_rect_right(&buttons.topFillRect) <= test_rect_right(&screen.centerControlsRect));
    assert_true("menu_buttons_route_stack_inside_route_zone",
                buttons.spaceModeRect.x >= screen.routeStackRect.x &&
                test_rect_right(&buttons.previewRect) <= test_rect_right(&screen.routeStackRect) &&
                buttons.spaceModeRect.y >= screen.routeStackRect.y &&
                test_rect_bottom(&buttons.previewRect) <= test_rect_bottom(&screen.routeStackRect));
    assert_true("menu_buttons_frame_resume_controls_inside_route_zone",
                buttons.resumeFramesRect.x >= screen.routeStackRect.x &&
                test_rect_right(&buttons.nextFrameRect) <= test_rect_right(&screen.routeStackRect) &&
                buttons.resumeFramesRect.y >= screen.routeStackRect.y &&
                test_rect_bottom(&buttons.nextFrameRect) <= test_rect_bottom(&screen.routeStackRect));
    assert_true("menu_buttons_frame_resume_controls_left_of_start_stack",
                test_rect_right(&buttons.nextFrameRect) <= buttons.startRect.x);
    assert_true("menu_buttons_footer_inside_bottom_row",
                buttons.exitRect.y >= screen.bottomActionRowRect.y &&
                test_rect_bottom(&buttons.exitRect) <= test_rect_bottom(&screen.bottomActionRowRect) &&
                test_rect_right(&buttons.saveRect) <= test_rect_right(&screen.bottomActionRowRect));
    animSettings = saved_anim;
    return 0;
}

static int test_menu_button_layout_exposes_volume_controls_in_3d_only(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    MenuButtonLayout buttons;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&buttons, 0, sizeof(buttons));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    assert_true("menu_buttons_volume_attach_visible_3d", buttons.attachVolumeRect.w > 0);
    assert_true("menu_buttons_volume_toggle_visible_3d", buttons.volumeToggleRect.w > 0);
    assert_true("menu_buttons_volume_clear_visible_3d", buttons.volumeClearRect.w > 0);

    animSettings.spaceMode = SPACE_MODE_2D;
    memset(&buttons, 0, sizeof(buttons));
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    assert_true("menu_buttons_volume_attach_hidden_2d", buttons.attachVolumeRect.w == 0);
    assert_true("menu_buttons_volume_toggle_hidden_2d", buttons.volumeToggleRect.w == 0);
    assert_true("menu_buttons_volume_clear_hidden_2d", buttons.volumeClearRect.w == 0);

    animSettings = saved_anim;
    return 0;
}

static int test_menu_batch_panel_layout_centers_inside_batch_zone(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    MenuButtonLayout buttons;
    MenuBatchPanelLayout batch;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&buttons, 0, sizeof(buttons));
    memset(&batch, 0, sizeof(batch));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    menu_layout_finalize_with_buttons(&screen, &buttons, &state);
    menu_batch_panel_build_layout(NULL, &state, &screen, &batch);

    assert_true("menu_batch_panel_inside_center_zone_left",
                batch.panelRect.x >= screen.centerBatchRect.x);
    assert_true("menu_batch_panel_inside_center_zone_right",
                test_rect_right(&batch.panelRect) <= test_rect_right(&screen.centerBatchRect));
    assert_true("menu_batch_panel_inside_center_zone_vertical",
                batch.panelRect.y >= screen.centerBatchRect.y &&
                test_rect_bottom(&batch.panelRect) <= test_rect_bottom(&screen.centerBatchRect));
    assert_true("menu_batch_panel_centered_horizontally",
                abs((batch.panelRect.x - screen.centerBatchRect.x) -
                    (test_rect_right(&screen.centerBatchRect) - test_rect_right(&batch.panelRect))) <= 2);

    animSettings = saved_anim;
    return 0;
}

static int test_menu_batch_panel_header_does_not_overlap_route_rows(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    MenuButtonLayout buttons;
    MenuBatchPanelLayout batch;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&buttons, 0, sizeof(buttons));
    memset(&batch, 0, sizeof(batch));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    menu_layout_finalize_with_buttons(&screen, &buttons, &state);
    menu_batch_panel_build_layout(NULL, &state, &screen, &batch);

    assert_true("menu_batch_header_label_inside_panel",
                batch.videoFileLabelRect.y >= batch.panelRect.y + MENU_PANEL_CHROME_TITLE_BAND);
    assert_true("menu_batch_header_divider_below_label",
                batch.headerDividerRect.y > batch.videoFileLabelRect.y);
    assert_true("menu_batch_frame_row_below_header_divider",
                batch.frameDirValueRect.y >= batch.headerDividerRect.y + batch.headerDividerRect.h + 6);
    assert_true("menu_batch_video_row_below_frame_row",
                batch.videoRootValueRect.y >= batch.frameDirValueRect.y + batch.frameDirValueRect.h + 8);

    animSettings = saved_anim;
    return 0;
}

static int test_integrator_catalog_menu_routes_by_space_mode(void) {
    AnimationConfig saved_anim = animSettings;
    RayTracingIntegratorMenuState menu_state;
    MenuButtonLayout buttons;
    MenuRuntimeState state;
    MenuScreenLayout screen;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_3d_uses_3d_catalog", menu_state.uses3DCatalog);
    assert_true("integrator_menu_3d_label",
                strcmp(menu_state.buttonLabel, "Integrator: 3D Disney") == 0);
    assert_true("integrator_menu_3d_no_path_toggles", !menu_state.showPathToggles);
    assert_true("integrator_menu_3d_visible_count_five", menu_state.visibleCount == 5);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_3d_diffuse_label",
                strcmp(menu_state.buttonLabel, "Integrator: 3D Diffuse Bounce") == 0);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_MATERIAL;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_3d_material_label",
                strcmp(menu_state.buttonLabel, "Integrator: 3D Material") == 0);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_3d_emission_transparency_label",
                strcmp(menu_state.buttonLabel,
                       "Integrator: 3D Emission / Transparency") == 0);

    memset(&buttons, 0, sizeof(buttons));
    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    assert_true("integrator_menu_3d_layout_no_path_toggles", !buttons.showPathToggles);
    assert_true("integrator_menu_top_fill_button_in_center_controls",
                buttons.topFillRect.x >= screen.centerControlsRect.x &&
                buttons.topFillRect.x + buttons.topFillRect.w <=
                    screen.centerControlsRect.x + screen.centerControlsRect.w);
    assert_true("integrator_menu_denoise_button_in_center_controls",
                buttons.denoiseRect.x >= screen.centerControlsRect.x &&
                buttons.denoiseRect.x + buttons.denoiseRect.w <=
                    screen.centerControlsRect.x + screen.centerControlsRect.w);
    assert_true("integrator_menu_denoise_button_right_of_tile_column",
                buttons.denoiseRect.x > buttons.tilePreviewRect.x);
    assert_true("integrator_menu_top_fill_button_below_denoise",
                buttons.topFillRect.y >= buttons.denoiseRect.y + buttons.denoiseRect.h);

    animSettings.spaceMode = SPACE_MODE_2D;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_2d_uses_legacy_catalog", !menu_state.uses3DCatalog);
    assert_true("integrator_menu_2d_label",
                strcmp(menu_state.buttonLabel, "Integrator: Hybrid") == 0);
    assert_true("integrator_menu_2d_show_path_toggles", menu_state.showPathToggles);

    animSettings = saved_anim;
    return 0;
}

static int test_menu_slider_layout_includes_environment_control(void) {
    AnimationConfig saved_anim = animSettings;
    MenuRuntimeState state;
    MenuScreenLayout screen;
    SliderLayout sliders;
    bool found_environment = false;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&sliders, 0, sizeof(sliders));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.environmentBrightness = 128.0;

    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_slider_layout(NULL, &state, &screen, &sliders);

    for (size_t i = 0; i < sliders.count; ++i) {
        if (sliders.items[i].label &&
            strcmp(sliders.items[i].label, "Environment") == 0 &&
            sliders.items[i].value == &state.envSliderValue) {
            found_environment = true;
            break;
        }
    }

    assert_true("menu_slider_layout_environment_present", found_environment);
    assert_true("menu_slider_layout_environment_value_synced",
                state.envSliderValue == 128);
    animSettings = saved_anim;
    return 0;
}

static int test_menu_slider_layout_routes_bounce_controls_by_space_mode(void) {
    AnimationConfig saved_anim = animSettings;
    MenuRuntimeState state;
    MenuScreenLayout screen;
    SliderLayout sliders;
    bool found_legacy_bounce = false;
    bool found_legacy_roulette = false;
    bool found_3d_bounce = false;
    bool found_3d_roulette = false;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&sliders, 0, sizeof(sliders));
    memset(&animSettings, 0, sizeof(animSettings));

    animSettings.spaceMode = SPACE_MODE_2D;
    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_slider_layout(NULL, &state, &screen, &sliders);
    for (size_t i = 0; i < sliders.count; ++i) {
        if (sliders.items[i].label &&
            strcmp(sliders.items[i].label, "Bounce Limit") == 0) {
            found_legacy_bounce = true;
        }
        if (sliders.items[i].label &&
            strcmp(sliders.items[i].label, "Roulette Threshold") == 0) {
            found_legacy_roulette = true;
        }
    }
    assert_true("menu_slider_layout_2d_legacy_bounce_present", found_legacy_bounce);
    assert_true("menu_slider_layout_2d_legacy_roulette_present", found_legacy_roulette);

    memset(&sliders, 0, sizeof(sliders));
    found_legacy_bounce = false;
    found_legacy_roulette = false;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.bounceDepth3D = 4;
    animSettings.rouletteThreshold3D = 0.02;
    menu_layout_build_base(NULL, &state, &screen);
    menu_render_build_slider_layout(NULL, &state, &screen, &sliders);
    for (size_t i = 0; i < sliders.count; ++i) {
        if (sliders.items[i].label &&
            strcmp(sliders.items[i].label, "Bounce Limit") == 0) {
            found_legacy_bounce = true;
        }
        if (sliders.items[i].label &&
            strcmp(sliders.items[i].label, "Roulette Threshold") == 0) {
            found_legacy_roulette = true;
        }
        if (sliders.items[i].label &&
            strcmp(sliders.items[i].label, "3D Bounce Depth") == 0 &&
            sliders.items[i].value == &state.bounceDepth3DSliderValue) {
            found_3d_bounce = true;
        }
        if (sliders.items[i].label &&
            strcmp(sliders.items[i].label, "3D Roulette Threshold") == 0 &&
            sliders.items[i].value == &state.rouletteThreshold3DSliderValue) {
            found_3d_roulette = true;
        }
    }
    assert_true("menu_slider_layout_3d_legacy_bounce_hidden", !found_legacy_bounce);
    assert_true("menu_slider_layout_3d_legacy_roulette_hidden", !found_legacy_roulette);
    assert_true("menu_slider_layout_3d_bounce_present", found_3d_bounce);
    assert_true("menu_slider_layout_3d_roulette_present", found_3d_roulette);
    assert_true("menu_slider_layout_3d_bounce_synced",
                state.bounceDepth3DSliderValue == 4);
    assert_true("menu_slider_layout_3d_roulette_synced",
                state.rouletteThreshold3DSliderValue == 20);

    animSettings = saved_anim;
    return 0;
}

static int test_integrator_catalog_cycle_preserves_inactive_mode(void) {
    AnimationConfig saved_anim = animSettings;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    animSettings.spaceMode = SPACE_MODE_3D;

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_keeps_2d",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_HYBRID);
    assert_true("integrator_cycle_3d_wraps_disney_to_direct_light",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_advanced_to_diffuse_bounce",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE);

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_advanced_to_material",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_MATERIAL);

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_advanced_to_emission_transparency",
                animSettings.integratorMode3D ==
                    RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY);

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_advanced_to_disney",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DISNEY);

    animSettings.spaceMode = SPACE_MODE_2D;
    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_2d_advanced_to_direct",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("integrator_cycle_2d_keeps_3d",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DISNEY);

    animSettings = saved_anim;
    return 0;
}

static int test_menu_fit_text_to_width_supports_in_place_buffer(void) {
    char text[128];

    snprintf(text, sizeof(text), "Render Frames Root: /tmp/example/very/long/path/value");
    menu_render_fit_text_to_width(NULL, text, 0, text, sizeof(text));

    assert_true("menu_fit_text_in_place_nonempty", text[0] != '\0');
    assert_true("menu_fit_text_in_place_prefix",
                strstr(text, "Render Frames Root") == text);
    return 0;
}

static int test_manifest_default_roots_expands_runtime_and_legacy_paths(void) {
    const char *input_env = getenv("RAY_TRACING_INPUT_ROOT");
    const char *output_env = getenv("RAY_TRACING_OUTPUT_ROOT");
    char input_backup[PATH_MAX] = {0};
    char output_backup[PATH_MAX] = {0};
    bool had_input_env = false;
    bool had_output_env = false;
    const char **roots = NULL;
    size_t root_count = 0;

    if (input_env && input_env[0]) {
        strncpy(input_backup, input_env, sizeof(input_backup) - 1);
        input_backup[sizeof(input_backup) - 1] = '\0';
        had_input_env = true;
    }
    if (output_env && output_env[0]) {
        strncpy(output_backup, output_env, sizeof(output_backup) - 1);
        output_backup[sizeof(output_backup) - 1] = '\0';
        had_output_env = true;
    }

    unsetenv("RAY_TRACING_INPUT_ROOT");
    unsetenv("RAY_TRACING_OUTPUT_ROOT");

    root_count = ray_tracing_manifest_default_roots(&roots);
    assert_true("manifest_roots_nonempty", root_count > 0 && roots != NULL);
    assert_true("manifest_roots_has_config_samples",
                path_list_contains(roots, root_count, "config/samples"));
    assert_true("manifest_roots_has_runtime_scenes",
                path_list_contains(roots, root_count, "data/runtime/scenes"));
    assert_true("manifest_roots_has_legacy_physics_samples",
                path_list_contains(roots, root_count, "../physics_sim/config/samples"));

    if (had_input_env) {
        setenv("RAY_TRACING_INPUT_ROOT", input_backup, 1);
    } else {
        unsetenv("RAY_TRACING_INPUT_ROOT");
    }
    if (had_output_env) {
        setenv("RAY_TRACING_OUTPUT_ROOT", output_backup, 1);
    } else {
        unsetenv("RAY_TRACING_OUTPUT_ROOT");
    }
    return 0;
}

static int test_scene_source_catalog_collect_admits_runtime_and_manifest_lanes(void) {
    char tmp_template[] = "/tmp/ray_tracing_catalog_s6_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char runtime_path[PATH_MAX];
    char authoring_path[PATH_MAX];
    char manifest_path[PATH_MAX];
    const char *roots[1];
    SceneSourceCatalogEntry entries[16];
    size_t entry_count = 0;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_catalog_runtime\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    const char *authoring_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_catalog_authoring\","
        "\"objects\":[]"
        "}";
    const char *manifest_json =
        "{"
        "\"schema\":\"fluid_manifest_v1\","
        "\"meta\":{\"name\":\"s6_catalog\"}"
        "}";

    assert_true("scene_catalog_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", tmp_root);
    snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", tmp_root);
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", tmp_root);

    assert_true("scene_catalog_write_runtime", write_text_file(runtime_path, runtime_json));
    assert_true("scene_catalog_write_authoring", write_text_file(authoring_path, authoring_json));
    assert_true("scene_catalog_write_manifest", write_text_file(manifest_path, manifest_json));

    roots[0] = tmp_root;
    entry_count = scene_source_catalog_collect(entries,
                                               sizeof(entries) / sizeof(entries[0]),
                                               roots,
                                               1,
                                               manifest_path,
                                               authoring_path);

    assert_true("scene_catalog_entry_count_min", entry_count >= 2);
    assert_true("scene_catalog_runtime_count_one",
                catalog_count_source(entries, entry_count, SCENE_SOURCE_RUNTIME_SCENE) == 1);
    assert_true("scene_catalog_manifest_present",
                catalog_contains_path_source(entries,
                                             entry_count,
                                             manifest_path,
                                             SCENE_SOURCE_FLUID_MANIFEST));
    assert_true("scene_catalog_runtime_present",
                catalog_contains_path_source(entries,
                                             entry_count,
                                             runtime_path,
                                             SCENE_SOURCE_RUNTIME_SCENE));
    assert_true("scene_catalog_reject_authoring_variant",
                !catalog_contains_path_any_source(entries, entry_count, authoring_path));

    remove(runtime_path);
    remove(authoring_path);
    remove(manifest_path);
    rmdir(tmp_root);
    return 0;
}

static int test_menu_state_manifest_label_uses_scene_directory_name(void) {
    char label[128];
    menu_state_build_manifest_label("/tmp/scene_library/My Lobby/scene_runtime.json",
                                    label,
                                    sizeof(label));
    assert_true("menu_manifest_label_runtime_dir_name",
                strcmp(label, "My Lobby") == 0);
    return 0;
}

static int test_menu_state_manifest_option_visibility_matrix(void) {
    const int original_space_mode = animSettings.spaceMode;
    MenuRuntimeState state = {0};
    ManifestOption config_2d = {0};
    ManifestOption fluid_manifest = {0};
    ManifestOption runtime_scene = {0};

    config_2d.source = SCENE_SOURCE_CONFIG_2D;
    fluid_manifest.source = SCENE_SOURCE_FLUID_MANIFEST;
    runtime_scene.source = SCENE_SOURCE_RUNTIME_SCENE;

    animSettings.spaceMode = SPACE_MODE_2D;
    assert_true("menu_manifest_visible_2d_config",
                menu_state_manifest_option_visible(&state, &config_2d));
    assert_true("menu_manifest_visible_2d_fluid",
                menu_state_manifest_option_visible(&state, &fluid_manifest));
    assert_true("menu_manifest_visible_2d_runtime_hidden",
                !menu_state_manifest_option_visible(&state, &runtime_scene));

    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("menu_manifest_visible_3d_runtime",
                menu_state_manifest_option_visible(&state, &runtime_scene));
    assert_true("menu_manifest_visible_3d_config_hidden",
                !menu_state_manifest_option_visible(&state, &config_2d));
    assert_true("menu_manifest_visible_3d_fluid_hidden",
                !menu_state_manifest_option_visible(&state, &fluid_manifest));

    animSettings.spaceMode = original_space_mode;
    return 0;
}

static int test_scene_source_ui_runtime_labels_expose_paired_and_disabled_state(void) {
    AnimationConfig saved_anim = animSettings;
    char tmp_template[] = "/tmp/ray_tracing_scene_source_ui_XXXXXX";
    char* tmp_root = mkdtemp(tmp_template);
    char runtime_path[PATH_MAX] = {0};
    char bundle_path[PATH_MAX] = {0};
    char option_label[128] = {0};
    char button_label[128] = {0};
    char status_label[64] = {0};
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_ui_state\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    const char* bundle_json =
        "{\n"
        "  \"bundle_type\": \"physics_scene_bundle_v1\",\n"
        "  \"bundle_version\": 1,\n"
        "  \"profile\": \"physics\",\n"
        "  \"fluid_source\": {\n"
        "    \"kind\": \"pack\",\n"
        "    \"path\": \"frame_000017.pack\"\n"
        "  }\n"
        "}\n";

    assert_true("scene_source_ui_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", tmp_root);
    snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", tmp_root);
    assert_true("scene_source_ui_write_runtime", write_text_file(runtime_path, runtime_json));
    assert_true("scene_source_ui_write_bundle", write_text_file(bundle_path, bundle_json));

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    animSettings.volumeInteractionEnabled = true;
    animSettings.volumeSourceKind = VOLUME_SOURCE_MANIFEST;
    snprintf(animSettings.volumeSourcePath, sizeof(animSettings.volumeSourcePath), "%s", bundle_path);

    scene_source_ui_format_catalog_option_label(runtime_path,
                                                SCENE_SOURCE_RUNTIME_SCENE,
                                                option_label,
                                                sizeof(option_label));
    scene_source_ui_format_active_button_label(button_label, sizeof(button_label));
    scene_source_ui_format_scene_select_status(SCENE_SOURCE_RUNTIME_SCENE,
                                               runtime_path,
                                               status_label,
                                               sizeof(status_label));
    assert_true("scene_source_ui_option_auto_atmosphere",
                strstr(option_label, "atmosphere") != NULL);
    assert_true("scene_source_ui_button_auto_atmosphere",
                strstr(button_label, "Atmosphere") != NULL);
    assert_true("scene_source_ui_status_auto_atmosphere",
                strcmp(status_label, "Runtime scene + atmosphere") == 0);

    animSettings.volumeInteractionEnabled = false;
    scene_source_ui_format_catalog_option_label(runtime_path,
                                                SCENE_SOURCE_RUNTIME_SCENE,
                                                option_label,
                                                sizeof(option_label));
    scene_source_ui_format_active_button_label(button_label, sizeof(button_label));
    scene_source_ui_format_scene_select_status(SCENE_SOURCE_RUNTIME_SCENE,
                                               runtime_path,
                                               status_label,
                                               sizeof(status_label));
    assert_true("scene_source_ui_option_atmosphere_off",
                strstr(option_label, "atmosphere off") != NULL);
    assert_true("scene_source_ui_button_atmosphere_off",
                strstr(button_label, "Atmosphere Off") != NULL);
    assert_true("scene_source_ui_status_atmosphere_off",
                strcmp(status_label, "Runtime scene; atmosphere off") == 0);

    animSettings.volumeInteractionEnabled = true;
    animSettings.volumeSourceKind = VOLUME_SOURCE_PACK;
    snprintf(animSettings.volumeSourcePath,
             sizeof(animSettings.volumeSourcePath),
             "%s",
             "/tmp/custom_runtime_volume.pack");
    scene_source_ui_format_catalog_option_label(runtime_path,
                                                SCENE_SOURCE_RUNTIME_SCENE,
                                                option_label,
                                                sizeof(option_label));
    scene_source_ui_format_active_button_label(button_label, sizeof(button_label));
    scene_source_ui_format_scene_select_status(SCENE_SOURCE_RUNTIME_SCENE,
                                               runtime_path,
                                               status_label,
                                               sizeof(status_label));
    assert_true("scene_source_ui_option_custom_volume",
                strstr(option_label, "custom volume") != NULL);
    assert_true("scene_source_ui_button_custom_volume",
                strstr(button_label, "Custom Volume") != NULL);
    assert_true("scene_source_ui_status_custom_volume",
                strcmp(status_label, "Runtime scene + custom volume") == 0);

    remove(runtime_path);
    remove(bundle_path);
    rmdir(tmp_root);
    animSettings = saved_anim;
    return 0;
}

static int test_volume_source_catalog_collects_bundle_and_direct_files(void) {
    char root_template[] = "/tmp/ray_tracing_volume_catalog_XXXXXX";
    char* root = mkdtemp(root_template);
    char child_dir[PATH_MAX];
    char bundle_path[PATH_MAX];
    char pack_path[PATH_MAX];
    char vf3d_path[PATH_MAX];
    const char* roots[1];
    VolumeSourceCatalogEntry entries[16];
    size_t count = 0;

    assert_true("volume_source_catalog_tmpdir", root != NULL);
    if (!root) return 0;

    snprintf(child_dir, sizeof(child_dir), "%s/Waist", root);
    assert_true("volume_source_catalog_child_mkdir", mkdir(child_dir, 0700) == 0);
    snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", child_dir);
    snprintf(pack_path, sizeof(pack_path), "%s/frame_000000.pack", child_dir);
    snprintf(vf3d_path, sizeof(vf3d_path), "%s/frame_000000.vf3d", child_dir);
    assert_true("volume_source_catalog_write_bundle", write_text_file(bundle_path, "{}"));
    assert_true("volume_source_catalog_write_pack", write_text_file(pack_path, "pack"));
    assert_true("volume_source_catalog_write_vf3d", write_text_file(vf3d_path, "vf3d"));

    roots[0] = root;
    count = volume_source_catalog_collect(entries, 16, roots, 1, "");
    assert_true("volume_source_catalog_count_nonzero", count >= 3u);
    assert_true("volume_source_catalog_has_bundle",
                test_volume_catalog_entry_path(entries, count, bundle_path));
    assert_true("volume_source_catalog_has_pack",
                test_volume_catalog_entry_path(entries, count, pack_path));
    assert_true("volume_source_catalog_has_vf3d",
                test_volume_catalog_entry_path(entries, count, vf3d_path));

    remove(bundle_path);
    remove(pack_path);
    remove(vf3d_path);
    rmdir(child_dir);
    rmdir(root);
    return 0;
}

static int test_volume_source_ui_labels_expose_auto_custom_and_none(void) {
    char tmp_template[] = "/tmp/ray_tracing_volume_ui_XXXXXX";
    char* tmp_root = mkdtemp(tmp_template);
    char runtime_path[PATH_MAX];
    char manifest_path[PATH_MAX];
    char label[160];
    AnimationConfig saved_anim = animSettings;

    assert_true("volume_source_ui_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", tmp_root);
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", tmp_root);
    assert_true("volume_source_ui_runtime_written", write_text_file(runtime_path, "{}"));
    assert_true("volume_source_ui_manifest_written",
                write_text_file(manifest_path,
                                "{"
                                "\"frame_contract\":\"vf3d\","
                                "\"space_mode\":\"3d\","
                                "\"frames\":[{\"path\":\"frame_000000.vf3d\"}]"
                                "}"));

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath), "%s", runtime_path);
    animSettings.volumeSourceKind = VOLUME_SOURCE_MANIFEST;
    snprintf(animSettings.volumeSourcePath, sizeof(animSettings.volumeSourcePath), "%s", manifest_path);
    animSettings.volumeInteractionEnabled = true;
    volume_source_ui_format_active_button_label(label, sizeof(label));
    assert_true("volume_source_ui_auto_label", strstr(label, "[Auto]") != NULL);

    animSettings.volumeSourceKind = VOLUME_SOURCE_PACK;
    snprintf(animSettings.volumeSourcePath, sizeof(animSettings.volumeSourcePath), "%s", "/tmp/custom.pack");
    animSettings.volumeInteractionEnabled = false;
    volume_source_ui_format_active_button_label(label, sizeof(label));
    assert_true("volume_source_ui_custom_off_label", strstr(label, "[Custom Off]") != NULL);

    AnimationClearVolumeSource();
    volume_source_ui_format_active_button_label(label, sizeof(label));
    assert_true("volume_source_ui_none_label", strstr(label, "[None]") != NULL);

    remove(runtime_path);
    remove(manifest_path);
    rmdir(tmp_root);
    animSettings = saved_anim;
    return 0;
}

static int test_depth_projection_scalars(void) {
    double scale_far = RenderHelper_DepthScaleForObjectZ(4.0);
    double scale_near = RenderHelper_DepthScaleForObjectZ(-4.0);
    double scale_zero = RenderHelper_DepthScaleForObjectZ(0.0);
    double yoff_far = RenderHelper_DepthYOffsetPixelsForObjectZ(3.0, 1.0);
    double yoff_near = RenderHelper_DepthYOffsetPixelsForObjectZ(-3.0, 1.0);

    assert_true("depth_scale_far_smaller_than_zero", scale_far < scale_zero);
    assert_true("depth_scale_near_larger_than_zero", scale_near > scale_zero);
    assert_true("depth_scale_positive", scale_far > 0.0 && scale_near > 0.0);
    assert_true("depth_yoff_far_positive", yoff_far > 0.0);
    assert_true("depth_yoff_near_negative", yoff_near < 0.0);
    return 0;
}


int run_test_ui_menu_contract_tests(void) {
    int before = test_support_failures();

    test_menu_batch_panel_click_starts_frame_dir_edit();
    test_menu_batch_panel_clear_button_updates_frame_count();
    test_menu_layout_builds_non_overlapping_primary_zones();
    test_menu_layout_keeps_manifest_dropdown_inside_left_panel();
    test_menu_button_layout_respects_owned_screen_zones();
    test_menu_button_layout_exposes_volume_controls_in_3d_only();
    test_menu_batch_panel_layout_centers_inside_batch_zone();
    test_menu_batch_panel_header_does_not_overlap_route_rows();
    test_integrator_catalog_menu_routes_by_space_mode();
    test_menu_slider_layout_includes_environment_control();
    test_menu_slider_layout_routes_bounce_controls_by_space_mode();
    test_integrator_catalog_cycle_preserves_inactive_mode();
    test_menu_fit_text_to_width_supports_in_place_buffer();
    test_manifest_default_roots_expands_runtime_and_legacy_paths();
    test_scene_source_catalog_collect_admits_runtime_and_manifest_lanes();
    test_volume_source_catalog_collects_bundle_and_direct_files();
    test_menu_state_manifest_label_uses_scene_directory_name();
    test_menu_state_manifest_option_visibility_matrix();
    test_scene_source_ui_runtime_labels_expose_paired_and_disabled_state();
    test_volume_source_ui_labels_expose_auto_custom_and_none();
    test_depth_projection_scalars();

    return test_support_failures() - before;
}
