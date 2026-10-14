#ifndef MENU_SHARED_CONTROLS_H
#define MENU_SHARED_CONTROLS_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include "kit_ui_surface.h"
#include "kit_ui_window_sdl.h"
typedef struct MenuRuntimeState MenuRuntimeState;
typedef struct MenuSharedControls {
    KitUiSurface surface;
    KitUiWindowState observed;
    SDL_Window *window;
    SDL_Event press;
    Uint8 blocked_keys[SDL_NUM_SCANCODES];
    bool geometry_dirty;
    bool release_blocked;
} MenuSharedControls;
enum MenuSharedControlKey {
    MENU_CONTROL_interactiveRect = 1,
    MENU_CONTROL_deepRenderRect = 2,
    MENU_CONTROL_asyncDeepRenderRect = 3,
    MENU_CONTROL_bounceRect = 4,
    MENU_CONTROL_autoMp4Rect = 5,
    MENU_CONTROL_integratorRect = 6,
    MENU_CONTROL_pathRouletteRect = 7,
    MENU_CONTROL_pathBsdfRect = 8,
    MENU_CONTROL_loadSceneRect = 9,
    MENU_CONTROL_attachVolumeRect = 10,
    MENU_CONTROL_volumeToggleRect = 11,
    MENU_CONTROL_volumeClearRect = 12,
    MENU_CONTROL_inputRootValueRect = 13,
    MENU_CONTROL_inputRootEditRect = 14,
    MENU_CONTROL_inputRootFolderRect = 15,
    MENU_CONTROL_inputRootApplyRect = 16,
    MENU_CONTROL_meshAssetRootValueRect = 17,
    MENU_CONTROL_meshAssetRootEditRect = 18,
    MENU_CONTROL_meshAssetRootFolderRect = 19,
    MENU_CONTROL_meshAssetRootApplyRect = 20,
    MENU_CONTROL_meshImportNormalPolicyRect = 21,
    MENU_CONTROL_outputRootValueRect = 22,
    MENU_CONTROL_outputRootEditRect = 23,
    MENU_CONTROL_outputRootFolderRect = 24,
    MENU_CONTROL_outputRootApplyRect = 25,
    MENU_CONTROL_rendererLightingTabRect = 26,
    MENU_CONTROL_rendererPerformanceTabRect = 27,
    MENU_CONTROL_rendererCausticsTabRect = 28,
    MENU_CONTROL_falloffRect = 29,
    MENU_CONTROL_tileRect = 30,
    MENU_CONTROL_tilePreviewRect = 31,
    MENU_CONTROL_denoiseRect = 32,
    MENU_CONTROL_topFillRect = 33,
    MENU_CONTROL_environmentPresetRect = 34,
    MENU_CONTROL_environmentBackgroundModeRect = 35,
    MENU_CONTROL_upscaleModeRect = 36,
    MENU_CONTROL_lightHeightRect = 37,
    MENU_CONTROL_causticModeRect = 38,
    MENU_CONTROL_causticEngineRect = 39,
    MENU_CONTROL_causticSurfaceRect = 40,
    MENU_CONTROL_causticVolumeRect = 41,
    MENU_CONTROL_causticDebugSummaryRect = 42,
    MENU_CONTROL_causticDebugExportRect = 43,
    MENU_CONTROL_sceneEditorRect = 44,
    MENU_CONTROL_sceneModeRect = 45,
    MENU_CONTROL_spaceModeRect = 46,
    MENU_CONTROL_saveRect = 47,
    MENU_CONTROL_restoreRect = 48,
    MENU_CONTROL_previewRect = 49,
    MENU_CONTROL_exitRect = 50,
    MENU_CONTROL_startRect = 51,
    MENU_CONTROL_batch_panelRect = 1000,
    MENU_CONTROL_batch_videoFileLabelRect = 1001,
    MENU_CONTROL_batch_headerDividerRect = 1002,
    MENU_CONTROL_batch_frameDirValueRect = 1003,
    MENU_CONTROL_batch_frameDirEditRect = 1004,
    MENU_CONTROL_batch_frameDirFolderRect = 1005,
    MENU_CONTROL_batch_frameDirApplyRect = 1006,
    MENU_CONTROL_batch_videoRootValueRect = 1007,
    MENU_CONTROL_batch_videoRootEditRect = 1008,
    MENU_CONTROL_batch_videoRootFolderRect = 1009,
    MENU_CONTROL_batch_videoRootApplyRect = 1010,
    MENU_CONTROL_batch_sceneProjectValueRect = 1011,
    MENU_CONTROL_batch_workerPackageValueRect = 1012,
    MENU_CONTROL_batch_workerExportRect = 1013,
    MENU_CONTROL_batch_frameCountValueRect = 1014,
    MENU_CONTROL_batch_fpsValueRect = 1015,
    MENU_CONTROL_batch_clearFramesRect = 1016,
    MENU_CONTROL_batch_makeVideoRect = 1017,
    MENU_CONTROL_resume_panelRect = 2000,
    MENU_CONTROL_resume_statusRect = 2001,
    MENU_CONTROL_resume_resumeToggleRect = 2002,
    MENU_CONTROL_resume_startFrameRect = 2003,
    MENU_CONTROL_resume_nextExistingRect = 2004,
    MENU_CONTROL_WORKSPACE = 3000
};
void menu_shared_controls_cancel(MenuRuntimeState *state);
bool menu_shared_controls_filter_window(MenuRuntimeState *state, SDL_Window *window, const SDL_Event *event);
void menu_shared_controls_begin(MenuRuntimeState *state);
void menu_shared_controls_end(MenuRuntimeState *state);
bool menu_shared_controls_route(MenuRuntimeState *state, const SDL_Event *event, SDL_Event *activation);
void menu_shared_controls_button(MenuRuntimeState *state, unsigned key, SDL_Renderer *renderer,
    TTF_Font *font, const SDL_Rect *rect, const char *label, bool selected);
#endif
