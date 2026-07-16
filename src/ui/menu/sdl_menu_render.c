#include "ui/sdl_menu_render.h"
#include "ui/menu/sdl_menu_render_internal.h"

#include <math.h>
#include <stdio.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "config/mesh_import_policy.h"
#include "editor/editor_mode_router.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/render_helper.h"
#include "render/text_draw.h"
#include "render/text_upload_policy.h"
#include "ui/menu_batch_panel.h"
#include "ui/menu_resume_panel.h"
#include "ui/menu_panel_chrome.h"
#include "ui/menu/workspace_authoring/ray_tracing_workspace_authoring_overlay.h"
#include "ui/shared_theme_font_adapter.h"

#define MENU_WIDTH 1200
#define MENU_HEIGHT 900
#define MENU_MARGIN_X 30
#define MENU_MARGIN_Y 30
#define TOGGLE_BUTTON_WIDTH 200
#define TOGGLE_BUTTON_HEIGHT 50
#define TOGGLE_BUTTON_MARGIN_X MENU_MARGIN_X
#define TOGGLE_BUTTON_MARGIN_Y MENU_MARGIN_Y
#define TOGGLE_BUTTON_SPACING 10
#define SUBSETTING_BUTTON_WIDTH 175
#define SUBSETTING_BUTTON_HEIGHT 40
#define SUBSETTING_BUTTON_MARGIN_X (MENU_MARGIN_X + 10)
#define BOTTOM_BUTTON_SPACING 10
#define BOTTOM_BUTTON_WIDTH_START 200
#define BOTTOM_BUTTON_HEIGHT_START 50
#define BOTTOM_BUTTON_MARGIN_Y_START (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_START)
#define BOTTOM_BUTTON_WIDTH_EXIT 180
#define BOTTOM_BUTTON_HEIGHT_EXIT 40
#define BOTTOM_BUTTON_MARGIN_X_EXIT (MENU_MARGIN_X)
#define BOTTOM_BUTTON_MARGIN_Y_EXIT (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_EXIT)
#define BOTTOM_BUTTON_WIDTH_PREVIEW 180
#define BOTTOM_BUTTON_HEIGHT_PREVIEW 40
#define BOTTOM_BUTTON_MARGIN_X_PREVIEW (MENU_MARGIN_X)
#define BOTTOM_BUTTON_MARGIN_Y_PREVIEW (BOTTOM_BUTTON_MARGIN_Y_EXIT - BOTTOM_BUTTON_HEIGHT_PREVIEW - BOTTOM_BUTTON_SPACING)
#define BOTTOM_BUTTON_WIDTH_RESTORE 180
#define BOTTOM_BUTTON_HEIGHT_RESTORE 40
#define BOTTOM_BUTTON_MARGIN_Y_RESTORE (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE)
#define BOTTOM_BUTTON_WIDTH_SAVE 100
#define BOTTOM_BUTTON_HEIGHT_SAVE 40
#define BOTTOM_BUTTON_MARGIN_Y_SAVE (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE)
#define SLIDER_WIDTH 250
#define SLIDER_HEIGHT 8
#define SLIDER_SPACING 10
#define SLIDER_MARGIN_X (MENU_WIDTH - SLIDER_WIDTH - MENU_MARGIN_X - 40)
#define SLIDER_MARGIN_Y MENU_MARGIN_Y
#define FORWARD_FALLOFF_BUTTON_WIDTH 200
#define FORWARD_FALLOFF_BUTTON_HEIGHT 40
#define FORWARD_FALLOFF_BUTTON_SPACING 10
#define SLIDER_SECTION_GAP 30
#define TILE_BUTTON_WIDTH 200
#define TILE_BUTTON_HEIGHT 40
#define INTEGRATOR_BUTTON_WIDTH 220
#define INTEGRATOR_BUTTON_HEIGHT 40
#define PATH_TOGGLE_WIDTH 180
#define PATH_TOGGLE_HEIGHT 35
#define PATH_TOGGLE_SPACING 10
#define LOAD_SCENE_BUTTON_WIDTH INTEGRATOR_BUTTON_WIDTH
#define LOAD_SCENE_BUTTON_HEIGHT 36
#define LOAD_SCENE_BUTTON_X TOGGLE_BUTTON_MARGIN_X
#define LOAD_SCENE_BUTTON_SPACING 20
#define ROOT_ROW_HEIGHT 34
#define ROOT_ROW_SPACING 8
#define ROOT_CTRL_BUTTON_W 56

static void render_centered_text_color(SDL_Renderer *renderer,
                                       TTF_Font *font,
                                       const SDL_Rect *rect,
                                       SDL_Color color,
                                       const char *text) {
    SDL_Rect textRect;
    int text_w = 0;
    int text_h = 0;
    if (!font || !rect || !text || !text[0]) return;
    if (!ray_tracing_text_measure_utf8(renderer, font, text, &text_w, &text_h)) return;
    textRect = (SDL_Rect){
        rect->x + (rect->w - text_w) / 2,
        rect->y + (rect->h - text_h) / 2,
        text_w,
        text_h
    };
    (void)ray_tracing_text_draw_utf8(renderer, font, text, color, &textRect);
}

static const char* menu_upscale_mode_button_label(void) {
    switch ((Runtime3DUpscaleMode)animSettings.upscaleMode3D) {
        case RUNTIME_3D_UPSCALE_MODE_NEAREST:
            return "Upscale: Nearest";
        case RUNTIME_3D_UPSCALE_MODE_BILINEAR:
            return "Upscale: Bilinear";
        case RUNTIME_3D_UPSCALE_MODE_OFF:
        default:
            return "Upscale: OFF";
    }
}

static const char* menu_environment_preset_button_label(void) {
    switch ((EnvironmentPreset)animSettings.environmentPreset) {
        case ENVIRONMENT_PRESET_NEUTRAL:
            return "Preset: Neutral";
        case ENVIRONMENT_PRESET_WARM_SKY:
            return "Preset: Warm";
        case ENVIRONMENT_PRESET_SKY:
        default:
            return "Preset: Sky";
    }
}

static const char* menu_environment_light_button_label(void) {
    if (animSettings.environmentLightMode == ENVIRONMENT_LIGHT_MODE_TOP_FILL) {
        return "Env: Top Fill";
    }
    if (animSettings.environmentLightMode == ENVIRONMENT_LIGHT_MODE_AMBIENT) {
        return "Env: Ambient";
    }
    return "Env: Off";
}

static const char* menu_forward_falloff_button_label(void) {
    if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR) {
        return "Falloff: Linear";
    }
    if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_NONE) {
        return "Falloff: None";
    }
    return "Falloff: Quadratic";
}

void menu_render_frame(SDL_Renderer* renderer,
                       TTF_Font* font,
                       MenuRuntimeState* state,
                       const RayTracingWorkspaceAuthoringHostState* authoring_host) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    MenuButtonLayout buttons;
    MenuScreenLayout screenLayout;
    SliderLayout sliderLayout;
    MenuBatchPanelLayout batchPanel;
    MenuResumePanelLayout resumePanel;
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus digestStatus;
    RenderContext* render_ctx = NULL;
    int menu_width = MENU_WIDTH;
    int menu_height = MENU_HEIGHT;
    if (!state) return;

    render_ctx = getRenderContext();
    if (render_ctx && render_ctx->window) {
        SDL_GetWindowSize(render_ctx->window, &menu_width, &menu_height);
    }
    menu_layout_build_base(font, state, menu_width, menu_height, &screenLayout);
    menu_render_build_button_layout(font, state, &screenLayout, &buttons);
    menu_layout_finalize_with_buttons(&screenLayout, &buttons, state);
    menu_render_build_slider_layout(font, state, &screenLayout, &sliderLayout);
    menu_batch_panel_build_layout(font, state, &screenLayout, &batchPanel);
    menu_resume_panel_build_layout(font, state, &screenLayout, &resumePanel);
    route = RayTracingModeBackend_ResolveRoute();
    digestStatus = RayTracingModeBackend_BuildSceneDigestStatus(&route);

    if (has_shared_palette) {
        render_set_clear_color(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        render_set_clear_color(renderer, 0, 0, 0, 255);
    }
    if (!render_begin_frame()) {
        return;
    }

    menu_panel_chrome_draw(renderer, font, &screenLayout.leftPanelRect, "Scene + Mode", true);
    menu_panel_chrome_draw(renderer, font, &screenLayout.workspace.frame_rect, NULL, false);
    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER) {
        menu_panel_chrome_draw(renderer, font, &screenLayout.centerControlsRect, "Render Recipe", false);
    }
    menu_panel_chrome_draw(renderer, font, &screenLayout.routeStackRect, "Runtime Route", false);
    menu_panel_chrome_draw(renderer, font, &screenLayout.bottomActionRowRect, NULL, false);
    {
        RayTracingMenuPaneSplitterVisual splitters[RAY_TRACING_MENU_PANE_SPLITTER_CAP];
        uint32_t splitter_count = ray_tracing_menu_pane_host_splitter_visuals(
            &state->menuPaneHost,
            splitters,
            RAY_TRACING_MENU_PANE_SPLITTER_CAP);
        uint32_t splitter_index;
        for (splitter_index = 0u; splitter_index < splitter_count; ++splitter_index) {
            SDL_Color splitter_color = has_shared_palette
                                           ? palette.panel_border
                                           : (SDL_Color){72, 76, 86, 255};
            if (splitters[splitter_index].hovered) {
                splitter_color = has_shared_palette
                                     ? palette.accent_primary
                                     : (SDL_Color){125, 155, 215, 255};
            }
            if (splitters[splitter_index].active) {
                splitter_color = has_shared_palette
                                     ? palette.accent_primary
                                     : (SDL_Color){170, 205, 255, 255};
            }
            SDL_SetRenderDrawColor(renderer,
                                   splitter_color.r,
                                   splitter_color.g,
                                   splitter_color.b,
                                   splitter_color.a);
            SDL_RenderFillRect(renderer, &splitters[splitter_index].rect);
        }
    }

    menu_render_draw_button_rect(renderer, font, &buttons.interactiveRect, "Interactive Mode", animSettings.interactiveMode);
    menu_render_draw_button_rect(renderer,
                                 font,
                                 &buttons.deepRenderRect,
                                 animation_config_space_mode_clamp(animSettings.spaceMode) == SPACE_MODE_3D
                                     ? "3D Render"
                                     : "Deep Render",
                                 animSettings.deepRenderMode);
    menu_render_draw_button_rect(renderer,
                                 font,
                                 &buttons.asyncDeepRenderRect,
                                 "Async Render",
                                 animSettings.asyncDeepRender);
    menu_render_draw_button_rect(renderer,
                                 font,
                                 &buttons.bounceRect,
                                 animation_config_space_mode_clamp(animSettings.spaceMode) == SPACE_MODE_3D
                                     ? "Bounce"
                                     : "Bounce Mode",
                                 animSettings.bounceMode);
    menu_render_draw_button_rect(renderer, font, &buttons.autoMp4Rect, "Auto MP4", animSettings.autoMP4);

    const RayTracingIntegratorMenuState integrator_menu =
        RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    menu_render_draw_button_rect(renderer, font, &buttons.integratorRect, integrator_menu.buttonLabel, true);

    if (buttons.showPathToggles) {
        menu_render_draw_button_rect(renderer, font, &buttons.pathRouletteRect,
                                     animSettings.pathRussianRoulette ? "Roulette: ON" : "Roulette: OFF",
                                     animSettings.pathRussianRoulette);
        const char* bsdfLabel = (animSettings.bsdfModel == 0) ? "BSDF: Lambert" : "BSDF: GGX";
        menu_render_draw_button_rect(renderer, font, &buttons.pathBsdfRect, bsdfLabel, animSettings.bsdfModel != 0);
    }

    char manifestLabel[160];
    const bool sceneListVisible =
        state->manifestDropdownOpen ||
        animation_config_space_mode_clamp(animSettings.spaceMode) == SPACE_MODE_3D;
    menu_render_format_manifest_button_label(state, manifestLabel, sizeof(manifestLabel));
    menu_render_draw_button_rect(renderer, font, &buttons.loadSceneRect, manifestLabel, sceneListVisible);
    if (sceneListVisible) {
        menu_render_draw_manifest_dropdown(renderer, font, state, &buttons, &screenLayout);
    } else {
        state->manifestPanelRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestListRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestScrollbarRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestScrollbarVisible = false;
        state->manifestThumbHeight = 0.0f;
        state->manifestTrackHeight = 0.0f;
    }
    menu_render_draw_root_row(renderer, font, &buttons.inputRootValueRect,
                              "Input Root",
                              state->editingInputRoot ? state->pathInputBuffer : animSettings.inputRoot,
                              state->editingInputRoot);
    menu_render_draw_button_rect(renderer, font, &buttons.inputRootEditRect, "Edit", state->editingInputRoot);
    menu_render_draw_button_rect(renderer, font, &buttons.inputRootFolderRect, "Folder", false);
    menu_render_draw_button_rect(renderer, font, &buttons.inputRootApplyRect, "Apply", false);
    menu_render_draw_root_row(renderer, font, &buttons.meshAssetRootValueRect,
                              "Mesh Root",
                              state->editingMeshAssetRoot ? state->pathInputBuffer : animSettings.meshAssetRoot,
                              state->editingMeshAssetRoot);
    menu_render_draw_button_rect(renderer, font, &buttons.meshAssetRootEditRect, "Edit", state->editingMeshAssetRoot);
    menu_render_draw_button_rect(renderer, font, &buttons.meshAssetRootFolderRect, "Folder", false);
    menu_render_draw_button_rect(renderer, font, &buttons.meshAssetRootApplyRect, "Apply", false);
    {
        char mesh_policy_label[96];
        ray_tracing_mesh_import_policy_format_label(&animSettings,
                                                    mesh_policy_label,
                                                    sizeof(mesh_policy_label));
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.meshImportNormalPolicyRect,
                                     mesh_policy_label,
                                     animSettings.meshImportNormalMode ==
                                         RAY_TRACING_MESH_IMPORT_NORMAL_MODE_CREASE_AWARE);
    }
    if (buttons.attachVolumeRect.w > 0 && buttons.attachVolumeRect.h > 0) {
        char volumeLabel[160];
        menu_render_format_volume_button_label(state, volumeLabel, sizeof(volumeLabel));
        menu_render_draw_button_rect(renderer, font, &buttons.attachVolumeRect, volumeLabel, state->volumeDropdownOpen);
        if (state->volumeDropdownOpen) {
            menu_render_draw_volume_dropdown(renderer, font, state, &buttons, &screenLayout);
        } else {
            state->volumePanelRect = (SDL_Rect){0, 0, 0, 0};
            state->volumeListRect = (SDL_Rect){0, 0, 0, 0};
            state->volumeScrollbarRect = (SDL_Rect){0, 0, 0, 0};
            state->volumeScrollbarVisible = false;
            state->volumeThumbHeight = 0.0f;
            state->volumeTrackHeight = 0.0f;
        }
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.volumeToggleRect,
                                     animSettings.volumeInteractionEnabled ? "Atmosphere: ON"
                                                                          : "Atmosphere: OFF",
                                     animSettings.volumeInteractionEnabled);
        menu_render_draw_button_rect(renderer, font, &buttons.volumeClearRect, "Clear Volume", false);
        if (state->volumeSummaryLine1[0]) {
            SDL_Color summary_color = has_shared_palette ? palette.text_muted
                                                         : (SDL_Color){210, 210, 210, 255};
            int summary_x = buttons.attachVolumeRect.x + 4;
            int summary_y = buttons.volumeToggleRect.y + buttons.volumeToggleRect.h + 8;
            int summary_w = screenLayout.leftPanelRect.x + screenLayout.leftPanelRect.w - summary_x - 18;
            char summary_fit[192];
            menu_render_fit_text_to_width(font,
                                          state->volumeSummaryLine1,
                                          summary_w,
                                          summary_fit,
                                          sizeof(summary_fit));
            menu_render_draw_text_color(renderer, font, summary_x, summary_y, summary_color, summary_fit);
            if (state->volumeSummaryLine2[0]) {
                menu_render_fit_text_to_width(font,
                                              state->volumeSummaryLine2,
                                              summary_w,
                                              summary_fit,
                                              sizeof(summary_fit));
                menu_render_draw_text_color(renderer,
                                            font,
                                            summary_x,
                                            summary_y + 16,
                                            summary_color,
                                            summary_fit);
            }
        }
    } else {
        state->volumePanelRect = (SDL_Rect){0, 0, 0, 0};
        state->volumeListRect = (SDL_Rect){0, 0, 0, 0};
        state->volumeScrollbarRect = (SDL_Rect){0, 0, 0, 0};
        state->volumeScrollbarVisible = false;
        state->volumeThumbHeight = 0.0f;
        state->volumeTrackHeight = 0.0f;
    }
    {
        int module_index;
        for (module_index = 0;
             module_index < MENU_WORKSPACE_MODULE_COUNT;
             ++module_index) {
            menu_render_draw_button_rect(
                renderer,
                font,
                &screenLayout.workspace.tab_rects[module_index],
                menu_workspace_module_label((MenuWorkspaceModule)module_index),
                state->menuWorkspaceHost.active_module ==
                    (MenuWorkspaceModule)module_index);
        }
    }
    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER) {
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.rendererLightingTabRect,
                                     "Lighting",
                                     state->rendererControlsTab == MENU_RENDERER_CONTROLS_LIGHTING);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.rendererPerformanceTabRect,
                                     "Performance",
                                     state->rendererControlsTab == MENU_RENDERER_CONTROLS_PERFORMANCE);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.rendererCausticsTabRect,
                                     "Caustics",
                                     state->rendererControlsTab == MENU_RENDERER_CONTROLS_CAUSTICS);
    }
    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER &&
        state->rendererControlsTab == MENU_RENDERER_CONTROLS_CAUSTICS) {
        char mode_label[96];
        char engine_label[96];
        (void)snprintf(mode_label,
                       sizeof(mode_label),
                       "Caustics: %s",
                       RuntimeCausticMode3D_Label(state->causticSettings.mode));
        (void)snprintf(engine_label,
                       sizeof(engine_label),
                       "Deposit: %s",
                       RuntimeCausticTransportEngine3D_Label(
                           state->causticSettings.transportEngine));
        menu_render_draw_button_rect(renderer, font, &buttons.causticModeRect,
                                     mode_label,
                                     state->causticSettings.mode != RUNTIME_CAUSTIC_MODE_OFF);
        menu_render_draw_button_rect(renderer, font, &buttons.causticEngineRect,
                                     engine_label,
                                     state->causticSettings.transportEngine ==
                                         RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP);
        menu_render_draw_button_rect(renderer, font, &buttons.causticSurfaceRect,
                                     state->causticSettings.surfaceCacheEnabled
                                         ? "Surface Cache: ON"
                                         : "Surface Cache: OFF",
                                     state->causticSettings.surfaceCacheEnabled);
        menu_render_draw_button_rect(renderer, font, &buttons.causticVolumeRect,
                                     state->causticSettings.volumeCacheEnabled
                                         ? "Volume Cache: ON"
                                         : "Volume Cache: OFF",
                                     state->causticSettings.volumeCacheEnabled);
        menu_render_draw_button_rect(renderer, font, &buttons.causticDebugSummaryRect,
                                     state->causticSettings.debugSummaryEnabled
                                         ? "Summary: ON"
                                         : "Summary: OFF",
                                     state->causticSettings.debugSummaryEnabled);
        menu_render_draw_button_rect(renderer, font, &buttons.causticDebugExportRect,
                                     state->causticSettings.debugExportEnabled
                                         ? "Debug Export: ON"
                                         : "Debug Export: OFF",
                                     state->causticSettings.debugExportEnabled);
    } else if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER &&
        state->rendererControlsTab == MENU_RENDERER_CONTROLS_PERFORMANCE) {
        const char* tileButtonLabel =
            animSettings.useTiledRenderer ? "Tile Renderer: ON" : "Tile Renderer: OFF";
        const char* previewLabel =
            animSettings.tilePreviewEnabled ? "Tile Preview: ON" : "Tile Preview: OFF";
        const char* denoiseLabel = animSettings.disneyDenoiseEnabled
                                       ? "Disney Denoise: ON"
                                       : "Disney Denoise: OFF";
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.tileRect,
                                     tileButtonLabel,
                                     animSettings.useTiledRenderer);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.tilePreviewRect,
                                     previewLabel,
                                     animSettings.tilePreviewEnabled);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.denoiseRect,
                                     denoiseLabel,
                                     animSettings.disneyDenoiseEnabled);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.upscaleModeRect,
                                     menu_upscale_mode_button_label(),
                                     animSettings.upscaleMode3D != RUNTIME_3D_UPSCALE_MODE_OFF);
    } else if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER) {
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.topFillRect,
                                     menu_environment_light_button_label(),
                                     animSettings.environmentLightMode != ENVIRONMENT_LIGHT_MODE_OFF);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.environmentPresetRect,
                                     menu_environment_preset_button_label(),
                                     animSettings.environmentPreset != ENVIRONMENT_PRESET_NEUTRAL);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.falloffRect,
                                     menu_forward_falloff_button_label(),
                                     animSettings.forwardFalloffMode != FORWARD_FALLOFF_MODE_QUADRATIC);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.environmentBackgroundModeRect,
                                     animSettings.environmentBackgroundBrightnessAuto
                                         ? "BG: Auto"
                                         : "BG: Manual",
                                     !animSettings.environmentBackgroundBrightnessAuto);
    }
    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RENDER) {
        menu_render_draw_slider_items(renderer,
                                      font,
                                      state,
                                      &buttons.rendererControlSliders,
                                      NULL);
    }

    menu_render_draw_sliders(renderer, font, state, &sliderLayout);
    {
        const bool high_cost =
            animSettings.transmissionSamples3D > 8 ||
            animSettings.secondaryDiffuseSamples3D > 16 ||
            animSettings.temporalFrames3D > 8 ||
            state->causticSettings.sampleBudget > 10000;
        SDL_Color recipe_color = has_shared_palette
                                     ? palette.text_muted
                                     : (SDL_Color){210, 210, 210, 255};
        SDL_Color cost_color = high_cost
                                   ? (SDL_Color){255, 196, 96, 255}
                                   : recipe_color;
        const int summary_x = screenLayout.sliderPanelRect.x + 12;
        const int summary_w = screenLayout.sliderPanelRect.w - 24;
        const int recipe_y = screenLayout.sliderPanelRect.y +
                             screenLayout.sliderPanelRect.h - 78;
        const int cost_y = recipe_y + 18;
        char summary[256];
        char fitted[256];

        snprintf(summary,
                 sizeof(summary),
                 "Recipe: %s | Caustics: %s",
                 RayTracingModeBackend_IntegratorStatusLabel(&route),
                 RuntimeCausticMode3D_Label(state->causticSettings.mode));
        menu_render_fit_text_to_width(font, summary, summary_w, fitted, sizeof(fitted));
        menu_render_draw_text_color(renderer,
                                    font,
                                    summary_x,
                                    recipe_y,
                                    recipe_color,
                                    fitted);
        snprintf(summary,
                 sizeof(summary),
                 "%s T=%d S=%d F=%d Photon=%d",
                 high_cost ? "Cost warning:" : "Cost:",
                 animSettings.transmissionSamples3D,
                 animSettings.secondaryDiffuseSamples3D,
                 animSettings.temporalFrames3D,
                 state->causticSettings.sampleBudget);
        menu_render_fit_text_to_width(font, summary, summary_w, fitted, sizeof(fitted));
        menu_render_draw_text_color(renderer,
                                    font,
                                    summary_x,
                                    cost_y,
                                    cost_color,
                                    fitted);
    }
    if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_OUTPUT) {
        menu_batch_panel_render(renderer, font, state, &batchPanel);
    } else if (state->menuWorkspaceHost.active_module == MENU_WORKSPACE_RUN) {
        menu_resume_panel_render(renderer, font, state, &resumePanel);
    }

    menu_render_draw_button_rect(renderer, font, &buttons.sceneEditorRect, "Scene Editor", false);
    int currentEditorMode = EditorModeRouter_ClampEditorMode(animSettings.editorMode,
                                                             AnimationUseFluidScene());
    if (currentEditorMode != animSettings.editorMode) {
        animSettings.editorMode = currentEditorMode;
    }
    const char* editorModeText = (currentEditorMode == EDITOR_MODE_PATH) ? "Editor: Path" :
                                 (currentEditorMode == EDITOR_MODE_OBJECT) ? "Editor: Scene" :
                                 (currentEditorMode == EDITOR_MODE_CAMERA) ? "Editor: Camera" :
                                 "Editor: Material";
    menu_render_draw_button_rect(renderer, font, &buttons.sceneModeRect, editorModeText, false);
    menu_render_draw_button_rect(renderer, font, &buttons.spaceModeRect,
                                 menu_space_mode_button_label(),
                                 animSettings.spaceMode == SPACE_MODE_3D);
    if (EditorModeRouter_IsControlled3D()) {
        SDL_Color scaffoldHintColor = {255, 220, 140, 240};
        SDL_Color digestHintColor = has_shared_palette
                                        ? (SDL_Color){palette.text_muted.r,
                                                      palette.text_muted.g,
                                                      palette.text_muted.b,
                                                      230}
                                        : (SDL_Color){210, 210, 210, 230};
        int hintX = screenLayout.sliderPanelRect.x + 12;
        int hintMaxWidth = screenLayout.sliderPanelRect.w - 24;
        int hintLine1Y = screenLayout.sliderPanelRect.y +
                         screenLayout.sliderPanelRect.h - 40;
        int hintLine2Y = hintLine1Y + 18;
        char hintLine1[192];
        char hintLine2[256];
        char hintFit[256];

        snprintf(hintLine1,
                 sizeof(hintLine1),
                 "%s | %s",
                 EditorModeRouter_RuntimeHintLabel(),
                 RayTracingModeBackend_IntegratorStatusLabel(&route));
        menu_render_fit_text_to_width(font, hintLine1, hintMaxWidth, hintFit, sizeof(hintFit));
        menu_render_draw_text_color(renderer,
                                    font,
                                    hintX,
                                    hintLine1Y,
                                    scaffoldHintColor,
                                    hintFit);

        if (!digestStatus.valid) {
            snprintf(hintLine2, sizeof(hintLine2), "3D digest pending runtime payload");
        } else if (digestStatus.hasSceneBounds) {
            snprintf(hintLine2,
                     sizeof(hintLine2),
                     "3D digest prim=%d plane=%d prism=%d bx=%.1f..%.1f by=%.1f..%.1f",
                     digestStatus.digestPrimitiveCount,
                     digestStatus.planePrimitiveCount,
                     digestStatus.rectPrismPrimitiveCount,
                     digestStatus.boundsMinX,
                     digestStatus.boundsMaxX,
                     digestStatus.boundsMinY,
                     digestStatus.boundsMaxY);
        } else {
            snprintf(hintLine2,
                     sizeof(hintLine2),
                     "3D digest prim=%d plane=%d prism=%d",
                     digestStatus.digestPrimitiveCount,
                     digestStatus.planePrimitiveCount,
                     digestStatus.rectPrismPrimitiveCount);
        }
        menu_render_fit_text_to_width(font, hintLine2, hintMaxWidth, hintFit, sizeof(hintFit));
        menu_render_draw_text_color(renderer,
                                    font,
                                    hintX,
                                    hintLine2Y,
                                    digestHintColor,
                                    hintFit);
    }

    menu_render_draw_button_rect(renderer, font, &buttons.saveRect, "Save", false);
    menu_render_draw_button_rect(renderer, font, &buttons.restoreRect, "Restore Defaults", false);
    menu_render_draw_button_rect(renderer, font, &buttons.previewRect, "Preview", animSettings.previewMode);
    menu_render_draw_button_rect(renderer, font, &buttons.exitRect, "Exit w/o Saving", false);

    if (buttons.startRect.w > 0 && buttons.startRect.h > 0) {
        if (has_shared_palette) {
            SDL_Color startFill = menu_render_ensure_highlight_fill_contrast(palette.accent_primary,
                                                                             palette.button_text,
                                                                             palette.panel_fill);
            SDL_SetRenderDrawColor(renderer,
                                   startFill.r, startFill.g,
                                   startFill.b, startFill.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 90, 220, 110, 255);
        }
        SDL_RenderFillRect(renderer, &buttons.startRect);
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r, palette.panel_border.g,
                                   palette.panel_border.b, palette.panel_border.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        }
        SDL_RenderDrawRect(renderer, &buttons.startRect);
        if (has_shared_palette) {
            SDL_Color startFill = menu_render_ensure_highlight_fill_contrast(palette.accent_primary,
                                                                             palette.button_text,
                                                                             palette.panel_fill);
            SDL_Color startText = menu_render_choose_readable_text(startFill, palette.button_text);
            render_centered_text_color(renderer, font, &buttons.startRect, startText, "Start");
        } else {
            RenderButtonText(renderer, buttons.startRect, "Start");
        }
    }

    Uint32 now = SDL_GetTicks();
    if (state->statusExpireMs > now) {
        double remaining = (double)(state->statusExpireMs - now);
        double frac = remaining / 1500.0;
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;
        SDL_Color c = state->statusColor;
        int alpha = (int)lrint((double)c.a * frac);
        if (alpha < 5) {
            state->statusExpireMs = 0;
        } else {
            c.a = (Uint8)alpha;
            int textX = buttons.saveRect.x + buttons.saveRect.w + 15;
            int textY = buttons.saveRect.y + (buttons.saveRect.h / 2) - 10;
            menu_render_draw_text_color(renderer, font, textX, textY, c, state->statusLabel);
        }
    }

    ray_tracing_workspace_authoring_overlay_draw(renderer,
                                                 font,
                                                 authoring_host,
                                                 menu_width,
                                                 menu_height,
                                                 NULL);
    render_end_frame();
}
