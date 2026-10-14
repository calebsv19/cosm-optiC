#include "editor/scene_editor_scene_commands.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_typography.h"
#include "editor/scene_editor_chrome_shell.h"
#include <stdio.h>
#include "editor/scene_editor_shared_text.h"
#include <string.h>
static bool active;
static char operation[16],path[4096],feedback[512];
static unsigned long long revision;
static SceneEditorSharedText draft;
bool SceneEditorSceneDialogActive(void) {return active;}
void SceneEditorSceneDialogOpen(const char* command) {
    if(active) return;
    snprintf(operation,sizeof(operation),"%s",command);
    snprintf(path,sizeof(path),"%s",SceneEditorDocumentPath());
    scene_editor_shared_text_bind(&draft,path,sizeof(path),KIT_UI_TEXT_SINGLE_LINE);
    feedback[0]=0;revision=SceneEditorDocumentRevision();active=true;SDL_StartTextInput();
}
bool SceneEditorSceneDialogEvent(SDL_Event* event) {
    if(!active) return false;
    if(event->type==SDL_QUIT || (event->type==SDL_WINDOWEVENT && event->window.event==SDL_WINDOWEVENT_CLOSE)) {active=false;SDL_StopTextInput();return false;}
    KitUiTextEventResult result=scene_editor_shared_text_event(&draft,event);
    if(result.cancel) {active=false;SDL_StopTextInput();}
    else if(result.submit) {
        active=false;SDL_StopTextInput();
        if(!SceneEditorSceneCommand(operation,path,revision,feedback,sizeof(feedback))) {active=true;SDL_StartTextInput();}
        else SceneEditorChromeShellSetActionFeedback(feedback,6000);
    }
    if(result.status.code!=CORE_OK) snprintf(feedback,sizeof(feedback),"Path edit rejected: %s",result.status.message);
    if(event->type==SDL_DROPFILE) {SDL_free(event->drop.file);event->drop.file=NULL;}
    return event->type!=SDL_WINDOWEVENT && event->type!=SDL_QUIT;
}
void SceneEditorSceneDialogDraw(SDL_Renderer* renderer) {
    if(!active) return;
    SceneEditorPaneLayout layout;if(!SceneEditorGetPaneLayout(&layout)) return;
    RayTracingThemePalette p=SceneEditorChromeShellResolvePalette();
    SDL_Rect previous;SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&previous);SDL_RenderSetClipRect(renderer,NULL);
    int width=layout.workspace_header_rect.w-40;if(width>680)width=680;if(width<240)width=240;
    SDL_Rect box={layout.workspace_header_rect.x+20,100,width,220};
    SDL_SetRenderDrawColor(renderer,p.panel_fill.r,p.panel_fill.g,p.panel_fill.b,255);SDL_RenderFillRect(renderer,&box);
    static const char* open_title="Open scene";const char* title=!strcmp(operation,"open")?open_title:!strcmp(operation,"create")?"Create scene":"Save scene as";
    SceneEditorLabelLeft(renderer,(SDL_Rect){box.x+16,box.y+12,box.w-32,24},title,p.text_primary);
    SDL_Rect field={box.x+16,box.y+52,box.w-32,32};
    SDL_SetRenderDrawColor(renderer,p.background_fill.r,p.background_fill.g,p.background_fill.b,255);SDL_RenderFillRect(renderer,&field);
    SDL_SetRenderDrawColor(renderer,p.accent_primary.r,p.accent_primary.g,p.accent_primary.b,255);SDL_RenderDrawRect(renderer,&field);
    scene_editor_shared_text_draw(&draft,renderer,field,p.text_primary);
    SceneEditorLabelWrapped(renderer,(SDL_Rect){box.x+16,box.y+90,box.w-32,110},feedback[0]?feedback:"Absolute runtime JSON path. Cmd/Ctrl+A selects the path. Enter applies; Escape cancels. Open/Create require saved edits. Save As copies contained dependencies into a different directory.",p.text_primary);
    SDL_RenderSetClipRect(renderer,clipped?&previous:NULL);
}
