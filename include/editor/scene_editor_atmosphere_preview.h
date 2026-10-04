#ifndef SCENE_EDITOR_ATMOSPHERE_PREVIEW_H
#define SCENE_EDITOR_ATMOSPHERE_PREVIEW_H
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>
void SceneEditorAtmospherePreviewTick(void);
bool SceneEditorAtmospherePreviewDraw(SDL_Renderer* renderer,SDL_Rect viewport);
void SceneEditorAtmospherePreviewSetEnabled(bool enabled);
bool SceneEditorAtmospherePreviewEnabled(void);
bool SceneEditorAtmospherePreviewPending(void);
void SceneEditorAtmospherePreviewReset(void);
/* Native acceptance readback: a complete image for the current evaluated scene. */
bool SceneEditorAtmospherePreviewCopyPixels(unsigned char* out,size_t size,int* width,int* height);
const char* SceneEditorAtmospherePreviewStatus(void);
#endif
