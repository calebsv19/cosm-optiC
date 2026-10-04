#ifndef SCENE_EDITOR_ATMOSPHERE_VIEWPORT_H
#define SCENE_EDITOR_ATMOSPHERE_VIEWPORT_H
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_atmosphere.h"
bool SceneEditorAtmosphereSelected(void);
void SceneEditorAtmosphereSelect(bool selected);
void SceneEditorAtmosphereViewportReset(void);
bool SceneEditorAtmosphereGestureActive(void);
bool SceneEditorAtmospherePlacementPreview(SceneAtmosphereAttachment* out);
bool SceneEditorAtmosphereViewportEvent(const SDL_Event* event,SDL_Window* window);
void SceneEditorAtmosphereViewportDraw(SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,const RuntimeSceneBridge3DDigestState* digest);
bool SceneEditorAtmosphereFrame(void);
bool SceneEditorAtmosphereDensityProxy(void);
void SceneEditorAtmosphereDensityProxyToggle(void);
#endif
