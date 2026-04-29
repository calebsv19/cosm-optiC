#include "ui/sdl_menu_render.h"

#include <math.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "ui/menu_panel_chrome.h"
#include "ui/shared_theme_font_adapter.h"
#include "engine/Render/render_pipeline.h"
#include "render/text_upload_policy.h"

#define MENU_WIDTH 1200
#define MENU_HEIGHT 900
#define MENU_MARGIN_X 30
#define MENU_MARGIN_Y 30
#define SLIDER_WIDTH 250
#define SLIDER_HEIGHT 6
#define SLIDER_SPACING 4
#define SLIDER_MARGIN_X (MENU_WIDTH - SLIDER_WIDTH - MENU_MARGIN_X - 40)
#define SLIDER_MARGIN_Y MENU_MARGIN_Y
#define SLIDER_SECTION_GAP 30

static int max_int(int a, int b) {
    return (a > b) ? a : b;
}

void menu_render_build_slider_layout(TTF_Font* font,
                                     MenuRuntimeState* state,
                                     const MenuScreenLayout* screen_layout,
                                     SliderLayout* out_layout) {
    SliderLayout layout = {0};
    int textHeight = 18;
    int valueReserve = 102;
    int sliderX = SLIDER_MARGIN_X;
    int sliderWidth = SLIDER_WIDTH;
    int rightLimit = MENU_WIDTH - MENU_MARGIN_X - 10;
    int panelTop = SLIDER_MARGIN_Y - 4;
    int panelHeight = 0;
    int visibleBottom;
    int scrollOffset;

    if (font) {
        textHeight = TTF_FontHeight(font);
        textHeight = ray_tracing_text_logical_pixels(getRenderContext() ? getRenderContext()->renderer : NULL,
                                                     textHeight);
    }
    if (textHeight < 12) textHeight = 12;
    if (screen_layout) {
        sliderX = screen_layout->sliderPanelRect.x + 12;
        rightLimit = screen_layout->sliderPanelRect.x + screen_layout->sliderPanelRect.w - 10;
        panelTop = screen_layout->sliderPanelRect.y;
        panelHeight = screen_layout->sliderPanelRect.h;
    }
    sliderWidth = rightLimit - sliderX - valueReserve;
    if (sliderWidth < 130) {
        sliderWidth = 130;
        sliderX = rightLimit - valueReserve - sliderWidth;
    }
    if (sliderX < SLIDER_MARGIN_X) sliderX = SLIDER_MARGIN_X;
    layout.trackHeight = max_int(SLIDER_HEIGHT, textHeight / 3);
    layout.knobWidth = max_int(8, (textHeight * 3) / 8);
    layout.knobHeight = layout.trackHeight + 4;
    if (panelHeight < 120) panelHeight = 120;
    layout.panelRect = (SDL_Rect){sliderX - 12, panelTop, rightLimit - (sliderX - 12), panelHeight};
    layout.nextY = panelTop + MENU_PANEL_CHROME_TITLE_BAND + 8;

    menu_state_sync_from_anim(state);
    const bool is_3d = animation_config_space_mode_clamp(animSettings.spaceMode) == SPACE_MODE_3D;

#define ADD_SLIDER(targetPtr, minVal, maxVal, labelText) \
    do { \
        if (layout.count < SDL_MENU_MAX_SLIDERS) { \
            int labelY_ = layout.nextY; \
            int trackY_ = labelY_ + textHeight + 2; \
            SDL_Rect track_ = { sliderX, trackY_, sliderWidth, layout.trackHeight }; \
            SDL_Rect hit_ = { sliderX, trackY_ - 6, sliderWidth, layout.trackHeight + 12 }; \
            layout.items[layout.count++] = (MenuSlider){ \
                targetPtr, minVal, maxVal, track_, hit_, \
                sliderX, labelY_, \
                sliderX + sliderWidth + 10, trackY_ - ((textHeight - layout.trackHeight) / 2), \
                labelText \
            }; \
            layout.nextY = trackY_ + layout.trackHeight + SLIDER_SPACING; \
        } \
    } while (0)

    if (!is_3d) {
        ADD_SLIDER(&animSettings.bounceLimit, 0, 100, "Bounce Limit");
    }
    ADD_SLIDER(&animSettings.frameLimit, 1, 5000, "Frame Limit");
    ADD_SLIDER(&animSettings.framesForTravel, 1, 5000, "Path Points");
    ADD_SLIDER(&animSettings.fps, 1, 240, "FPS");
    ADD_SLIDER(&sceneSettings.rays, 0, 10000, "Num Rays");
    ADD_SLIDER(&sceneSettings.windowWidth, 200, 4000, "Width");
    ADD_SLIDER(&sceneSettings.windowHeight, 200, 2400, "Height");
    ADD_SLIDER(&animSettings.tileSize, 4, 256, "Tile Size");
    if (!is_3d) {
        ADD_SLIDER(&state->rouletteSliderValue, 1, 2000, "Roulette Threshold");
    }
    ADD_SLIDER(&state->envSliderValue, 0, 255, "Environment");
    ADD_SLIDER(&state->lightIntensitySliderValue, 0, 2000, "Light Intensity");
    ADD_SLIDER(&state->lightDecaySoftnessSliderValue, 10, 1000, "Falloff Softness");
    ADD_SLIDER(&state->forwardDecaySliderValue,
               SDL_MENU_FORWARD_FALLOFF_DISTANCE_MIN,
               SDL_MENU_FORWARD_FALLOFF_DISTANCE_MAX,
               "Falloff Distance");
    if (is_3d) {
        ADD_SLIDER(&state->bounceDepth3DSliderValue,
                   RUNTIME_3D_BOUNCE_DEPTH_MIN,
                   RUNTIME_3D_BOUNCE_DEPTH_MAX,
                   "3D Bounce Depth");
        ADD_SLIDER(&state->rouletteThreshold3DSliderValue,
                   0,
                   100,
                   "3D Roulette Threshold");
        ADD_SLIDER(&state->secondaryDiffuseSamples3DSliderValue,
                   RUNTIME_3D_SECONDARY_SAMPLES_MIN,
                   RUNTIME_3D_SECONDARY_SAMPLES_MAX,
                   "3D Secondary Samples");
        ADD_SLIDER(&state->transmissionSamples3DSliderValue,
                   RUNTIME_3D_TRANSMISSION_SAMPLES_MIN,
                   RUNTIME_3D_TRANSMISSION_SAMPLES_MAX,
                   "3D Transmission Samples");
        ADD_SLIDER(&state->temporalFrames3DSliderValue,
                   RUNTIME_3D_TEMPORAL_FRAMES_MIN,
                   RUNTIME_3D_TEMPORAL_FRAMES_MAX,
                   "3D Temporal Frames");
        ADD_SLIDER(&state->renderScale3DSliderValue,
                   RUNTIME_3D_RENDER_SCALE_MIN,
                   RUNTIME_3D_RENDER_SCALE_MAX,
                   "3D Render Scale");
    }
    layout.nextY += SLIDER_SECTION_GAP;

    if (RayTracingIntegratorCatalog_BuildMenuState(&animSettings).showPathToggles) {
        ADD_SLIDER(&animSettings.pathSamplesPerPixel, 1, 128, "Path SPP");
        ADD_SLIDER(&animSettings.pathMaxDepth, 1, 16, "Path Depth");
    }
#undef ADD_SLIDER

    layout.contentBottomY = layout.nextY;
    visibleBottom = panelTop + panelHeight - 8;
    if (layout.contentBottomY > visibleBottom) {
        layout.maxScroll = (float)(layout.contentBottomY - visibleBottom);
    } else {
        layout.maxScroll = 0.0f;
    }

    layout.scroll = menu_state_slider_clamp_scroll(state->sliderScroll, layout.maxScroll);
    scrollOffset = (int)lround(layout.scroll);
    if (scrollOffset != 0) {
        for (size_t i = 0; i < layout.count; ++i) {
            layout.items[i].labelY -= scrollOffset;
            layout.items[i].trackRect.y -= scrollOffset;
            layout.items[i].hitRect.y -= scrollOffset;
            layout.items[i].valueY -= scrollOffset;
        }
    }
    state->sliderScroll = layout.scroll;
    state->sliderMaxScroll = layout.maxScroll;
    state->sliderPanelRect = layout.panelRect;

    if (out_layout) {
        *out_layout = layout;
    }
}

void menu_render_draw_sliders(SDL_Renderer* renderer,
                              TTF_Font* font,
                              MenuRuntimeState* state,
                              const SliderLayout* layout) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    if (!state || !layout) return;
    if (layout->panelRect.w > 0 && layout->panelRect.h > 0) {
        SDL_Rect panel = layout->panelRect;
        menu_panel_chrome_draw(renderer, font, &panel, "Render Settings", false);
        SDL_RenderSetClipRect(renderer, &panel);
    }

    for (size_t i = 0; i < layout->count; i++) {
        const MenuSlider* slider = &layout->items[i];
        if (slider->trackRect.y + slider->trackRect.h < layout->panelRect.y ||
            slider->trackRect.y > layout->panelRect.y + layout->panelRect.h) {
            continue;
        }
        RenderText(renderer, font, slider->labelX, slider->labelY, "%s", slider->label);

        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.button_fill.r, palette.button_fill.g,
                                   palette.button_fill.b, palette.button_fill.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        }
        SDL_Rect sliderBar = slider->trackRect;
        SDL_RenderFillRect(renderer, &sliderBar);

        int range = slider->max - slider->min;
        float percent = (range > 0) ? ((float)(*slider->value - slider->min) / (float)range) : 0.0f;
        if (percent < 0.0f) percent = 0.0f;
        if (percent > 1.0f) percent = 1.0f;

        int knobX = slider->trackRect.x + (int)(percent * slider->trackRect.w);
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.accent_primary.r, palette.accent_primary.g,
                                   palette.accent_primary.b, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        }
        SDL_Rect knob = {
            knobX - layout->knobWidth / 2,
            slider->trackRect.y - ((layout->knobHeight - slider->trackRect.h) / 2),
            layout->knobWidth,
            layout->knobHeight
        };
        SDL_RenderFillRect(renderer, &knob);

        if (slider->value == &state->rouletteSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.3f", state->rouletteSliderValue / 1000.0);
        } else if (slider->value == &state->envSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%d", state->envSliderValue);
        } else if (slider->value == &state->cacheWeightSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.2f", state->cacheWeightSliderValue / 100.0);
        } else if (slider->value == &state->lightIntensitySliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.2f", state->lightIntensitySliderValue / 100.0);
        } else if (slider->value == &state->lightDecaySoftnessSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.2f", state->lightDecaySoftnessSliderValue / 100.0);
        } else if (slider->value == &state->forwardDecaySliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%d", state->forwardDecaySliderValue);
        } else if (slider->value == &state->bounceDepth3DSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%d", state->bounceDepth3DSliderValue);
        } else if (slider->value == &state->rouletteThreshold3DSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.3f", state->rouletteThreshold3DSliderValue / 1000.0);
        } else if (slider->value == &state->secondaryDiffuseSamples3DSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%d", state->secondaryDiffuseSamples3DSliderValue);
        } else if (slider->value == &state->transmissionSamples3DSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%d", state->transmissionSamples3DSliderValue);
        } else if (slider->value == &state->temporalFrames3DSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%d", state->temporalFrames3DSliderValue);
        } else if (slider->value == &state->renderScale3DSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%dx", state->renderScale3DSliderValue);
        } else {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%d", *slider->value);
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);
    if (layout->maxScroll > 0.5f && layout->panelRect.w > 0 && layout->panelRect.h > 0) {
        SDL_Rect track = {
            layout->panelRect.x + layout->panelRect.w - 8,
            layout->panelRect.y + 6,
            4,
            layout->panelRect.h - 12
        };
        float ratio = (float)(layout->panelRect.h - 12) / (float)(layout->contentBottomY - layout->panelRect.y);
        int thumbH = (int)lround((float)track.h * ratio);
        int minThumb = max_int(20, layout->trackHeight + 4);
        if (thumbH < minThumb) thumbH = minThumb;
        if (thumbH > track.h) thumbH = track.h;
        float scrollRatio = (layout->maxScroll > 0.0f) ? (layout->scroll / layout->maxScroll) : 0.0f;
        int thumbY = track.y + (int)lround((float)(track.h - thumbH) * scrollRatio);
        SDL_Rect thumb = {track.x, thumbY, track.w, thumbH};
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r, palette.panel_border.g,
                                   palette.panel_border.b, 180);
            SDL_RenderFillRect(renderer, &track);
            SDL_SetRenderDrawColor(renderer,
                                   palette.accent_primary.r, palette.accent_primary.g,
                                   palette.accent_primary.b, 220);
            SDL_RenderFillRect(renderer, &thumb);
        } else {
            SDL_SetRenderDrawColor(renderer, 70, 70, 76, 170);
            SDL_RenderFillRect(renderer, &track);
            SDL_SetRenderDrawColor(renderer, 180, 180, 190, 220);
            SDL_RenderFillRect(renderer, &thumb);
        }
    }
}
