// scene_editor.h
#ifndef SCENE_EDITOR_H
#define SCENE_EDITOR_H

#include <SDL2/SDL.h>
#include "config/config_manager.h"
#include "render/render_helper.h"
#include "editor/bezier_editor.h"
#include "editor/object_editor.h"
#include "editor/camera_editor.h"

extern SDL_Rect applyButton;
extern SDL_Rect previewButton;
extern bool sceneEditorExitFlag;

// Scene Editor: Manages all scene-wide settings
typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;

    bool running;
    int currentMode;  // 0 = Bezier, 1 = Objects, 2 = Camera
} SceneEditor;

// Initialization & Cleanup
void InitializeSceneEditor(SceneEditor* editor);
void DestroySceneEditor(SceneEditor* editor);

// Main Loop & Event Handling
void SceneEditorLoop(SceneEditor* editor);
void HandleSceneEditorEvents(SceneEditor* editor, SDL_Event* event);
bool IsClickingButtonMain(int mx,int my);

// Scene Mode Management
void SetSceneMode(SceneEditor* editor, int mode);

#endif // SCENE_EDITOR_H
