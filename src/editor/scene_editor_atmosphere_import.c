#define _POSIX_C_SOURCE 200809L
#include "editor/scene_editor_atmosphere.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_atmosphere_geometry.h"
#include "editor/scene_editor_atmosphere_viewport.h"
#include "app/data_paths.h"
#include <errno.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;

bool SceneEditorAtmosphereImport(const char* source,bool preset,unsigned long long revision,
    char* d,size_t n) {
    char root[PATH_MAX],script[PATH_MAX],response[8192];int fds[2];pid_t pid;
    if(!SceneEditorDocumentIsOpen() || SceneEditorDocumentRevision()!=revision) {
        if(d && n) snprintf(d,n,"Scene changed; refresh before attaching atmosphere.");return false;
    }
    const char* bundled=getenv("RAY_TRACING_ATMOSPHERE_HELPER");
    bool found=bundled?snprintf(script,sizeof(script),"%s",bundled)<(int)sizeof(script):
        ray_tracing_find_program_root(root,sizeof(root)) &&
        snprintf(script,sizeof(script),"%s/tools/scene_atmosphere_assets.py",root)<(int)sizeof(script);
    if(!source || !*source || !found || access(script,R_OK)!=0 || pipe(fds)!=0) {
        if(d && n) snprintf(d,n,"Atmosphere import helper unavailable.");return false;
    }
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,fds[1],STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions,fds[1],STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions,fds[0]);
    posix_spawn_file_actions_addclose(&actions,fds[1]);
    char* args[]={"python3",script,"--scene",(char*)SceneEditorDocumentPath(),
        preset?"--preset":"--source",(char*)source,NULL};
    int error=posix_spawnp(&pid,"python3",&actions,NULL,args,environ);
    posix_spawn_file_actions_destroy(&actions);close(fds[1]);
    if(error) {close(fds[0]);if(d && n) snprintf(d,n,"Cannot start atmosphere importer: %s",strerror(error));return false;}
    size_t used=0;char chunk[1024];ssize_t bytes;
    while((bytes=read(fds[0],chunk,sizeof(chunk)))>0 || (bytes<0 && errno==EINTR)) {
        if(bytes<0) continue;
        size_t copy=(size_t)bytes;if(copy>sizeof(response)-1-used) copy=sizeof(response)-1-used;
        memcpy(response+used,chunk,copy);used+=copy;
    }
    close(fds[0]);response[used]=0;int status;
    while(waitpid(pid,&status,0)<0) {if(errno!=EINTR) return false;}
    json_object* result=json_tokener_parse(response),*path=NULL,*kind=NULL;
    bool ok=WIFEXITED(status) && WEXITSTATUS(status)==0 && result &&
        json_object_object_get_ex(result,"source_path",&path) && json_object_is_type(path,json_type_string) &&
        json_object_object_get_ex(result,"source_kind",&kind) && json_object_is_type(kind,json_type_string);
    if(!ok) {
        if(d && n) snprintf(d,n,"Atmosphere import failed: %.400s",response);
        if(result) json_object_put(result);return false;
    }
    SceneAtmosphereAttachment value,previous;
    bool replacing=SceneEditorAtmosphereRead(&previous) && previous.present;
    if(replacing) value=previous;else SceneAtmosphereAttachmentDefaults(&value);
    value.present=true;value.animated=false;value.source_start=0;value.timeline_start=0;
    snprintf(value.source_path,sizeof(value.source_path),"%s",json_object_get_string(path));
    snprintf(value.source_kind,sizeof(value.source_kind),"%s",json_object_get_string(kind));
    json_object_put(result);
    if(!replacing) {
        RuntimeVolumeAttachment3D volume;
        if(SceneEditorAtmosphereLoadGeometry(&value,&volume,d,n)) {
            bool fit=SceneEditorAtmosphereFit(&value,&volume,preset?source:NULL);
            RuntimeVolumeAttachment3D_Free(&volume);
            if(!fit) return false;
        } else {RuntimeVolumeAttachment3D_Free(&volume);return false;}
    }
    bool applied=SceneEditorAtmosphereSet(&value,revision,d,n);
    if(applied) {
        SceneEditorAtmosphereSelect(true);SceneEditorAtmosphereFrame();
        if(d && n) snprintf(d,n,replacing?"Atmosphere replaced; placement and appearance preserved. Undo available.":
            "Atmosphere loaded and selected. Edit its volume in the Inspector; File Save to retain.");
    }
    return applied;
}
