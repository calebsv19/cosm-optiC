#include "editor/camera_editor.h"
#include <stdio.h>

void HandleCameraEditorEvents(SDL_Event* event) {
    (void)event;
    printf("Camera Editor: Handling event.\n");
}

void RenderCameraEditor(SDL_Renderer* renderer) {
    (void)renderer;
    printf("Camera Editor: Rendering.\n");
}
