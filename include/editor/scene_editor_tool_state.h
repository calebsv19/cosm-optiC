#ifndef SCENE_EDITOR_TOOL_STATE_H
#define SCENE_EDITOR_TOOL_STATE_H

#include <stdbool.h>

#include <SDL2/SDL.h>

typedef enum SceneEditorTool {
    SCENE_EDITOR_TOOL_SELECT = 0,
    SCENE_EDITOR_TOOL_ADD = 1,
    SCENE_EDITOR_TOOL_DELETE = 2
} SceneEditorTool;

void SceneEditorToolStateReset(void);
void SceneEditorToolStateSetActive(SceneEditorTool tool);
void SceneEditorToolStateToggleOrReset(SceneEditorTool tool);
SceneEditorTool SceneEditorToolStateGetActive(void);
bool SceneEditorToolStateToolIsActive(SceneEditorTool tool);
SceneEditorTool SceneEditorToolStateResolveEffective(SceneEditorTool active_tool, SDL_Keymod mods);
SceneEditorTool SceneEditorToolStateGetEffective(SDL_Keymod mods);
const char* SceneEditorToolStateToolLabel(SceneEditorTool tool);

#endif
