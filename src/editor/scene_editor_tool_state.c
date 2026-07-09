#include "editor/scene_editor_tool_state.h"

static SceneEditorTool g_scene_editor_active_tool = SCENE_EDITOR_TOOL_SELECT;

static SceneEditorTool scene_editor_tool_state_clamp(SceneEditorTool tool) {
    switch (tool) {
        case SCENE_EDITOR_TOOL_SELECT:
        case SCENE_EDITOR_TOOL_ADD:
        case SCENE_EDITOR_TOOL_DELETE:
            return tool;
        default:
            return SCENE_EDITOR_TOOL_SELECT;
    }
}

void SceneEditorToolStateReset(void) {
    g_scene_editor_active_tool = SCENE_EDITOR_TOOL_SELECT;
}

void SceneEditorToolStateSetActive(SceneEditorTool tool) {
    g_scene_editor_active_tool = scene_editor_tool_state_clamp(tool);
}

void SceneEditorToolStateToggleOrReset(SceneEditorTool tool) {
    SceneEditorTool clamped_tool = scene_editor_tool_state_clamp(tool);
    if (g_scene_editor_active_tool == clamped_tool) {
        g_scene_editor_active_tool = SCENE_EDITOR_TOOL_SELECT;
        return;
    }
    g_scene_editor_active_tool = clamped_tool;
}

SceneEditorTool SceneEditorToolStateGetActive(void) {
    return g_scene_editor_active_tool;
}

bool SceneEditorToolStateToolIsActive(SceneEditorTool tool) {
    return SceneEditorToolStateGetActive() == scene_editor_tool_state_clamp(tool);
}

SceneEditorTool SceneEditorToolStateResolveEffective(SceneEditorTool active_tool, SDL_Keymod mods) {
    SceneEditorTool clamped_tool = scene_editor_tool_state_clamp(active_tool);
    if ((mods & KMOD_SHIFT) != 0 && clamped_tool == SCENE_EDITOR_TOOL_SELECT) {
        return SCENE_EDITOR_TOOL_ADD;
    }
    return clamped_tool;
}

SceneEditorTool SceneEditorToolStateGetEffective(SDL_Keymod mods) {
    return SceneEditorToolStateResolveEffective(g_scene_editor_active_tool, mods);
}

const char* SceneEditorToolStateToolLabel(SceneEditorTool tool) {
    switch (scene_editor_tool_state_clamp(tool)) {
        case SCENE_EDITOR_TOOL_SELECT:
            return "Select";
        case SCENE_EDITOR_TOOL_ADD:
            return "Add";
        case SCENE_EDITOR_TOOL_DELETE:
            return "Delete";
        default:
            return "Select";
    }
}
