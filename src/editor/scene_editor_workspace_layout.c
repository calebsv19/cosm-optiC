#include "editor/scene_editor_workspace_layout.h"

#include <string.h>

static SDL_Rect row_item(SDL_Rect row, int index, int count) {
    const int gap = 6;
    int usable = row.w - gap * (count - 1);
    int begin = usable * index / count;
    int end = usable * (index + 1) / count;
    return (SDL_Rect){row.x + begin + gap * index, row.y, end - begin, row.h};
}

void SceneEditorWorkspaceLayoutChrome(const SceneEditorPaneLayout* layout,
                                     SceneEditorWorkspaceChrome* chrome) {
    if (!chrome) return;
    memset(chrome, 0, sizeof(*chrome));
    if (!layout || layout->workspace_header_rect.w <= 0) return;
    SDL_Rect modes = layout->mode_router_rect;
    for (int i = 0; i < SCENE_WORKSPACE_MODE_COUNT; ++i) {
        chrome->modes[i] = row_item(modes, i, SCENE_WORKSPACE_MODE_COUNT);
    }
    chrome->expand = (SDL_Rect){modes.x + modes.w + 8, modes.y, 112, modes.h};
    chrome->restore = (SDL_Rect){chrome->expand.x + chrome->expand.w + 6,
                                modes.y, 112, modes.h};
    for (int i = 0; i < SCENE_WORKSPACE_ACTION_COUNT; ++i) {
        chrome->actions[i] = row_item(layout->workspace_actions_rect,
                                      i, SCENE_WORKSPACE_ACTION_COUNT);
    }
}
