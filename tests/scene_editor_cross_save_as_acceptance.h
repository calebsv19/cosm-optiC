#include "render/runtime_surface_sampling.h"
static void cross_save_as_acceptance(SceneEditor* editor,const char* scene) {
    char destination[4096],message[512];snprintf(destination,sizeof(destination),"%s",scene);*strrchr(destination,'/')=0;
    strncat(destination,"/cross-ui/scene_runtime.json",sizeof(destination)-strlen(destination)-1);
    SceneEditorAgentControlPoll();
    assert(RuntimeSurfaceSamplingActive(0));
    assert(SceneEditorDocumentSurfaceSamplingSetChannel(0,"base_color",getenv("OPTIC_CROSS_RELINK"),"srgb",SceneEditorDocumentRevision(),message,sizeof(message)));
    assert(SceneEditorDocumentIsDirty());
    char failed[4096];snprintf(failed,sizeof(failed),"%s",scene);*strrchr(failed,'/')=0;strncat(failed,"/cross-failed/scene_runtime.json",sizeof(failed)-strlen(failed)-1);
    unsigned long long revision=SceneEditorDocumentRevision();SceneEditorDocumentFailNextForTests(SCENE_DOCUMENT_FAIL_SWITCH_APPLY);
    assert(!SceneEditorSceneCommand("save_as",failed,revision,message,sizeof(message)));
    assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty() && !strcmp(SceneEditorDocumentPath(),scene));assert(RuntimeSurfaceSamplingActive(0));
    lifecycle_ui(editor,4,destination);assert(RuntimeSurfaceSamplingActive(0));
    assert(SceneEditorDocumentUndo(message,sizeof(message)));assert(RuntimeSurfaceSamplingActive(0));
    assert(SceneEditorDocumentRedo(message,sizeof(message)));assert(RuntimeSurfaceSamplingActive(0));
    assert(SceneEditorChromeActionsSaveAuthoring());
    lifecycle_ui(editor,2,scene);assert(RuntimeSurfaceSamplingActive(0));
    pid_t pid=fork();assert(pid>=0);
    if(!pid) {execlp("python3","python3","-B",getenv("OPTIC_CROSS_SAVE_AS_MCP_TEST"),scene,(char*)NULL);_exit(127);}
    bool finished=false;int status=0;
    for(int i=0;i<12000;++i) {SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid) {finished=true;break;}SDL_Delay(10);}
    if(!finished) {kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    assert(!strcmp(SceneEditorDocumentPath(),scene));assert(!SceneEditorDocumentIsDirty());
}
