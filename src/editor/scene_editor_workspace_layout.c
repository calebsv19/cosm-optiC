#include "editor/scene_editor_workspace_layout.h"

#include <string.h>

static SDL_Rect row_item(SDL_Rect row, int index, int count, const int* widths) {
    const int gap = 4;
    int usable = row.w - gap * (count - 1);
    int total = 0, prefix = 0;
    for (int i = 0; i < count; ++i) {
        total += widths[i];
        if (i < index) prefix += widths[i];
    }
    if (usable <= 0 || row.h <= 0) return (SDL_Rect){0};
    /* Grow with text scale, but never stretch tabs across unused window space. */
    int natural = total * row.h / 24;
    if (usable > natural) usable = natural;
    int begin = usable * prefix / total;
    int end = usable * (prefix + widths[index]) / total;
    return (SDL_Rect){row.x + begin + gap * index, row.y, end - begin, row.h};
}

void SceneEditorWorkspaceLayoutChrome(const SceneEditorPaneLayout* layout,
                                     SceneEditorWorkspaceChrome* chrome) {
    if (!chrome) return;
    memset(chrome, 0, sizeof(*chrome));
    if (!layout || layout->workspace_header_rect.w <= 0) return;
    SDL_Rect document = layout->mode_router_rect;
    SDL_Rect workspaces = layout->workspace_actions_rect;
    SDL_Rect tools = layout->viewport_tools_rect;
    static const int document_widths[] = {420, 58, 58, 70, 74, 96};
    static const int workspace_widths[] = {82, 72, 88, 76, 112, 72, 104, 96};
    static const int tool_widths[] = {58, 48, 58, 58, 62, 58, 62, 72, 72, 94, 66, 58, 78};

    chrome->document_identity = row_item(document, 0, 6, document_widths);
    chrome->undo = row_item(document, 1, 6, document_widths);
    chrome->redo = row_item(document, 2, 6, document_widths);
    chrome->actions[6] = row_item(document, 3, 6, document_widths);
    chrome->actions[3] = row_item(document, 4, 6, document_widths);
    chrome->actions[8] = row_item(document, 5, 6, document_widths);

    chrome->workspace = row_item(workspaces, 0, 8, workspace_widths);
    for (int i = 0; i < SCENE_WORKSPACE_MODE_COUNT; ++i)
        chrome->modes[i] = row_item(workspaces, i + 1, 8, workspace_widths);
    chrome->expand = row_item(workspaces, 6, 8, workspace_widths);
    chrome->restore = row_item(workspaces, 7, 8, workspace_widths);

    chrome->actions[0] = row_item(tools, 0, 13, tool_widths);
    chrome->actions[1] = row_item(tools, 1, 13, tool_widths);
    chrome->actions[2] = row_item(tools, 2, 13, tool_widths);
    chrome->transforms[0] = row_item(tools, 3, 13, tool_widths);
    chrome->transforms[1] = row_item(tools, 4, 13, tool_widths);
    chrome->transforms[2] = row_item(tools, 5, 13, tool_widths);
    chrome->transform_space = row_item(tools, 6, 13, tool_widths);
    chrome->transform_snap = row_item(tools, 7, 13, tool_widths);
    chrome->frame_all = row_item(tools, 8, 13, tool_widths);
    chrome->frame_selected = row_item(tools, 9, 13, tool_widths);
    chrome->actions[4] = row_item(tools, 10, 13, tool_widths);
    chrome->actions[5] = row_item(tools, 11, 13, tool_widths);
    chrome->actions[7] = row_item(tools, 12, 13, tool_widths);
    chrome->gizmo_label = (SDL_Rect){0};
}
