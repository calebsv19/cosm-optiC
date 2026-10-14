#ifndef SCENE_EDITOR_RENDER_CONTROL_H
#define SCENE_EDITOR_RENDER_CONTROL_H
#include <stdbool.h>
#include <stdint.h>
#include <json-c/json.h>
#include "editor/scene_editor_pane_host.h"
typedef struct SceneEditorRenderOptions {int width,height,temporal_frames,frame,integrator;uint32_t sample_sequence;} SceneEditorRenderOptions;
bool SceneEditorRenderOptionsValid(const SceneEditorRenderOptions* options);
SceneEditorRenderOptions SceneEditorRenderOptionsGet(void);
bool SceneEditorRenderStart(const char* request_id,const SceneEditorRenderOptions* options,unsigned long long revision,char* message,size_t size);
bool SceneEditorRenderCancel(const char* job_id,char* message,size_t size);
bool SceneEditorRenderActive(void);
void SceneEditorRenderPoll(void);
void SceneEditorRenderShutdown(void);
json_object* SceneEditorRenderRead(void);
json_object* SceneEditorRenderJobRead(const char* job_id,bool results,char* message,size_t size);
const char* SceneEditorRenderLatestId(void);
bool SceneEditorRenderPanelEvent(SDL_Event* event);
void SceneEditorRenderPanelDraw(SDL_Renderer* renderer);
bool SceneEditorRenderPanelDraftActive(void);
bool SceneEditorRenderPanelResultVerified(void);
void SceneEditorRenderPanelReset(void);
bool SceneEditorRenderPanelControl(const char* name,SDL_Rect* out);
#endif
