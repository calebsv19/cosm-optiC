#ifndef SDL_MENU_SECTIONS_H
#define SDL_MENU_SECTIONS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <limits.h>

typedef struct {
    char name[128];
    char path[PATH_MAX];
} ManifestOption;

extern bool draggingSlider;
extern int *selectedSlider;
extern int selectedSliderMin;
extern int selectedSliderMax;
extern int sliderStartX;
extern int sliderWidth;
extern char inputBuffer[10];
extern bool editingBounce;
extern bool editingFrame;
extern int rouletteSliderValue;
extern int envSliderValue;
extern int cacheWeightSliderValue;
extern int lightIntensitySliderValue;
extern int lightDecaySoftnessSliderValue;
extern int forwardDecaySliderValue;
extern int oldWindowWidth;
extern int oldWindowHeight;
extern Uint32 statusExpireMs;
extern SDL_Color statusColor;
extern char statusLabel[64];

extern ManifestOption g_manifestOptions[128];
extern size_t g_manifestOptionCount;
extern bool g_manifestDropdownOpen;
extern bool g_manifestLoadEnabled;
extern SDL_Rect g_manifestPanelRect;
extern SDL_Rect g_manifestListRect;
extern SDL_Rect g_manifestScrollbarRect;
extern bool g_manifestScrollbarVisible;
extern bool g_manifestScrollbarDragging;
extern float g_manifestThumbHeight;
extern float g_manifestTrackHeight;
extern int g_manifestDragStartY;
extern float g_manifestScrollStart;
extern float g_manifestScroll;
extern float g_manifestMaxScroll;
extern SDL_Rect g_sliderPanelRect;
extern float g_sliderScroll;
extern float g_sliderMaxScroll;

const char* SpaceModeButtonLabel(void);
bool InitializeMenu(SDL_Window** window, SDL_Renderer** renderer, TTF_Font** font);
void ResetAnimationSettings(void);
void RenderText(SDL_Renderer *renderer, TTF_Font *font, int x, int y, const char *format, ...);
void RenderButton(SDL_Renderer *renderer, TTF_Font *font, int x, int y, int width, int height, const char *text, bool active);
void RenderMenu(SDL_Renderer* renderer, TTF_Font* font);
void HandleKeyPress(SDL_Event* event, bool* running, TTF_Font** font);
void HandleMouseMotion(SDL_Event* event);
void HandleMouseWheel(SDL_Event *event);
void HandleMouseClick(SDL_Event* event, bool* running, bool* menuExitedNormally, SDL_Renderer* renderer, TTF_Font** font);

#endif
