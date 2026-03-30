#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <math.h>
#include <json-c/json.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include "engine/Render/render_pipeline.h"
#include "editor/scene_editor.h"
#include "app/animation.h"  // Include the header where RunMainLoop() is declared
#include "config/config_manager.h"
#include "camera/camera.h"
#include "engine/Render/render_font.h"
#include "render/vk_shared_device.h"
#include "ui/shared_theme_font_adapter.h"
#include "ui/text_zoom_shortcuts.h"

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

#if USE_VULKAN
static VkRenderer g_menu_renderer_storage;
#endif

static void RenderSurface(SDL_Renderer* renderer, SDL_Surface* surface, const SDL_Rect* dst) {
    if (!renderer || !surface || !dst) return;
#if USE_VULKAN
    VkRendererTexture texture;
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer, surface, &texture,
                                                   VK_FILTER_LINEAR) != VK_SUCCESS) {
        return;
    }
    vk_renderer_draw_texture((VkRenderer*)renderer, &texture, NULL, dst);
    vk_renderer_queue_texture_destroy((VkRenderer*)renderer, &texture);
#else
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!textTexture) return;
    SDL_RenderCopy(renderer, textTexture, NULL, dst);
    SDL_DestroyTexture(textTexture);
#endif
}

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

#define LOAD_SCENE_BUTTON_WIDTH INTEGRATOR_BUTTON_WIDTH
#define LOAD_SCENE_BUTTON_HEIGHT 36
#define LOAD_SCENE_BUTTON_X TOGGLE_BUTTON_MARGIN_X
#define LOAD_SCENE_BUTTON_SPACING 20
#define MANIFEST_PANEL_EXTRA_WIDTH 40
#define MANIFEST_PANEL_MIN_HEIGHT 140
#define MANIFEST_PANEL_MAX_HEIGHT 260
#define MANIFEST_ITEM_HEIGHT 26
#define MANIFEST_ITEM_PADDING 6
#define MANIFEST_SCROLLBAR_WIDTH 10
#define MAX_MANIFEST_OPTIONS 128


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

typedef struct {
    char name[128];
    char path[PATH_MAX];
} ManifestOption;

static ManifestOption g_manifestOptions[MAX_MANIFEST_OPTIONS];
static size_t g_manifestOptionCount = 0;
static bool g_manifestDropdownOpen = false;
static bool g_manifestLoadEnabled = false;
static SDL_Rect g_manifestPanelRect = {0};
static SDL_Rect g_manifestListRect = {0};
static SDL_Rect g_manifestScrollbarRect = {0};
static bool g_manifestScrollbarVisible = false;
static bool g_manifestScrollbarDragging = false;
static float g_manifestThumbHeight = 0.0f;
static float g_manifestTrackHeight = 0.0f;
static int g_manifestDragStartY = 0;
static float g_manifestScrollStart = 0.0f;
static float g_manifestScroll = 0.0f;
static float g_manifestMaxScroll = 0.0f;
static SDL_Rect g_sliderPanelRect = {0};
static float g_sliderScroll = 0.0f;
static float g_sliderMaxScroll = 0.0f;

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

static void RefreshManifestOptions(void);

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

static void SetLoadSceneEnabled(bool enabled) {
    g_manifestLoadEnabled = enabled;
    animSettings.useFluidScene = enabled;
    g_manifestDropdownOpen = enabled;
    if (enabled) {
        RefreshManifestOptions();
        g_manifestScroll = 0.0f;
        g_manifestScrollbarDragging = false;
        if (animSettings.fluidManifest[0]) {
            AnimationApplyFluidScene(animSettings.fluidManifest);
        }
    } else {
        g_manifestDropdownOpen = false;
        AnimationClearFluidGrid();
        LoadSceneConfig();
    }
}

static bool PointInRect(const SDL_Rect *rect, int x, int y) {
    if (!rect) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static void ManifestClampScroll(void) {
    if (g_manifestScroll < 0.0f) g_manifestScroll = 0.0f;
    if (g_manifestScroll > g_manifestMaxScroll) g_manifestScroll = g_manifestMaxScroll;
}

static void ManifestScrollBy(float delta) {
    g_manifestScroll += delta;
    ManifestClampScroll();
}

static float SliderClampScroll(float value, float maxScroll) {
    if (value < 0.0f) return 0.0f;
    if (value > maxScroll) return maxScroll;
    return value;
}

static bool FileExistsRegular(const char *path) {
    if (!path || !*path) return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static void BuildManifestLabel(const char *path, char *out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (!path || !*path) return;
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    if (strcmp(filename, "manifest.json") == 0 || strcmp(filename, "scene_bundle.json") == 0) {
        char parentBuf[PATH_MAX];
        size_t len = (size_t)(filename - path - 1); // exclude trailing slash
        if (len >= sizeof(parentBuf)) len = sizeof(parentBuf) - 1;
        memcpy(parentBuf, path, len);
        parentBuf[len] = '\0';
        const char *dirName = strrchr(parentBuf, '/');
        filename = dirName ? dirName + 1 : parentBuf;
    }

    const char *suffix = NULL;
    if (strstr(path, "physics_sim")) suffix = "phys";
    else if (strstr(path, "ray_tracing")) suffix = "ray";
    else if (strstr(path, "shared")) suffix = "shared";

    if (suffix) {
        snprintf(out, outSize, "%s (%s)", filename, suffix);
    } else {
        snprintf(out, outSize, "%s", filename);
    }
}

static void AddManifestOption(const char *path) {
    if (!path || !*path) return;
    if (g_manifestOptionCount >= MAX_MANIFEST_OPTIONS) return;

    char resolved[PATH_MAX];
    const char *usePath = path;
    if (realpath(path, resolved)) {
        usePath = resolved;
    }
    if (!FileExistsRegular(usePath)) return;

    for (size_t i = 0; i < g_manifestOptionCount; ++i) {
        if (strcmp(g_manifestOptions[i].path, usePath) == 0) {
            return; // already present
        }
    }

    ManifestOption *opt = &g_manifestOptions[g_manifestOptionCount++];
    strncpy(opt->path, usePath, sizeof(opt->path) - 1);
    opt->path[sizeof(opt->path) - 1] = '\0';
    BuildManifestLabel(usePath, opt->name, sizeof(opt->name));
}

static void ScanManifestRoot(const char *root) {
    if (!root || !*root) return;

    char pathBuf[PATH_MAX];
    snprintf(pathBuf, sizeof(pathBuf), "%s/manifest.json", root);
    AddManifestOption(pathBuf);
    snprintf(pathBuf, sizeof(pathBuf), "%s/scene_bundle.json", root);
    AddManifestOption(pathBuf);

    DIR *dir = opendir(root);
    if (!dir) return;

    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        snprintf(pathBuf, sizeof(pathBuf), "%s/%s", root, ent->d_name);
        struct stat st;
        if (stat(pathBuf, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            char manifestPath[PATH_MAX];
            snprintf(manifestPath, sizeof(manifestPath), "%s/manifest.json", pathBuf);
            AddManifestOption(manifestPath);
            char bundlePath[PATH_MAX];
            snprintf(bundlePath, sizeof(bundlePath), "%s/scene_bundle.json", pathBuf);
            AddManifestOption(bundlePath);
        } else if (S_ISREG(st.st_mode) && strcmp(ent->d_name, "manifest.json") == 0) {
            AddManifestOption(pathBuf);
        } else if (S_ISREG(st.st_mode) && strcmp(ent->d_name, "scene_bundle.json") == 0) {
            AddManifestOption(pathBuf);
        }
    }
    closedir(dir);
}

static void RefreshManifestOptions(void) {
    g_manifestOptionCount = 0;
    const char *roots[] = {
        "export/volume_frames",
        "../physics_sim/export/volume_frames",
        "../ray_tracing/export/volume_frames",
        "../shared/assets/scenes"
    };
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
        ScanManifestRoot(roots[i]);
    }
    if (animSettings.fluidManifest[0]) {
        AddManifestOption(animSettings.fluidManifest);
    }
    g_manifestScroll = 0.0f;
    g_manifestMaxScroll = 0.0f;
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

static bool OpenMenuFontForCurrentZoom(TTF_Font** out_font) {
    char shared_font_path[256];
    const char* font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
    int point_size = TOGGLE_BUTTON_TEXT_SIZE;
    TTF_Font* opened_font;
    if (!out_font) return false;

    if (ray_tracing_shared_font_resolve_ui_regular(
            shared_font_path, sizeof(shared_font_path), &point_size)) {
        font_path = shared_font_path;
    }
    point_size = animation_config_scale_text_point_size(&animSettings, point_size, 12);
    opened_font = TTF_OpenFont(font_path, point_size);
    if (!opened_font && font_path != NULL && strcmp(font_path, "/System/Library/Fonts/Supplemental/Arial.ttf") != 0) {
        opened_font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", point_size);
    }
    if (!opened_font) return false;
    *out_font = opened_font;
    return true;
}

static bool ReloadMenuFont(TTF_Font** font) {
    TTF_Font* replacement = NULL;
    if (!font) return false;
    if (!OpenMenuFontForCurrentZoom(&replacement)) {
        return false;
    }
    if (*font) {
        TTF_CloseFont(*font);
    }
    *font = replacement;
    return true;
}

bool InitializeMenu(SDL_Window** window, SDL_Renderer** renderer, TTF_Font** font) {
    ray_tracing_shared_theme_load_persisted();

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
                               MENU_WIDTH, MENU_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!*window) {
        printf("Window Creation Failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

#if USE_VULKAN
    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 0.0f;
    cfg.clear_color[1] = 0.0f;
    cfg.clear_color[2] = 0.0f;
    cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(*window, &cfg)) {
        printf("vk_shared_device_init failed.\n");
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        printf("vk_shared_device_get failed.\n");
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    VkResult init = vk_renderer_init_with_device(&g_menu_renderer_storage, shared_device, *window, &cfg);
    if (init != VK_SUCCESS) {
        printf("vk_renderer_init failed: %d\n", init);
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }
    *renderer = (SDL_Renderer*)&g_menu_renderer_storage;
    vk_renderer_set_logical_size((VkRenderer*)*renderer, (float)MENU_WIDTH, (float)MENU_HEIGHT);
#else
    // Create SDL Renderer
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer) {
        printf("Renderer Creation Failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }
#endif

    LoadAnimationConfig();
    LoadSceneConfig();
    animSettings.previewMode = false; // Preview is transient
    SyncMenuSliderValues();
    g_manifestLoadEnabled = animSettings.useFluidScene;
    g_manifestDropdownOpen = g_manifestLoadEnabled;
    RefreshManifestOptions();
    g_sliderScroll = 0.0f;
    g_sliderMaxScroll = 0.0f;
    g_sliderPanelRect = (SDL_Rect){0, 0, 0, 0};
    oldWindowWidth = sceneSettings.windowWidth;
    oldWindowHeight = sceneSettings.windowHeight;

    // Load Font (shared adapter path is opt-in and falls back to legacy menu font)
    *font = NULL;
    ReloadMenuFont(font);
    if (!*font) {
        printf("Font Loading Failed: %s\n", TTF_GetError());
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)*renderer);
        vk_renderer_shutdown_surface((VkRenderer*)*renderer);
#else
        SDL_DestroyRenderer(*renderer);
#endif
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

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
    animSettings.tilePreviewEnabled = false;
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
    animSettings.textZoomStep = 0;
    animSettings.previewMode = false;
    animSettings.previewDuration = 5.0;
    g_sliderScroll = 0.0f;
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

    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    SDL_Color textColor = has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255};
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, buffer, textColor);
    if (!textSurface) return;

    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    RenderSurface(renderer, textSurface, &textRect);
    SDL_FreeSurface(textSurface);
}

static void RenderTextColor(SDL_Renderer *renderer, TTF_Font *font, int x, int y, SDL_Color color, const char *text) {
    if (!text || !*text) return;
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, color);
    if (!textSurface) return;
    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    RenderSurface(renderer, textSurface, &textRect);
    SDL_FreeSurface(textSurface);
}

static void RenderCenteredTextColor(SDL_Renderer *renderer,
                                    TTF_Font *font,
                                    const SDL_Rect *rect,
                                    SDL_Color color,
                                    const char *text) {
    SDL_Surface *textSurface;
    SDL_Rect textRect;
    if (!font || !rect || !text || !text[0]) return;
    textSurface = TTF_RenderText_Solid(font, text, color);
    if (!textSurface) return;
    textRect = (SDL_Rect){
        rect->x + (rect->w - textSurface->w) / 2,
        rect->y + (rect->h - textSurface->h) / 2,
        textSurface->w,
        textSurface->h
    };
    RenderSurface(renderer, textSurface, &textRect);
    SDL_FreeSurface(textSurface);
}

static int color_luma(SDL_Color color) {
    return (299 * (int)color.r + 587 * (int)color.g + 114 * (int)color.b) / 1000;
}

static int color_contrast_gap(SDL_Color a, SDL_Color b) {
    int gap = color_luma(a) - color_luma(b);
    return gap < 0 ? -gap : gap;
}

static Uint8 mix_u8(Uint8 a, Uint8 b, int a_weight, int b_weight) {
    int total = a_weight + b_weight;
    if (total <= 0) return a;
    return (Uint8)(((int)a * a_weight + (int)b * b_weight) / total);
}

static SDL_Color mix_color(SDL_Color a, SDL_Color b, int a_weight, int b_weight) {
    return (SDL_Color){
        mix_u8(a.r, b.r, a_weight, b_weight),
        mix_u8(a.g, b.g, a_weight, b_weight),
        mix_u8(a.b, b.b, a_weight, b_weight),
        mix_u8(a.a, b.a, a_weight, b_weight)
    };
}

static SDL_Color ensure_highlight_fill_contrast(SDL_Color fill,
                                                SDL_Color preferred_text,
                                                SDL_Color darker_anchor) {
    if (color_contrast_gap(fill, preferred_text) >= 110) {
        return fill;
    }
    if (color_luma(preferred_text) >= 150) {
        return mix_color(fill, darker_anchor, 1, 2);
    }
    return mix_color(fill, (SDL_Color){240, 243, 247, fill.a}, 1, 2);
}

static SDL_Color choose_readable_text(SDL_Color background, SDL_Color preferred_text) {
    if (color_contrast_gap(background, preferred_text) >= 110) {
        return preferred_text;
    }
    if (color_luma(background) >= 150) {
        return (SDL_Color){24, 28, 34, preferred_text.a ? preferred_text.a : 255};
    }
    return (SDL_Color){245, 247, 250, preferred_text.a ? preferred_text.a : 255};
}

static void FormatManifestButtonLabel(char *out, size_t outSize) {
    if (!out || outSize == 0) return;
    const char *base = "Load Scene";
    if (!animSettings.fluidManifest[0]) {
        snprintf(out, outSize, "%s", base);
        return;
    }
    char label[128];
    BuildManifestLabel(animSettings.fluidManifest, label, sizeof(label));
    snprintf(out, outSize, "%s: %s", base, label);
    if (strlen(out) >= outSize) {
        out[outSize - 1] = '\0';
    }
}

static void RenderManifestDropdown(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect* loadButtonRect) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    int panelX;
    int panelY;
    int panelW;
    int available = 0;
    if (!loadButtonRect) return;
    panelX = loadButtonRect->x;
    panelY = loadButtonRect->y + loadButtonRect->h + 6;
    panelW = loadButtonRect->w + MANIFEST_PANEL_EXTRA_WIDTH;
    available = BOTTOM_BUTTON_MARGIN_Y_PREVIEW - 10 - panelY;
    int panelH = MANIFEST_PANEL_MIN_HEIGHT;
    if (available > MANIFEST_PANEL_MIN_HEIGHT) panelH = available;
    if (panelH > MANIFEST_PANEL_MAX_HEIGHT) panelH = MANIFEST_PANEL_MAX_HEIGHT;
    int minH = MANIFEST_ITEM_HEIGHT + MANIFEST_ITEM_PADDING * 2 + 4;
    if (panelH < minH) panelH = minH;

    g_manifestPanelRect = (SDL_Rect){panelX, panelY, panelW, panelH};
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 28, 28, 30, 230);
    }
    SDL_RenderFillRect(renderer, &g_manifestPanelRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
    }
    SDL_RenderDrawRect(renderer, &g_manifestPanelRect);

    int listX = panelX + MANIFEST_ITEM_PADDING;
    int listY = panelY + MANIFEST_ITEM_PADDING;
    int listW = panelW - MANIFEST_ITEM_PADDING * 2 - MANIFEST_SCROLLBAR_WIDTH - 4;
    if (listW < 40) listW = 40;
    int listH = panelH - MANIFEST_ITEM_PADDING * 2;
    g_manifestListRect = (SDL_Rect){listX, listY, listW, listH};

    int contentH = (int)(g_manifestOptionCount * MANIFEST_ITEM_HEIGHT);
    g_manifestMaxScroll = (contentH > listH) ? (float)(contentH - listH) : 0.0f;
    g_manifestScrollbarVisible = g_manifestMaxScroll > 0.5f;
    ManifestClampScroll();
    g_manifestTrackHeight = (float)listH;
    g_manifestThumbHeight = 0.0f;
    g_manifestScrollbarRect = (SDL_Rect){0, 0, 0, 0};

    if (g_manifestScrollbarVisible) {
        float thumb = ((float)listH * (float)listH) / (float)contentH;
        if (thumb < 16.0f) thumb = 16.0f;
        g_manifestThumbHeight = thumb;
        float trackRange = (float)listH - thumb;
        float thumbY = (trackRange > 0.0f && g_manifestMaxScroll > 0.0f)
                           ? (float)listY + (g_manifestScroll / g_manifestMaxScroll) * trackRange
                           : (float)listY;
        int scrollX = panelX + panelW - MANIFEST_SCROLLBAR_WIDTH - MANIFEST_ITEM_PADDING;
        SDL_Rect track = {scrollX, listY, MANIFEST_SCROLLBAR_WIDTH, listH};
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r, palette.panel_border.g,
                                   palette.panel_border.b, palette.panel_border.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
        }
        SDL_RenderFillRect(renderer, &track);

        g_manifestScrollbarRect = (SDL_Rect){scrollX, (int)thumbY, MANIFEST_SCROLLBAR_WIDTH, (int)thumb};
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.accent_primary.r, palette.accent_primary.g,
                                   palette.accent_primary.b, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 120, 120, 140, 255);
        }
        SDL_RenderFillRect(renderer, &g_manifestScrollbarRect);
    }

    int firstIndex = (int)(g_manifestScroll / MANIFEST_ITEM_HEIGHT);
    int yOffset = -(int)g_manifestScroll % MANIFEST_ITEM_HEIGHT;
    for (int i = firstIndex; i < (int)g_manifestOptionCount; ++i) {
        int itemY = listY + yOffset + (i - firstIndex) * MANIFEST_ITEM_HEIGHT;
        if (itemY > listY + listH - MANIFEST_ITEM_HEIGHT) break;
        SDL_Rect itemRect = {listX, itemY, listW, MANIFEST_ITEM_HEIGHT};
        bool isSelected = animSettings.fluidManifest[0] &&
                          strcmp(animSettings.fluidManifest, g_manifestOptions[i].path) == 0;
        if (has_shared_palette) {
            SDL_Color fill = isSelected ? palette.accent_primary : palette.button_fill;
            if (isSelected) {
                fill = ensure_highlight_fill_contrast(fill, palette.text_primary, palette.panel_fill);
            }
            SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
        } else {
            SDL_SetRenderDrawColor(renderer,
                                   isSelected ? 70 : 50,
                                   isSelected ? 120 : 70,
                                   isSelected ? 90 : 70,
                                   255);
        }
        SDL_RenderFillRect(renderer, &itemRect);
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r, palette.panel_border.g,
                                   palette.panel_border.b, palette.panel_border.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        }
        SDL_RenderDrawRect(renderer, &itemRect);
        SDL_Color textColor;
        if (has_shared_palette) {
            SDL_Color itemFill = isSelected
                                     ? ensure_highlight_fill_contrast(palette.accent_primary,
                                                                      palette.text_primary,
                                                                      palette.panel_fill)
                                     : palette.button_fill;
            textColor = choose_readable_text(itemFill, palette.text_primary);
        } else {
            textColor = isSelected ? (SDL_Color){220, 240, 220, 255} : (SDL_Color){230, 230, 230, 255};
        }
        RenderTextColor(renderer, font, itemRect.x + 6, itemRect.y + 4, textColor, g_manifestOptions[i].name);
    }

    if (g_manifestOptionCount == 0) {
        SDL_Color c = has_shared_palette ? palette.text_muted : (SDL_Color){210, 210, 210, 255};
        RenderTextColor(renderer, font, listX, listY + 4, c, "No manifests found");
    }
}


static int max_int(int a, int b) {
    return (a > b) ? a : b;
}

static int min_int(int a, int b) {
    return (a < b) ? a : b;
}

static SDL_Rect BuildAdaptiveButtonRect(TTF_Font* font,
                                        int x,
                                        int y,
                                        int minWidth,
                                        int minHeight,
                                        const char* text,
                                        int maxWidth) {
    int width = minWidth;
    int height = minHeight;
    int textW = 0;
    int textH = 0;
    if (font && text && text[0] && TTF_SizeUTF8(font, text, &textW, &textH) == 0) {
        width = max_int(width, textW + 24);
        height = max_int(height, textH + 14);
    }
    if (maxWidth > 0) {
        width = min_int(width, maxWidth);
    }
    return (SDL_Rect){x, y, width, height};
}

static SDL_Rect BuildAdaptiveButtonRectRight(TTF_Font* font,
                                             int rightX,
                                             int y,
                                             int minWidth,
                                             int minHeight,
                                             const char* text,
                                             int maxWidth) {
    SDL_Rect rect = BuildAdaptiveButtonRect(font, 0, y, minWidth, minHeight, text, maxWidth);
    rect.x = rightX - rect.w;
    return rect;
}

static void RenderButtonRect(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect* rect, const char *text, bool active) {
    if (!rect) return;
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    SDL_Color fill = has_shared_palette
                         ? (active ? palette.button_active_fill : palette.button_fill)
                         : (active ? (SDL_Color){0, 255, 0, 255} : (SDL_Color){100, 100, 100, 255});
    SDL_Color textColor = has_shared_palette ? palette.button_text : (SDL_Color){255, 255, 255, 255};
    if (has_shared_palette && active) {
        fill = ensure_highlight_fill_contrast(fill, textColor, palette.panel_fill);
    }
    if (has_shared_palette) {
        textColor = choose_readable_text(fill, textColor);
    }

    // Toggle button color based on active state
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, rect);

    RenderCenteredTextColor(renderer, font, rect, textColor, text);
}

void RenderButton(SDL_Renderer *renderer, TTF_Font *font, int x, int y, int width, int height, const char *text, bool active) {
    SDL_Rect rect = {x, y, width, height};
    RenderButtonRect(renderer, font, &rect, text, active);
}

typedef struct {
    int *value;
    int min, max;
    SDL_Rect trackRect;
    SDL_Rect hitRect;
    int labelX, labelY;
    int valueX, valueY;
    const char *label;
} MenuSlider;

typedef struct {
    MenuSlider items[MAX_MENU_SLIDERS];
    size_t count;
    int nextY;
    int trackHeight;
    int knobWidth;
    int knobHeight;
    SDL_Rect panelRect;
    int contentBottomY;
    float maxScroll;
    float scroll;
} SliderLayout;

typedef struct {
    SDL_Rect interactiveRect;
    SDL_Rect deepRenderRect;
    SDL_Rect bounceRect;
    SDL_Rect autoMp4Rect;
    SDL_Rect integratorRect;
    SDL_Rect pathRouletteRect;
    SDL_Rect pathBsdfRect;
    SDL_Rect loadSceneRect;
    SDL_Rect falloffRect;
    SDL_Rect tileRect;
    SDL_Rect tilePreviewRect;
    SDL_Rect lightHeightRect;
    SDL_Rect sceneEditorRect;
    SDL_Rect sceneModeRect;
    SDL_Rect saveRect;
    SDL_Rect restoreRect;
    SDL_Rect previewRect;
    SDL_Rect exitRect;
    SDL_Rect startRect;
    bool showLightHeight;
    bool showPathToggles;
} MenuButtonLayout;

static MenuButtonLayout BuildMenuButtonLayout(TTF_Font* font) {
    MenuButtonLayout layout;
    char manifestLabel[160];
    const char* integratorLabel = "Integrator: Forward Light";
    int leftX = TOGGLE_BUTTON_MARGIN_X;
    int subX = SUBSETTING_BUTTON_MARGIN_X;
    int maxLeftWidth = SLIDER_MARGIN_X - leftX - 24;
    int leftColumnRight;
    int centerX;
    int centerMaxWidth;
    int rightEdge = MENU_WIDTH - MENU_MARGIN_X;
    int bottomGap = BOTTOM_BUTTON_SPACING;
    int rightLimit;

    memset(&layout, 0, sizeof(layout));
    layout.showPathToggles = (animSettings.integratorMode == 1);
    layout.showLightHeight = false;
    if (maxLeftWidth < 120) maxLeftWidth = 120;

    layout.interactiveRect = BuildAdaptiveButtonRect(font, leftX, TOGGLE_BUTTON_MARGIN_Y,
                                                     TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT,
                                                     "Interactive Mode", maxLeftWidth);
    layout.deepRenderRect = BuildAdaptiveButtonRect(font, leftX,
                                                    layout.interactiveRect.y + layout.interactiveRect.h + TOGGLE_BUTTON_SPACING,
                                                    TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT,
                                                    "Deep Render", maxLeftWidth);

    layout.bounceRect = BuildAdaptiveButtonRect(font, subX,
                                                layout.deepRenderRect.y + layout.deepRenderRect.h + 15,
                                                SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT,
                                                "Bounce Mode", maxLeftWidth);
    layout.autoMp4Rect = BuildAdaptiveButtonRect(font, subX,
                                                 layout.bounceRect.y + layout.bounceRect.h + SUBSETTING_BUTTON_SPACING,
                                                 SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT,
                                                 "Auto MP4", maxLeftWidth);

    if (animSettings.integratorMode == 1) integratorLabel = "Integrator: Hybrid";
    else if (animSettings.integratorMode == 2) integratorLabel = "Integrator: Direct Light";
    layout.integratorRect = BuildAdaptiveButtonRect(font, leftX,
                                                    layout.autoMp4Rect.y + layout.autoMp4Rect.h + 10,
                                                    INTEGRATOR_BUTTON_WIDTH, INTEGRATOR_BUTTON_HEIGHT,
                                                    integratorLabel, maxLeftWidth);

    layout.pathRouletteRect = BuildAdaptiveButtonRect(font, leftX,
                                                      layout.integratorRect.y + layout.integratorRect.h + 10,
                                                      PATH_TOGGLE_WIDTH, PATH_TOGGLE_HEIGHT,
                                                      animSettings.pathRussianRoulette ? "Roulette: ON" : "Roulette: OFF",
                                                      maxLeftWidth);
    layout.pathBsdfRect = BuildAdaptiveButtonRect(font, leftX,
                                                  layout.pathRouletteRect.y + layout.pathRouletteRect.h + PATH_TOGGLE_SPACING,
                                                  PATH_TOGGLE_WIDTH, PATH_TOGGLE_HEIGHT,
                                                  (animSettings.bsdfModel == 0) ? "BSDF: Lambert" : "BSDF: GGX",
                                                  maxLeftWidth);

    FormatManifestButtonLabel(manifestLabel, sizeof(manifestLabel));
    layout.loadSceneRect = BuildAdaptiveButtonRect(font, LOAD_SCENE_BUTTON_X,
                                                   (layout.showPathToggles ? layout.pathBsdfRect.y + layout.pathBsdfRect.h
                                                                           : layout.integratorRect.y + layout.integratorRect.h) + LOAD_SCENE_BUTTON_SPACING,
                                                   LOAD_SCENE_BUTTON_WIDTH, LOAD_SCENE_BUTTON_HEIGHT,
                                                   manifestLabel, maxLeftWidth);

    leftColumnRight = max_int(layout.interactiveRect.x + layout.interactiveRect.w,
                              layout.deepRenderRect.x + layout.deepRenderRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.bounceRect.x + layout.bounceRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.autoMp4Rect.x + layout.autoMp4Rect.w);
    leftColumnRight = max_int(leftColumnRight, layout.integratorRect.x + layout.integratorRect.w);
    if (layout.showPathToggles) {
        leftColumnRight = max_int(leftColumnRight, layout.pathRouletteRect.x + layout.pathRouletteRect.w);
        leftColumnRight = max_int(leftColumnRight, layout.pathBsdfRect.x + layout.pathBsdfRect.w);
    }
    leftColumnRight = max_int(leftColumnRight, layout.loadSceneRect.x + layout.loadSceneRect.w);

    centerX = leftColumnRight + 24;
    centerMaxWidth = SLIDER_MARGIN_X - centerX - 16;
    if (centerMaxWidth < 120) centerMaxWidth = 120;
    layout.falloffRect = BuildAdaptiveButtonRect(font, centerX, TOGGLE_BUTTON_MARGIN_Y + 10,
                                                 FORWARD_FALLOFF_BUTTON_WIDTH, FORWARD_FALLOFF_BUTTON_HEIGHT,
                                                 "Quadratic (1/r^2)", centerMaxWidth);
    layout.tileRect = BuildAdaptiveButtonRect(font, centerX,
                                              layout.falloffRect.y + layout.falloffRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                              TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                              animSettings.useTiledRenderer ? "Tile Renderer: ON" : "Tile Renderer: OFF",
                                              centerMaxWidth);
    layout.tilePreviewRect = BuildAdaptiveButtonRect(font, centerX,
                                                     layout.tileRect.y + layout.tileRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                                     TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                     animSettings.tilePreviewEnabled ? "Tile Preview: ON" : "Tile Preview: OFF",
                                                     centerMaxWidth);
    layout.lightHeightRect = BuildAdaptiveButtonRect(font, centerX,
                                                     layout.tilePreviewRect.y + layout.tilePreviewRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                                     TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                     "Light Height", centerMaxWidth);

    layout.startRect = BuildAdaptiveButtonRectRight(font, rightEdge, BOTTOM_BUTTON_MARGIN_Y_START,
                                                    BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                    "Start", 0);
    layout.sceneEditorRect = BuildAdaptiveButtonRectRight(font, rightEdge,
                                                          layout.startRect.y - (BOTTOM_BUTTON_HEIGHT_START + 8),
                                                          BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                          "Scene Editor", 0);
    layout.sceneModeRect = BuildAdaptiveButtonRectRight(font, rightEdge,
                                                        layout.sceneEditorRect.y - (BOTTOM_BUTTON_HEIGHT_START + 6),
                                                        BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                        "Path", 0);
    layout.exitRect = BuildAdaptiveButtonRect(font, BOTTOM_BUTTON_MARGIN_X_EXIT, BOTTOM_BUTTON_MARGIN_Y_EXIT,
                                              BOTTOM_BUTTON_WIDTH_EXIT, BOTTOM_BUTTON_HEIGHT_EXIT,
                                              "Exit w/o Saving", 280);
    layout.previewRect = BuildAdaptiveButtonRect(font, BOTTOM_BUTTON_MARGIN_X_PREVIEW, BOTTOM_BUTTON_MARGIN_Y_PREVIEW,
                                                 BOTTOM_BUTTON_WIDTH_PREVIEW, BOTTOM_BUTTON_HEIGHT_PREVIEW,
                                                 "Preview", 240);
    layout.restoreRect = BuildAdaptiveButtonRect(font, layout.exitRect.x + layout.exitRect.w + 10,
                                                 BOTTOM_BUTTON_MARGIN_Y_RESTORE,
                                                 BOTTOM_BUTTON_WIDTH_RESTORE, BOTTOM_BUTTON_HEIGHT_RESTORE,
                                                 "Restore Defaults", 260);
    layout.saveRect = BuildAdaptiveButtonRect(font, layout.restoreRect.x + layout.restoreRect.w + 10,
                                              BOTTOM_BUTTON_MARGIN_Y_SAVE,
                                              BOTTOM_BUTTON_WIDTH_SAVE, BOTTOM_BUTTON_HEIGHT_SAVE,
                                              "Save", 180);
    rightLimit = layout.startRect.x - 14;
    if (layout.saveRect.x + layout.saveRect.w > rightLimit) {
        int overflow = (layout.saveRect.x + layout.saveRect.w) - rightLimit;
        layout.saveRect.x -= overflow;
    }
    if (layout.saveRect.x < layout.restoreRect.x + layout.restoreRect.w + bottomGap) {
        layout.saveRect.x = layout.restoreRect.x + layout.restoreRect.w + bottomGap;
        if (layout.saveRect.x + layout.saveRect.w > rightLimit) {
            layout.saveRect.w = max_int(70, rightLimit - layout.saveRect.x);
        }
    }
    return layout;
}

static SliderLayout BuildSliderLayout(TTF_Font* font, const MenuButtonLayout* buttons) {
    SliderLayout layout = {0};
    int textHeight = 18;
    int valueReserve = 110;
    int sliderX = SLIDER_MARGIN_X;
    int sliderWidth = SLIDER_WIDTH;
    int rightLimit = MENU_WIDTH - MENU_MARGIN_X - 10;
    int centerRight = 0;
    int panelTop = SLIDER_MARGIN_Y - 4;
    int panelBottom = MENU_HEIGHT - MENU_MARGIN_Y - 10;
    int panelHeight;
    int visibleBottom;
    int scrollOffset;
    if (font) {
        textHeight = TTF_FontHeight(font);
    }
    if (textHeight < 12) textHeight = 12;
    if (buttons) {
        centerRight = buttons->falloffRect.x + buttons->falloffRect.w;
        centerRight = max_int(centerRight, buttons->tileRect.x + buttons->tileRect.w);
        centerRight = max_int(centerRight, buttons->tilePreviewRect.x + buttons->tilePreviewRect.w);
        sliderX = max_int(sliderX, centerRight + 24);
        panelBottom = min_int(panelBottom, buttons->startRect.y - 14);
    }
    sliderWidth = rightLimit - sliderX - valueReserve;
    if (sliderWidth < 130) {
        sliderWidth = 130;
        sliderX = rightLimit - valueReserve - sliderWidth;
    }
    if (sliderX < SLIDER_MARGIN_X) sliderX = SLIDER_MARGIN_X;
    layout.trackHeight = max_int(SLIDER_HEIGHT, textHeight / 2);
    layout.knobWidth = max_int(10, textHeight / 2);
    layout.knobHeight = layout.trackHeight + 10;
    panelHeight = panelBottom - panelTop;
    if (panelHeight < 120) panelHeight = 120;
    layout.panelRect = (SDL_Rect){sliderX - 12, panelTop, rightLimit - (sliderX - 12), panelHeight};
    layout.nextY = panelTop + 8;
    SyncMenuSliderValues();

#define ADD_SLIDER(targetPtr, minVal, maxVal, labelText) \
    do { \
        if (layout.count < MAX_MENU_SLIDERS) { \
            int labelY_ = layout.nextY; \
            int trackY_ = labelY_ + textHeight + 4; \
            SDL_Rect track_ = { sliderX, trackY_, sliderWidth, layout.trackHeight }; \
            SDL_Rect hit_ = { sliderX, trackY_ - 8, sliderWidth, layout.trackHeight + 16 }; \
            layout.items[layout.count++] = (MenuSlider){ \
                targetPtr, minVal, maxVal, track_, hit_, \
                sliderX, labelY_, \
                sliderX + sliderWidth + 10, trackY_ - ((textHeight - layout.trackHeight) / 2), \
                labelText \
            }; \
            layout.nextY = trackY_ + layout.trackHeight + SLIDER_SPACING + 4; \
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
    }
#undef ADD_SLIDER
    layout.contentBottomY = layout.nextY;
    visibleBottom = panelTop + panelHeight - 8;
    if (layout.contentBottomY > visibleBottom) {
        layout.maxScroll = (float)(layout.contentBottomY - visibleBottom);
    } else {
        layout.maxScroll = 0.0f;
    }
    layout.scroll = SliderClampScroll(g_sliderScroll, layout.maxScroll);
    scrollOffset = (int)lround(layout.scroll);
    if (scrollOffset != 0) {
        for (size_t i = 0; i < layout.count; ++i) {
            layout.items[i].labelY -= scrollOffset;
            layout.items[i].trackRect.y -= scrollOffset;
            layout.items[i].hitRect.y -= scrollOffset;
            layout.items[i].valueY -= scrollOffset;
        }
    }
    g_sliderScroll = layout.scroll;
    g_sliderMaxScroll = layout.maxScroll;
    g_sliderPanelRect = layout.panelRect;
    return layout;
}

static void RenderSliders(SDL_Renderer* renderer, TTF_Font* font, const SliderLayout* layout) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    if (!layout) return;
    if (layout->panelRect.w > 0 && layout->panelRect.h > 0) {
        SDL_Rect panel = layout->panelRect;
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_fill.r, palette.panel_fill.g,
                                   palette.panel_fill.b, palette.panel_fill.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 22, 22, 24, 220);
        }
        SDL_RenderFillRect(renderer, &panel);
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r, palette.panel_border.g,
                                   palette.panel_border.b, palette.panel_border.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
        }
        SDL_RenderDrawRect(renderer, &panel);
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

        if (slider->value == &rouletteSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.3f", rouletteSliderValue / 1000.0);
        } else if (slider->value == &envSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.2f", envSliderValue / 100.0);
        } else if (slider->value == &cacheWeightSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.2f", cacheWeightSliderValue / 100.0);
        } else if (slider->value == &lightIntensitySliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.2f", lightIntensitySliderValue / 100.0);
        } else if (slider->value == &lightDecaySoftnessSliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%.2f", lightDecaySoftnessSliderValue / 100.0);
        } else if (slider->value == &forwardDecaySliderValue) {
            RenderText(renderer, font, slider->valueX, slider->valueY,
                       "%d", forwardDecaySliderValue);
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

void RenderMenu(SDL_Renderer* renderer, TTF_Font* font) {  
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    MenuButtonLayout buttons = BuildMenuButtonLayout(font);
    SliderLayout sliderLayout = BuildSliderLayout(font, &buttons);

    // Clear the screen with a black background
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
    
    // Main mode buttons (left column anchor)
    RenderButtonRect(renderer, font, &buttons.interactiveRect, "Interactive Mode", animSettings.interactiveMode);
    
    RenderButtonRect(renderer, font, &buttons.deepRenderRect, "Deep Render", animSettings.deepRenderMode);

    // Left column: bounce/autoMP4 always visible, integrator + path extras
    RenderButtonRect(renderer, font, &buttons.bounceRect, "Bounce Mode", animSettings.bounceMode);

    RenderButtonRect(renderer, font, &buttons.autoMp4Rect, "Auto MP4", animSettings.autoMP4);

    const char* integratorLabel = "Integrator: Forward Light";
    if (animSettings.integratorMode == 1) integratorLabel = "Integrator: Hybrid";
    else if (animSettings.integratorMode == 2) integratorLabel = "Integrator: Direct Light";
    RenderButtonRect(renderer, font, &buttons.integratorRect, integratorLabel, true);

    if (buttons.showPathToggles) {
        RenderButtonRect(renderer, font, &buttons.pathRouletteRect,
                     animSettings.pathRussianRoulette ? "Roulette: ON" : "Roulette: OFF",
                     animSettings.pathRussianRoulette);
        const char* bsdfLabel = (animSettings.bsdfModel == 0) ? "BSDF: Lambert" : "BSDF: GGX";
        RenderButtonRect(renderer, font, &buttons.pathBsdfRect,
                     bsdfLabel,
                     animSettings.bsdfModel != 0);
    }

    char manifestLabel[160];
    FormatManifestButtonLabel(manifestLabel, sizeof(manifestLabel));
    RenderButtonRect(renderer, font, &buttons.loadSceneRect, manifestLabel, g_manifestLoadEnabled);
    if (g_manifestLoadEnabled) {
        RenderManifestDropdown(renderer, font, &buttons.loadSceneRect);
    } else {
        g_manifestPanelRect = (SDL_Rect){0, 0, 0, 0};
        g_manifestListRect = (SDL_Rect){0, 0, 0, 0};
        g_manifestScrollbarRect = (SDL_Rect){0, 0, 0, 0};
        g_manifestScrollbarVisible = false;
        g_manifestThumbHeight = 0.0f;
        g_manifestTrackHeight = 0.0f;
    }
                 
    const char* falloffLabel = "Quadratic (1/r^2)";
    if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR) {
        falloffLabel = "Linear (1/r)";
    } else if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_NONE) {
        falloffLabel = "Falloff: None";
    }
    RenderButtonRect(renderer, font, &buttons.falloffRect, falloffLabel,
                     animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR);

    const char* tileButtonLabel = animSettings.useTiledRenderer ? "Tile Renderer: ON" : "Tile Renderer: OFF";
    RenderButtonRect(renderer, font, &buttons.tileRect, tileButtonLabel, animSettings.useTiledRenderer);

    const char* previewLabel = animSettings.tilePreviewEnabled ? "Tile Preview: ON" : "Tile Preview: OFF";
    RenderButtonRect(renderer, font, &buttons.tilePreviewRect, previewLabel, animSettings.tilePreviewEnabled);

    // Light height (2.5D shading) disabled while Disney integrator is paused.
    if (buttons.showLightHeight) {
        char heightLabel[64];
        snprintf(heightLabel, sizeof(heightLabel), "Light Height: %.1f", animSettings.lightHeight);
        RenderButtonRect(renderer, font, &buttons.lightHeightRect, heightLabel, true);
    }

    // Right column: Scene Editor + mode above Start
    RenderButtonRect(renderer, font, &buttons.sceneEditorRect, "Scene Editor", false);
    const char* editorModeText = (animSettings.editorMode == 0) ? "Path" :
                                 (animSettings.editorMode == 1) ? "Scene" : "Camera";
    RenderButtonRect(renderer, font, &buttons.sceneModeRect, editorModeText, false);

    // Render Bottom Buttons
    RenderButtonRect(renderer, font, &buttons.saveRect, "Save", false);
    RenderButtonRect(renderer, font, &buttons.restoreRect, "Restore Defaults", false);
    RenderButtonRect(renderer, font, &buttons.previewRect, "Preview", animSettings.previewMode);
    RenderButtonRect(renderer, font, &buttons.exitRect, "Exit w/o Saving", false);
    // Start button with subtle green tint
    if (has_shared_palette) {
        SDL_Color startFill = ensure_highlight_fill_contrast(palette.accent_primary,
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
        SDL_Color startFill = ensure_highlight_fill_contrast(palette.accent_primary,
                                                             palette.button_text,
                                                             palette.panel_fill);
        SDL_Color startText = choose_readable_text(startFill, palette.button_text);
        RenderCenteredTextColor(renderer, font, &buttons.startRect, startText, "Start");
    } else {
        RenderButtonText(renderer, buttons.startRect, "Start");
    }
   

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
            int textX = buttons.saveRect.x + buttons.saveRect.w + 15;
            int textY = buttons.saveRect.y + (buttons.saveRect.h / 2) - 10;
            RenderTextColor(renderer, font, textX, textY, c, statusLabel);
        }
    }

    // Present the updated UI 
    render_end_frame();
} 

void HandleKeyPress(SDL_Event* event, bool* running, TTF_Font** font) {
    SDL_Keymod mod = event->key.keysym.mod;
    bool ctrl_or_cmd = (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
    bool shift = (mod & KMOD_SHIFT) != 0;
    bool zoom_changed = false;
    int zoom_step = 0;
    int zoom_percent = 100;
    if (ctrl_or_cmd && shift) {
        if (event->key.keysym.sym == SDLK_t) {
            ray_tracing_shared_theme_cycle_next();
            ray_tracing_shared_theme_save_persisted();
            ReloadMenuFont(font);
            return;
        }
        if (event->key.keysym.sym == SDLK_y) {
            ray_tracing_shared_theme_cycle_prev();
            ray_tracing_shared_theme_save_persisted();
            ReloadMenuFont(font);
            return;
        }
    }
    if (ray_tracing_text_zoom_apply_shortcut(event->key.keysym.sym,
                                             mod,
                                             &zoom_changed,
                                             &zoom_step,
                                             &zoom_percent)) {
        if (zoom_changed) {
            ReloadMenuFont(font);
        }
        snprintf(statusLabel, sizeof(statusLabel), "Text %d%%", zoom_percent);
        statusLabel[sizeof(statusLabel) - 1] = '\0';
        statusColor = (SDL_Color){120, 210, 240, 255};
        statusExpireMs = SDL_GetTicks() + 1700;
        (void)zoom_step;
        return;
    }
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
                ReloadMenuFont(font);
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
            animSettings.textZoomStep = 0;
            (void)refreshActiveFontFromAnimationConfig();
            ReloadMenuFont(font);
            break;
    }
}

void HandleMouseMotion(SDL_Event* event) {
    if (g_manifestScrollbarDragging && g_manifestDropdownOpen && g_manifestScrollbarVisible) {
        float trackRange = g_manifestTrackHeight - g_manifestThumbHeight;
        if (trackRange < 1.0f) trackRange = 1.0f;
        int deltaY = event->motion.y - g_manifestDragStartY;
        float newScroll = g_manifestScrollStart + ((float)deltaY * g_manifestMaxScroll / trackRange);
        g_manifestScroll = newScroll;
        ManifestClampScroll();
    }

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

void HandleMouseWheel(SDL_Event *event) {
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    if (g_manifestLoadEnabled && g_manifestDropdownOpen && PointInRect(&g_manifestPanelRect, mx, my)) {
        float delta = (float)event->wheel.y * (float)(MANIFEST_ITEM_HEIGHT * 2);
        // SDL wheel is positive when scrolling up; scrolling up should decrease scroll offset
        ManifestScrollBy(-delta);
        return;
    }
    if (PointInRect(&g_sliderPanelRect, mx, my) && g_sliderMaxScroll > 0.5f) {
        float delta = (float)event->wheel.y * 28.0f;
        g_sliderScroll = SliderClampScroll(g_sliderScroll - delta, g_sliderMaxScroll);
    }
}

static void HandleSliderClick(SDL_Event* event, const SliderLayout* layout) {
    if (!layout) return;
    int x = event->button.x, y = event->button.y;
    if (!PointInRect(&layout->panelRect, x, y)) return;
    for (size_t i = 0; i < layout->count; i++) {
        const MenuSlider* slider = &layout->items[i];
        if (PointInRect(&slider->hitRect, x, y)) {
            
            // Activate dragging mode
            draggingSlider = true;
            selectedSlider = slider->value;
            selectedSliderMin = slider->min;
            selectedSliderMax = slider->max;
            sliderStartX = slider->trackRect.x;
            sliderWidth = slider->trackRect.w;

            bool adjustCamera = (selectedSlider == &sceneSettings.windowWidth ||
                                 selectedSlider == &sceneSettings.windowHeight);
            int prevWidth = sceneSettings.windowWidth;
            int prevHeight = sceneSettings.windowHeight;

            // Immediately update slider value to where the user clicked
            float percent = (float)(x - slider->trackRect.x) / slider->trackRect.w;
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 1.0f) percent = 1.0f;
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


void HandleMouseClick(SDL_Event* event, bool* running, bool* menuExitedNormally, SDL_Renderer* renderer, TTF_Font** font) {
    (void)renderer;
    MenuButtonLayout buttons = BuildMenuButtonLayout(*font);
    SliderLayout layout = BuildSliderLayout(*font, &buttons);
    HandleSliderClick(event, &layout);

    int x = event->button.x, y = event->button.y;

    if (g_manifestLoadEnabled && g_manifestDropdownOpen) {
        if (PointInRect(&g_manifestPanelRect, x, y)) {
            if (g_manifestScrollbarVisible && PointInRect(&g_manifestScrollbarRect, x, y)) {
                g_manifestScrollbarDragging = true;
                g_manifestDragStartY = y;
                g_manifestScrollStart = g_manifestScroll;
                return;
            }
            if (g_manifestOptionCount > 0 && PointInRect(&g_manifestListRect, x, y)) {
                int relativeY = y - g_manifestListRect.y + (int)g_manifestScroll;
                int idx = relativeY / MANIFEST_ITEM_HEIGHT;
                if (idx >= 0 && idx < (int)g_manifestOptionCount) {
                    strncpy(animSettings.fluidManifest, g_manifestOptions[idx].path, sizeof(animSettings.fluidManifest) - 1);
                    animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
                    strncpy(statusLabel, "Scene set", sizeof(statusLabel) - 1);
                    statusLabel[sizeof(statusLabel) - 1] = '\0';
                    statusColor = (SDL_Color){140, 220, 200, 255};
                    statusExpireMs = SDL_GetTicks() + 1800;
                    AnimationApplyFluidScene(animSettings.fluidManifest);
                    return;
                }
            }
            return;
        }
    }

    if (PointInRect(&buttons.loadSceneRect, x, y)) {
        SetLoadSceneEnabled(!g_manifestLoadEnabled);
        return;
    }
                
    // Toggle Interactive Mode
    if (PointInRect(&buttons.interactiveRect, x, y)) {
        animSettings.interactiveMode = true;
        animSettings.deepRenderMode = false;
        return;
    }
        
    // Toggle Deep Render Mode
    if (PointInRect(&buttons.deepRenderRect, x, y)) {
        animSettings.deepRenderMode = true;
        animSettings.interactiveMode = false;
        return;
    }
            
    // Deep Render Mode Options (Only Available When Deep Render is ON)
    if (animSettings.deepRenderMode) {
        if (PointInRect(&buttons.bounceRect, x, y)) {
            animSettings.bounceMode = !animSettings.bounceMode;
            return;
        }
        
        if (PointInRect(&buttons.autoMp4Rect, x, y)) {
            animSettings.autoMP4 = !animSettings.autoMP4;
            return;
        }

        // Launch Scene Editor
        if (PointInRect(&buttons.sceneEditorRect, x, y)) {
            SceneEditor editor = {0};  // Zero-initialize struct
            InitializeSceneEditor(&editor);
            editor.running = true;
            SceneEditorLoop(&editor);
            ReloadMenuFont(font);
            return;
        }

	// Toggle Scene Editor Mode
        if (PointInRect(&buttons.sceneModeRect, x, y)) {
        
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
    if (PointInRect(&buttons.falloffRect, x, y)) {
        animSettings.forwardFalloffMode = (animSettings.forwardFalloffMode + 1) % 3;
        return;
    }

    if (PointInRect(&buttons.tileRect, x, y)) {
        animSettings.useTiledRenderer = !animSettings.useTiledRenderer;
        return;
    }
    if (PointInRect(&buttons.tilePreviewRect, x, y)) {
        animSettings.tilePreviewEnabled = !animSettings.tilePreviewEnabled;
        return;
    }
    if (buttons.showLightHeight && PointInRect(&buttons.lightHeightRect, x, y)) {
        // Cycle through a small set of useful light heights
        double options[] = {2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 16.0, 20.0};
        int count = (int)(sizeof(options) / sizeof(options[0]));
        int idx = 0;
        double current = animSettings.lightHeight;
        for (int i = 0; i < count; i++) {
            if (fabs(options[i] - current) < 1e-3) { idx = i; break; }
            if (options[i] > current) { idx = i; break; }
            idx = i;
        }
        idx = (idx + 1) % count;
        animSettings.lightHeight = options[idx];
        return;
    }

    if (PointInRect(&buttons.integratorRect, x, y)) {
        animSettings.integratorMode = (animSettings.integratorMode + 1) % 3;
        SyncMenuSliderValues();
        return;
    }

    if (buttons.showPathToggles) {
        if (PointInRect(&buttons.pathRouletteRect, x, y)) {
            animSettings.pathRussianRoulette = !animSettings.pathRussianRoulette;
            return;
        }
        if (PointInRect(&buttons.pathBsdfRect, x, y)) {
            animSettings.bsdfModel = (animSettings.bsdfModel == 0) ? 1 : 0;
            SyncMenuSliderValues();
            return;
        }
    }

    // Save Button Click
    if (PointInRect(&buttons.saveRect, x, y)) {
	SaveAllSettings();
        strncpy(statusLabel, "Saved", sizeof(statusLabel) - 1);
        statusLabel[sizeof(statusLabel) - 1] = '\0';
        statusColor = (SDL_Color){120, 220, 120, 255};
        statusExpireMs = SDL_GetTicks() + 2000;
    }

    // Restore Defaults Button Click
    if (PointInRect(&buttons.restoreRect, x, y)) {
	ResetAnimationSettings();
        (void)refreshActiveFontFromAnimationConfig();
        ReloadMenuFont(font);
        strncpy(statusLabel, "Restored", sizeof(statusLabel) - 1);
        statusLabel[sizeof(statusLabel) - 1] = '\0';
        statusColor = (SDL_Color){200, 180, 120, 255};
        statusExpireMs = SDL_GetTicks() + 2000;
    }
    // Preview Button Click
    if (PointInRect(&buttons.previewRect, x, y)) {
        SyncMenuSliderValues();
        SaveAllSettings();
        animSettings.previewMode = true;
        *menuExitedNormally = true;
        *running = false;
        return;
    }
    // Exit without saving Button Click
    if (PointInRect(&buttons.exitRect, x, y)) {
	*running = false;
    }
    
    // Start Button Click
    if (PointInRect(&buttons.startRect, x, y)) {
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
                    HandleKeyPress(&event, &running, &font);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    HandleMouseClick(&event, &running, &menuExitedNormally, renderer, &font);
                    break;

                case SDL_MOUSEBUTTONUP:
                    draggingSlider = false;  // Stop dragging on mouse release
                    g_manifestScrollbarDragging = false;
                    break;

                case SDL_MOUSEWHEEL:
                    HandleMouseWheel(&event);
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
        setRenderContext(renderer, window, MENU_WIDTH, MENU_HEIGHT);
        RenderMenu(renderer, font);
        if (render_device_lost()) {
            running = false;
            menuExitedNormally = false;
        }
    }
    //  Only Quit SDL if the User Exits the Menu
    if (menuExitedNormally) {
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)renderer);
        vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
        SDL_DestroyRenderer(renderer);
#endif
        SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
#if USE_VULKAN
        vk_shared_device_shutdown();
#endif
        SDL_Quit();
    }
    ray_tracing_shared_theme_save_persisted();
    SaveAllSettings();
    return menuExitedNormally;
}
