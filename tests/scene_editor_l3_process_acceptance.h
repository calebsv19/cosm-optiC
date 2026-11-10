/* Real editor lifecycle with owned local helpers; fixture paths are explicit. */
#include <sys/wait.h>
#include <errno.h>
#include "editor/scene_editor_managed_import.h"
static pid_t l3_wait_pid(const char *path) {
    for(int i=0;i<200;i++) {FILE *f=fopen(path,"r");long pid=0;
        if(f){int n=fscanf(f,"%ld",&pid);fclose(f);if(n==1 && pid>0)return (pid_t)pid;}
        SDL_Delay(10);
    }
    assert(!"helper readiness timed out");return -1;
}
static void l3_process_acceptance(SceneEditor *editor,const char *scene) {
    char message[1024];
    assert(SceneEditorRigidJobBegin(getenv("OPTIC_L3_PROFILE"),message,sizeof(message)));
    pid_t rigid=l3_wait_pid("run.pid");
    assert(SceneEditorTransformPanelImportSTL(getenv("OPTIC_L3_STL")));
    pid_t imported=l3_wait_pid("apply.pid");
    assert(SceneEditorRigidJobBusy());
    Uint64 start=SDL_GetPerformanceCounter();
    DestroySceneEditor(editor);active_editor=NULL;
    double elapsed=(double)(SDL_GetPerformanceCounter()-start)/SDL_GetPerformanceFrequency();
    assert(elapsed<3.0);
    assert(waitpid(rigid,NULL,WNOHANG)==-1 && errno==ECHILD);
    assert(waitpid(imported,NULL,WNOHANG)==-1 && errno==ECHILD);
    assert(!SceneEditorRigidJobBusy());
    snprintf(animSettings.runtimeScenePath,sizeof(animSettings.runtimeScenePath),"%s",scene);
    assert(InitializeSceneEditor(editor));active_editor=editor;
    assert(SceneEditorDocumentIsOpen());assert(!strcmp(SceneEditorDocumentPath(),scene));
    assert(!SceneEditorRigidJobBusy());
    for(int i=0;i<3;i++){SDL_Delay(30);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);}
    puts("PASS native pending import/rigid close, direct-child reap, and reopen");
    assert(unlink("run.pid")==0);
    assert(SceneEditorRigidJobBegin(getenv("OPTIC_L3_PROFILE"),message,sizeof(message)));
    rigid=l3_wait_pid("run.pid");
    char second[PATH_MAX];assert(getcwd(second,sizeof(second)));strcat(second,"/second.json");
    assert(SceneEditorDocumentSwitch(second,false,message,sizeof(message)));
    FILE *release=fopen("release","w");assert(release);fclose(release);
    for(int i=0;i<200 && SceneEditorRigidJobBusy();i++){SDL_Delay(10);SceneEditorRigidJobPoll(message,sizeof(message));}
    assert(!SceneEditorRigidJobBusy());assert(strstr(message,"source changed"));
    assert(!strcmp(SceneEditorDocumentPath(),second));
    assert(waitpid(rigid,NULL,WNOHANG)==-1 && errno==ECHILD);
    puts("PASS native completed rigid candidate rejected after document replacement");
    assert(SceneEditorDocumentSwitch(scene,false,message,sizeof(message)));
    assert(unlink("apply.pid")==0);
    assert(SceneEditorTransformPanelImportSTL(getenv("OPTIC_L3_STL")));
    imported=l3_wait_pid("apply.pid");char import_id[65];snprintf(import_id,sizeof(import_id),"%s",SceneEditorManagedImportLatestId());
    assert(SceneEditorDocumentSwitch(second,false,message,sizeof(message)));
    unsigned long long second_revision=SceneEditorDocumentRevision();
    release=fopen("import-release","w");assert(release);fclose(release);
    bool terminal=false;for(int i=0;i<200 && !terminal;i++){SDL_Delay(10);terminal=SceneEditorTransformPanelPoll();}
    assert(terminal);assert(!strcmp(SceneEditorDocumentPath(),second));assert(SceneEditorDocumentRevision()==second_revision);
    assert(waitpid(imported,NULL,WNOHANG)==-1 && errno==ECHILD);
    assert(SceneEditorDocumentSwitch(scene,false,message,sizeof(message)));
    json_object *import_receipt=json_object_new_object();assert(SceneEditorManagedImportRead(import_id,import_receipt,message,sizeof(message)));
    json_object *state=authoring_t1_member(authoring_t1_member(import_receipt,"import"),"state");assert(!strcmp(json_object_get_string(state),"failed"));json_object_put(import_receipt);
    puts("PASS native late managed-import candidate rejected after document replacement");
}
