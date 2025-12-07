#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <json-c/json.h>
#include "editor/scene_editor.h"
#include "app/animation.h"  // Include the header where RunMainLoop() is declared
#include "config/config_manager.h"
#include "camera/camera.h"

// Window & Menu Layout
#define MENU_WIDTH 1000
#define MENU_HEIGHT 900
#define MENU_MARGIN_X 30   // Distance from left edge
#define MENU_MARGIN_Y 30   // Distance from top

// Main Toggle Buttons (Interactive vs. Deep Render)
#define TOGGLE_BUTTON_WIDTH 200
#define TOGGLE_BUTTON_HEIGHT 50
#define TOGGLE_BUTTON_MARGIN_X MENU_MARGIN_X + 0
#define TOGGLE_BUTTON_MARGIN_Y MENU_MARGIN_Y + 0
#define TOGGLE_BUTTON_TEXT_SIZE 24
#define TOGGLE_BUTTON_SPACING 10

// Deep Render Sub-Settings Buttons (reused for general left-column stack)
#define SUBSETTING_BUTTON_WIDTH 175
#define SUBSETTING_BUTTON_HEIGHT 40
#define SUBSETTING_BUTTON_MARGIN_X (MENU_MARGIN_X + 10)
#define SUBSETTING_BUTTON_MARGIN_Y (TOGGLE_BUTTON_MARGIN_Y + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING) * 2 + 15)
#define SUBSETTING_TEXT_SIZE 22
#define SUBSETTING_BUTTON_SPACING 10

// Editable Number Fields (Bounce Limit & Frame Limit)
#define VALUE_BOX_WIDTH 50
#define VALUE_BOX_HEIGHT 50
#define VALUE_BOX_MARGIN_X 20  // Right-aligned to corresponding button
#define VALUE_BOX_MARGIN_Y 10   // Slightly below the top of button
#define VALUE_BOX_TEXT_SIZE 20

// Bottom-Right Buttons (Start & Restore Defaults)
#define BOTTOM_BUTTON_SPACING 10

#define BOTTOM_BUTTON_WIDTH_START 200
#define BOTTOM_BUTTON_HEIGHT_START 50
#define BOTTOM_BUTTON_MARGIN_X_START (MENU_WIDTH - MENU_MARGIN_X - BOTTOM_BUTTON_WIDTH_START)
#define BOTTOM_BUTTON_MARGIN_Y_START (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_START - 0)

#define BOTTOM_BUTTON_WIDTH_EXIT 180
#define BOTTOM_BUTTON_HEIGHT_EXIT 40 
#define BOTTOM_BUTTON_MARGIN_X_EXIT (MENU_MARGIN_X)
#define BOTTOM_BUTTON_MARGIN_Y_EXIT (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE - 0)

#define BOTTOM_BUTTON_WIDTH_PREVIEW 180
#define BOTTOM_BUTTON_HEIGHT_PREVIEW 40
#define BOTTOM_BUTTON_MARGIN_X_PREVIEW (MENU_MARGIN_X)
#define BOTTOM_BUTTON_MARGIN_Y_PREVIEW (BOTTOM_BUTTON_MARGIN_Y_EXIT - BOTTOM_BUTTON_HEIGHT_PREVIEW - BOTTOM_BUTTON_SPACING)

#define BOTTOM_BUTTON_WIDTH_RESTORE 180
#define BOTTOM_BUTTON_HEIGHT_RESTORE 40
#define BOTTOM_BUTTON_MARGIN_X_RESTORE (BOTTOM_BUTTON_MARGIN_X_EXIT + BOTTOM_BUTTON_WIDTH_RESTORE + 10)
#define BOTTOM_BUTTON_MARGIN_Y_RESTORE (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE - 0)

#define BOTTOM_BUTTON_WIDTH_SAVE 100
#define BOTTOM_BUTTON_HEIGHT_SAVE 40
#define BOTTOM_BUTTON_MARGIN_X_SAVE (BOTTOM_BUTTON_MARGIN_X_RESTORE + BOTTOM_BUTTON_WIDTH_RESTORE + 10)
#define BOTTOM_BUTTON_MARGIN_Y_SAVE (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE - 0)

// Slider Layout Constants
#define SLIDER_WIDTH 250
#define SLIDER_HEIGHT 10
#define SLIDER_SPACING 20  // Vertical spacing between sliders

// Align all sliders to the top-right of the menu
#define SLIDER_MARGIN_X (MENU_WIDTH - SLIDER_WIDTH - MENU_MARGIN_X - 40)
#define SLIDER_MARGIN_Y MENU_MARGIN_Y  // Align with top of window

#define FORWARD_FALLOFF_BUTTON_WIDTH 200
#define FORWARD_FALLOFF_BUTTON_HEIGHT 40
#define FORWARD_FALLOFF_BUTTON_SPACING 10
#define FORWARD_FALLOFF_DISTANCE_MIN 100
#define FORWARD_FALLOFF_DISTANCE_MAX 40000

#define SLIDER_SECTION_GAP 30
#define MAX_MENU_SLIDERS 24

#define TILE_BUTTON_WIDTH 200
#define TILE_BUTTON_HEIGHT 40
#define TILE_BUTTON_X SLIDER_MARGIN_X
#define INTEGRATOR_BUTTON_WIDTH 220
#define INTEGRATOR_BUTTON_HEIGHT 40
#define INTEGRATOR_BUTTON_X TILE_BUTTON_X
#define PATH_TOGGLE_WIDTH 180
#define PATH_TOGGLE_HEIGHT 35
#define PATH_TOGGLE_X INTEGRATOR_BUTTON_X
#define PATH_TOGGLE_SPACING 10


// Default settings
#define DEFAULT_BOUNCE_LIMIT 10
#define DEFAULT_FRAME_LIMIT 50
#define DEFAULT_FRAME_FOR_TRAVEL 40

// Global variables for slider interaction
bool draggingSlider = false;  // Tracks whether a slider is being dragged
int *selectedSlider = NULL;   // Pointer to the currently selected slider value
int selectedSliderMin = 0;    // Min value of the selected slider
int selectedSliderMax = 100;  // Max value of the selected slider
int sliderStartX = 0;         // X position of the slider bar
int sliderWidth = 0;          // Width of the slider
char inputBuffer[10] = "";   // Stores user-typed numbers
bool editingBounce = false;  // Tracks if bounce limit is being edited
bool editingFrame = false;   // Tracks if frame limit is being edited
static int rouletteSliderValue = 10; // stores threshold * 1000
static int envSliderValue = 0;
static int cacheWeightSliderValue = 100;
static int lightIntensitySliderValue = 500;
static int lightDecaySoftnessSliderValue = 100; // maps to 0.1..10.0
static int forwardDecaySliderValue = 2000;
static int oldWindowWidth = 0;
static int oldWindowHeight = 0;
static Uint32 statusExpireMs = 0;
static SDL_Color statusColor = {255, 255, 255, 255};
static char statusLabel[16] = "";

static int ClampTileSizeMenu(int value) {
    if (value < 4) value = 4;
    if (value % 4 != 0) {
        value += 4 - (value % 4);
    }
    return value;
}

static void SyncRouletteSliderFromSettings(void) {
    if (draggingSlider && selectedSlider == &rouletteSliderValue) {
        return;
    }
    rouletteSliderValue = (int)lround(animSettings.rouletteThreshold * 1000.0);
    if (rouletteSliderValue < 1) rouletteSliderValue = 1;
}

static void SyncEnvSliderFromSettings(void) {
    if (draggingSlider && selectedSlider == &envSliderValue) {
        return;
    }
    envSliderValue = (int)lround(animSettings.environmentBrightness * 100.0);
    if (envSliderValue < 0) envSliderValue = 0;
    if (envSliderValue > 200) envSliderValue = 200;
}

static double ClampDouble(double value, double minV, double maxV) {
    if (value < minV) return minV;
    if (value > maxV) return maxV;
    return value;
}

static void SyncCacheSliderFromSettings(void) {
    if (draggingSlider && selectedSlider == &cacheWeightSliderValue) {
        return;
    }
    double weight = ClampDouble(animSettings.cacheContributionWeight, 0.0, 1.0);
    cacheWeightSliderValue = (int)lround(weight * 100.0);
}

static void SyncLightSliderFromSettings(void) {
    if (draggingSlider && selectedSlider == &lightIntensitySliderValue) {
        return;
    }
    double intensity = ClampDouble(animSettings.lightIntensity, 0.0, 20.0);
    lightIntensitySliderValue = (int)lround(intensity * 100.0);
}

static void SyncDecaySoftnessSliderFromSettings(void) {
    if (draggingSlider && selectedSlider == &lightDecaySoftnessSliderValue) {
        return;
    }
    double softness = ClampDouble(animSettings.lightDecaySoftness, 0.1, 10.0);
    lightDecaySoftnessSliderValue = (int)lround(softness * 100.0);
}

static void SyncForwardDecaySliderFromSettings(void) {
    if (draggingSlider && selectedSlider == &forwardDecaySliderValue) {
        return;
    }
    double distance = ClampDouble(animSettings.forwardDecay,
                                  FORWARD_FALLOFF_DISTANCE_MIN,
                                  FORWARD_FALLOFF_DISTANCE_MAX);
    forwardDecaySliderValue = (int)lround(distance);
}

static void SyncMenuSliderValues(void) {
    SyncRouletteSliderFromSettings();
    SyncEnvSliderFromSettings();
    SyncCacheSliderFromSettings();
    SyncLightSliderFromSettings();
    SyncDecaySoftnessSliderFromSettings();
    SyncForwardDecaySliderFromSettings();
}

static void ReanchorCameraAfterResize(int previousWidth, int previousHeight) {
    if (previousWidth <= 0 || previousHeight <= 0) return;
    if (sceneSettings.windowWidth <= 0 || sceneSettings.windowHeight <= 0) return;
    CameraPoint topLeft = CameraScreenToWorld(&sceneSettings.camera,
                                              0.0,
                                              0.0,
                                              previousWidth,
                                              previousHeight);
    sceneSettings.camera.x = topLeft.x + (sceneSettings.windowWidth * 0.5) / sceneSettings.camera.zoom;
    sceneSettings.camera.y = topLeft.y + (sceneSettings.windowHeight * 0.5) / sceneSettings.camera.zoom;
    sceneSettings.cameraMargin = CameraClampMarginPixels(sceneSettings.cameraMargin,
                                                        sceneSettings.windowWidth,
                                                        sceneSettings.windowHeight);
}

static void ApplySpecialSliderRules(int* target) {
    if (target == &animSettings.tileSize) {
        animSettings.tileSize = ClampTileSizeMenu(animSettings.tileSize);
    } else if (target == &rouletteSliderValue) {
        if (rouletteSliderValue < 1) rouletteSliderValue = 1;
        if (rouletteSliderValue > 2000) rouletteSliderValue = 2000;
        animSettings.rouletteThreshold = rouletteSliderValue / 1000.0;
    } else if (target == &animSettings.pathSamplesPerPixel) {
        if (animSettings.pathSamplesPerPixel < 1) animSettings.pathSamplesPerPixel = 1;
    } else if (target == &animSettings.pathMaxDepth) {
        if (animSettings.pathMaxDepth < 1) animSettings.pathMaxDepth = 1;
    } else if (target == &envSliderValue) {
        if (envSliderValue < 0) envSliderValue = 0;
        if (envSliderValue > 200) envSliderValue = 200;
        animSettings.environmentBrightness = envSliderValue / 100.0;
    } else if (target == &cacheWeightSliderValue) {
        if (cacheWeightSliderValue < 0) cacheWeightSliderValue = 0;
        if (cacheWeightSliderValue > 100) cacheWeightSliderValue = 100;
        animSettings.cacheContributionWeight = cacheWeightSliderValue / 100.0;
    } else if (target == &lightIntensitySliderValue) {
        if (lightIntensitySliderValue < 0) lightIntensitySliderValue = 0;
        if (lightIntensitySliderValue > 2000) lightIntensitySliderValue = 2000;
        animSettings.lightIntensity = lightIntensitySliderValue / 100.0;
    } else if (target == &lightDecaySoftnessSliderValue) {
        if (lightDecaySoftnessSliderValue < 10) lightDecaySoftnessSliderValue = 10;
        if (lightDecaySoftnessSliderValue > 1000) lightDecaySoftnessSliderValue = 1000;
        animSettings.lightDecaySoftness = lightDecaySoftnessSliderValue / 100.0;
    } else if (target == &forwardDecaySliderValue) {
        if (forwardDecaySliderValue < FORWARD_FALLOFF_DISTANCE_MIN) {
            forwardDecaySliderValue = FORWARD_FALLOFF_DISTANCE_MIN;
        }
        if (forwardDecaySliderValue > FORWARD_FALLOFF_DISTANCE_MAX) {
            forwardDecaySliderValue = FORWARD_FALLOFF_DISTANCE_MAX;
        }
        animSettings.forwardDecay = forwardDecaySliderValue;
    } else if (target == &sceneSettings.windowWidth || target == &sceneSettings.windowHeight) {
        if (sceneSettings.windowWidth < 200) sceneSettings.windowWidth = 200;
        if (sceneSettings.windowHeight < 200) sceneSettings.windowHeight = 200;
        if (sceneSettings.windowWidth % 2) sceneSettings.windowWidth += 1;
        if (sceneSettings.windowHeight % 2) sceneSettings.windowHeight += 1;
    }
}

bool InitializeMenu(SDL_Window** window, SDL_Renderer** renderer, TTF_Font** font) {
    // Initialize SDL video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL Initialization Failed: %s\n", SDL_GetError());
        return false;
    }

    // Initialize SDL_ttf for text rendering
    if (TTF_Init() == -1) {
        printf("TTF Initialization Failed: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    // Create SDL Window
    *window = SDL_CreateWindow("RayTracing Menu",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               MENU_WIDTH, MENU_HEIGHT, SDL_WINDOW_SHOWN);
    if (!*window) {
        printf("Window Creation Failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Create SDL Renderer
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer) {
        printf("Renderer Creation Failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Load Font
    *font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", TOGGLE_BUTTON_TEXT_SIZE);
    if (!*font) {
        printf("Font Loading Failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Load animation settings
    LoadAnimationConfig();
    LoadSceneConfig();
    animSettings.previewMode = false; // Preview is transient
    SyncMenuSliderValues();
    oldWindowWidth = sceneSettings.windowWidth;
    oldWindowHeight = sceneSettings.windowHeight;
    return true;  // Menu initialized successfully
}


void ResetAnimationSettings(void) {
    animSettings.interactiveMode = true;
    animSettings.deepRenderMode = false;
    animSettings.bounceMode = false;
    animSettings.autoMP4 = false;
    animSettings.bounceLimit = DEFAULT_BOUNCE_LIMIT;
    animSettings.frameLimit = DEFAULT_FRAME_LIMIT;
    animSettings.framesForTravel = DEFAULT_FRAME_FOR_TRAVEL;
    animSettings.fps = 30;
    animSettings.useTiledRenderer = false;
    animSettings.tileSize = 16;
    animSettings.rouletteThreshold = 0.01;
    animSettings.integratorMode = 0;
    animSettings.pathSamplesPerPixel = 4;
    animSettings.pathMaxDepth = 4;
    animSettings.pathDirectLighting = true;
    animSettings.pathRussianRoulette = true;
    animSettings.pathEnableMIS = true;
    animSettings.environmentBrightness = 0.0;
    animSettings.pathSeed = 1;
    animSettings.cacheContributionWeight = 1.0;
    animSettings.bsdfModel = 1;
    animSettings.lightIntensity = 5.0;
    animSettings.previewMode = false;
    animSettings.previewDuration = 5.0;
    double diag = hypot(sceneSettings.windowWidth, sceneSettings.windowHeight);
    animSettings.forwardDecay = (diag > 0.0) ? diag : 2000.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_QUADRATIC;
    SyncMenuSliderValues();
    oldWindowWidth = sceneSettings.windowWidth;
    oldWindowHeight = sceneSettings.windowHeight;
}

                     
void RenderText(SDL_Renderer *renderer, TTF_Font *font, int x, int y, const char *format, ...) {
    char buffer[32];  // Store formatted text
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, buffer, textColor);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                        
    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                 
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}

static void RenderTextColor(SDL_Renderer *renderer, TTF_Font *font, int x, int y, SDL_Color color, const char *text) {
    if (!text || !*text) return;
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, color);
    if (!textSurface) return;
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        return;
    }
    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}
            

void RenderButton(SDL_Renderer *renderer, TTF_Font *font, int x, int y, int width, int height, const char *text, bool active) {
    SDL_Rect rect = {x, y, width, height};

    // Toggle button color based on active state
    SDL_SetRenderDrawColor(renderer, active ? 0 : 100, active ? 255 : 100, active ? 0 : 100, 255);
    SDL_RenderFillRect(renderer, &rect);

    // Render text centered inside the button
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

    int textX = x + (width - textSurface->w) / 2;  // Center text horizontally
    int textY = y + (height - textSurface->h) / 2; // Center text vertically

    SDL_Rect textRect = {textX, textY, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}
typedef struct {
    int *value;
    int min, max, x, y, width;
    const char *label;
} MenuSlider;

typedef struct {
    MenuSlider items[MAX_MENU_SLIDERS];
    size_t count;
    int nextY;
} SliderLayout;

static SliderLayout BuildSliderLayout(void) {
    SliderLayout layout = {0};
    layout.nextY = SLIDER_MARGIN_Y;
    SyncMenuSliderValues();
    const int sliderX = SLIDER_MARGIN_X;
    const int sliderWidth = SLIDER_WIDTH;

#define ADD_SLIDER(targetPtr, minVal, maxVal, labelText) \
    do { \
        if (layout.count < MAX_MENU_SLIDERS) { \
            layout.items[layout.count++] = (MenuSlider){ targetPtr, minVal, maxVal, sliderX, layout.nextY, sliderWidth, labelText }; \
            layout.nextY += SLIDER_HEIGHT + SLIDER_SPACING; \
        } \
    } while (0)

    ADD_SLIDER(&animSettings.bounceLimit, 0, 100, "Bounce Limit");
    ADD_SLIDER(&animSettings.frameLimit, 1, 5000, "Frame Limit");
    ADD_SLIDER(&animSettings.framesForTravel, 1, 5000, "Path Points");
    ADD_SLIDER(&animSettings.fps, 1, 240, "FPS");
    ADD_SLIDER(&sceneSettings.rays, 0, 10000, "Num Rays");
    ADD_SLIDER(&sceneSettings.windowWidth, 200, 4000, "Width");
    ADD_SLIDER(&sceneSettings.windowHeight, 200, 2400, "Height");
    ADD_SLIDER(&animSettings.tileSize, 4, 256, "Tile Size");
    ADD_SLIDER(&rouletteSliderValue, 1, 2000, "Roulette Threshold");
    ADD_SLIDER(&lightIntensitySliderValue, 0, 2000, "Light Intensity");
    ADD_SLIDER(&lightDecaySoftnessSliderValue, 10, 1000, "Falloff Softness");
    ADD_SLIDER(&forwardDecaySliderValue, FORWARD_FALLOFF_DISTANCE_MIN, FORWARD_FALLOFF_DISTANCE_MAX, "Falloff Distance");
    layout.nextY += SLIDER_SECTION_GAP;

    if (animSettings.integratorMode == 1) {
        ADD_SLIDER(&animSettings.pathSamplesPerPixel, 1, 128, "Path SPP");
        ADD_SLIDER(&animSettings.pathMaxDepth, 1, 16, "Path Depth");
        ADD_SLIDER(&envSliderValue, 0, 200, "Environment %");
        ADD_SLIDER(&cacheWeightSliderValue, 0, 100, "Cache Weight %");
    }
#undef ADD_SLIDER
    return layout;
}

static void RenderSliders(SDL_Renderer* renderer, TTF_Font* font, const SliderLayout* layout) {
    if (!layout) return;
    for (size_t i = 0; i < layout->count; i++) {
        const MenuSlider* slider = &layout->items[i];
        int textMarginX = 5;
        RenderText(renderer, font, slider->x - 150, slider->y - textMarginX, "%s", slider->label);

        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect sliderBar = {slider->x, slider->y, slider->width, SLIDER_HEIGHT};
        SDL_RenderFillRect(renderer, &sliderBar);

        int range = slider->max - slider->min;
        float percent = (range > 0) ? ((float)(*slider->value - slider->min) / (float)range) : 0.0f;
        if (percent < 0.0f) percent = 0.0f;
        if (percent > 1.0f) percent = 1.0f;

        int knobX = slider->x + (int)(percent * slider->width);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect knob = {knobX - 5, slider->y - 5, 10, 20};
        SDL_RenderFillRect(renderer, &knob);

        if (slider->value == &rouletteSliderValue) {
            RenderText(renderer, font, slider->x + slider->width + 10, slider->y - textMarginX,
                       "%.3f", rouletteSliderValue / 1000.0);
        } else if (slider->value == &envSliderValue) {
            RenderText(renderer, font, slider->x + slider->width + 10, slider->y - textMarginX,
                       "%.2f", envSliderValue / 100.0);
        } else if (slider->value == &cacheWeightSliderValue) {
            RenderText(renderer, font, slider->x + slider->width + 10, slider->y - textMarginX,
                       "%.2f", cacheWeightSliderValue / 100.0);
        } else if (slider->value == &lightIntensitySliderValue) {
            RenderText(renderer, font, slider->x + slider->width + 10, slider->y - textMarginX,
                       "%.2f", lightIntensitySliderValue / 100.0);
        } else if (slider->value == &lightDecaySoftnessSliderValue) {
            RenderText(renderer, font, slider->x + slider->width + 10, slider->y - textMarginX,
                       "%.2f", lightDecaySoftnessSliderValue / 100.0);
        } else if (slider->value == &forwardDecaySliderValue) {
            RenderText(renderer, font, slider->x + slider->width + 10, slider->y - textMarginX,
                       "%d", forwardDecaySliderValue);
        } else {
            RenderText(renderer, font, slider->x + slider->width + 10, slider->y - textMarginX,
                       "%d", *slider->value);
        }
    }
}

void RenderMenu(SDL_Renderer* renderer, TTF_Font* font) {  
    SliderLayout sliderLayout = BuildSliderLayout();
    int centerX = TOGGLE_BUTTON_MARGIN_X + TOGGLE_BUTTON_WIDTH + 60; // middle column anchor between left and sliders
    int falloffButtonY = TOGGLE_BUTTON_MARGIN_Y + 10;
    int tileButtonY = falloffButtonY + FORWARD_FALLOFF_BUTTON_HEIGHT + FORWARD_FALLOFF_BUTTON_SPACING;
    int integratorButtonY = SUBSETTING_BUTTON_MARGIN_Y + 2 * (SUBSETTING_BUTTON_HEIGHT + SUBSETTING_BUTTON_SPACING) + 10;
    int pathToggleDirectY = integratorButtonY + INTEGRATOR_BUTTON_HEIGHT + 10;
    int pathToggleRRY = pathToggleDirectY + PATH_TOGGLE_HEIGHT + PATH_TOGGLE_SPACING;
    int pathToggleMISY = pathToggleRRY + PATH_TOGGLE_HEIGHT + PATH_TOGGLE_SPACING;
    int pathToggleBSDFY = pathToggleMISY + PATH_TOGGLE_HEIGHT + PATH_TOGGLE_SPACING;

    // Clear the screen with a black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    // Main mode buttons (left column anchor)
    RenderButton(renderer, font, TOGGLE_BUTTON_MARGIN_X, TOGGLE_BUTTON_MARGIN_Y,
                 TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT, "Interactive Mode", animSettings.interactiveMode);
    
    RenderButton(renderer, font, TOGGLE_BUTTON_MARGIN_X,
                 TOGGLE_BUTTON_MARGIN_Y + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING),
                 TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT, "Deep Render", animSettings.deepRenderMode);

    // Left column: bounce/autoMP4 always visible, integrator + path extras
    RenderButton(renderer, font, SUBSETTING_BUTTON_MARGIN_X, SUBSETTING_BUTTON_MARGIN_Y,
                 SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT, "Bounce Mode", animSettings.bounceMode);

    RenderButton(renderer, font, SUBSETTING_BUTTON_MARGIN_X,
                 SUBSETTING_BUTTON_MARGIN_Y + (SUBSETTING_BUTTON_SPACING + SUBSETTING_BUTTON_HEIGHT),
                 SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT, "Auto MP4", animSettings.autoMP4);

    const char* integratorLabel = "Integrator: Forward Light";
    if (animSettings.integratorMode == 1) integratorLabel = "Integrator: Hybrid";
    else if (animSettings.integratorMode == 2) integratorLabel = "Integrator: Disney Path";
    else if (animSettings.integratorMode == 3) integratorLabel = "Integrator: Direct Light";
    RenderButton(renderer, font, TOGGLE_BUTTON_MARGIN_X, integratorButtonY,
                 INTEGRATOR_BUTTON_WIDTH, INTEGRATOR_BUTTON_HEIGHT, integratorLabel, true);

    if (animSettings.integratorMode == 1) {
        RenderButton(renderer, font, TOGGLE_BUTTON_MARGIN_X, pathToggleDirectY,
                     PATH_TOGGLE_WIDTH, PATH_TOGGLE_HEIGHT,
                     animSettings.pathDirectLighting ? "Direct Light: ON" : "Direct Light: OFF",
                     animSettings.pathDirectLighting);
        RenderButton(renderer, font, TOGGLE_BUTTON_MARGIN_X, pathToggleRRY,
                     PATH_TOGGLE_WIDTH, PATH_TOGGLE_HEIGHT,
                     animSettings.pathRussianRoulette ? "Roulette: ON" : "Roulette: OFF",
                     animSettings.pathRussianRoulette);
        RenderButton(renderer, font, TOGGLE_BUTTON_MARGIN_X, pathToggleMISY,
                     PATH_TOGGLE_WIDTH, PATH_TOGGLE_HEIGHT,
                     animSettings.pathEnableMIS ? "MIS: ON" : "MIS: OFF",
                     animSettings.pathEnableMIS);
        const char* bsdfLabel = (animSettings.bsdfModel == 0) ? "BSDF: Lambert" : "BSDF: GGX";
        RenderButton(renderer, font, TOGGLE_BUTTON_MARGIN_X, pathToggleBSDFY,
                     PATH_TOGGLE_WIDTH, PATH_TOGGLE_HEIGHT,
                     bsdfLabel,
                     animSettings.bsdfModel != 0);
    }
                 
    const char* falloffLabel = "Quadratic (1/r^2)";
    if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR) {
        falloffLabel = "Linear (1/r)";
    } else if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_NONE) {
        falloffLabel = "Falloff: None";
    }
    RenderButton(renderer, font, centerX, falloffButtonY,
                 FORWARD_FALLOFF_BUTTON_WIDTH, FORWARD_FALLOFF_BUTTON_HEIGHT,
                 falloffLabel, animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR);

    const char* tileButtonLabel = animSettings.useTiledRenderer ? "Tile Renderer: ON" : "Tile Renderer: OFF";
    RenderButton(renderer, font, centerX, tileButtonY,
                 TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT, tileButtonLabel, animSettings.useTiledRenderer);

    // Right column: Scene Editor + mode above Start
    int sceneBtnX = BOTTOM_BUTTON_MARGIN_X_START;
    int sceneBtnY = BOTTOM_BUTTON_MARGIN_Y_START - (BOTTOM_BUTTON_HEIGHT_START + 8);
    int sceneBtnW = BOTTOM_BUTTON_WIDTH_START;
    int sceneBtnH = BOTTOM_BUTTON_HEIGHT_START;
    int editorModeY = sceneBtnY - (sceneBtnH + 6);
    RenderButton(renderer, font, sceneBtnX, sceneBtnY,
                 sceneBtnW, sceneBtnH,
                 "Scene Editor", false);
    const char* editorModeText = (animSettings.editorMode == 0) ? "Path" :
                                 (animSettings.editorMode == 1) ? "Scene" : "Camera";
    RenderButton(renderer, font, sceneBtnX,
                 editorModeY,
                 sceneBtnW, sceneBtnH, editorModeText, false);

    // Render Bottom Buttons
    RenderButton(renderer, font, BOTTOM_BUTTON_MARGIN_X_SAVE, BOTTOM_BUTTON_MARGIN_Y_SAVE,
                 BOTTOM_BUTTON_WIDTH_SAVE, BOTTOM_BUTTON_HEIGHT_SAVE, "Save", false);
    RenderButton(renderer, font, BOTTOM_BUTTON_MARGIN_X_RESTORE, BOTTOM_BUTTON_MARGIN_Y_RESTORE,
                 BOTTOM_BUTTON_WIDTH_RESTORE, BOTTOM_BUTTON_HEIGHT_RESTORE, "Restore Defaults", false);
    RenderButton(renderer, font, BOTTOM_BUTTON_MARGIN_X_PREVIEW, BOTTOM_BUTTON_MARGIN_Y_PREVIEW,
                 BOTTOM_BUTTON_WIDTH_PREVIEW, BOTTOM_BUTTON_HEIGHT_PREVIEW, "Preview", animSettings.previewMode);
    RenderButton(renderer, font, BOTTOM_BUTTON_MARGIN_X_EXIT, BOTTOM_BUTTON_MARGIN_Y_EXIT,
                 BOTTOM_BUTTON_WIDTH_EXIT, BOTTOM_BUTTON_HEIGHT_EXIT, "Exit w/o Saving", false); 
    // Start button with subtle green tint
    SDL_Rect startRect = {BOTTOM_BUTTON_MARGIN_X_START, BOTTOM_BUTTON_MARGIN_Y_START,
                          BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START};
    SDL_SetRenderDrawColor(renderer, 90, 220, 110, 255);
    SDL_RenderFillRect(renderer, &startRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &startRect);
    RenderButtonText(renderer, startRect, "Start");
   

    RenderSliders(renderer, font, &sliderLayout);

    // Status label near Save button
    Uint32 now = SDL_GetTicks();
    if (statusExpireMs > now) {
        double remaining = (double)(statusExpireMs - now);
        double frac = remaining / 1500.0; // shorter lifetime to avoid tail flash
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;
        SDL_Color c = statusColor;
        int alpha = (int)lrint((double)c.a * frac);
        if (alpha < 5) {
            statusExpireMs = 0; // fully cleared
        } else {
            c.a = (Uint8)alpha;
            int textX = BOTTOM_BUTTON_MARGIN_X_SAVE + BOTTOM_BUTTON_WIDTH_SAVE + 15;
            int textY = BOTTOM_BUTTON_MARGIN_Y_SAVE + (BOTTOM_BUTTON_HEIGHT_SAVE / 2) - 10;
            RenderTextColor(renderer, font, textX, textY, c, statusLabel);
        }
    }

    // Present the updated UI 
    SDL_RenderPresent(renderer);
} 

void HandleKeyPress(SDL_Event* event, bool* running) {
    switch (event->key.keysym.sym) {
        case SDLK_BACKSPACE:
            if ((editingBounce || editingFrame) && strlen(inputBuffer) > 0) {
                inputBuffer[strlen(inputBuffer) - 1] = '\0';
            }
            break;
        case SDLK_RETURN:
            if (strlen(inputBuffer) > 0) {
                int newValue = atoi(inputBuffer);
                if (editingBounce) animSettings.bounceLimit = newValue;
                if (editingFrame) animSettings.frameLimit = newValue;
            }
            editingBounce = false;
            editingFrame = false;
            inputBuffer[0] = '\0'; 
            break;
        case SDLK_ESCAPE:
            *running = false;
            break;
        case SDLK_i:
            animSettings.interactiveMode = true;
            animSettings.deepRenderMode = false;
            break;
        case SDLK_d:
            animSettings.deepRenderMode = true;
            animSettings.interactiveMode = false;
            break;
        case SDLK_b:
            if (animSettings.deepRenderMode) animSettings.bounceMode = !animSettings.bounceMode;
            break;
        case SDLK_m:
            if (animSettings.deepRenderMode) animSettings.autoMP4 = !animSettings.autoMP4;
            break;
        case SDLK_p:
            if (animSettings.deepRenderMode){
		// ✅ Initialize the Scene Editor and Run the Loop
                SceneEditor editor = {0};  // Zero-initialize struct
                InitializeSceneEditor(&editor);
                editor.running = true;
                SceneEditorLoop(&editor);
                *running = false;  // Exit menu after scene starts
	    }
            break;
        case SDLK_r:
            animSettings.interactiveMode = true;
            animSettings.deepRenderMode = false;
            animSettings.bounceMode = false;
            animSettings.autoMP4 = false;
            animSettings.bounceLimit = DEFAULT_BOUNCE_LIMIT;
            animSettings.frameLimit = DEFAULT_FRAME_LIMIT;
            animSettings.framesForTravel = DEFAULT_FRAME_FOR_TRAVEL;
            break;
    }
}

void HandleMouseMotion(SDL_Event* event) {
    if (!draggingSlider || !selectedSlider) return;  // Ensure a slider is being dragged

    int x = event->motion.x;

    // Calculate new slider value based on mouse position
    bool adjustCamera = (selectedSlider == &sceneSettings.windowWidth ||
                         selectedSlider == &sceneSettings.windowHeight);
    int prevWidth = sceneSettings.windowWidth;
    int prevHeight = sceneSettings.windowHeight;

    float percent = (float)(x - sliderStartX) / sliderWidth;
    int newValue = selectedSliderMin + percent * (selectedSliderMax - selectedSliderMin);

    // Clamp value within range
    if (newValue < selectedSliderMin) newValue = selectedSliderMin;
    if (newValue > selectedSliderMax) newValue = selectedSliderMax;

    *selectedSlider = newValue;  // Update the selected value
    ApplySpecialSliderRules(selectedSlider);
    if (adjustCamera &&
        (prevWidth != sceneSettings.windowWidth || prevHeight != sceneSettings.windowHeight)) {
        ReanchorCameraAfterResize(prevWidth, prevHeight);
        oldWindowWidth = sceneSettings.windowWidth;
        oldWindowHeight = sceneSettings.windowHeight;
    }
}

static void HandleSliderClick(SDL_Event* event, const SliderLayout* layout) {
    if (!layout) return;
    int x = event->button.x, y = event->button.y;
    for (size_t i = 0; i < layout->count; i++) {
        const MenuSlider* slider = &layout->items[i];
        if (x > slider->x && x < slider->x + slider->width &&
            y > slider->y - 5 && y < slider->y + 15) {
            
            // Activate dragging mode
            draggingSlider = true;
            selectedSlider = slider->value;
            selectedSliderMin = slider->min;
            selectedSliderMax = slider->max;
            sliderStartX = slider->x;
            sliderWidth = slider->width;

            bool adjustCamera = (selectedSlider == &sceneSettings.windowWidth ||
                                 selectedSlider == &sceneSettings.windowHeight);
            int prevWidth = sceneSettings.windowWidth;
            int prevHeight = sceneSettings.windowHeight;

            // Immediately update slider value to where the user clicked
            float percent = (float)(x - slider->x) / slider->width;
            int newValue = slider->min + percent * (slider->max - slider->min);

            // Clamp value within range
            if (newValue < slider->min) newValue = slider->min;
            if (newValue > slider->max) newValue = slider->max;

            *selectedSlider = newValue;  // Apply updated value instantly
            ApplySpecialSliderRules(selectedSlider);
            if (adjustCamera &&
                (prevWidth != sceneSettings.windowWidth || prevHeight != sceneSettings.windowHeight)) {
                ReanchorCameraAfterResize(prevWidth, prevHeight);
                oldWindowWidth = sceneSettings.windowWidth;
                oldWindowHeight = sceneSettings.windowHeight;
            }
            return;
        }
    }
}


void HandleMouseClick(SDL_Event* event, bool* running, bool* menuExitedNormally, SDL_Renderer* renderer) {
    (void)renderer;
    SliderLayout layout = BuildSliderLayout();
    HandleSliderClick(event, &layout);

    int x = event->button.x, y = event->button.y;
    int falloffButtonY = TOGGLE_BUTTON_MARGIN_Y + 10;
    int tileButtonY = falloffButtonY + FORWARD_FALLOFF_BUTTON_HEIGHT + FORWARD_FALLOFF_BUTTON_SPACING;
    int integratorButtonY = SUBSETTING_BUTTON_MARGIN_Y + 2 * (SUBSETTING_BUTTON_HEIGHT + SUBSETTING_BUTTON_SPACING) + 10;
    int pathToggleDirectY = integratorButtonY + INTEGRATOR_BUTTON_HEIGHT + 10;
    int pathToggleRRY = pathToggleDirectY + PATH_TOGGLE_HEIGHT + PATH_TOGGLE_SPACING;
    int pathToggleMISY = pathToggleRRY + PATH_TOGGLE_HEIGHT + PATH_TOGGLE_SPACING;
    int pathToggleBSDFY = pathToggleMISY + PATH_TOGGLE_HEIGHT + PATH_TOGGLE_SPACING;
                
    // Toggle Interactive Mode
    if (x > TOGGLE_BUTTON_MARGIN_X && x < TOGGLE_BUTTON_MARGIN_X + TOGGLE_BUTTON_WIDTH &&
        y > TOGGLE_BUTTON_MARGIN_Y && y < TOGGLE_BUTTON_MARGIN_Y + TOGGLE_BUTTON_HEIGHT) {
        animSettings.interactiveMode = true;
        animSettings.deepRenderMode = false;
        return;
    }
        
    // Toggle Deep Render Mode
    if (x > TOGGLE_BUTTON_MARGIN_X && x < TOGGLE_BUTTON_MARGIN_X + TOGGLE_BUTTON_WIDTH &&
        y > TOGGLE_BUTTON_MARGIN_Y + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING) &&
        y < TOGGLE_BUTTON_MARGIN_Y + TOGGLE_BUTTON_HEIGHT + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING)) {
        animSettings.deepRenderMode = true;
        animSettings.interactiveMode = false;
        return;
    }
            
    // Deep Render Mode Options (Only Available When Deep Render is ON)
    if (animSettings.deepRenderMode) {
        if (x > SUBSETTING_BUTTON_MARGIN_X && x < SUBSETTING_BUTTON_MARGIN_X + SUBSETTING_BUTTON_WIDTH &&
            y > SUBSETTING_BUTTON_MARGIN_Y && y < SUBSETTING_BUTTON_MARGIN_Y + SUBSETTING_BUTTON_HEIGHT) {
            animSettings.bounceMode = !animSettings.bounceMode;
            return;
        }
        
        if (x > SUBSETTING_BUTTON_MARGIN_X && x < SUBSETTING_BUTTON_MARGIN_X + SUBSETTING_BUTTON_WIDTH &&
            y > SUBSETTING_BUTTON_MARGIN_Y + (SUBSETTING_BUTTON_HEIGHT + SUBSETTING_BUTTON_SPACING) &&
            y < SUBSETTING_BUTTON_MARGIN_Y + SUBSETTING_BUTTON_HEIGHT + (SUBSETTING_BUTTON_HEIGHT + 
			SUBSETTING_BUTTON_SPACING)) {
            animSettings.autoMP4 = !animSettings.autoMP4;
            return;
        }
            
        int sceneBtnX = BOTTOM_BUTTON_MARGIN_X_START;
        int sceneBtnY = BOTTOM_BUTTON_MARGIN_Y_START - (BOTTOM_BUTTON_HEIGHT_START + 8);
        int sceneBtnW = BOTTOM_BUTTON_WIDTH_START;
        int sceneBtnH = BOTTOM_BUTTON_HEIGHT_START;
        int editorModeY = sceneBtnY - (sceneBtnH + 6);

        // Launch Scene Editor
        if (x > sceneBtnX && x < sceneBtnX + sceneBtnW &&
            y > sceneBtnY && y < sceneBtnY + sceneBtnH) {
            SceneEditor editor = {0};  // Zero-initialize struct
            InitializeSceneEditor(&editor);
            editor.running = true;
            SceneEditorLoop(&editor);
            return;
        }

	// Toggle Scene Editor Mode
        if (x >= sceneBtnX &&
            x <= sceneBtnX + sceneBtnW &&
            y >= editorModeY &&
            y <= editorModeY + sceneBtnH) {
        
            //  Cycle through the three modes
            animSettings.editorMode = (animSettings.editorMode + 1) % 3;

            //  Print the new mode
            const char* newModeText = (animSettings.editorMode == 0) ? "Camera" :
                                  (animSettings.editorMode == 1) ? "Scene" : "Path";
            printf("Scene Editor Mode Toggled: %s\n", newModeText);
        }


	/*
        // Click on Bounce Limit Box
        if (x > VALUE_BOX_MARGIN_X + SUBSETTING_BUTTON_WIDTH + SUBSETTING_BUTTON_MARGIN_X &&
            x < VALUE_BOX_MARGIN_X + VALUE_BOX_WIDTH + SUBSETTING_BUTTON_WIDTH + SUBSETTING_BUTTON_MARGIN_X &&
            y > SUBSETTING_BUTTON_MARGIN_Y && y < SUBSETTING_BUTTON_MARGIN_Y + VALUE_BOX_HEIGHT + 
			VALUE_BOX_MARGIN_Y) {
            editingBounce = true; 
            editingFrame = false;
            snprintf(inputBuffer, sizeof(inputBuffer), "%d", animSettings.bounceLimit);
            return;
        }
        
        // Click anywhere else → Save current input
        if (!(editingBounce || editingFrame)) {
            if (strlen(inputBuffer) > 0) {
                int newValue = atoi(inputBuffer);
                if (editingBounce) animSettings.bounceLimit = newValue;
                if (editingFrame) animSettings.framesForTravel = newValue;
            }
            editingBounce = false;
            editingFrame = false;
            inputBuffer[0] = '\0';  // Clear buffer
        } 
	*/       
    } 
    int centerX = TOGGLE_BUTTON_MARGIN_X + TOGGLE_BUTTON_WIDTH + 60; // mirror render calc for hits

    if (x >= centerX && x <= centerX + FORWARD_FALLOFF_BUTTON_WIDTH &&
        y >= falloffButtonY && y <= falloffButtonY + FORWARD_FALLOFF_BUTTON_HEIGHT) {
        animSettings.forwardFalloffMode = (animSettings.forwardFalloffMode + 1) % 3;
        return;
    }

    if (x >= centerX && x <= centerX + TILE_BUTTON_WIDTH &&
        y >= tileButtonY && y <= tileButtonY + TILE_BUTTON_HEIGHT) {
        animSettings.useTiledRenderer = !animSettings.useTiledRenderer;
        return;
    }

    if (x >= TOGGLE_BUTTON_MARGIN_X && x <= TOGGLE_BUTTON_MARGIN_X + INTEGRATOR_BUTTON_WIDTH &&
        y >= integratorButtonY && y <= integratorButtonY + INTEGRATOR_BUTTON_HEIGHT) {
        animSettings.integratorMode = (animSettings.integratorMode + 1) % 4;
        SyncMenuSliderValues();
        return;
    }

    if (animSettings.integratorMode == 1 || animSettings.integratorMode == 2) {
        if (x >= TOGGLE_BUTTON_MARGIN_X && x <= TOGGLE_BUTTON_MARGIN_X + PATH_TOGGLE_WIDTH &&
            y >= pathToggleDirectY && y <= pathToggleDirectY + PATH_TOGGLE_HEIGHT) {
            animSettings.pathDirectLighting = !animSettings.pathDirectLighting;
            return;
        }
        if (x >= TOGGLE_BUTTON_MARGIN_X && x <= TOGGLE_BUTTON_MARGIN_X + PATH_TOGGLE_WIDTH &&
            y >= pathToggleRRY && y <= pathToggleRRY + PATH_TOGGLE_HEIGHT) {
            animSettings.pathRussianRoulette = !animSettings.pathRussianRoulette;
            return;
        }
        if (x >= TOGGLE_BUTTON_MARGIN_X && x <= TOGGLE_BUTTON_MARGIN_X + PATH_TOGGLE_WIDTH &&
            y >= pathToggleMISY && y <= pathToggleMISY + PATH_TOGGLE_HEIGHT) {
            animSettings.pathEnableMIS = !animSettings.pathEnableMIS;
            return;
        }
        if (x >= TOGGLE_BUTTON_MARGIN_X && x <= TOGGLE_BUTTON_MARGIN_X + PATH_TOGGLE_WIDTH &&
            y >= pathToggleBSDFY && y <= pathToggleBSDFY + PATH_TOGGLE_HEIGHT) {
            animSettings.bsdfModel = (animSettings.bsdfModel == 0) ? 1 : 0;
            SyncMenuSliderValues();
            return;
        }
    }

    // Save Button Click
    if (x > BOTTOM_BUTTON_MARGIN_X_SAVE && x < BOTTOM_BUTTON_MARGIN_X_SAVE + BOTTOM_BUTTON_WIDTH_SAVE &&
        y > BOTTOM_BUTTON_MARGIN_Y_SAVE && y < BOTTOM_BUTTON_MARGIN_Y_SAVE + BOTTOM_BUTTON_HEIGHT_SAVE) {
	SaveAllSettings();
        strncpy(statusLabel, "Saved", sizeof(statusLabel) - 1);
        statusLabel[sizeof(statusLabel) - 1] = '\0';
        statusColor = (SDL_Color){120, 220, 120, 255};
        statusExpireMs = SDL_GetTicks() + 2000;
    }

    // Restore Defaults Button Click
    if (x > BOTTOM_BUTTON_MARGIN_X_RESTORE && x < BOTTOM_BUTTON_MARGIN_X_RESTORE + BOTTOM_BUTTON_WIDTH_RESTORE &&
        y > BOTTOM_BUTTON_MARGIN_Y_RESTORE && y < BOTTOM_BUTTON_MARGIN_Y_RESTORE + BOTTOM_BUTTON_HEIGHT_RESTORE) {
	ResetAnimationSettings();
        strncpy(statusLabel, "Restored", sizeof(statusLabel) - 1);
        statusLabel[sizeof(statusLabel) - 1] = '\0';
        statusColor = (SDL_Color){200, 180, 120, 255};
        statusExpireMs = SDL_GetTicks() + 2000;
    }
    // Preview Button Click
    if (x > BOTTOM_BUTTON_MARGIN_X_PREVIEW && x < BOTTOM_BUTTON_MARGIN_X_PREVIEW + BOTTOM_BUTTON_WIDTH_PREVIEW &&
        y > BOTTOM_BUTTON_MARGIN_Y_PREVIEW && y < BOTTOM_BUTTON_MARGIN_Y_PREVIEW + BOTTOM_BUTTON_HEIGHT_PREVIEW) {
        SyncMenuSliderValues();
        SaveAllSettings();
        animSettings.previewMode = true;
        *menuExitedNormally = true;
        *running = false;
        return;
    }
    // Exit without saving Button Click
    if (x > BOTTOM_BUTTON_MARGIN_X_EXIT && x < BOTTOM_BUTTON_MARGIN_X_EXIT + BOTTOM_BUTTON_WIDTH_EXIT &&
        y > BOTTOM_BUTTON_MARGIN_Y_EXIT && y < BOTTOM_BUTTON_MARGIN_Y_EXIT + BOTTOM_BUTTON_HEIGHT_EXIT) {
	*running = false;
    }
    
    // Start Button Click
    if (x > BOTTOM_BUTTON_MARGIN_X_START && x < BOTTOM_BUTTON_MARGIN_X_START + BOTTOM_BUTTON_WIDTH_START &&
        y > BOTTOM_BUTTON_MARGIN_Y_START && y < BOTTOM_BUTTON_MARGIN_Y_START + BOTTOM_BUTTON_HEIGHT_START) {
        // Capture any in-flight slider edits, then persist
        SyncMenuSliderValues();
        printf("[Menu] Start pressed: integrator=%d falloffMode=%d decay=%.2f softness=%.2f intensity=%.2f\n",
               animSettings.integratorMode,
               animSettings.forwardFalloffMode,
               animSettings.forwardDecay,
               animSettings.lightDecaySoftness,
               animSettings.lightIntensity);
        SaveAllSettings();
        animSettings.previewMode = false;
        *menuExitedNormally = true;
        *running = false;
    }
}

bool RunMenu(void) {        
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
            
    if (!InitializeMenu(&window, &renderer, &font)) {
        return false;
    }   
        
    bool running = true;
    bool menuExitedNormally = false;
    SDL_Event event;
                
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    menuExitedNormally = false;
                    running = false;
                    break;

                case SDL_KEYDOWN:
                    HandleKeyPress(&event, &running);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    HandleMouseClick(&event, &running, &menuExitedNormally, renderer);
                    break;

                case SDL_MOUSEBUTTONUP:
                    draggingSlider = false;  // Stop dragging on mouse release
                    break;

                case SDL_MOUSEMOTION:
                    HandleMouseMotion(&event);  // Update slider value on drag
                    break;

                case SDL_TEXTINPUT:
                    if (editingBounce || editingFrame) {
                        if (strlen(inputBuffer) < sizeof(inputBuffer) - 1) {
                            strcat(inputBuffer, event.text.text);
                        }
                    }
                    break;
            }
        }
        RenderMenu(renderer, font);
    }
    //  Only Quit SDL if the User Exits the Menu
    if (menuExitedNormally) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
    }
    SaveAllSettings();
    return menuExitedNormally;
}
