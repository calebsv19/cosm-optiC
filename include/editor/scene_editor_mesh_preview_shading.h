#pragma once

#include <SDL2/SDL.h>

typedef struct SceneEditorMeshPreviewShadeNormal {
    double x;
    double y;
    double z;
} SceneEditorMeshPreviewShadeNormal;

double SceneEditorMeshPreviewShadeFactor(SceneEditorMeshPreviewShadeNormal normal);
SDL_Color SceneEditorMeshPreviewShadeColor(SDL_Color base,
                                           SceneEditorMeshPreviewShadeNormal normal);
