#define _XOPEN_SOURCE 700
#include "editor/scene_editor_save_as_assets.h"
#include "editor/scene_editor_document.h"
#include "scene_editor_document_transaction.h"
#include "app/data_paths.h"
#include <json-c/json.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
static bool invoke(const char* target,const char* stage,const char* finish,char* message,size_t size) {
    char script[PATH_MAX],root[PATH_MAX],response[2048];int fds[2];pid_t pid;
    const char* bundled=getenv("RAY_TRACING_SAVE_AS_HELPER");
    bool found=bundled?snprintf(script,sizeof(script),"%s",bundled)<(int)sizeof(script):
        ray_tracing_find_program_root(root,sizeof(root)) && snprintf(script,sizeof(script),"%s/tools/scene_editor_save_as_assets.py",root)<(int)sizeof(script);
    if(!found || access(script,R_OK)!=0 || pipe(fds)!=0) {snprintf(message,size,"Save As dependency helper unavailable.");return false;}
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,fds[1],STDOUT_FILENO);posix_spawn_file_actions_adddup2(&actions,fds[1],STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions,fds[0]);posix_spawn_file_actions_addclose(&actions,fds[1]);
    char* prepare[]={"python3","-B",script,"--source-scene",(char*)SceneEditorDocumentPath(),"--destination",(char*)target,"--stage",(char*)stage,NULL};
    char* end[]={"python3","-B",script,"--stage",(char*)stage,"--finish",(char*)finish,NULL};
    int error=posix_spawnp(&pid,"python3",&actions,NULL,finish?end:prepare,environ);posix_spawn_file_actions_destroy(&actions);close(fds[1]);
    if(error) {close(fds[0]);snprintf(message,size,"Cannot start Save As helper: %s",strerror(error));return false;}
    size_t used=0;char chunk[1024];ssize_t bytes;
    while((bytes=read(fds[0],chunk,sizeof(chunk)))>0 || (bytes<0 && errno==EINTR)) {
        if(bytes<0) continue;size_t copy=(size_t)bytes;if(copy>sizeof(response)-1-used) copy=sizeof(response)-1-used;
        memcpy(response+used,chunk,copy);used+=copy;
    }
    close(fds[0]);response[used]=0;int status;
    while(waitpid(pid,&status,0)<0) if(errno!=EINTR) return false;
    json_object* result=json_tokener_parse(response),*ok=NULL;
    bool passed=WIFEXITED(status) && WEXITSTATUS(status)==0 && result && json_object_object_get_ex(result,"ok",&ok) && json_object_get_boolean(ok);
    if(!passed) snprintf(message,size,"Save As dependencies failed: %.400s",response);
    if(result) json_object_put(result);return passed;
}
bool SceneEditorSaveAsAssetsFinish(const char* stage,bool commit,char* message,size_t size) {
    if(!stage || !*stage) return true;
    if(access(stage,F_OK)!=0 && errno==ENOENT) return true;
    return invoke(NULL,stage,commit?"commit":"rollback",message,size);
}
bool SceneEditorSaveAsAssetsBegin(const char* target,char* stage,size_t capacity,char* message,size_t size) {
    char parent[PATH_MAX],snapshot_path[PATH_MAX];snprintf(parent,sizeof(parent),"%s",target);char* slash=strrchr(parent,'/');if(!slash) return false;*slash=0;
    if(snprintf(stage,capacity,"%s/.optic-save-as-XXXXXX",parent)>=(int)capacity || !mkdtemp(stage)) {snprintf(message,size,"Cannot create private Save As staging.");stage[0]=0;return false;}
    snprintf(snapshot_path,sizeof(snapshot_path),"%s/snapshot.json",stage);char* snapshot=document_dependency_roots_json();
    int fd=snapshot && strlen(snapshot)<=64*1024*1024?open(snapshot_path,O_CREAT|O_EXCL|O_WRONLY,0600):-1;bool ok=fd>=0;
    if(ok) for(size_t at=0,length=strlen(snapshot);at<length;) {ssize_t n=write(fd,snapshot+at,length-at);if(n<0 && errno==EINTR) continue;if(n<=0) {ok=false;break;}at+=n;}
    if(fd>=0) {if(fsync(fd)!=0) ok=false;close(fd);}free(snapshot);
    if(!ok) {unlink(snapshot_path);rmdir(stage);stage[0]=0;snprintf(message,size,"Cannot snapshot working scene and retained history.");return false;}
    if(invoke(target,stage,NULL,message,size)) return true;
    /* Discovery failures have not created assets/receipt. Remove only our file.
       A helper failure after publication retains an ownership receipt for recovery. */
    char receipt[PATH_MAX];snprintf(receipt,sizeof(receipt),"%s/receipt.json",stage);
    if(access(receipt,F_OK)!=0) {unlink(snapshot_path);rmdir(stage);}
    return false;
}
