#ifndef SDL_MENU_STATE_H
#define SDL_MENU_STATE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <limits.h>
#include <stddef.h>

#define SDL_MENU_MAX_MANIFEST_OPTIONS 128
#define SDL_MENU_MANIFEST_ITEM_HEIGHT 26

#define SDL_MENU_DEFAULT_BOUNCE_LIMIT 10
#define SDL_MENU_DEFAULT_FRAME_LIMIT 50
#define SDL_MENU_DEFAULT_FRAME_FOR_TRAVEL 40

#define SDL_MENU_FORWARD_FALLOFF_DISTANCE_MIN 100
#define SDL_MENU_FORWARD_FALLOFF_DISTANCE_MAX 40000

typedef struct {
    char name[128];
    char path[PATH_MAX];
} ManifestOption;

typedef struct {
    bool draggingSlider;
    int *selectedSlider;
    int selectedSliderMin;
    int selectedSliderMax;
    int sliderStartX;
    int sliderWidth;

    char inputBuffer[10];
    bool editingBounce;
    bool editingFrame;
    bool editingInputRoot;
    bool editingOutputRoot;
    char pathInputBuffer[PATH_MAX];

    int rouletteSliderValue;
    int envSliderValue;
    int cacheWeightSliderValue;
    int lightIntensitySliderValue;
    int lightDecaySoftnessSliderValue;
    int forwardDecaySliderValue;

    int oldWindowWidth;
    int oldWindowHeight;

    Uint32 statusExpireMs;
    SDL_Color statusColor;
    char statusLabel[64];

    ManifestOption manifestOptions[SDL_MENU_MAX_MANIFEST_OPTIONS];
    size_t manifestOptionCount;
    bool manifestDropdownOpen;
    bool manifestLoadEnabled;
    SDL_Rect manifestPanelRect;
    SDL_Rect manifestListRect;
    SDL_Rect manifestScrollbarRect;
    bool manifestScrollbarVisible;
    bool manifestScrollbarDragging;
    float manifestThumbHeight;
    float manifestTrackHeight;
    int manifestDragStartY;
    float manifestScrollStart;
    float manifestScroll;
    float manifestMaxScroll;

    SDL_Rect sliderPanelRect;
    float sliderScroll;
    float sliderMaxScroll;
} MenuRuntimeState;

void menu_state_init(MenuRuntimeState* state);
void menu_state_reset_defaults(MenuRuntimeState* state);
void menu_state_sync_from_anim(MenuRuntimeState* state);

void menu_state_manifest_clamp_scroll(MenuRuntimeState* state);
void menu_state_manifest_scroll_by(MenuRuntimeState* state, float delta);
float menu_state_slider_clamp_scroll(float value, float maxScroll);

void menu_state_refresh_manifest_options(MenuRuntimeState* state);
void menu_state_set_load_scene_enabled(MenuRuntimeState* state, bool enabled);
void menu_state_apply_special_slider_rules(MenuRuntimeState* state, int* target);
void menu_state_reanchor_camera_after_resize(int previousWidth, int previousHeight);

void menu_state_build_manifest_label(const char *path, char *out, size_t outSize);

bool menu_state_reload_font(TTF_Font** font);

#endif
