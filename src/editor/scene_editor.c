// scene_editor.c  
#include "editor/scene_editor.h"
#include "editor/bezier_editor.h"
#include "editor/object_editor.h"   //  Required for object editing
#include "editor/camera_editor.h"   //  Required for camera adjustments
#include "config/config_manager.h"  //  Required for loading/saving scene settings
#include "scene/object_manager.h"
#include "app/animation.h"
#include "render/fluid_state.h"
#include "camera/camera.h"
#include "engine/Render/render_pipeline.h"
#include "render/vk_shared_device.h"
#include "ui/text_zoom_shortcuts.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// UI Button
SDL_Rect applyButton = {1000, 700, 150, 50};
SDL_Rect previewButton;
SDL_Rect changeModeButton;
SDL_Rect addButton;  // Small square
SDL_Rect deleteButton;
SDL_Rect toggleButton;

#if USE_VULKAN
static VkRenderer g_scene_renderer_storage;
#endif

bool sceneEditorExitFlag = false;  //  Used to signal Scene Editor should exit
static void InitializeEditorMode(SceneEditor* editor);

static bool FluidSceneLocksObjects(void) {
    return AnimationUseFluidScene();
}

static int SceneEditorMeasureButtonWidth(const char* label, int min_width) {
    const char* font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
    int point_size = animation_config_scale_text_point_size(&animSettings, 24, 12);
    int text_w = 0;
    int text_h = 0;
    TTF_Font* font = NULL;
    if (!label || !label[0]) return min_width;
    font = TTF_OpenFont(font_path, point_size);
    if (!font) return min_width;
    if (TTF_SizeUTF8(font, label, &text_w, &text_h) != 0) {
        TTF_CloseFont(font);
        return min_width;
    }
    TTF_CloseFont(font);
    if (text_w + 28 > min_width) {
        min_width = text_w + 28;
    }
    return min_width;
}

static int SceneEditorMeasureButtonHeight(int min_height) {
    int point_size = animation_config_scale_text_point_size(&animSettings, 24, 12);
    int target = point_size + 14;
    if (target > min_height) return target;
    return min_height;
}

static int NextEditorMode(int current_mode, bool reverse) {
    if (!FluidSceneLocksObjects()) {
        if (reverse) {
            return (current_mode == 0) ? 2 : (current_mode - 1);
        }
        return (current_mode + 1) % 3;
    }
    // Fluid scenes: lock object edits, only Bezier(0) + Camera(2).
    if (current_mode != 0 && current_mode != 2) current_mode = 0;
    if (reverse) {
        return (current_mode == 0) ? 2 : 0;
    }
    return (current_mode == 0) ? 2 : 0;
}

static void RenderFluidBounds(SDL_Renderer* renderer) {
    if (!g_fluidGrid.valid) return;
    Camera cam = CameraBuildPreviewCamera(&sceneSettings.camera,
                                          GetCurrentMarginPixels(),
                                          sceneSettings.windowWidth,
                                          sceneSettings.windowHeight);
    CameraPoint minS = CameraWorldToScreen(&cam,
                                           g_fluidGrid.min_x,
                                           g_fluidGrid.min_y,
                                           sceneSettings.windowWidth,
                                           sceneSettings.windowHeight);
    CameraPoint maxS = CameraWorldToScreen(&cam,
                                           g_fluidGrid.max_x,
                                           g_fluidGrid.max_y,
                                           sceneSettings.windowWidth,
                                           sceneSettings.windowHeight);
    int x0 = (int)lrint(fmin(minS.x, maxS.x));
    int x1 = (int)lrint(fmax(minS.x, maxS.x));
    int y0 = (int)lrint(fmin(minS.y, maxS.y));
    int y1 = (int)lrint(fmax(minS.y, maxS.y));
    SDL_Rect rect = {x0, y0, x1 - x0, y1 - y0};
    SDL_SetRenderDrawColor(renderer, 120, 200, 255, 180);
    SDL_RenderDrawRect(renderer, &rect);
}

void InitializeSceneEditor(SceneEditor* editor) {
    LoadAnimationConfig();
    //  Load all scene configurations (window size, objects, paths)
    LoadSceneConfig();
    if (animSettings.useFluidScene && animSettings.fluidManifest[0]) {
        if (!AnimationApplyFluidScene(animSettings.fluidManifest)) {
            fprintf(stderr, "[editor] failed to apply fluid scene: %s\n", animSettings.fluidManifest);
        }
    } else {
        AnimationClearFluidGrid();
    }
    if (animSettings.editorMode < 0)
        animSettings.editorMode = 0;
    editor->currentMode = animSettings.editorMode % 3;
    if (FluidSceneLocksObjects() && editor->currentMode == 1) {
        editor->currentMode = 0;
    }

    //  Create the window using stored scene settings
    editor->window = SDL_CreateWindow("Scene Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      sceneSettings.windowWidth, sceneSettings.windowHeight,
                                      SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!editor->window) {
        fprintf(stderr, "Error: Failed to create scene window.\n");
        return;
    }

#if USE_VULKAN
    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 0.0f;
    cfg.clear_color[1] = 0.0f;
    cfg.clear_color[2] = 0.0f;
    cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(editor->window, &cfg)) {
        fprintf(stderr, "vk_shared_device_init failed.\n");
        SDL_DestroyWindow(editor->window);
        return;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        fprintf(stderr, "vk_shared_device_get failed.\n");
        SDL_DestroyWindow(editor->window);
        return;
    }

    VkResult init = vk_renderer_init_with_device(&g_scene_renderer_storage, shared_device, editor->window, &cfg);
    if (init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed: %d\n", init);
        SDL_DestroyWindow(editor->window);
        return;
    }
    editor->renderer = (SDL_Renderer*)&g_scene_renderer_storage;
    vk_renderer_set_logical_size((VkRenderer*)editor->renderer,
                                 (float)sceneSettings.windowWidth,
                                 (float)sceneSettings.windowHeight);
#else
    //  Create the renderer
    editor->renderer = SDL_CreateRenderer(editor->window, -1, SDL_RENDERER_ACCELERATED | 
			SDL_RENDERER_PRESENTVSYNC);
    if (!editor->renderer) {
        fprintf(stderr, "Error: Failed to create scene renderer.\n");
        SDL_DestroyWindow(editor->window);
        return;
    }
#endif

    //  Initialize TTF for font rendering
    if (TTF_Init() == -1) {
        fprintf(stderr, "Error: TTF_Init failed: %s\n", TTF_GetError());
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)editor->renderer);
        vk_renderer_shutdown_surface((VkRenderer*)editor->renderer);
#else
        SDL_DestroyRenderer(editor->renderer);
#endif
        SDL_DestroyWindow(editor->window);
        return;
    }
    // **Initialize Button Positions Based on Window Size**
    int width = sceneSettings.windowWidth;
    int height = sceneSettings.windowHeight;
    int compactButtonWidth = 70;
    int compactButtonHeight = SceneEditorMeasureButtonHeight(40);
    int footerButtonHeight = SceneEditorMeasureButtonHeight(50);
    int applyWidth = SceneEditorMeasureButtonWidth("Apply", 150);
    int previewWidth = SceneEditorMeasureButtonWidth("Preview", 150);
    int changeModeWidth = SceneEditorMeasureButtonWidth("Change Mode", 130);
    addButton = (SDL_Rect){width - compactButtonWidth - 20, 20, compactButtonWidth, compactButtonHeight};
    deleteButton = (SDL_Rect){width - compactButtonWidth - 20, 20 + compactButtonHeight + 20,
                              compactButtonWidth, compactButtonHeight};
    toggleButton = (SDL_Rect){width - compactButtonWidth - 20, 20 + (compactButtonHeight + 20) * 2,
                              compactButtonWidth, compactButtonHeight};
    
    applyButton = (SDL_Rect){width - applyWidth - 30, height - footerButtonHeight - 30,
                             applyWidth, footerButtonHeight};  // Bottom-right apply button
    previewButton = (SDL_Rect){applyButton.x - previewWidth - 20, applyButton.y,
                               previewWidth, footerButtonHeight}; // Left of apply
    changeModeButton = (SDL_Rect){width - changeModeWidth - 30,
                                  applyButton.y - footerButtonHeight - 12,
                                  changeModeWidth, footerButtonHeight};

    InitializeEditorMode(editor);


    UpdateObjects();
    printf("Scene Editor Initialized. Window Size: %dx%d\n", sceneSettings.windowWidth, 
		sceneSettings.windowHeight);
}

void RenderSceneButtons(SDL_Renderer* renderer) {
    //Individual Buttons
    SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
    SDL_RenderFillRect(renderer, &applyButton);
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderFillRect(renderer, &previewButton);
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_RenderFillRect(renderer, &changeModeButton);
    
    // Outlines and Text
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &applyButton);
    RenderButtonText(renderer, applyButton, "Apply");
    SDL_RenderDrawRect(renderer, &previewButton);
    RenderButtonText(renderer, previewButton, "Preview");
    SDL_RenderDrawRect(renderer, &changeModeButton);
    RenderButtonText(renderer, changeModeButton, "Change Mode");
}

void SceneEditorLoop(SceneEditor* editor) {
    SDL_Event event;
    sceneEditorExitFlag = false;

    while (editor->running && !sceneEditorExitFlag) {
        while (SDL_PollEvent(&event)) {
            HandleSceneEditorEvents(editor, &event);
            if (sceneEditorExitFlag)
                break;
        }

        // **Check for Dirty Objects and Update Them**
        for (int i = 0; i < sceneSettings.objectCount; i++) {
            SceneObject* obj = &sceneSettings.sceneObjects[i];
            if (IsObjectDirty(obj)) {
                UpdateObject(obj);
            }
        }

        setRenderContext(editor->renderer, editor->window,
                         sceneSettings.windowWidth, sceneSettings.windowHeight);
        render_set_clear_color(editor->renderer, 0, 0, 0, 255);
        if (!render_begin_frame()) {
            if (render_device_lost()) {
                editor->running = false;
                sceneEditorExitFlag = true;
            }
            SDL_Delay(10);
            continue;
        }

        // **Render Active Editor Mode**
        switch (editor->currentMode) {
            case 0:
                RenderBezierEditor(editor->renderer);
                break;
            case 1:
                RenderObjectEditor(editor->renderer);
                break;
            case 2:
                RenderCameraEditor(editor->renderer);
                break;
        }
        RenderFluidBounds(editor->renderer);
	RenderSceneButtons(editor->renderer);

        render_end_frame();
        SDL_Delay(16);  // Maintain ~60 FPS
    }

    DestroySceneEditor(editor);
}


void HandleSceneEditorEvents(SceneEditor* editor, SDL_Event* event) {
    if (event->type == SDL_QUIT ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE)) {
        printf("Received SDL_QUIT event. Closing Scene Editor.\n");
        editor->running = false;
        sceneEditorExitFlag = true;
        return;  // ✅ Exit immediately instead of calling object editor events
    }
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_TAB) {
        editor->currentMode = NextEditorMode(editor->currentMode,
                                             (event->key.keysym.mod & KMOD_SHIFT) != 0);
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
        printf("Changed Mode to %d via TAB\n", editor->currentMode);
        return;
    }
    if (event->type == SDL_KEYDOWN) {
        bool changed = false;
        int zoom_step = 0;
        int zoom_percent = 100;
        if (ray_tracing_text_zoom_apply_shortcut(event->key.keysym.sym,
                                                 event->key.keysym.mod,
                                                 &changed,
                                                 &zoom_step,
                                                 &zoom_percent)) {
            printf("[font] text zoom %d%% step=%d%s\n",
                   zoom_percent,
                   zoom_step,
                   changed ? "" : " (clamped)");
            return;
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        int mx = event->button.x;
        int my = event->button.y;

        // Preview Button
        if (mx >= previewButton.x && mx <= previewButton.x + previewButton.w &&
            my >= previewButton.y && my <= previewButton.y + previewButton.h) {
            RunPreviewModeEmbedded();
            return;
        }
        // Check if clicking on Change Mode Button
        if (mx >= changeModeButton.x && mx <= changeModeButton.x + changeModeButton.w &&
            my >= changeModeButton.y && my <= changeModeButton.y + changeModeButton.h) {
            editor->currentMode = NextEditorMode(editor->currentMode, false);  // Cycle through modes
            animSettings.editorMode = editor->currentMode;
            InitializeEditorMode(editor);
	    SaveAllSettings();
	    LoadAllSettings();
            printf("Changed Mode to %d\n", editor->currentMode);
            return;
        }
 	// Check if clicking on Change Mode Button
        if (mx >= applyButton.x && mx <= applyButton.x + applyButton.w &&
            my >= applyButton.y && my <= applyButton.y + applyButton.h) {
            SaveAllSettings();
	    editor->running = false;
            sceneEditorExitFlag = true;
            printf("Changed Mode to %d\n", editor->currentMode);
            return;
        }
    }

    switch (editor->currentMode) {
        case 0:  // Bezier Editor Mode
            HandleBezierEditorEvents(event, &draggingPoint, &draggingVelocity);
            break;
    
        case 1:  // Object Editor Mode
            if (!FluidSceneLocksObjects()) {
                HandleObjectEditorEvents(event);
            }
            break;
    
        case 2:  // Camera Editor Mode
            HandleCameraEditorEvents(event);
            break;
    }
    
}

bool IsClickingButtonMain(int mx, int my) {
    // Check if click is within main buttons
    if ((mx >= applyButton.x && mx <= applyButton.x + applyButton.w && my >= applyButton.y 
                && my <= applyButton.y + applyButton.h) ||
        (mx >= previewButton.x && mx <= previewButton.x + previewButton.w && my >= previewButton.y
                && my <= previewButton.y + previewButton.h) ||
	(mx >= changeModeButton.x && mx <= changeModeButton.x + changeModeButton.w &&
            my >= changeModeButton.y && my <= changeModeButton.y + changeModeButton.h)) {
	return true;  // Click is inside a UI button
    }


    if ((mx >= addButton.x && mx <= addButton.x + addButton.w && my >= addButton.y 
		&& my <= addButton.y + addButton.h) ||
        (mx >= deleteButton.x && mx <= deleteButton.x + deleteButton.w && my >= deleteButton.y 
		&& my <= deleteButton.y + deleteButton.h) ||  
        (mx >= toggleButton.x && mx <= toggleButton.x + toggleButton.w && my >= toggleButton.y 
		&& my <= toggleButton.y+ toggleButton.h)) {
        return true;  // Click is inside a UI button
    }
         
    return false;  // Click is not inside a UI button
}

void ToggleSceneMode(SceneEditor* editor) {
    editor->currentMode = NextEditorMode(editor->currentMode, false);
    animSettings.editorMode = editor->currentMode;
    InitializeEditorMode(editor);
    printf("Switched to mode: %d\n", editor->currentMode);
}

// Set Scene Mode
void SetSceneMode(SceneEditor* editor, int mode) {
    if (mode >= 0 && mode <= 2) {
        if (FluidSceneLocksObjects() && mode == 1) {
            mode = 0;
        }
        editor->currentMode = mode;
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
    }
}

void ResetSceneEditor(SceneEditor* editor) {
    LoadSceneConfig();  // Reload all scene settings
    editor->currentMode = 0;  // Default to Bezier Editor Mode
    animSettings.editorMode = 0;
    InitializeEditorMode(editor);
    printf("Scene Editor reset to default settings.\n");
}


void DestroySceneEditor(SceneEditor* editor) {
    if (editor->renderer) {
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)editor->renderer);
        vk_renderer_shutdown_surface((VkRenderer*)editor->renderer);
#else
        SDL_DestroyRenderer(editor->renderer);
#endif
        editor->renderer = NULL;
    }
    if (editor->window) {
        SDL_DestroyWindow(editor->window);
        editor->window = NULL;
    }
    printf("Scene Editor Closed. Returning to main menu...\n");
}

static void InitializeEditorMode(SceneEditor* editor) {
    switch (editor->currentMode) {
        case 0:
            InitializeBezierEditor();
            break;
        case 1:
            InitializeObjectEditor();
            break;
        case 2:
            InitializeCameraEditor();
            break;
        default:
            break;
    }
}
