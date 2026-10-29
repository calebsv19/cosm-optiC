#define _XOPEN_SOURCE 700
#include "scene_editor_rigid_job.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_timeline.h"
#include <json-c/json.h>
#ifdef RAY_RIGID_RESULT_ENABLED
#include "core_scene_rigid_result.h"
#define RIGID_JOB_MAX_TICKS CORE_SCENE_RIGID_RESULT_TICKS
#else
#define RIGID_JOB_MAX_TICKS 0u
#endif
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
static pid_t child;
static char source[4096],destination[4096],log_path[4096];
static unsigned long long revision;
static const char *str(json_object *o,const char *key) {
    json_object *v=NULL;json_object_object_get_ex(o,key,&v);
    return json_object_is_type(v,json_type_string)?json_object_get_string(v):"";
}
bool SceneEditorRigidJobBusy(void) {return child>0;}
bool SceneEditorRigidJobBegin(const char *profile,char *message,size_t size) {
    if(child || !SceneEditorDocumentIsOpen() || SceneEditorDocumentIsDirty()) {
        snprintf(message,size,"Save the scene first; only one local preparation can run.");return false;
    }
    json_object *o=json_object_from_file(profile);
    if(!o || strcmp(str(o,"schema"),"optic_rigid_run_profile_v1")) {
        if(o) json_object_put(o);snprintf(message,size,"Expected optic_rigid_run_profile_v1.");return false;
    }
    const char *keys[]={"workflow","ball_tool","backend","pack_io","assets","store","renderer","output"};
    for(size_t i=0;i<8;i++) if(str(o,keys[i])[0]!='/' || strlen(str(o,keys[i]))>3400) {
        json_object_put(o);snprintf(message,size,"Profile paths must be explicit absolute local paths.");return false;
    }
    const char *head=str(o,"expected_head"),*ticks=str(o,"ticks");
    if((strcmp(head,"none") && (strlen(head)!=64 || strspn(head,"0123456789abcdef")!=64)) ||
       !ticks[0] || strlen(ticks)>5 || strspn(ticks,"0123456789")!=strlen(ticks) || atoi(ticks)<1 || (unsigned)atoi(ticks)>RIGID_JOB_MAX_TICKS) {
        json_object_put(o);snprintf(message,size,"Profile needs expected_head (none or SHA-256) and ticks (1..%u string).",RIGID_JOB_MAX_TICKS);return false;
    }
    json_object *wall_value=NULL;
    bool has_wall=json_object_object_get_ex(o,"wall_seconds",&wall_value);
    if(has_wall && !json_object_is_type(wall_value,json_type_string)) {
        json_object_put(o);snprintf(message,size,"wall_seconds must be a decimal string.");return false;
    }
    const char *wall=has_wall?json_object_get_string(wall_value):"60";
    if(!wall[0] || strlen(wall)>4 || strspn(wall,"0123456789")!=strlen(wall) || atoi(wall)<1 || atoi(wall)>1800) {
        json_object_put(o);snprintf(message,size,"wall_seconds must be a decimal string in 1..1800.");return false;
    }
    snprintf(source,sizeof(source),"%s",SceneEditorDocumentPath());
    snprintf(destination,sizeof(destination),"%s/scene.json",str(o,"output"));
    snprintf(log_path,sizeof(log_path),"/tmp/optic-rigid-job-XXXXXX");
    int fd=mkstemp(log_path);
    if(fd<0) {json_object_put(o);snprintf(message,size,"Cannot create local preparation log.");return false;}
    char *args[]={"python3","-B",(char*)str(o,"workflow"),"run","--scene",source,
      "--ball-tool",(char*)str(o,"ball_tool"),"--backend",(char*)str(o,"backend"),
      "--pack-io",(char*)str(o,"pack_io"),"--assets",(char*)str(o,"assets"),
      "--store",(char*)str(o,"store"),"--renderer",(char*)str(o,"renderer"),
      "--output",(char*)str(o,"output"),"--expected-head",(char*)head,"--ticks",(char*)ticks,"--wall-seconds",(char*)wall,NULL};
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,fd,STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions,fd,STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions,fd);
    pid_t pid=0;int error=posix_spawnp(&pid,"python3",&actions,NULL,args,environ);
    posix_spawn_file_actions_destroy(&actions);close(fd);json_object_put(o);
    if(error) {unlink(log_path);snprintf(message,size,"Cannot start local preparation: %s",strerror(error));return false;}
    child=pid;revision=SceneEditorDocumentRevision();SceneEditorTimelinePause();
    snprintf(message,size,"Preparing locally. The current result remains active until completion.");return true;
}
void SceneEditorRigidJobPoll(char *message,size_t size) {
    if(!child) return;
    int status=0;pid_t result=waitpid(child,&status,WNOHANG);
    if(result==0 || (result<0 && errno==EINTR)) return;
    child=0;
    if(result<0 || !WIFEXITED(status) || WEXITSTATUS(status)!=0) {
        FILE *f=fopen(log_path,"rb");char detail[512]={0};
        if(f) {size_t n=fread(detail,1,sizeof(detail)-1,f);detail[n]=0;fclose(f);}
        snprintf(message,size,"Preparation failed: %.320s. Log: %s",detail,log_path);return;
    }
    if(strcmp(source,SceneEditorDocumentPath()) || revision!=SceneEditorDocumentRevision() || SceneEditorDocumentIsDirty()) {
        snprintf(message,size,"Preparation ready at %s; source changed, so it was not opened.",destination);unlink(log_path);return;
    }
    if(SceneEditorDocumentSwitch(destination,false,message,size)) {
        snprintf(message,size,"Prepared result opened. Source scene retained at %s",source);
        unlink(log_path);
    }
}
