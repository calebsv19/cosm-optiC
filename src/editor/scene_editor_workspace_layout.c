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
    SDL_Rect modes = layout->mode_router_rect;
    static const int action_widths[] = {64, 52, 62, 72, 72, 58, 56, 86, 58};
    static const int document_widths[]={190,80,100,48,48};
    chrome->workspace=row_item(modes,0,5,document_widths);
    chrome->frame_all=row_item(modes,1,5,document_widths);
    chrome->frame_selected=row_item(modes,2,5,document_widths);
    chrome->undo=row_item(modes,3,5,document_widths);
    chrome->redo=row_item(modes,4,5,document_widths);
    int menu_width=chrome->workspace.w;
    for (int i = 0; i < SCENE_WORKSPACE_MODE_COUNT; ++i) {
        chrome->modes[i] = (SDL_Rect){modes.x,modes.y+modes.h+4+i*(modes.h+4),
                                      menu_width,modes.h+4};
    }
    chrome->expand = (SDL_Rect){modes.x + modes.w + 8, modes.y, 112, modes.h};
    chrome->restore = (SDL_Rect){chrome->expand.x + chrome->expand.w + 6,
                                modes.y, 112, modes.h};
    for (int i = 0; i < SCENE_WORKSPACE_ACTION_COUNT; ++i) {
        chrome->actions[i] = row_item(layout->workspace_actions_rect,
                                      i, SCENE_WORKSPACE_ACTION_COUNT, action_widths);
    }
}
