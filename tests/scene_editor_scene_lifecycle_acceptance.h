#include "editor/scene_editor_scene_commands.h"
#include "editor/scene_editor_agent_control.h"
#include <sys/wait.h>
#include <signal.h>
static void lifecycle_ui(SceneEditor* editor,int row,const char* path) {
    choose_menu(editor,0,row);assert(SceneEditorSceneDialogActive());
    SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_a;event.key.keysym.mod=KMOD_GUI;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);
    size_t length=strlen(path);
    for(size_t i=0;i<length;i+=24) {event=(SDL_Event){0};event.type=SDL_TEXTINPUT;snprintf(event.text.text,sizeof(event.text.text),"%.*s",(int)(length-i>24?24:length-i),path+i);SceneEditorSessionRuntimeHandleEvent(editor,&event);}
    key(editor,SDLK_RETURN);assert(!SceneEditorSceneDialogActive());
    assert(!strcmp(SceneEditorDocumentPath(),path));
}
static void scene_lifecycle_acceptance(SceneEditor* editor,const char* scene) {
    char parent[4096],created[4096],copy[4096];snprintf(parent,sizeof(parent),"%s",scene);*strrchr(parent,'/')=0;
    snprintf(created,sizeof(created),"%s/ui-created.json",parent);snprintf(copy,sizeof(copy),"%s/ui-save-as.json",parent);
    SceneEditorAgentControlPoll();unsigned long long original=SceneEditorDocumentRevision();
    lifecycle_ui(editor,0+3,created);assert(SceneEditorDocumentRevision()>original);assert(SceneEditorDocumentObjectCount()==0);
    int index=-1;char message[512];assert(SceneEditorDocumentCreatePrimitive("box",&index,message,sizeof(message)));
    unsigned long long dirty=SceneEditorDocumentRevision();
    assert(!SceneEditorSceneCommand("open",scene,dirty,message,sizeof(message)));assert(SceneEditorDocumentIsDirty());
    lifecycle_ui(editor,4,copy);assert(!SceneEditorDocumentIsDirty());assert(SceneEditorDocumentObjectCount()==1);
    assert(SceneEditorDocumentCanUndo());assert(SceneEditorDocumentUndo(message,sizeof(message)));assert(SceneEditorDocumentObjectCount()==0);
    assert(SceneEditorDocumentRedo(message,sizeof(message)));assert(SceneEditorDocumentObjectCount()==1);
    assert(SceneEditorChromeActionsSaveAuthoring());
    lifecycle_ui(editor,2,created);assert(SceneEditorDocumentObjectCount()==0);
    lifecycle_ui(editor,2,copy);assert(SceneEditorDocumentObjectCount()==1);
    lifecycle_ui(editor,2,scene);
    pid_t pid=fork();assert(pid>=0);
    if(!pid) {execlp("python3","python3","-B",getenv("OPTIC_LIFECYCLE_MCP_TEST"),scene,(char*)NULL);_exit(127);}
    bool finished=false;int status=0;
    for(int i=0;i<9000;++i) {SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid) {finished=true;break;}SDL_Delay(10);}
    if(!finished) {kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    assert(!strcmp(SceneEditorDocumentPath(),scene));assert(!SceneEditorDocumentIsDirty());
}
