#include "ui/menu_path_edit.h"
#include "ui/menu_batch_panel.h"
#include "config/config_manager.h"
#include "config/core/config_runtime_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool active(const MenuRuntimeState* s) {
    return s && (s->editingInputRoot || s->editingMeshAssetRoot ||
        s->editingOutputRoot || s->editingFrameDir || s->editingVideoOutputRoot);
}
static bool error(MenuRuntimeState* s,const char* message) {
    snprintf(s->statusLabel,sizeof(s->statusLabel),"%s",message);
    s->statusColor=(SDL_Color){255,170,140,255};s->statusExpireMs=SDL_GetTicks()+8000;
    return false;
}
void menu_path_edit_begin(MenuRuntimeState* s) {
    if(!s) return;
    s->pathSelectAll=true;SDL_StartTextInput();
}
void menu_path_edit_end(MenuRuntimeState* s) {
    if(!s) return;
    s->pathSelectAll=false;SDL_StopTextInput();
}
bool menu_path_edit_insert(MenuRuntimeState* s,const char* text) {
    if(!active(s) || !text) return false;
    size_t old=s->pathSelectAll?0:strlen(s->pathInputBuffer),n=strlen(text);
    for(size_t i=0;i<n;++i) if((unsigned char)text[i]<32 || text[i]==127)
        return error(s,"Folder paths cannot contain tabs or line breaks.");
    if(old+n>=sizeof(s->pathInputBuffer)) return error(s,"Folder path is too long.");
    memcpy(s->pathInputBuffer+old,text,n+1);s->pathSelectAll=false;return true;
}
bool menu_path_edit_key(MenuRuntimeState* s,const SDL_Event* e) {
    if(!active(s) || !e || e->type!=SDL_KEYDOWN) return false;
    SDL_Keycode key=e->key.keysym.sym;
    bool command=(e->key.keysym.mod&(KMOD_GUI|KMOD_CTRL))!=0;
    if(command && key==SDLK_a) {s->pathSelectAll=true;return true;}
    if(command && key==SDLK_v) {
        char* text=SDL_GetClipboardText();
        if(text) {menu_path_edit_insert(s,text);SDL_free(text);}return true;
    }
    if(command && key==SDLK_c) {if(s->pathSelectAll) SDL_SetClipboardText(s->pathInputBuffer);return true;}
    if(key==SDLK_BACKSPACE || key==SDLK_DELETE) {
        size_t n=strlen(s->pathInputBuffer);
        if(s->pathSelectAll) s->pathInputBuffer[0]=0;
        else if(key==SDLK_BACKSPACE && n) {
            do {--n;} while(n && ((unsigned char)s->pathInputBuffer[n]&0xc0)==0x80);
            s->pathInputBuffer[n]=0;
        }
        s->pathSelectAll=false;return true;
    }
    if(key==SDLK_END || key==SDLK_RIGHT) {s->pathSelectAll=false;return true;}
    return false;
}
bool menu_path_edit_apply_output(MenuRuntimeState* s,const char* path,bool frames) {
    char chosen[PATH_MAX],new_frames[PATH_MAX];size_t n;
    if(!s || !path) return false;
    if(path[0]=='~' && path[1]=='/' && getenv("HOME"))
        n=(size_t)snprintf(chosen,sizeof(chosen),"%s/%s",getenv("HOME"),path+2);
    else n=(size_t)snprintf(chosen,sizeof(chosen),"%s",path);
    if(n>=sizeof(chosen)) return error(s,"Folder path is too long.");
    while(n>1 && chosen[n-1]=='/') chosen[--n]=0;
    bool parent=false;
    for(const char* component=chosen; *component;) {
        while(*component=='/') ++component;
        const char* end=strchr(component,'/');size_t length=end?(size_t)(end-component):strlen(component);
        if(length==2 && !strncmp(component,"..",2)) parent=true;
        if(!end) break;
        component=end+1;
    }
    if(chosen[0]!='/' || parent)
        return error(s,"Choose an absolute folder path without '..'.");
    for(size_t i=0;i<n;++i) if((unsigned char)chosen[i]<32 || chosen[i]==127)
        return error(s,"Folder paths cannot contain tabs or line breaks.");
    if(frames) {
        if(n==1) return error(s,"Choose a frames folder beneath an output folder.");
        if(n>=sizeof(animSettings.frameDir)) return error(s,"Frames folder path is too long.");
        snprintf(animSettings.frameDir,sizeof(animSettings.frameDir),"%s",chosen);
        (void)config_runtime_paths_reconcile_frame_output_root();
    } else {
        const char* suffix="frames/default";size_t old=strlen(animSettings.outputRoot);
        while(old>1 && animSettings.outputRoot[old-1]=='/') --old;
        if(old && !strncmp(animSettings.frameDir,animSettings.outputRoot,old) && animSettings.frameDir[old]=='/')
            suffix=animSettings.frameDir+old+1;
        if(n>=sizeof(animSettings.outputRoot) ||
            snprintf(new_frames,sizeof(new_frames),"%s%s%s",chosen,n==1?"":"/",suffix)>=(int)sizeof(animSettings.frameDir))
            return error(s,"Output folder leaves no room for its frames path.");
        snprintf(animSettings.outputRoot,sizeof(animSettings.outputRoot),"%s",chosen);
        snprintf(animSettings.frameDir,sizeof(animSettings.frameDir),"%s",new_frames);
    }
    (void)setenv("RAY_TRACING_OUTPUT_ROOT",animSettings.outputRoot,1);
    menu_batch_panel_refresh(s);
    if(!SaveAnimationConfigChecked()) return error(s,"Folder changed in memory; settings save failed. Retry Save before rendering.");
    snprintf(s->statusLabel,sizeof(s->statusLabel),"%s folder applied",frames?"Frames":"Output");
    s->statusColor=(SDL_Color){120,220,180,255};s->statusExpireMs=SDL_GetTicks()+3000;
    return true;
}
