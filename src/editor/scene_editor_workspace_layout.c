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
    SDL_Rect tools = layout->viewport_tools_rect;
    int h=document.h;
    int menu_w=50*h/24;
    for (int i=0;i<3;++i) chrome->menus[i]=(SDL_Rect){document.x+i*menu_w,document.y,menu_w-4,h};
    chrome->actions[3]=(SDL_Rect){document.x+document.w-90*h/24,document.y,90*h/24,h};
    chrome->document_identity=(SDL_Rect){document.x+3*menu_w+12,document.y,
        document.w-3*menu_w-90*h/24-24,h};
    int display_w=84*h/24;
    chrome->display_mode=(SDL_Rect){tools.x+tools.w-display_w,tools.y,display_w,h};
    tools.w-=display_w+8;
    /* The center header owns workspace and transform tools. Extra settings live in View. */
    static const int widths[]={112,48,48,54,48};
    chrome->workspace=row_item(tools,0,5,widths);
    int context_x=chrome->workspace.x+chrome->workspace.w+6;
    int context_w=(tools.x+tools.w-context_x-4)/2;
    if(context_w>90*h/24) context_w=90*h/24;
    for(int i=0;i<2;++i) chrome->context_views[i]=(SDL_Rect){context_x+i*(context_w+4),tools.y,context_w,h};
    chrome->actions[0]=row_item(tools,1,5,widths);
    for(int i=0;i<3;++i) chrome->transforms[i]=row_item(tools,i+2,5,widths);
    if (!layout->viewport_expanded)
        chrome->actions[1]=(SDL_Rect){layout->left_pane_rect.x+layout->left_pane_rect.w-60,
            tools.y,50,h};
    /* Popup row geometry is stable for keyboard and native interaction tests. */
    for(int i=0;i<5;++i) chrome->modes[i]=(SDL_Rect){chrome->workspace.x,tools.y+h+4+i*(h+4),210,h+4};
}
