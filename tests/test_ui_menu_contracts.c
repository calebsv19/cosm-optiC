#include "ui/menu_numeric_controls.h"
#include "ui/sdl_menu_input.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/animation.h"
#include "app/data_paths.h"
#include "config/config_manager.h"
#include "editor/object_editor_motion.h"
#include "editor/object_editor_panels_internal.h"
#include "editor/scene_editor_digest_overlay.h"
#include "app/ray_tracing_build_identity.h"
#include "import/runtime_scene_bridge.h"
#include "motion/runtime_motion_track_3d.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/render_helper.h"
#include "render/runtime_native_3d_prepare_cache.h"
#include "render/runtime_native_3d_render.h"
#include "test_support.h"
#include "ui/menu_batch_panel.h"
#include "ui/menu_caustic_product.h"
#include "ui/menu_environment_settings.h"
#include "ui/menu_settings_readback.h"
#include "ui/menu_layout.h"
#include "ui/menu_panel_chrome.h"
#include "ui/menu_resume_panel.h"
#include "ui/menu_scene_project_summary.h"
#include "ui/scene_source_catalog.h"
#include "ui/scene_source_ui_labels.h"
#include "ui/sdl_menu_render.h"
#include "ui/sdl_menu_state.h"
#include "ui/volume_source_catalog.h"
#include "ui/volume_source_ui_labels.h"

#define TEST_MENU_WIDTH 1200
#define TEST_MENU_HEIGHT 900

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
                strncmp(state.statusLabel, "Cleared ", strlen("Cleared ")) == 0 &&
                    strstr(state.statusLabel, "frame") != NULL);

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    rmdir(tmp_root);
    return 0;
}

static int test_menu_scene_project_summary_detects_project_files(void) {
    char tmp_template[] = "/tmp/ray_tracing_scene_project_summary_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char runtime_path[PATH_MAX];
    char path[PATH_MAX];
    RayTracingMenuSceneProjectSummary summary;

    assert_true("scene_project_summary_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", tmp_root);
    snprintf(path, sizeof(path), "%s/scene_project.json", tmp_root);
    assert_true("scene_project_summary_write_project", write_text_file(path, "{}"));
    assert_true("scene_project_summary_write_runtime", write_text_file(runtime_path, "{}"));
    snprintf(path, sizeof(path), "%s/scene_authoring.json", tmp_root);
    assert_true("scene_project_summary_write_authoring", write_text_file(path, "{}"));
    snprintf(path, sizeof(path), "%s/assets", tmp_root);
    assert_true("scene_project_summary_mkdir_assets", mkdir(path, 0700) == 0);
    snprintf(path, sizeof(path), "%s/assets/mesh_assets", tmp_root);
    assert_true("scene_project_summary_mkdir_mesh", mkdir(path, 0700) == 0);
    snprintf(path, sizeof(path), "%s/assets/mesh_assets/test.runtime.json", tmp_root);
    assert_true("scene_project_summary_write_mesh", write_text_file(path, "{}"));
    snprintf(path, sizeof(path), "%s/assets/physics", tmp_root);
    assert_true("scene_project_summary_mkdir_physics", mkdir(path, 0700) == 0);
    snprintf(path, sizeof(path), "%s/assets/physics/active", tmp_root);
    assert_true("scene_project_summary_mkdir_physics_active", mkdir(path, 0700) == 0);
    snprintf(path, sizeof(path), "%s/assets/physics/active/scene_bundle.json", tmp_root);
    assert_true("scene_project_summary_write_bundle", write_text_file(path, "{}"));
    snprintf(path, sizeof(path), "%s/physics_sim", tmp_root);
    assert_true("scene_project_summary_mkdir_physics_sim", mkdir(path, 0700) == 0);
    snprintf(path, sizeof(path), "%s/physics_sim/cache_manifest.json", tmp_root);
    assert_true("scene_project_summary_write_cache", write_text_file(path, "{}"));
    snprintf(path, sizeof(path), "%s/ray_tracing", tmp_root);
    assert_true("scene_project_summary_mkdir_ray", mkdir(path, 0700) == 0);
    snprintf(path, sizeof(path), "%s/ray_tracing/render_request.json", tmp_root);
    assert_true("scene_project_summary_write_request", write_text_file(path, "{}"));

    memset(&summary, 0, sizeof(summary));
    assert_true("scene_project_summary_detects_project",
                ray_tracing_menu_scene_project_summary_for_runtime_scene(runtime_path, &summary));
    assert_true("scene_project_summary_project_detected", summary.project_detected);
    assert_true("scene_project_summary_runtime_present", summary.has_scene_runtime);
    assert_true("scene_project_summary_authoring_present", summary.has_scene_authoring);
    assert_true("scene_project_summary_mesh_present", summary.has_mesh_assets);
    assert_true("scene_project_summary_mesh_count", summary.mesh_asset_count == 1);
    assert_true("scene_project_summary_cache_present", summary.has_physics_cache_manifest);
    assert_true("scene_project_summary_bundle_present", summary.has_physics_scene_bundle);
    assert_true("scene_project_summary_request_present", summary.has_render_request);
    assert_true("scene_project_summary_label", strstr(summary.label, "Scene Project:") != NULL);
    assert_true("scene_project_summary_detail", strstr(summary.detail, "cache:ok") != NULL);

    snprintf(path, sizeof(path), "%s/ray_tracing/render_request.json", tmp_root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/ray_tracing", tmp_root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/physics_sim/cache_manifest.json", tmp_root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/physics_sim", tmp_root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/assets/physics/active/scene_bundle.json", tmp_root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/assets/physics/active", tmp_root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/assets/physics", tmp_root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/assets/mesh_assets/test.runtime.json", tmp_root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/assets/mesh_assets", tmp_root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/assets", tmp_root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/scene_authoring.json", tmp_root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/scene_runtime.json", tmp_root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/scene_project.json", tmp_root);
    unlink(path);
    rmdir(tmp_root);
    return 0;
}

static int test_object_editor_motion_panel_fits_material_space(void) {
    SceneConfig saved_scene = sceneSettings;
    bool saved_assets_collapsed = assetsCollapsed;
    bool saved_materials_collapsed = materialsCollapsed;
    SDL_Rect region = {12, 24, 190, 300};
    SDL_Rect motion_section = {0};
    SDL_Rect authored_button = {0};
    int list_y = 0;
    ObjectEditorPanelMotionAction action = OBJECT_EDITOR_PANEL_MOTION_ACTION_NONE;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].x = 32.0;
    sceneSettings.sceneObjects[0].y = 48.0;
    sceneSettings.sceneObjects[0].z = 2.0;
    sceneSettings.sceneObjects[0].scale = 1.0;
    sceneSettings.sceneObjects[0].material_id = 0;
    assetsCollapsed = false;
    materialsCollapsed = false;
    ObjectEditorSetSelectedObjectIndex(0);

    ObjectEditorPanels_UpdateLayoutForRegion(&region);
    ObjectEditorPanels_ResolveMotionSectionMetrics(&motion_section,
                                                   NULL,
                                                   NULL,
                                                   &authored_button,
                                                   NULL,
                                                   NULL,
                                                   NULL);
    ObjectEditorPanels_ResolveMaterialListMetrics(&list_y, NULL, NULL, NULL);

    assert_true("object_motion_panel_visible_for_selected",
                ObjectEditorPanels_MotionSectionHeight() > 0);
    assert_true("object_motion_panel_inside_material_panel",
                motion_section.x >= materialPanelRect.x &&
                motion_section.y >= materialPanelRect.y &&
                motion_section.x + motion_section.w <= materialPanelRect.x + materialPanelRect.w &&
                motion_section.y + motion_section.h <= materialPanelRect.y + materialPanelRect.h);
    assert_true("object_motion_panel_material_list_after_motion",
                list_y >= motion_section.y + motion_section.h);
    assert_true("object_motion_panel_authored_hit",
                ObjectEditorPanels_MotionActionAtPoint(authored_button.x + authored_button.w / 2,
                                                       authored_button.y + authored_button.h / 2,
                                                       &action) &&
                action == OBJECT_EDITOR_PANEL_MOTION_ACTION_AUTHORED);

    ObjectEditorSetSelectedObjectIndex(-1);
    ObjectEditorPanels_UpdateLayoutForRegion(&region);
    assert_true("object_motion_panel_hidden_without_selection",
                ObjectEditorPanels_MotionSectionHeight() == 0);

    sceneSettings = saved_scene;
    assetsCollapsed = saved_assets_collapsed;
    materialsCollapsed = saved_materials_collapsed;
    ObjectEditorSetSelectedObjectIndex(-1);
    return 0;
}

static int test_object_editor_motion_overlay_reports_selected_path_readback(void) {
    RuntimeMotionTrack3D path_track;
    SceneEditorDigestOverlayProjector projector;
    SceneEditorMotionOverlayMetrics metrics;
    const RuntimeMotionTrack3D* default_track = NULL;

    memset(&projector, 0, sizeof(projector));
    projector.viewport = (SDL_Rect){0, 0, 240, 200};
    projector.center_x = 0.0;
    projector.center_y = 0.0;
    projector.center_z = 0.0;
    projector.scale = 22.0;
    projector.distance = 10.0;
    projector.span_max = 12.0;

    ObjectEditorMotionReset();
    assert_true("object_motion_overlay_seed_authored",
                ObjectEditorMotionSetObjectAuthored("moving_plane", 1.0, 2.0, 0.5, 0.0));
    default_track = ObjectEditorMotionFindTrack("moving_plane");
    memset(&metrics, 0, sizeof(metrics));
    assert_true("object_motion_overlay_default_visible",
                SceneEditorDigestOverlayResolveMotionTrackMetrics(&projector,
                                                                  default_track,
                                                                  &metrics));
    assert_true("object_motion_overlay_default_center_marker",
                metrics.visible &&
                metrics.control_point_count == 1 &&
                metrics.projected_control_point_count == 1 &&
                metrics.center_marker_bounds.w > 0 &&
                metrics.center_marker_bounds.h > 0);
    assert_true("object_motion_overlay_default_no_curve",
                !metrics.has_path_curve && metrics.sampled_segment_count == 0);

    memset(&path_track, 0, sizeof(path_track));
    path_track.used = true;
    path_track.enabled = true;
    path_track.mode = RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH;
    path_track.supported_mode = true;
    path_track.has_position_path = true;
    path_track.position_path.mode = BEZIER_CUBIC;
    RuntimeMotionTrack3DCopyString(path_track.object_id,
                                   sizeof(path_track.object_id),
                                   "two_point_path");
    assert_true("object_motion_overlay_insert_path_a",
                CameraPath3D_InsertPoint(&path_track.position_path_3d,
                                         &path_track.position_path,
                                         -2.0,
                                         -1.0,
                                         0.0,
                                         0.0));
    assert_true("object_motion_overlay_insert_path_b",
                CameraPath3D_InsertPoint(&path_track.position_path_3d,
                                         &path_track.position_path,
                                         3.0,
                                         2.0,
                                         1.0,
                                         0.0));
    memset(&metrics, 0, sizeof(metrics));
    assert_true("object_motion_overlay_path_visible",
                SceneEditorDigestOverlayResolveMotionTrackMetrics(&projector,
                                                                  &path_track,
                                                                  &metrics));
    assert_true("object_motion_overlay_path_readback",
                metrics.has_path_curve &&
                metrics.control_point_count == 2 &&
                metrics.projected_control_point_count == 2 &&
                metrics.sampled_segment_count > 0);

    path_track.enabled = false;
    memset(&metrics, 0, sizeof(metrics));
    assert_true("object_motion_overlay_static_hidden",
                !SceneEditorDigestOverlayResolveMotionTrackMetrics(&projector,
                                                                   &path_track,
                                                                   &metrics) &&
                !metrics.visible);
    ObjectEditorMotionReset();
    return 0;
}

static int test_menu_scene_project_summary_keeps_loose_runtime_scene_separate(void) {
    char tmp_template[] = "/tmp/ray_tracing_loose_runtime_summary_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char runtime_path[PATH_MAX];
    RayTracingMenuSceneProjectSummary summary;

    assert_true("loose_runtime_summary_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;
    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", tmp_root);
    assert_true("loose_runtime_summary_write_runtime", write_text_file(runtime_path, "{}"));

    memset(&summary, 0, sizeof(summary));
    assert_true("loose_runtime_summary_returns_false",
                !ray_tracing_menu_scene_project_summary_for_runtime_scene(runtime_path, &summary));
    assert_true("loose_runtime_summary_selected", summary.selected_runtime_scene);
    assert_true("loose_runtime_summary_not_project", !summary.project_detected);
    assert_true("loose_runtime_summary_label",
                strcmp(summary.label, "Loose runtime scene") == 0);

    unlink(runtime_path);
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

    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);

    assert_true("menu_layout_left_panel_has_width", screen.leftPanelRect.w >= 350);
    assert_true("menu_layout_slider_panel_has_width", screen.sliderPanelRect.w >= 300);
    assert_true("menu_layout_render_info_uses_full_health_width",
                screen.renderInfoRect.x == screen.sliderPanelRect.x &&
                    screen.renderInfoRect.w == screen.sliderPanelRect.w);
    assert_true("menu_layout_render_info_separates_sliders_from_route",
                test_rect_bottom(&screen.sliderPanelRect) <= screen.renderInfoRect.y &&
                    test_rect_bottom(&screen.renderInfoRect) <= screen.routeStackRect.y);
    assert_true("menu_layout_center_panel_has_width", screen.centerBatchRect.w >= 300);
    assert_true("menu_layout_left_before_center",
                screen.leftPanelRect.x + screen.leftPanelRect.w < screen.centerBatchRect.x);
    assert_true("menu_layout_center_before_slider",
                screen.centerBatchRect.x + screen.centerBatchRect.w < screen.sliderPanelRect.x);
    assert_true("menu_layout_center_above_footer",
                screen.centerBatchRect.y + screen.centerBatchRect.h <= screen.bottomActionRowRect.y);
    assert_true("menu_layout_resume_panel_has_width", screen.centerResumeRect.w >= 300);
    assert_true("menu_layout_workspace_modules_share_content_rect",
                memcmp(&screen.centerBatchRect,
                       &screen.centerResumeRect,
                       sizeof(SDL_Rect)) == 0 &&
                memcmp(&screen.centerControlsRect,
                       &screen.centerBatchRect,
                       sizeof(SDL_Rect)) == 0);
    assert_true("menu_layout_resume_above_footer",
                screen.centerResumeRect.y + screen.centerResumeRect.h <= screen.bottomActionRowRect.y);
    assert_true("menu_layout_footer_starts_after_left_panel",
                screen.bottomActionRowRect.x > screen.leftPanelRect.x + screen.leftPanelRect.w);
    assert_true("menu_layout_left_panel_stops_above_global_footer",
                screen.leftPanelRect.y + screen.leftPanelRect.h <= screen.bottomActionRowRect.y);
    animSettings = saved_anim;
    return 0;
}

static int test_menu_workspace_registers_and_switches_nested_modules(void) {
    MenuWorkspaceHost host;
    MenuWorkspaceLayout layout;
    SDL_Rect frame = {400, 30, 420, 700};

    memset(&host, 0, sizeof(host));
    memset(&layout, 0, sizeof(layout));
    assert_true("menu_workspace_host_init", menu_workspace_host_init(&host));
    assert_true("menu_workspace_defaults_to_render",
                host.active_module == MENU_WORKSPACE_RENDER);
    assert_true("menu_workspace_selects_output",
                menu_workspace_host_select(&host, MENU_WORKSPACE_OUTPUT));
    assert_true("menu_workspace_output_active",
                host.active_module == MENU_WORKSPACE_OUTPUT);
    menu_workspace_build_layout(frame, &layout);
    assert_true("menu_workspace_content_below_tabs",
                layout.content_rect.y > layout.tab_rects[0].y + layout.tab_rects[0].h);
    assert_true("menu_workspace_tab_hit",
                menu_workspace_tab_at_point(&layout,
                                            layout.tab_rects[2].x + 2,
                                            layout.tab_rects[2].y + 2) ==
                    (int)MENU_WORKSPACE_RUN);
    return 0;
}

static int test_menu_layout_keeps_manifest_list_inside_left_panel(void) {
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
    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);

    menu_layout_finalize_with_buttons(&screen, &buttons, &state);

    assert_true("menu_layout_manifest_list_visible", screen.manifestReserveRect.w > 0 && screen.manifestReserveRect.h > 0);
    assert_true("menu_layout_manifest_list_leaves_room_for_controls",
                screen.manifestReserveRect.h >= SDL_MENU_MANIFEST_ITEM_HEIGHT * 2 &&
                buttons.volumeToggleRect.y + buttons.volumeToggleRect.h + 42 <=
                    screen.leftPanelRect.y + screen.leftPanelRect.h);
    assert_true("menu_layout_manifest_list_below_load_scene",
                screen.manifestReserveRect.y >= buttons.loadSceneRect.y + buttons.loadSceneRect.h);
    assert_true("menu_layout_input_root_below_manifest_list",
                buttons.inputRootValueRect.y >= screen.manifestReserveRect.y + screen.manifestReserveRect.h);
    assert_true("menu_layout_manifest_list_inside_left_panel_left",
                screen.manifestReserveRect.x >= screen.leftPanelRect.x);
    assert_true("menu_layout_manifest_list_inside_left_panel_right",
                test_rect_right(&screen.manifestReserveRect) <= test_rect_right(&screen.leftPanelRect));
    assert_true("menu_layout_manifest_list_inside_left_panel_bottom",
                test_rect_bottom(&screen.manifestReserveRect) <= test_rect_bottom(&screen.leftPanelRect));
    assert_true("menu_layout_workspace_unchanged_when_manifest_opens",
                memcmp(&screen.centerBatchRect,
                       &screen.centerControlsRect,
                       sizeof(SDL_Rect)) == 0);
    animSettings = saved_anim;
    return 0;
}

static int test_menu_button_layout_compacts_scene_mode_controls_in_3d(void) {
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
    animSettings.deepRenderMode = true;

    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    menu_layout_finalize_with_buttons(&screen, &buttons, &state);

    assert_true("menu_scene_mode_interactive_hidden_in_3d",
                buttons.interactiveRect.w == 0 && buttons.interactiveRect.h == 0);
    assert_true("menu_scene_mode_deep_render_compact_in_3d",
                buttons.deepRenderRect.h < 40);
    assert_true("menu_scene_mode_async_render_visible_in_3d",
                buttons.asyncDeepRenderRect.w > 0 &&
                    buttons.asyncDeepRenderRect.h > 0);
    assert_true("menu_scene_mode_async_render_shares_mode_row",
                buttons.asyncDeepRenderRect.y == buttons.deepRenderRect.y &&
                    buttons.asyncDeepRenderRect.x >=
                        buttons.deepRenderRect.x + buttons.deepRenderRect.w);
    assert_true("menu_scene_mode_bounce_below_async_render",
                buttons.bounceRect.y >=
                    buttons.deepRenderRect.y + buttons.deepRenderRect.h);
    assert_true("menu_scene_mode_bounce_auto_same_row",
                buttons.bounceRect.y == buttons.autoMp4Rect.y &&
                buttons.bounceRect.x < buttons.autoMp4Rect.x);
    assert_true("menu_scene_mode_load_scene_moves_up_in_3d",
                buttons.loadSceneRect.y < 240);

    animSettings.spaceMode = SPACE_MODE_2D;
    memset(&buttons, 0, sizeof(buttons));
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    assert_true("menu_scene_mode_interactive_visible_in_2d",
                buttons.interactiveRect.w > 0 && buttons.interactiveRect.h >= 40);

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

    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    menu_layout_finalize_with_buttons(&screen, &buttons, &state);

    assert_true("menu_buttons_left_controls_inside_left_panel",
                buttons.loadSceneRect.x >= screen.leftPanelRect.x &&
                test_rect_right(&buttons.inputRootApplyRect) <= test_rect_right(&screen.leftPanelRect));
    assert_true("menu_buttons_mesh_import_policy_inside_left_panel",
                buttons.meshImportNormalPolicyRect.w > 0 &&
                buttons.meshImportNormalPolicyRect.x >= screen.leftPanelRect.x &&
                test_rect_right(&buttons.meshImportNormalPolicyRect) <=
                    test_rect_right(&screen.leftPanelRect) &&
                buttons.meshImportNormalPolicyRect.y >=
                    buttons.meshAssetRootValueRect.y + buttons.meshAssetRootValueRect.h);
    assert_true("menu_buttons_center_controls_inside_center_zone",
                buttons.falloffRect.x >= screen.centerControlsRect.x &&
                test_rect_right(&buttons.topFillRect) <= test_rect_right(&screen.centerControlsRect));
    assert_true("menu_buttons_route_stack_inside_route_zone",
                buttons.spaceModeRect.x >= screen.routeStackRect.x &&
                buttons.spaceModeRect.y >= screen.routeStackRect.y &&
                buttons.startRect.y < test_rect_bottom(&screen.routeStackRect));
    assert_true("menu_buttons_launch_actions_stay_in_runtime_route",
                buttons.previewRect.x >= screen.routeStackRect.x &&
                buttons.startRect.x >= screen.routeStackRect.x &&
                test_rect_right(&buttons.previewRect) <= test_rect_right(&screen.routeStackRect) &&
                test_rect_right(&buttons.startRect) <= test_rect_right(&screen.routeStackRect) &&
                buttons.previewRect.y == buttons.sceneEditorRect.y &&
                buttons.startRect.y > buttons.previewRect.y);
    assert_true("menu_buttons_runtime_route_uses_two_column_rows",
                buttons.spaceModeRect.y == buttons.sceneModeRect.y &&
                    buttons.sceneEditorRect.y == buttons.previewRect.y &&
                    test_rect_right(&buttons.spaceModeRect) < buttons.sceneModeRect.x &&
                    test_rect_right(&buttons.sceneEditorRect) < buttons.previewRect.x &&
                    buttons.startRect.x == buttons.spaceModeRect.x &&
                    test_rect_right(&buttons.startRect) ==
                        test_rect_right(&buttons.sceneModeRect));
    assert_true("menu_buttons_runtime_route_is_compact",
                buttons.spaceModeRect.h <= 40 &&
                buttons.startRect.h <= 40);
    assert_true("menu_buttons_run_module_selects",
                menu_workspace_host_init(&state.menuWorkspaceHost) &&
                menu_workspace_host_select(&state.menuWorkspaceHost,
                                           MENU_WORKSPACE_RUN));
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    assert_true("menu_buttons_run_module_keeps_launch_actions_in_runtime_route",
                buttons.startRect.x >= screen.routeStackRect.x &&
                buttons.previewRect.x >= screen.routeStackRect.x &&
                buttons.startRect.y > buttons.previewRect.y);
    assert_true("menu_buttons_footer_inside_bottom_row",
                buttons.exitRect.x >= screen.bottomActionRowRect.x &&
                buttons.exitRect.y >= screen.bottomActionRowRect.y &&
                buttons.saveRect.y >= screen.bottomActionRowRect.y &&
                buttons.exitRect.y < test_rect_bottom(&screen.bottomActionRowRect) &&
                buttons.saveRect.y < test_rect_bottom(&screen.bottomActionRowRect));
    animSettings = saved_anim;
    return 0;
}

static int test_menu_right_pane_layout_scales_with_panel_geometry(void) {
    const int heights[] = {700, 900, 1100};
    const int health_widths[] = {250, 340, 480};
    AnimationConfig saved_anim = animSettings;
    int previous_slider_height = 0;
    size_t i;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    for (i = 0u; i < sizeof(heights) / sizeof(heights[0]); ++i) {
        MenuRuntimeState state;
        MenuScreenLayout screen;
        MenuButtonLayout buttons;
        SliderLayout sliders;
        memset(&state, 0, sizeof(state));
        memset(&screen, 0, sizeof(screen));
        memset(&buttons, 0, sizeof(buttons));
        memset(&sliders, 0, sizeof(sliders));

        menu_layout_build_base(NULL, &state, 1500, heights[i], &screen);
        ray_tracing_menu_pane_host_set_targets(
            &state.menuPaneHost, 390, health_widths[i]);
        assert_true("menu_right_pane_rebuild_for_requested_width",
                    ray_tracing_menu_pane_host_rebuild(
                        &state.menuPaneHost,
                        (SDL_Rect){30, 30, 1440, heights[i] - 172}));
        menu_layout_build_base(NULL, &state, 1500, heights[i], &screen);
        menu_render_build_button_layout(NULL, &state, &screen, &buttons);
        menu_render_build_slider_layout(NULL, &state, &screen, &sliders);

        assert_true("menu_right_pane_slider_panel_fills_owned_width",
                    sliders.panelRect.x == screen.sliderPanelRect.x &&
                        sliders.panelRect.w == screen.sliderPanelRect.w);
        assert_true("menu_right_pane_track_stays_inside_slider_panel",
                    sliders.count > 0u &&
                        sliders.items[0].trackRect.x >= screen.sliderPanelRect.x &&
                        test_rect_right(&sliders.items[0].trackRect) <=
                            test_rect_right(&screen.sliderPanelRect));
        assert_true("menu_right_pane_regions_share_responsive_width",
                    screen.sliderPanelRect.x == screen.renderInfoRect.x &&
                        screen.renderInfoRect.x == screen.routeStackRect.x &&
                        screen.sliderPanelRect.w == screen.renderInfoRect.w &&
                        screen.renderInfoRect.w == screen.routeStackRect.w);
        assert_true("menu_right_pane_regions_do_not_overlap",
                    test_rect_bottom(&screen.sliderPanelRect) <=
                        screen.renderInfoRect.y &&
                        test_rect_bottom(&screen.renderInfoRect) <=
                            screen.routeStackRect.y);
        assert_true("menu_right_pane_route_grid_bottom_aligned",
                    buttons.startRect.y > buttons.previewRect.y &&
                        test_rect_bottom(&buttons.startRect) <=
                            test_rect_bottom(&screen.routeStackRect) &&
                        test_rect_bottom(&screen.routeStackRect) -
                                test_rect_bottom(&buttons.startRect) <=
                            12);
        if (previous_slider_height > 0) {
            assert_true("menu_right_pane_taller_window_grows_slider_region",
                        screen.sliderPanelRect.h > previous_slider_height);
        }
        previous_slider_height = screen.sliderPanelRect.h;
    }
    animSettings = saved_anim;
    return 0;
}

static int test_menu_resume_panel_layout_owns_frame_resume_controls(void) {
    MenuRuntimeState state;
    MenuScreenLayout screen;
    MenuResumePanelLayout resume;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&resume, 0, sizeof(resume));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.integratorMode = 2;
    animSettings.spaceMode = SPACE_MODE_3D;

    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
    menu_resume_panel_build_layout(NULL, &state, &screen, &resume);

    assert_true("menu_resume_panel_inside_zone_left",
                resume.panelRect.x >= screen.centerResumeRect.x);
    assert_true("menu_resume_panel_inside_zone_right",
                test_rect_right(&resume.panelRect) <= test_rect_right(&screen.centerResumeRect));
    assert_true("menu_resume_panel_inside_zone_vertical",
                resume.panelRect.y >= screen.centerResumeRect.y &&
                test_rect_bottom(&resume.panelRect) <= test_rect_bottom(&screen.centerResumeRect));
    assert_true("menu_resume_status_above_controls",
                resume.statusRect.y < resume.resumeToggleRect.y);
    assert_true("menu_resume_start_next_are_compact",
                resume.startFrameRect.h < 40 &&
                resume.nextExistingRect.h < 40);

    animSettings = saved_anim;
    return 0;
}

static int test_menu_resume_panel_clicks_preserve_frame_behavior(void) {
    MenuRuntimeState state;
    MenuResumePanelLayout resume;
    SDL_Event event;
    AnimationConfig saved_anim = animSettings;

    memset(&state, 0, sizeof(state));
    memset(&resume, 0, sizeof(resume));
    memset(&event, 0, sizeof(event));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.startFrameIndex = 2;
    animSettings.resumeFromExistingFrames = false;
    state.exportBatchStatus.next_frame_index = 6;

    resume.panelRect = (SDL_Rect){100, 100, 420, 150};
    resume.resumeToggleRect = (SDL_Rect){112, 150, 190, 34};
    resume.startFrameRect = (SDL_Rect){312, 150, 190, 34};
    resume.nextExistingRect = (SDL_Rect){112, 192, 390, 34};

    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.x = resume.resumeToggleRect.x + 4;
    event.button.y = resume.resumeToggleRect.y + 4;
    assert_true("menu_resume_toggle_click_consumed",
                menu_resume_panel_handle_click(&event, &state, &resume));
    assert_true("menu_resume_toggle_sets_resume", animSettings.resumeFromExistingFrames);

    event.button.x = resume.startFrameRect.x + 4;
    event.button.y = resume.startFrameRect.y + 4;
    assert_true("menu_resume_start_click_consumed",
                menu_resume_panel_handle_click(&event, &state, &resume));
    assert_true("menu_resume_start_disables_resume", !animSettings.resumeFromExistingFrames);
    assert_true("menu_resume_start_editing", state.editingStartFrame);
    assert_true("menu_resume_start_buffer", strcmp(state.inputBuffer, "2") == 0);

    state.exportBatchStatus.next_frame_index = 6;
    event.button.x = resume.nextExistingRect.x + 4;
    event.button.y = resume.nextExistingRect.y + 4;
    assert_true("menu_resume_next_click_consumed",
                menu_resume_panel_handle_click(&event, &state, &resume));
    assert_true("menu_resume_next_sets_start", animSettings.startFrameIndex == 6);
    assert_true("menu_resume_next_clears_edit", !state.editingStartFrame);

    event.button.x = resume.panelRect.x + 4;
    event.button.y = resume.panelRect.y + 4;
    assert_true("menu_resume_blank_click_passes_through",
                !menu_resume_panel_handle_click(&event, &state, &resume));

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

    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
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

    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
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

    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
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
    assert_true("menu_batch_scene_project_row_below_video_row",
                batch.sceneProjectValueRect.y >= batch.videoRootValueRect.y + batch.videoRootValueRect.h + 8);
    assert_true("menu_batch_worker_row_below_scene_project_row",
                batch.workerPackageValueRect.y >= batch.sceneProjectValueRect.y + batch.sceneProjectValueRect.h + 8);

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
    assert_true("integrator_menu_3d_visible_count_six", menu_state.visibleCount == 6);

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

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY_V2;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_3d_disney_v2_label",
                strcmp(menu_state.buttonLabel, "Integrator: 3D Disney v2") == 0);
    assert_true("integrator_menu_3d_disney_v2_status_label",
                strcmp(RayTracingIntegratorCatalog_3DStatusLabel(RAY_TRACING_3D_INTEGRATOR_DISNEY_V2),
                       "integrator: 3D Disney v2") == 0);

    memset(&buttons, 0, sizeof(buttons));
    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    state.rendererControlsTab = MENU_RENDERER_CONTROLS_LIGHTING;
    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    assert_true("integrator_menu_top_fill_button_in_center_controls",
                buttons.topFillRect.x >= screen.centerControlsRect.x &&
                buttons.topFillRect.x + buttons.topFillRect.w <=
                    screen.centerControlsRect.x + screen.centerControlsRect.w);
    assert_true("integrator_menu_preset_button_in_center_controls",
                buttons.environmentPresetRect.x >= screen.centerControlsRect.x &&
                buttons.environmentPresetRect.x + buttons.environmentPresetRect.w <=
                    screen.centerControlsRect.x + screen.centerControlsRect.w);
    assert_true("integrator_menu_lighting_tab_rects_in_title_band",
                buttons.rendererLightingTabRect.y >= screen.centerControlsRect.y &&
                buttons.rendererPerformanceTabRect.y >= screen.centerControlsRect.y &&
                buttons.rendererLightingTabRect.x < buttons.rendererPerformanceTabRect.x &&
                buttons.rendererPerformanceTabRect.x < buttons.rendererCausticsTabRect.x);
    assert_true("integrator_menu_lighting_sliders_present",
                buttons.rendererControlSliders.count >= 5);

    memset(&buttons, 0, sizeof(buttons));
    state.rendererControlsTab = MENU_RENDERER_CONTROLS_PERFORMANCE;
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    assert_true("integrator_menu_denoise_button_in_performance_controls",
                buttons.denoiseRect.x >= screen.centerControlsRect.x &&
                buttons.denoiseRect.x + buttons.denoiseRect.w <=
                    screen.centerControlsRect.x + screen.centerControlsRect.w);
    assert_true("integrator_menu_performance_tile_buttons_share_top_row",
                buttons.tileRect.y == buttons.tilePreviewRect.y);
    assert_true("integrator_menu_performance_tile_size_slider_present",
                buttons.rendererControlSliders.count >= 1 &&
                buttons.rendererControlSliders.items[0].value == &animSettings.tileSize);

    memset(&buttons, 0, sizeof(buttons));
    state.rendererControlsTab = MENU_RENDERER_CONTROLS_CAUSTICS;
    RuntimeCausticSettings3D_Default(&state.causticSettings);
    state.causticSettings.mode = RUNTIME_CAUSTIC_MODE_OFF;
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    assert_true("integrator_menu_caustic_controls_present",
                buttons.causticModeRect.w > 0 &&
                buttons.causticEngineRect.w > 0 &&
                buttons.causticSurfaceRect.w > 0 &&
                buttons.causticVolumeRect.w > 0);
    assert_true("integrator_menu_caustic_budget_sliders_present",
                buttons.rendererControlSliders.count == 2 &&
                buttons.rendererControlSliders.items[0].value ==
                    &state.causticSettings.sampleBudget &&
                buttons.rendererControlSliders.items[1].value ==
                    &state.causticSettings.maxPathDepth);

    animSettings.spaceMode = SPACE_MODE_2D;
    menu_state = RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    assert_true("integrator_menu_2d_uses_legacy_catalog", !menu_state.uses3DCatalog);
    assert_true("integrator_menu_2d_label",
                strcmp(menu_state.buttonLabel, "Integrator: Hybrid") == 0);
    assert_true("integrator_menu_2d_show_path_toggles", menu_state.showPathToggles);

    animSettings = saved_anim;
    return 0;
}

static int test_menu_caustic_product_cycle_and_runtime_plan(void) {
    RuntimeCausticSettings3D settings;
    MenuCausticProductRuntimePlan plan;

    RuntimeCausticSettings3D_Default(&settings);
    menu_caustic_product_select(&settings, RUNTIME_CAUSTIC_PRODUCT_MODE_OFF);
    assert_true("menu_caustic_product_default_off",
                menu_caustic_product_mode(&settings) ==
                    RUNTIME_CAUSTIC_PRODUCT_MODE_OFF);
    assert_true("menu_caustic_product_off_label",
                strcmp(menu_caustic_product_label(&settings), "Off") == 0);
    assert_true("menu_caustic_product_off_next_analytic",
                menu_caustic_product_next_mode(&settings) ==
                    RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_ANALYTIC);

    menu_caustic_product_select(
        &settings, RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_ANALYTIC);
    assert_true("menu_caustic_product_analytic_selected",
                settings.mode == RUNTIME_CAUSTIC_MODE_ANALYTIC &&
                    !settings.surfaceCacheEnabled && !settings.volumeCacheEnabled);
    assert_true("menu_caustic_product_analytic_next_transport",
                menu_caustic_product_next_mode(&settings) ==
                    RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT);

    menu_caustic_product_select(
        &settings, RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT);
    assert_true("menu_caustic_product_reference_transport_selected",
                settings.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
                    settings.transportEngine ==
                        RUNTIME_CAUSTIC_TRANSPORT_ENGINE_EXPLORATORY_LENS_TRANSPORT &&
                    settings.surfaceCacheEnabled);
    assert_true("menu_caustic_product_transport_next_photon",
                menu_caustic_product_next_mode(&settings) ==
                    RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP);

    settings.surfaceCacheEnabled = false;
    settings.volumeCacheEnabled = true;
    settings.sampleBudget = 4096;
    settings.maxPathDepth = 8;
    settings.surfaceRadianceScale = 0.5;
    menu_caustic_product_select(&settings, RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP);
    menu_caustic_product_build_runtime_plan(&settings, &plan);
    assert_true("menu_caustic_product_photon_selected",
                settings.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
                    settings.transportEngine == RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP);
    assert_true("menu_caustic_product_photon_experimental_label",
                strcmp(menu_caustic_product_label(&settings),
                       "Photon Map (Experimental)") == 0);
    assert_true("menu_caustic_product_photon_plan_active",
                plan.productMode == RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP &&
                    plan.photonPopulationEnabled &&
                    plan.photonSettings.renderContributionEnabled);
    assert_true("menu_caustic_product_photon_plan_volume_only",
                !plan.photonSettings.surfaceQueryEnabled &&
                    plan.photonSettings.volumeQueryEnabled);
    assert_true("menu_caustic_product_photon_plan_budget_depth",
                plan.photonSettings.sampleBudget == 4096 &&
                    plan.photonSettings.maxPathDepth == 8);
    assert_true("menu_caustic_product_photon_plan_scale",
                plan.photonSettings.surfaceRadianceScale == 0.5);
    assert_true("menu_caustic_product_photon_next_off",
                menu_caustic_product_next_mode(&settings) ==
                    RUNTIME_CAUSTIC_PRODUCT_MODE_OFF);

    menu_caustic_product_select(&settings, RUNTIME_CAUSTIC_PRODUCT_MODE_OFF);
    menu_caustic_product_build_runtime_plan(&settings, &plan);
    assert_true("menu_caustic_product_off_clears_caches",
                !settings.surfaceCacheEnabled && !settings.volumeCacheEnabled);
    assert_true("menu_caustic_product_off_disables_population",
                plan.productMode == RUNTIME_CAUSTIC_PRODUCT_MODE_OFF &&
                    !plan.photonPopulationEnabled &&
                    !plan.photonSettings.renderContributionEnabled);

    settings.mode = RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE;
    assert_true("menu_caustic_product_legacy_spatial_is_reference_transport",
                menu_caustic_product_mode(&settings) ==
                    RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE_TRANSPORT);
    return 0;
}

static int test_menu_slider_layout_includes_environment_control(void) {
    AnimationConfig saved_anim = animSettings;
    MenuRuntimeState state;
    MenuScreenLayout screen;
    MenuButtonLayout buttons;
    SliderLayout sliders;
    bool found_environment = false;
    bool found_environment_in_right_panel = false;
    bool found_top_fill = false;
    bool found_background = false;
    bool found_background_red = false;
    bool found_background_green = false;
    bool found_background_blue = false;
    bool found_background_red_while_off = false;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&buttons, 0, sizeof(buttons));
    memset(&sliders, 0, sizeof(sliders));
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_AMBIENT;
    animSettings.environmentBrightness = 128.0;
    animSettings.topFillStrength = 1.5;
    animSettings.environmentBackgroundBrightnessAuto = false;
    animSettings.environmentBackgroundBrightness = 0.75;
    animSettings.environmentBackgroundColorR = 0.25;
    animSettings.environmentBackgroundColorG = 0.50;
    animSettings.environmentBackgroundColorB = 0.75;
    state.rendererControlsTab = MENU_RENDERER_CONTROLS_LIGHTING;

    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    menu_render_build_slider_layout(NULL, &state, &screen, &sliders);

    for (size_t i = 0; i < buttons.rendererControlSliders.count; ++i) {
        if (buttons.rendererControlSliders.items[i].label &&
            strcmp(buttons.rendererControlSliders.items[i].label, "Ambient Brightness") == 0 &&
            buttons.rendererControlSliders.items[i].value == &state.envSliderValue) {
            found_environment = true;
        }
        if (buttons.rendererControlSliders.items[i].label &&
            strcmp(buttons.rendererControlSliders.items[i].label, "Top Fill Strength") == 0 &&
            buttons.rendererControlSliders.items[i].value == &state.topFillStrengthSliderValue) {
            found_top_fill = true;
        }
        if (buttons.rendererControlSliders.items[i].label &&
            strcmp(buttons.rendererControlSliders.items[i].label, "BG Brightness") == 0 &&
            buttons.rendererControlSliders.items[i].value ==
                &state.environmentBackgroundBrightnessSliderValue) {
            found_background = true;
        }
        if (buttons.rendererControlSliders.items[i].label &&
            strcmp(buttons.rendererControlSliders.items[i].label, "BG Red") == 0 &&
            buttons.rendererControlSliders.items[i].value ==
                &state.environmentBackgroundRedSliderValue) {
            found_background_red = true;
        }
        if (buttons.rendererControlSliders.items[i].label &&
            strcmp(buttons.rendererControlSliders.items[i].label, "BG Green") == 0 &&
            buttons.rendererControlSliders.items[i].value ==
                &state.environmentBackgroundGreenSliderValue) {
            found_background_green = true;
        }
        if (buttons.rendererControlSliders.items[i].label &&
            strcmp(buttons.rendererControlSliders.items[i].label, "BG Blue") == 0 &&
            buttons.rendererControlSliders.items[i].value ==
                &state.environmentBackgroundBlueSliderValue) {
            found_background_blue = true;
        }
    }
    for (size_t i = 0; i < sliders.count; ++i) {
        if (sliders.items[i].label &&
            strcmp(sliders.items[i].label, "Ambient Brightness") == 0) {
            found_environment_in_right_panel = true;
        }
    }

    assert_true("menu_renderer_controls_environment_present", found_environment);
    assert_true("menu_renderer_controls_top_fill_present", found_top_fill);
    assert_true("menu_renderer_controls_background_present", found_background);
    assert_true("menu_renderer_controls_background_red_present", found_background_red);
    assert_true("menu_renderer_controls_background_green_present", found_background_green);
    assert_true("menu_renderer_controls_background_blue_present", found_background_blue);
    assert_true("menu_slider_layout_environment_moved_from_right_panel",
                !found_environment_in_right_panel);
    assert_true("menu_slider_layout_environment_value_synced",
                state.envSliderValue == 128);
    assert_true("menu_slider_layout_top_fill_value_synced",
                state.topFillStrengthSliderValue == 150);
    assert_true("menu_slider_layout_background_value_synced",
                state.environmentBackgroundBrightnessSliderValue == 75);
    assert_true("menu_slider_layout_background_red_value_synced",
                state.environmentBackgroundRedSliderValue == 25);
    assert_true("menu_slider_layout_background_green_value_synced",
                state.environmentBackgroundGreenSliderValue == 50);
    assert_true("menu_slider_layout_background_blue_value_synced",
                state.environmentBackgroundBlueSliderValue == 75);
    assert_true("menu_renderer_controls_lighting_sliders_fit_panel",
                buttons.rendererControlSliders.contentBottomY <=
                    buttons.rendererControlSliders.panelRect.y +
                        buttons.rendererControlSliders.panelRect.h);

    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    memset(&buttons, 0, sizeof(buttons));
    menu_render_build_button_layout(NULL, &state, &screen, &buttons);
    for (size_t i = 0; i < buttons.rendererControlSliders.count; ++i) {
        if (buttons.rendererControlSliders.items[i].label &&
            strcmp(buttons.rendererControlSliders.items[i].label, "BG Red") == 0) {
            found_background_red_while_off = true;
        }
    }
    assert_true("menu_renderer_controls_background_color_hidden_while_off",
                !found_background_red_while_off);
    animSettings = saved_anim;
    return 0;
}

static int test_menu_environment_settings_invalidate_and_read_back_runtime(void) {
    AnimationConfig saved_anim = animSettings;
    RuntimeNative3DPreparedSceneCacheStats before = {0};
    RuntimeNative3DPreparedSceneCacheStats after_change = {0};
    RuntimeNative3DPreparedSceneCacheStats after_noop = {0};
    RuntimeNative3DPreparedSceneCacheStats after_all_controls = {0};
    RuntimeNative3DPreparedSceneCacheStats after_background_noop = {0};
    RuntimeNative3DPreparedSceneCacheStats after_color_noop = {0};
    MenuEnvironmentRuntimeReadback readback = {0};

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    animSettings.environmentPreset = ENVIRONMENT_PRESET_SKY;
    animSettings.environmentBackgroundLightingAuthored = true;
    animSettings.environmentBackgroundBrightnessAuto = true;
    animSettings.environmentBackgroundColorR = 1.0;
    animSettings.environmentBackgroundColorG = 1.0;
    animSettings.environmentBackgroundColorB = 1.0;
    RuntimeNative3DPreparedSceneCacheResetForTests();
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&before);

    assert_true("menu_environment_settings_mode_change_reports_change",
                menu_environment_settings_set_light_mode(
                    ENVIRONMENT_LIGHT_MODE_AMBIENT));
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&after_change);
    assert_true("menu_environment_settings_mode_change_invalidates_generation",
                after_change.generation == before.generation + 1u &&
                    !after_change.valid);

    assert_true("menu_environment_settings_mode_noop_reports_no_change",
                !menu_environment_settings_set_light_mode(
                    ENVIRONMENT_LIGHT_MODE_AMBIENT));
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&after_noop);
    assert_true("menu_environment_settings_mode_noop_preserves_generation",
                after_noop.generation == after_change.generation);

    assert_true("menu_environment_settings_ambient_change_reports_change",
                menu_environment_settings_set_ambient_brightness(128.0));
    assert_true("menu_environment_settings_top_fill_change_reports_change",
                menu_environment_settings_set_top_fill_strength(2.5));
    assert_true("menu_environment_settings_top_fill_updates_authored_value",
                fabs(animSettings.topFillStrength - 2.5) <= 1e-9);
    assert_true("menu_environment_settings_preset_change_reports_change",
                menu_environment_settings_set_preset(
                    ENVIRONMENT_PRESET_WARM_SKY));
    assert_true("menu_environment_settings_preset_marks_background_authored",
                animSettings.environmentBackgroundLightingAuthored);
    assert_true("menu_environment_settings_background_auto_noop_reports_no_change",
                !menu_environment_settings_set_background_auto(true, 0.75));
    assert_true("menu_environment_settings_background_manual_reports_change",
                menu_environment_settings_set_background_auto(false, 0.75));
    assert_true("menu_environment_settings_background_brightness_reports_change",
                menu_environment_settings_set_background_brightness(1.25));
    assert_true("menu_environment_settings_background_color_reports_change",
                menu_environment_settings_set_background_color(0.25, 0.50, 0.75));
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&after_all_controls);
    assert_true("menu_environment_settings_each_changed_control_invalidates_once",
                after_all_controls.generation == after_change.generation + 6u);
    assert_true("menu_environment_settings_background_brightness_noop_reports_no_change",
                !menu_environment_settings_set_background_brightness(1.25));
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&after_background_noop);
    assert_true("menu_environment_settings_background_brightness_noop_preserves_generation",
                after_background_noop.generation == after_all_controls.generation);
    assert_true("menu_environment_settings_background_color_noop_reports_no_change",
                !menu_environment_settings_set_background_color(0.25, 0.50, 0.75));
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&after_color_noop);
    assert_true("menu_environment_settings_background_color_noop_preserves_generation",
                after_color_noop.generation == after_all_controls.generation);
    menu_environment_settings_readback(&readback);
    assert_true("menu_environment_settings_readback_normalizes_ambient",
                fabs(readback.ambientStrength - (128.0 / 255.0)) <= 1e-9);
    assert_true("menu_environment_settings_readback_reports_manual_background",
                !readback.backgroundBrightnessDerivedFromAmbient &&
                    fabs(readback.backgroundBrightness - 1.25) <= 1e-9);
    assert_true("menu_environment_settings_readback_reports_effective_preset",
                readback.preset == ENVIRONMENT_PRESET_WARM_SKY);
    assert_true("menu_environment_settings_readback_reports_background_color",
                fabs(readback.backgroundColorR - 0.25) <= 1e-9 &&
                    fabs(readback.backgroundColorG - 0.50) <= 1e-9 &&
                    fabs(readback.backgroundColorB - 0.75) <= 1e-9 &&
                    strstr(readback.summary, "rgb 0.25/0.50/0.75") != NULL);
    assert_true("menu_environment_settings_readback_reports_pending_generation",
                !readback.runtimeApplied &&
                    strstr(readback.summary, "refresh pending") != NULL);

    assert_true("menu_environment_settings_auto_mode_reports_change",
                menu_environment_settings_set_background_auto(true, 0.0));
    menu_environment_settings_readback(&readback);
    assert_true("menu_environment_settings_auto_preview_is_grayscale",
                fabs(readback.backgroundPreviewColorR - readback.backgroundPreviewColorG) <=
                        1e-9 &&
                    fabs(readback.backgroundPreviewColorG -
                         readback.backgroundPreviewColorB) <= 1e-9 &&
                    fabs(readback.backgroundPreviewColorR - (128.0 / 255.0)) <= 1e-9);
    assert_true("menu_environment_settings_auto_uses_neutral_runtime_tint",
                fabs(readback.backgroundColorR - 1.0) <= 1e-9 &&
                    fabs(readback.backgroundColorG - 1.0) <= 1e-9 &&
                    fabs(readback.backgroundColorB - 1.0) <= 1e-9);
    assert_true("menu_environment_settings_custom_mode_reports_change",
                menu_environment_settings_set_background_auto(false, 0.0));
    assert_true("menu_environment_settings_custom_values_survive_auto_roundtrip",
                fabs(animSettings.environmentBackgroundBrightness - 1.25) <= 1e-9 &&
                    fabs(animSettings.environmentBackgroundColorR - 0.25) <= 1e-9 &&
                    fabs(animSettings.environmentBackgroundColorG - 0.50) <= 1e-9 &&
                    fabs(animSettings.environmentBackgroundColorB - 0.75) <= 1e-9);

    animSettings = saved_anim;
    RuntimeNative3DPreparedSceneCacheResetForTests();
    return 0;
}

static int test_main_edit_build_identity_labels_windows_without_changing_standard(void) {
    const char* saved_profile_value = getenv("RAY_TRACING_PACKAGE_PROFILE");
    const char* saved_label_value = getenv("RAY_TRACING_BUILD_LABEL");
    char* saved_profile = saved_profile_value ? strdup(saved_profile_value) : NULL;
    char* saved_label = saved_label_value ? strdup(saved_label_value) : NULL;
    char title[192] = {0};

    setenv("RAY_TRACING_PACKAGE_PROFILE", "standard", 1);
    setenv("RAY_TRACING_BUILD_LABEL", "optiC-0.14.0", 1);
    ray_tracing_build_identity_format_window_title(
        title, sizeof(title), "RayTracing Menu");
    assert_true("standard_package_window_title_unchanged",
                strcmp(title, "RayTracing Menu") == 0);

    setenv("RAY_TRACING_PACKAGE_PROFILE", "main-edit", 1);
    setenv("RAY_TRACING_BUILD_LABEL", "main-edit-0.14.0-382b6871e785", 1);
    ray_tracing_build_identity_format_window_title(
        title, sizeof(title), "RayTracing Menu");
    assert_true("main_edit_package_window_title_is_visible",
                strcmp(title,
                       "RayTracing Menu - main-edit-0.14.0-382b6871e785") == 0);

    restore_env_or_unset("RAY_TRACING_PACKAGE_PROFILE", saved_profile);
    restore_env_or_unset("RAY_TRACING_BUILD_LABEL", saved_label);
    free(saved_profile);
    free(saved_label);
    return 0;
}

static int test_menu_environment_override_survives_one_scene_restore(void) {
    AnimationConfig saved_anim = animSettings;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_AMBIENT;
    animSettings.environmentBrightness = 96.0;
    animSettings.environmentPreset = ENVIRONMENT_PRESET_WARM_SKY;
    animSettings.environmentBackgroundLightingAuthored = true;
    animSettings.environmentBackgroundBrightnessAuto = false;
    animSettings.environmentBackgroundBrightness = 0.8;
    animSettings.environmentBackgroundColorR = 0.1;
    animSettings.environmentBackgroundColorG = 0.2;
    animSettings.environmentBackgroundColorB = 0.9;
    animSettings.topFillStrength = 1.7;
    animSettings.lightIntensity = 6.5;
    animSettings.lightRadius = 0.35;
    animSettings.lightHeight = 12.0;
    animSettings.forwardDecay = 420.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightDecaySoftness = 0.65;
    AnimationPreserveCurrentEnvironmentOnNextInit();

    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentPreset = ENVIRONMENT_PRESET_SKY;
    animSettings.environmentBackgroundBrightnessAuto = true;
    animSettings.environmentBackgroundBrightness = 0.0;
    animSettings.environmentBackgroundColorR = 1.0;
    animSettings.environmentBackgroundColorG = 1.0;
    animSettings.environmentBackgroundColorB = 1.0;
    animSettings.topFillStrength = 1.0;
    animSettings.lightIntensity = 0.4;
    animSettings.lightRadius = 0.0;
    animSettings.lightHeight = 2.0;
    animSettings.forwardDecay = 100.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_QUADRATIC;
    animSettings.lightDecaySoftness = 1.0;
    AnimationApplyPreservedEnvironmentAfterSceneRestore();

    assert_true("menu_environment_start_override_restores_mode",
                animSettings.environmentLightMode == ENVIRONMENT_LIGHT_MODE_AMBIENT);
    assert_true("menu_environment_start_override_restores_custom_policy",
                !animSettings.environmentBackgroundBrightnessAuto);
    assert_true("menu_environment_start_override_restores_custom_values",
                fabs(animSettings.environmentBrightness - 96.0) <= 1e-9 &&
                    fabs(animSettings.environmentBackgroundBrightness - 0.8) <= 1e-9 &&
                    fabs(animSettings.environmentBackgroundColorR - 0.1) <= 1e-9 &&
                    fabs(animSettings.environmentBackgroundColorG - 0.2) <= 1e-9 &&
                    fabs(animSettings.environmentBackgroundColorB - 0.9) <= 1e-9);
    assert_true("menu_environment_start_override_restores_direct_light_values",
                fabs(animSettings.lightIntensity - 6.5) <= 1e-9 &&
                    fabs(animSettings.lightRadius - 0.35) <= 1e-9 &&
                    fabs(animSettings.lightHeight - 12.0) <= 1e-9 &&
                    fabs(animSettings.forwardDecay - 420.0) <= 1e-9 &&
                    animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR &&
                    fabs(animSettings.lightDecaySoftness - 0.65) <= 1e-9);

    animSettings.environmentBackgroundColorB = 0.4;
    AnimationApplyPreservedEnvironmentAfterSceneRestore();
    assert_true("menu_environment_start_override_is_one_shot",
                fabs(animSettings.environmentBackgroundColorB - 0.4) <= 1e-9);

    animSettings = saved_anim;
    return 0;
}

static int test_menu_environment_settings_rebuild_cached_static_scene(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_environment_menu_cache\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"menu_cache_plane\","
          "\"object_type\":\"plane\","
          "\"primitive\":{\"kind\":\"plane\",\"width\":2.0,\"height\":2.0,"
          "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
          "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
          "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
          "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
          "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
          "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}}],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-4.0,\"z\":1.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DPreparedFrame initial = {0};
    RuntimeNative3DPreparedFrame rebuilt = {0};
    RuntimeNative3DPreparedSceneCacheStats cached = {0};
    RuntimeNative3DPreparedSceneCacheStats pending = {0};
    RuntimeNative3DPreparedSceneCacheStats applied = {0};
    MenuEnvironmentRuntimeReadback readback = {0};
    MenuSettingsRuntimeReadback settings_readback = {0};
    bool ok = false;

    RuntimeNative3DPreparedSceneCacheResetForTests();
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("menu_environment_cache_apply_fixture", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        RuntimeNative3DPreparedSceneCacheResetForTests();
        return 0;
    }

    animSettings.interactiveMode = true;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentPreset = ENVIRONMENT_PRESET_SKY;
    animSettings.environmentBackgroundLightingAuthored = true;
    animSettings.environmentBackgroundBrightnessAuto = true;
    animSettings.environmentBackgroundColorR = 1.0;
    animSettings.environmentBackgroundColorG = 1.0;
    animSettings.environmentBackgroundColorB = 1.0;
    animSettings.runtimeWindowWidth = sceneSettings.windowWidth;
    animSettings.runtimeWindowHeight = sceneSettings.windowHeight;
    animSettings.runtimeRayCount = sceneSettings.rays;

    ok = RuntimeNative3DPrepareFrame(&initial, 32, 32, 0.0, 0.0, -4.0);
    assert_true("menu_environment_cache_initial_prepare", ok);
    if (ok) {
        assert_true("menu_environment_cache_initial_mode_off",
                    initial.scene.environment.lightMode ==
                        ENVIRONMENT_LIGHT_MODE_OFF);
    }
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&cached);
    assert_true("menu_environment_cache_initial_generation_applied",
                cached.valid && cached.cachedGeneration == cached.generation);
    menu_settings_runtime_readback(&settings_readback);
    assert_true("menu_settings_readback_initial_recipe_applied",
                settings_readback.appliedState == MENU_SETTINGS_APPLIED_STATE_APPLIED &&
                    settings_readback.configuredValuesMatchRuntime &&
                    settings_readback.preparedSceneCurrent &&
                    strstr(settings_readback.summary, "applied") != NULL);

    sceneSettings.windowWidth += 2;
    menu_settings_runtime_readback(&settings_readback);
    assert_true("menu_settings_readback_detects_uncommitted_dimension_edit",
                settings_readback.appliedState == MENU_SETTINGS_APPLIED_STATE_EDIT_PENDING &&
                    !settings_readback.configuredValuesMatchRuntime);
    sceneSettings.windowWidth = animSettings.runtimeWindowWidth;

    assert_true("menu_environment_cache_mode_change",
                menu_environment_settings_set_light_mode(
                    ENVIRONMENT_LIGHT_MODE_AMBIENT));
    assert_true("menu_environment_cache_strength_change",
                menu_environment_settings_set_ambient_brightness(128.0));
    assert_true("menu_environment_cache_background_custom_brightness_change",
                menu_environment_settings_set_background_brightness(1.0));
    assert_true("menu_environment_cache_background_color_change",
                menu_environment_settings_set_background_color(0.25, 0.50, 0.75));
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&pending);
    assert_true("menu_environment_cache_change_pending",
                !pending.valid && pending.generation == cached.generation + 4u);
    menu_settings_runtime_readback(&settings_readback);
    assert_true("menu_settings_readback_detects_render_refresh_pending",
                settings_readback.appliedState ==
                        MENU_SETTINGS_APPLIED_STATE_RENDER_REFRESH_PENDING &&
                    settings_readback.configuredValuesMatchRuntime &&
                    !settings_readback.preparedSceneCurrent);

    ok = RuntimeNative3DPrepareFrame(&rebuilt, 32, 32, 0.0, 0.0, -4.0);
    assert_true("menu_environment_cache_rebuild", ok);
    if (ok) {
        assert_true("menu_environment_cache_rebuilt_mode",
                    rebuilt.scene.environment.lightMode ==
                        ENVIRONMENT_LIGHT_MODE_AMBIENT);
        assert_true("menu_environment_cache_rebuilt_strength",
                    fabs(rebuilt.scene.environment.ambientIntensity -
                         (128.0 / 255.0)) <= 1e-9);
        assert_true("menu_environment_cache_rebuilt_background_tint",
                    fabs(rebuilt.scene.environment.backgroundColor.x - 0.25) <= 1e-9 &&
                        fabs(rebuilt.scene.environment.backgroundColor.y - 0.50) <= 1e-9 &&
                        fabs(rebuilt.scene.environment.backgroundColor.z - 0.75) <= 1e-9);
        assert_true("menu_environment_cache_rebuilt_preset_tinted_gradient",
                    fabs(rebuilt.scene.environment.backgroundTopColor.x -
                         (0.66 * 0.25)) <= 1e-9 &&
                        fabs(rebuilt.scene.environment.backgroundTopColor.y -
                             (0.70 * 0.50)) <= 1e-9 &&
                        fabs(rebuilt.scene.environment.backgroundTopColor.z -
                             (0.76 * 0.75)) <= 1e-9);
    }
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&applied);
    menu_environment_settings_readback(&readback);
    menu_settings_runtime_readback(&settings_readback);
    assert_true("menu_environment_cache_rebuild_applied",
                applied.valid && applied.cachedGeneration == applied.generation &&
                    readback.runtimeApplied &&
                    strstr(readback.summary, "applied") != NULL);
    assert_true("menu_settings_readback_rebuild_applied",
                settings_readback.appliedState == MENU_SETTINGS_APPLIED_STATE_APPLIED &&
                    settings_readback.preparedSceneCurrent &&
                    strstr(settings_readback.summary, "applied") != NULL);

    RuntimeNative3DPreparedFrame_Free(&initial);
    RuntimeNative3DPreparedFrame_Free(&rebuilt);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeNative3DPreparedSceneCacheResetForTests();
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
    bool width_contract_ok = false;
    bool height_contract_ok = false;
    bool ray_contract_ok = false;

    memset(&state, 0, sizeof(state));
    memset(&screen, 0, sizeof(screen));
    memset(&sliders, 0, sizeof(sliders));
    memset(&animSettings, 0, sizeof(animSettings));

    animSettings.spaceMode = SPACE_MODE_2D;
    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
    menu_render_build_slider_layout(NULL, &state, &screen, &sliders);
    for (size_t i = 0; i < sliders.count; ++i) {
        if (sliders.items[i].value == &sceneSettings.windowWidth) {
            width_contract_ok =
                sliders.items[i].min == RAY_TRACING_RUNTIME_WINDOW_DIMENSION_MIN &&
                sliders.items[i].max == RAY_TRACING_RUNTIME_WINDOW_WIDTH_MAX;
        }
        if (sliders.items[i].value == &sceneSettings.windowHeight) {
            height_contract_ok =
                sliders.items[i].min == RAY_TRACING_RUNTIME_WINDOW_DIMENSION_MIN &&
                sliders.items[i].max == RAY_TRACING_RUNTIME_WINDOW_HEIGHT_MAX;
        }
        if (sliders.items[i].value == &sceneSettings.rays) {
            ray_contract_ok =
                sliders.items[i].min == RAY_TRACING_RUNTIME_RAY_COUNT_MIN &&
                sliders.items[i].max == RAY_TRACING_RUNTIME_RAY_COUNT_MAX;
        }
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
    assert_true("menu_slider_layout_width_uses_config_contract", width_contract_ok);
    assert_true("menu_slider_layout_height_uses_config_contract", height_contract_ok);
    assert_true("menu_slider_layout_rays_uses_config_contract", ray_contract_ok);

    memset(&sliders, 0, sizeof(sliders));
    found_legacy_bounce = false;
    found_legacy_roulette = false;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.bounceDepth3D = 4;
    animSettings.rouletteThreshold3D = 0.02;
    menu_layout_build_base(NULL, &state, TEST_MENU_WIDTH, TEST_MENU_HEIGHT, &screen);
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
    assert_true("integrator_cycle_3d_advances_disney_to_disney_v2",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DISNEY_V2);

    RayTracingIntegratorCatalog_CycleActiveSelection(&animSettings);
    assert_true("integrator_cycle_3d_wraps_disney_v2_to_direct_light",
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
    char tmp_input_template[] = "/tmp/ray_tracing_menu_input_XXXXXX";
    char *tmp_input = mkdtemp(tmp_input_template);
    const char *input_env = getenv("RAY_TRACING_INPUT_ROOT");
    const char *output_env = getenv("RAY_TRACING_OUTPUT_ROOT");
    char input_backup[PATH_MAX] = {0};
    char output_backup[PATH_MAX] = {0};
    char input_scenes_dir[PATH_MAX];
    char input_samples_dir[PATH_MAX];
    bool had_input_env = false;
    bool had_output_env = false;
    const char **roots = NULL;
    size_t root_count = 0;

    assert_true("manifest_roots_tmp_input_created", tmp_input != NULL);
    if (!tmp_input) return 0;

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

    snprintf(input_scenes_dir, sizeof(input_scenes_dir), "%s/scenes", tmp_input);
    snprintf(input_samples_dir, sizeof(input_samples_dir), "%s/samples", tmp_input);
    assert_true("manifest_roots_tmp_input_scenes_created", mkdir(input_scenes_dir, 0700) == 0);

    setenv("RAY_TRACING_INPUT_ROOT", tmp_input, 1);
    unsetenv("RAY_TRACING_OUTPUT_ROOT");

    root_count = ray_tracing_manifest_default_roots(&roots);
    assert_true("manifest_roots_nonempty", root_count > 0 && roots != NULL);
    assert_true("manifest_roots_has_configured_input_root",
                path_list_contains(roots, root_count, tmp_input));
    assert_true("manifest_roots_has_configured_input_scenes",
                path_list_contains(roots, root_count, input_scenes_dir));
    assert_true("manifest_roots_has_configured_input_samples",
                path_list_contains(roots, root_count, input_samples_dir));
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
    rmdir(input_scenes_dir);
    rmdir(tmp_input);
    return 0;
}

static int test_stable_roots_accept_packaged_runtime_cwd(void) {
    char tmp_template[] = "/tmp/ray_tracing_runtime_root_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char original_cwd[PATH_MAX];
    char config_dir[PATH_MAX];
    char data_dir[PATH_MAX];
    char runtime_dir[PATH_MAX];
    char resolved_config_dir[PATH_MAX];
    char resolved_runtime_dir[PATH_MAX];
    char input_root[PATH_MAX];
    char output_root[PATH_MAX];
    const char *configured_root = getenv("RAY_TRACING_PROGRAM_ROOT");
    char configured_root_backup[PATH_MAX] = {0};
    bool had_configured_root = false;

    assert_true("stable_roots_runtime_tmp_created", tmp_root != NULL);
    assert_true("stable_roots_runtime_cwd_read", getcwd(original_cwd, sizeof(original_cwd)) != NULL);
    if (!tmp_root || !original_cwd[0]) return 0;
    if (configured_root && configured_root[0]) {
        snprintf(configured_root_backup, sizeof(configured_root_backup), "%s", configured_root);
        had_configured_root = true;
    }
    snprintf(config_dir, sizeof(config_dir), "%s/config", tmp_root);
    snprintf(data_dir, sizeof(data_dir), "%s/data", tmp_root);
    snprintf(runtime_dir, sizeof(runtime_dir), "%s/runtime", data_dir);
    assert_true("stable_roots_runtime_config_created", mkdir(config_dir, 0700) == 0);
    assert_true("stable_roots_runtime_data_created", mkdir(data_dir, 0700) == 0);
    assert_true("stable_roots_runtime_output_created", mkdir(runtime_dir, 0700) == 0);
    unsetenv("RAY_TRACING_PROGRAM_ROOT");
    assert_true("stable_roots_runtime_chdir", chdir(tmp_root) == 0);
    assert_true("stable_roots_runtime_input_resolved",
                ray_tracing_find_stable_input_root(input_root, sizeof(input_root)) &&
                    strcmp(input_root,
                           realpath(config_dir, resolved_config_dir)
                               ? resolved_config_dir
                               : config_dir) == 0);
    assert_true("stable_roots_runtime_output_resolved",
                ray_tracing_find_stable_output_root(output_root, sizeof(output_root)) &&
                    strcmp(output_root,
                           realpath(runtime_dir, resolved_runtime_dir)
                               ? resolved_runtime_dir
                               : runtime_dir) == 0);
    (void)chdir(original_cwd);
    if (had_configured_root) {
        setenv("RAY_TRACING_PROGRAM_ROOT", configured_root_backup, 1);
    } else {
        unsetenv("RAY_TRACING_PROGRAM_ROOT");
    }
    rmdir(runtime_dir);
    rmdir(data_dir);
    rmdir(config_dir);
    rmdir(tmp_root);
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

static int test_scene_source_catalog_collects_all_runtime_scene_folders(void) {
    char tmp_template[] = "/tmp/ray_tracing_catalog_many_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    const char *scene_names[] = {
        "column_overhang",
        "column_overhang_wall",
        "prism_maze_corner_room_v1",
        "simple_plane",
        "skull_plane_with_platform",
        "texture_blocks"
    };
    char scene_dirs[6][PATH_MAX];
    char runtime_paths[6][PATH_MAX];
    const char *roots[1];
    SceneSourceCatalogEntry entries[16];
    size_t entry_count = 0;

    assert_true("scene_catalog_many_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    for (size_t i = 0; i < sizeof(scene_names) / sizeof(scene_names[0]); ++i) {
        char runtime_json[512];
        snprintf(scene_dirs[i], sizeof(scene_dirs[i]), "%s/%s", tmp_root, scene_names[i]);
        snprintf(runtime_paths[i], sizeof(runtime_paths[i]), "%s/scene_runtime.json", scene_dirs[i]);
        snprintf(runtime_json,
                 sizeof(runtime_json),
                 "{"
                 "\"schema_family\":\"codework_scene\","
                 "\"schema_variant\":\"scene_runtime_v1\","
                 "\"schema_version\":1,"
                 "\"scene_id\":\"%s\","
                 "\"unit_system\":\"meters\","
                 "\"world_scale\":1.0,"
                 "\"space_mode_default\":\"3d\","
                 "\"objects\":[],"
                 "\"materials\":[],"
                 "\"lights\":[],"
                 "\"cameras\":[]"
                 "}",
                 scene_names[i]);
        assert_true("scene_catalog_many_mkdir", mkdir(scene_dirs[i], 0700) == 0);
        assert_true("scene_catalog_many_write_runtime", write_text_file(runtime_paths[i], runtime_json));
    }

    roots[0] = tmp_root;
    entry_count = scene_source_catalog_collect(entries,
                                               sizeof(entries) / sizeof(entries[0]),
                                               roots,
                                               1,
                                               NULL,
                                               NULL);

    assert_true("scene_catalog_many_runtime_count",
                catalog_count_source(entries, entry_count, SCENE_SOURCE_RUNTIME_SCENE) == 6);
    for (size_t i = 0; i < sizeof(scene_names) / sizeof(scene_names[0]); ++i) {
        assert_true("scene_catalog_many_runtime_present",
                    catalog_contains_path_source(entries,
                                                 entry_count,
                                                 runtime_paths[i],
                                                 SCENE_SOURCE_RUNTIME_SCENE));
    }

    for (size_t i = 0; i < sizeof(scene_names) / sizeof(scene_names[0]); ++i) {
        remove(runtime_paths[i]);
        rmdir(scene_dirs[i]);
    }
    rmdir(tmp_root);
    return 0;
}

static int test_menu_state_manifest_options_follow_configured_input_root(void) {
    char tmp_template[] = "/tmp/ray_tracing_menu_input_scene_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char scenes_dir[PATH_MAX];
    char scene_dir[PATH_MAX];
    char runtime_path[PATH_MAX];
    char resolved_runtime_path[PATH_MAX];
    const char *input_env = getenv("RAY_TRACING_INPUT_ROOT");
    char input_backup[PATH_MAX] = {0};
    bool had_input_env = false;
    AnimationConfig saved_anim = animSettings;
    MenuRuntimeState state = {0};
    bool found = false;

    assert_true("menu_input_scene_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;
    if (input_env && input_env[0]) {
        strncpy(input_backup, input_env, sizeof(input_backup) - 1);
        input_backup[sizeof(input_backup) - 1] = '\0';
        had_input_env = true;
    }

    snprintf(scenes_dir, sizeof(scenes_dir), "%s/scenes", tmp_root);
    snprintf(scene_dir, sizeof(scene_dir), "%s/skull_plane_with_platform", scenes_dir);
    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", scene_dir);
    assert_true("menu_input_scene_mkdir_scenes", mkdir(scenes_dir, 0700) == 0);
    assert_true("menu_input_scene_mkdir_scene", mkdir(scene_dir, 0700) == 0);
    assert_true("menu_input_scene_write_runtime",
                write_text_file(runtime_path,
                                "{"
                                "\"schema_family\":\"codework_scene\","
                                "\"schema_variant\":\"scene_runtime_v1\","
                                "\"schema_version\":1,"
                                "\"scene_id\":\"configured_input_scene\","
                                "\"unit_system\":\"meters\","
                                "\"world_scale\":1.0,"
                                "\"space_mode_default\":\"3d\","
                                "\"objects\":[],"
                                "\"materials\":[],"
                                "\"lights\":[],"
                                "\"cameras\":[]"
                                "}"));
    if (!realpath(runtime_path, resolved_runtime_path)) {
        snprintf(resolved_runtime_path, sizeof(resolved_runtime_path), "%s", runtime_path);
    }

    snprintf(animSettings.inputRoot, sizeof(animSettings.inputRoot), "%s", tmp_root);
    animSettings.spaceMode = SPACE_MODE_3D;
    setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    menu_state_refresh_manifest_options(&state);
    for (size_t i = 0; i < state.manifestOptionCount; ++i) {
        if (state.manifestOptions[i].source == SCENE_SOURCE_RUNTIME_SCENE &&
            strcmp(state.manifestOptions[i].path, resolved_runtime_path) == 0) {
            found = true;
            break;
        }
    }
    assert_true("menu_input_scene_runtime_option_found", found);

    animSettings = saved_anim;
    if (had_input_env) {
        setenv("RAY_TRACING_INPUT_ROOT", input_backup, 1);
    } else {
        unsetenv("RAY_TRACING_INPUT_ROOT");
    }
    remove(runtime_path);
    rmdir(scene_dir);
    rmdir(scenes_dir);
    rmdir(tmp_root);
    return 0;
}

static int test_scene_source_catalog_runtime_discovery_is_filename_based(void) {
    char tmp_template[] = "/tmp/ray_tracing_catalog_lazy_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char scene_dir[PATH_MAX];
    char runtime_path[PATH_MAX];
    const char *roots[1];
    SceneSourceCatalogEntry entries[16];
    size_t entry_count = 0;

    assert_true("scene_catalog_lazy_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(scene_dir, sizeof(scene_dir), "%s/skull_plane_with_platform", tmp_root);
    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", scene_dir);
    assert_true("scene_catalog_lazy_mkdir", mkdir(scene_dir, 0700) == 0);
    assert_true("scene_catalog_lazy_write_runtime",
                write_text_file(runtime_path, "{\"not\":\"validated during catalog\"}"));

    roots[0] = tmp_root;
    entry_count = scene_source_catalog_collect(entries,
                                               sizeof(entries) / sizeof(entries[0]),
                                               roots,
                                               1,
                                               NULL,
                                               NULL);

    assert_true("scene_catalog_lazy_runtime_present",
                catalog_contains_path_source(entries,
                                             entry_count,
                                             runtime_path,
                                             SCENE_SOURCE_RUNTIME_SCENE));

    remove(runtime_path);
    rmdir(scene_dir);
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

static int test_object_editor_transparent_alpha_slider_is_labeled_transparency(void) {
    assert_true("object_editor_transparent_alpha_slider_label",
                strcmp(ObjectEditorPanels_LabelForSliderKind(OBJECT_EDITOR_PANEL_SLIDER_COLOR_A),
                       "Transparency") == 0);
    return 0;
}


static void test_menu_numeric_editing_and_constraints(void) {
    MenuNumericSpec frames = {0, 5000, 5, 1, 1, 0};
    MenuNumericSpec dimension = {2, 10000, 2, 2, 1, 0};
    MenuNumericSpec decimal = {0, 100, 1, 1, 1000, 3};
    MenuNumericSpec fps = {1, 240, 1, 1, 1, 0};
    MenuNumericEdit edit;
    int value = 1000, parsed = -1;
    menu_numeric_begin(&edit, &value, frames);
    menu_numeric_delete(&edit, false);
    assert_true("numeric_empty_draft_stays_empty", edit.text[0] == '\0');
    assert_true("numeric_draft_does_not_change_setting", value == 1000);
    assert_true("numeric_empty_commits_zero", menu_numeric_parse(frames, edit.text, &parsed) && parsed == 0);
    assert_true("numeric_insert_10", menu_numeric_insert(&edit, "10"));
    menu_numeric_move(&edit, 1, false);
    menu_numeric_delete(&edit, false);
    assert_true("numeric_replace_middle_digit", menu_numeric_insert(&edit, "2") && strcmp(edit.text, "20") == 0);
    menu_numeric_move(&edit, 0, false);
    menu_numeric_delete(&edit, true);
    assert_true("numeric_forward_delete", strcmp(edit.text, "0") == 0);
    menu_numeric_move(&edit, 1, true);
    assert_true("numeric_selection_replace", menu_numeric_insert(&edit, "1237") && strcmp(edit.text, "1237") == 0);
    assert_true("numeric_precise_integer", menu_numeric_parse(frames, edit.text, &parsed) && parsed == 1237);
    assert_true("numeric_drag_step_five", menu_numeric_normalize(frames, 1237, true) == 1235);
    assert_true("numeric_nudge_relative", menu_numeric_normalize(frames, 1237 + frames.step, false) == 1242);
    assert_true("numeric_dimension_odd_rounds_up", menu_numeric_parse(dimension, "1201", &parsed) && parsed == 1202);
    assert_true("numeric_dimension_even_preserved", menu_numeric_parse(dimension, "1200", &parsed) && parsed == 1200);
    assert_true("numeric_dimension_empty_valid_min", menu_numeric_parse(dimension, "", &parsed) && parsed == 2);
    assert_true("numeric_fps_empty_valid_min", menu_numeric_parse(fps, "", &parsed) && parsed == 1);
    assert_true("numeric_decimal_display_units", menu_numeric_parse(decimal, "0.005", &parsed) && parsed == 5);
    assert_true("numeric_bounds_clamp", menu_numeric_parse(frames, "999999999", &parsed) && parsed == 5000);
    assert_true("numeric_reject_nonfinite", !menu_numeric_parse(frames, "nan", &parsed));
    assert_true("numeric_reject_overflow", !menu_numeric_parse(frames, "1e999", &parsed));
    assert_true("numeric_reject_partial_number", !menu_numeric_parse(decimal, "-", &parsed));
    assert_true("numeric_reject_garbage", !menu_numeric_parse(decimal, "1.2.3", &parsed));
    assert_true("numeric_reject_fractional_integer", !menu_numeric_parse(frames, "1.2", &parsed));
    char too_long[80]; memset(too_long, '1', sizeof(too_long) - 1); too_long[79] = 0;
    assert_true("numeric_paste_is_bounded", !menu_numeric_insert(&edit, too_long));

    MenuRuntimeState *state = calloc(1, sizeof(*state));
    assert_true("numeric_state_allocated", state != NULL);
    if (!state) return;
    SDL_Event event = {0};
    bool running = true;
    TTF_Font *font = NULL;
    menu_numeric_begin(&state->numericEdit, &value, frames);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_i;
    bool prior_mode = animSettings.interactiveMode;
    menu_input_handle_key(&event, &running, &font, state);
    assert_true("numeric_captures_menu_shortcuts", animSettings.interactiveMode == prior_mode);
    event.key.keysym.sym = SDLK_BACKSPACE;
    menu_input_handle_key(&event, &running, &font, state);
    assert_true("numeric_key_deletes_selection", state->numericEdit.text[0] == 0);
    event.key.keysym.sym = SDLK_ESCAPE;
    menu_input_handle_key(&event, &running, &font, state);
    assert_true("numeric_escape_cancels_without_closing", running && !state->numericEdit.active && value == 1000);
    MenuNumericSpec actual = menu_numeric_spec(state, &sceneSettings.windowWidth, 2, 10000);
    assert_true("numeric_width_uses_even_policy", actual.step == 2 && actual.multiple == 2);
    actual = menu_numeric_spec(state, &animSettings.tileSize, 4, 256);
    assert_true("numeric_tile_uses_four_pixel_policy", actual.step == 4 && actual.multiple == 4);
    assert_true("numeric_tile_decrease_changes_value", menu_numeric_normalize(actual, 32 - actual.step, false) == 28);
    actual = menu_numeric_spec(state, &state->rouletteThreshold3DSliderValue, 0, 100);
    assert_true("numeric_roulette_uses_display_scale", actual.divisor == 1000 && actual.decimals == 3);

    MenuScreenLayout screen = {0};
    SliderLayout sliders = {0};
    screen.sliderPanelRect = (SDL_Rect){800, 30, 330, 500};
    menu_render_build_slider_layout(NULL, state, &screen, &sliders);
    assert_true("numeric_slider_rows_have_value_targets", sliders.count > 0 && sliders.items[0].valueRect.w > 0);
    const MenuSlider *slider = &sliders.items[0];
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = slider->valueRect.x + 2;
    event.button.y = slider->valueRect.y + 2;
    event.button.clicks = 1;
    (void)menu_numeric_click(&event, &sliders, state, NULL);
    assert_true("numeric_single_click_does_not_edit", !state->numericEdit.active);
    event.button.clicks = 2;
    (void)menu_numeric_click(&event, &sliders, state, NULL);
    assert_true("numeric_double_click_enters_edit", state->numericEdit.active && state->numericEdit.target == slider->value);
    (void)menu_numeric_finish(state, false);
    AnimationConfig saved_anim = animSettings;
    value = 1237;
    SliderLayout nudge = {0};
    nudge.panelRect = (SDL_Rect){0, 0, 300, 80};
    nudge.count = 1;
    nudge.items[0].value = &value;
    nudge.items[0].min = 0;
    nudge.items[0].max = 5000;
    nudge.items[0].increaseRect = (SDL_Rect){100, 20, 16, 12};
    nudge.items[0].decreaseRect = (SDL_Rect){100, 32, 16, 12};
    event.button.x = 105;
    event.button.y = 25;
    assert_true("numeric_up_arrow_nudges_exact_value",
                menu_numeric_click(&event, &nudge, state, NULL) && value == 1242);
    event.button.y = 35;
    assert_true("numeric_down_arrow_nudges_exact_value",
                menu_numeric_click(&event, &nudge, state, NULL) && value == 1237);
    menu_numeric_begin(&state->numericEdit, &value, frames);
    (void)menu_numeric_insert(&state->numericEdit, "-");
    assert_true("numeric_invalid_commit_retains_draft",
                !menu_numeric_finish(state, true) && state->numericEdit.active &&
                state->numericEdit.invalid && value == 1237);
    menu_numeric_move(&state->numericEdit, 0, false);
    menu_numeric_move(&state->numericEdit, 1, true);
    menu_numeric_delete(&state->numericEdit, false);
    assert_true("numeric_empty_enter_applies_zero",
                menu_numeric_finish(state, true) && value == 0 && !state->numericEdit.active);
    animSettings = saved_anim;
    menu_layout_build_base(NULL, state, 1200, 900, &screen);
    int compact = screen.renderInfoRect.h;
    state->renderInfoExpanded = true;
    menu_layout_build_base(NULL, state, 1200, 900, &screen);
    assert_true("numeric_info_expansion_reclaims_space", screen.renderInfoRect.h > compact);
    free(state);
}

static void test_menu_scroll_interactions(void) {
    MenuScroll scroll = {0};
    float offset = 0;
    SDL_Rect viewport = {10, 20, 200, 100};
    menu_scroll_setup(&scroll, viewport, 500, &offset);
    assert_true("scroll_shared_slim_geometry", scroll.layout.track.w == 6 && scroll.maximum == 400);
    SDL_Event event = {0};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = scroll.layout.thumb.x + 1;
    event.button.y = scroll.layout.thumb.y + 2;
    assert_true("scroll_thumb_capture", menu_scroll_event(&scroll, &event) && scroll.dragging);
    event.type = SDL_MOUSEMOTION;
    event.motion.y = 102;
    assert_true("scroll_drag_bottom", menu_scroll_event(&scroll, &event) && offset == 400);
    event.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_RIGHT;
    assert_true("scroll_other_button_preserves_capture", !menu_scroll_event(&scroll, &event) && scroll.dragging);
    event.button.button = SDL_BUTTON_LEFT;
    assert_true("scroll_release_outside", menu_scroll_event(&scroll, &event) && !scroll.dragging);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.y = viewport.y + 2;
    assert_true("scroll_track_pages", menu_scroll_event(&scroll, &event) && offset < 400);
    scroll.dragging = true;
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    assert_true("scroll_focus_loss_releases", menu_scroll_event(&scroll, &event) && !scroll.dragging);
    menu_scroll_setup(&scroll, viewport, 50, &offset);
    assert_true("scroll_content_shrink_clamps", offset == 0 && !scroll.layout.scrollable);
    scroll.visible = false;
    event.type = SDL_MOUSEBUTTONDOWN;
    assert_true("scroll_hidden_does_not_capture", !menu_scroll_event(&scroll, &event));
}

static void test_menu_compact_layout_and_long_caret(void) {
    MenuRuntimeState *state = calloc(1, sizeof(*state));
    AnimationConfig saved = animSettings;
    animSettings.spaceMode = SPACE_MODE_3D;
    state->menuWorkspaceHost.active_module = MENU_WORKSPACE_RENDER;
    const int sizes[][2] = {{960,640},{1080,760},{1200,900}};
    const bool init_ttf = !TTF_WasInit();
    assert_true("menu_layout_font_init", !init_ttf || TTF_Init() == 0);
    for (int font_size = 18; font_size <= 24; font_size += 6) {
        TTF_Font *font = TTF_OpenFont("third_party/codework_shared/assets/fonts/Lato-Regular.ttf", font_size);
        assert_true("menu_layout_font_loaded", font != NULL);
        if (!font) continue;
        for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
            MenuScreenLayout screen;
            MenuButtonLayout buttons;
            SliderLayout sliders;
            menu_layout_build_base(font, state, sizes[i][0], sizes[i][1], &screen);
            menu_render_build_button_layout(font, state, &screen, &buttons);
            menu_layout_finalize_with_buttons(&screen, &buttons, state);
            menu_render_build_slider_layout(font, state, &screen, &sliders);
            assert_true("compact_scene_list_stops_before_roots", screen.manifestReserveRect.y + screen.manifestReserveRect.h <= buttons.inputRootValueRect.y);
            assert_true("compact_volume_controls_fit", buttons.volumeToggleRect.y + buttons.volumeToggleRect.h <= screen.leftPanelRect.y + screen.leftPanelRect.h);
            for (size_t j = 1; j < sliders.count; ++j)
                assert_true("numeric_rows_do_not_overlap", sliders.items[j-1].valueRect.y + sliders.items[j-1].valueRect.h <= sliders.items[j].labelY);
        }
        MenuSlider slider = {0};
        int value = 12;
        slider.value = &value; slider.min = 0; slider.max = INT_MAX;
        slider.valueX = 100; slider.valueY = 100;
        menu_numeric_layout(&slider, 24, 180);
        SliderLayout layout = {0};
        layout.panelRect = (SDL_Rect){50,50,200,200}; layout.count = 1; layout.items[0] = slider;
        menu_numeric_begin(&state->numericEdit, &value, (MenuNumericSpec){0,INT_MAX,1,1,1,0});
        strcpy(state->numericEdit.text, "12345678901234567890");
        state->numericEdit.cursor = state->numericEdit.anchor = strlen(state->numericEdit.text);
        SDL_Event event = {0}; event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT; event.button.clicks = 1;
        event.button.x = slider.valueRect.x + slider.valueRect.w - 3; event.button.y = slider.valueY + 5;
        assert_true("long_draft_click_hits_value", menu_numeric_click(&event, &layout, state, font));
        assert_true("long_draft_click_uses_scrolled_origin", state->numericEdit.cursor >= 19);
        menu_numeric_finish(state, false);
        TTF_CloseFont(font);
    }
    if (init_ttf) TTF_Quit();
    animSettings = saved;
    free(state);
}

static void test_menu_focus_and_save_failures(void) {
    MenuRuntimeState *state = calloc(1, sizeof(*state));
    AnimationConfig saved = animSettings;
    SDL_Event event = {0};
    TTF_Font *font = NULL;
    bool running = true;
    event.type = SDL_KEYDOWN;
    state->editingInputRoot = true;
    strcpy(state->pathInputBuffer, "river");
    const SDL_Keycode shortcuts[] = {SDLK_i, SDLK_r, SDLK_d, SDLK_b, SDLK_m};
    for (size_t i = 0; i < sizeof(shortcuts)/sizeof(shortcuts[0]); ++i) {
        event.key.keysym.sym = shortcuts[i];
        menu_input_handle_key(&event, &running, &font, state);
        assert_true("path_edit_owns_shortcuts", memcmp(&saved, &animSettings, sizeof(saved)) == 0);
    }
    event.key.keysym.sym = SDLK_ESCAPE;
    menu_input_handle_key(&event, &running, &font, state);
    assert_true("path_escape_cancels_only_field", running && !state->editingInputRoot);
    state->editingStartFrame = true;
    strcpy(state->inputBuffer, "12oops");
    event.key.keysym.sym = SDLK_RETURN;
    menu_input_handle_key(&event, &running, &font, state);
    assert_true("start_frame_rejects_partial_number", state->editingStartFrame && animSettings.startFrameIndex == saved.startFrameIndex);
    event.key.keysym.sym = SDLK_ESCAPE;
    menu_input_handle_key(&event, &running, &font, state);
    assert_true("start_frame_escape_keeps_menu", running && !state->editingStartFrame);

    char cwd[PATH_MAX], tmp[] = "/tmp/optic-menu-save-XXXXXX";
    if (getcwd(cwd, sizeof(cwd)) && mkdtemp(tmp) && chdir(tmp) == 0) {
        mkdir("data", 0700); mkdir("data/runtime", 0700);
        assert_true("checked_animation_save_success", SaveAnimationConfigChecked());
        assert_true("checked_scene_save_success", SaveSceneConfigChecked());
        unlink("data/runtime/animation_config.json");
        mkdir("data/runtime/animation_config.json", 0700);
        assert_true("checked_animation_save_reports_replace_failure", !SaveAnimationConfigChecked());
        assert_true("checked_all_save_reports_partial_failure", !SaveAllSettingsChecked());
        rmdir("data/runtime/animation_config.json");
        unlink("data/runtime/scene_config.json");
        rmdir("data/runtime"); rmdir("data");
        assert_true("save_test_restore_cwd", chdir(cwd) == 0);
        rmdir(tmp);
    } else assert_true("save_test_fixture_created", false);
    MenuScreenLayout screen;
    MenuButtonLayout buttons;
    const bool owns_ttf = !TTF_WasInit();
    if (owns_ttf) TTF_Init();
    font = TTF_OpenFont("third_party/codework_shared/assets/fonts/Lato-Regular.ttf", 18);
    assert_true("close_fixture_font", font != NULL);
    menu_layout_build_base(font, state, 1200, 900, &screen);
    menu_render_build_button_layout(font, state, &screen, &buttons);
    int draft_target = 42;
    menu_numeric_begin(&state->numericEdit, &draft_target, (MenuNumericSpec){0,5000,5,1,1,0});
    strcpy(state->numericEdit.text, "invalid");
    event.type = SDL_MOUSEBUTTONDOWN; event.button.button = SDL_BUTTON_LEFT;
    event.button.x = buttons.exitRect.x + 2; event.button.y = buttons.exitRect.y + 2;
    bool normal = false;
    menu_input_handle_mouse_click(&event, &running, &normal, NULL, &font, state);
    assert_true("close_discards_draft_without_applying", !running && !state->numericEdit.active && draft_target == 42);
    if (font) TTF_CloseFont(font);
    if (owns_ttf) TTF_Quit();
    animSettings = saved;
    free(state);
}

int run_test_ui_menu_contract_tests(void) {
    int before = test_support_failures();

    test_menu_compact_layout_and_long_caret();
    test_menu_focus_and_save_failures();
    test_menu_numeric_editing_and_constraints();
    test_menu_scroll_interactions();
    test_menu_batch_panel_click_starts_frame_dir_edit();
    test_menu_batch_panel_clear_button_updates_frame_count();
    test_menu_scene_project_summary_detects_project_files();
    test_object_editor_motion_panel_fits_material_space();
    test_object_editor_motion_overlay_reports_selected_path_readback();
    test_menu_scene_project_summary_keeps_loose_runtime_scene_separate();
    test_menu_layout_builds_non_overlapping_primary_zones();
    test_menu_workspace_registers_and_switches_nested_modules();
    test_menu_layout_keeps_manifest_list_inside_left_panel();
    test_menu_button_layout_respects_owned_screen_zones();
    test_menu_right_pane_layout_scales_with_panel_geometry();
    test_menu_button_layout_compacts_scene_mode_controls_in_3d();
    test_menu_resume_panel_layout_owns_frame_resume_controls();
    test_menu_resume_panel_clicks_preserve_frame_behavior();
    test_menu_button_layout_exposes_volume_controls_in_3d_only();
    test_menu_batch_panel_layout_centers_inside_batch_zone();
    test_menu_batch_panel_header_does_not_overlap_route_rows();
    test_integrator_catalog_menu_routes_by_space_mode();
    test_menu_caustic_product_cycle_and_runtime_plan();
    test_menu_slider_layout_includes_environment_control();
    test_menu_environment_settings_invalidate_and_read_back_runtime();
    test_menu_environment_override_survives_one_scene_restore();
    test_main_edit_build_identity_labels_windows_without_changing_standard();
    test_menu_environment_settings_rebuild_cached_static_scene();
    test_menu_slider_layout_routes_bounce_controls_by_space_mode();
    test_integrator_catalog_cycle_preserves_inactive_mode();
    test_menu_fit_text_to_width_supports_in_place_buffer();
    test_manifest_default_roots_expands_runtime_and_legacy_paths();
    test_stable_roots_accept_packaged_runtime_cwd();
    test_scene_source_catalog_collect_admits_runtime_and_manifest_lanes();
    test_scene_source_catalog_collects_all_runtime_scene_folders();
    test_menu_state_manifest_options_follow_configured_input_root();
    test_scene_source_catalog_runtime_discovery_is_filename_based();
    test_volume_source_catalog_collects_bundle_and_direct_files();
    test_menu_state_manifest_label_uses_scene_directory_name();
    test_menu_state_manifest_option_visibility_matrix();
    test_scene_source_ui_runtime_labels_expose_paired_and_disabled_state();
    test_volume_source_ui_labels_expose_auto_custom_and_none();
    test_depth_projection_scalars();
    test_object_editor_transparent_alpha_slider_is_labeled_transparency();

    return test_support_failures() - before;
}
