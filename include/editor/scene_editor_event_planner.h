#pragma once
#include "editor/scene_editor.h"
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_pane_host.h"
#include <json-c/json.h>
bool SceneEditorEventPlannerVisible(void);
bool SceneEditorEventPlannerInteraction(void);
void SceneEditorEventPlannerOpen(bool);
void SceneEditorEventPlannerReset(void);
void SceneEditorEventPlannerDraw(SceneEditor *, const SceneEditorPaneLayout *,
                                 int);
bool SceneEditorEventPlannerEvent(SDL_Event *);
bool SceneEditorEventPlannerControl(const char *, SDL_Rect *);
void SceneEditorEventPlannerViewport(SDL_Renderer *,
                                     const SceneEditorDigestOverlayProjector *);
json_object *SceneEditorEventPlannerRead(void);
bool SceneEditorEventPlannerDraft(json_object *, unsigned long long, char *,
                                  size_t);
bool SceneEditorEventPlannerPick(json_object *, json_object *, char *, size_t);

void SceneEditorEventPlannerViewportPrepare(const SceneEditorDigestOverlayProjector *);
bool SceneEditorEventPlannerWorldPoint(const char *,const double[3],double[3]);
bool SceneEditorEventPlannerWorldDirection(const char *,const double[3],double[3]);
uint64_t SceneEditorEventPlannerGeometryStamp(void);
