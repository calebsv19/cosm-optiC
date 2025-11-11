// scene_editor.c  
#include "editor/scene_editor.h"
#include "editor/bezier_editor.h"
#include "editor/object_editor.h"   //  Required for object editing
#include "editor/camera_editor.h"   //  Required for camera adjustments
#include "config/config_manager.h"  //  Required for loading/saving scene settings
#include "scene/object_manager.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// UI Button
SDL_Rect applyButton = {1000, 700, 150, 50};
SDL_Rect changeModeButton;
SDL_Rect addButton;  // Small square
SDL_Rect deleteButton;
SDL_Rect toggleButton;


bool sceneEditorExitFlag = false;  //  Used to signal Scene Editor should exit
static void InitializeEditorMode(SceneEditor* editor);

void InitializeSceneEditor(SceneEditor* editor) {
    //  Load all scene configurations (window size, objects, paths)
    LoadSceneConfig();
    if (animSettings.editorMode < 0)
        animSettings.editorMode = 0;
    editor->currentMode = animSettings.editorMode % 3;

    //  Create the window using stored scene settings
    editor->window = SDL_CreateWindow("Scene Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      sceneSettings.windowWidth, sceneSettings.windowHeight, SDL_WINDOW_SHOWN);
    if (!editor->window) {
        fprintf(stderr, "Error: Failed to create scene window.\n");
        return;
    }

    //  Create the renderer
    editor->renderer = SDL_CreateRenderer(editor->window, -1, SDL_RENDERER_ACCELERATED | 
			SDL_RENDERER_PRESENTVSYNC);
    if (!editor->renderer) {
        fprintf(stderr, "Error: Failed to create scene renderer.\n");
        SDL_DestroyWindow(editor->window);
        return;
    }

    //  Initialize TTF for font rendering
    if (TTF_Init() == -1) {
        fprintf(stderr, "Error: TTF_Init failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(editor->renderer);
        SDL_DestroyWindow(editor->window);
        return;
    }
    // **Initialize Button Positions Based on Window Size**
    int width = sceneSettings.windowWidth;
    int height = sceneSettings.windowHeight;
    int buttonWidth = 70;
    addButton = (SDL_Rect){width - buttonWidth - 20, 20, buttonWidth, 40};
    deleteButton = (SDL_Rect){width - buttonWidth - 20, 80, buttonWidth, 40};
    toggleButton = (SDL_Rect){width - buttonWidth - 20, 140, buttonWidth, 40};
    
    applyButton = (SDL_Rect){width - 180, height - 80, 150, 50};  // Bottom-right apply button
    changeModeButton = (SDL_Rect){width - 160, height - 135, 130, 40};  

    InitializeEditorMode(editor);


    UpdateObjects();
    printf("Scene Editor Initialized. Window Size: %dx%d\n", sceneSettings.windowWidth, 
		sceneSettings.windowHeight);
}

void RenderSceneButtons(SDL_Renderer* renderer) {
    //Individual Buttons
    SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
    SDL_RenderFillRect(renderer, &applyButton);
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_RenderFillRect(renderer, &changeModeButton);
    
    // Outlines and Text
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &applyButton);
    RenderButtonText(renderer, applyButton, "Apply");
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

        SDL_RenderClear(editor->renderer);

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
	RenderSceneButtons(editor->renderer);

        SDL_RenderPresent(editor->renderer);
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
        if ((event->key.keysym.mod & KMOD_SHIFT) != 0) {
            editor->currentMode = (editor->currentMode == 0) ? 2 : editor->currentMode - 1;
        } else {
            editor->currentMode = (editor->currentMode + 1) % 3;
        }
        animSettings.editorMode = editor->currentMode;
        InitializeEditorMode(editor);
        printf("Changed Mode to %d via TAB\n", editor->currentMode);
        return;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        int mx = event->button.x;
        int my = event->button.y;

        // Check if clicking on Change Mode Button
        if (mx >= changeModeButton.x && mx <= changeModeButton.x + changeModeButton.w &&
            my >= changeModeButton.y && my <= changeModeButton.y + changeModeButton.h) {
            editor->currentMode = (editor->currentMode + 1) % 3;  // Cycle through modes
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
            HandleObjectEditorEvents(event);
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
    editor->currentMode = (editor->currentMode + 1) % 3;
    animSettings.editorMode = editor->currentMode;
    InitializeEditorMode(editor);
    printf("Switched to mode: %d\n", editor->currentMode);
}

// Set Scene Mode
void SetSceneMode(SceneEditor* editor, int mode) {
    if (mode >= 0 && mode <= 2) {
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
        SDL_DestroyRenderer(editor->renderer);
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
