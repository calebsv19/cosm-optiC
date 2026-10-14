#define _XOPEN_SOURCE 700
#include "editor/scene_editor_scene_commands.h"
#include "editor/scene_editor_save_as_assets.h"
#include "editor/scene_editor_document.h"
#include "config/config_manager.h"
#include "editor/scene_editor_command_guard.h"
#include "editor/scene_editor_agent_control.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_render_control.h"
#include "scene_editor_document_transaction.h"
#include <json-c/json.h>
#include "app/ray_tracing_sha256.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static bool refuse(char* message,size_t size,const char* why) {snprintf(message,size,"%s",why);return false;}
/* Resolve only a real parent. Never replace a destination or follow a leaf symlink. */
static bool destination(const char* path,char* resolved,char* parent) {
    if(!path || path[0]!='/' || strlen(path)>=PATH_MAX) return false;
    char copy[PATH_MAX];snprintf(copy,sizeof(copy),"%s",path);char* leaf=strrchr(copy,'/');
    if(!leaf || !leaf[1] || !strcmp(leaf+1,".") || !strcmp(leaf+1,"..")) return false;
    char name[PATH_MAX];snprintf(name,sizeof(name),"%s",leaf+1);*leaf=0;
    if(!realpath(copy[0]?copy:"/",parent)) return false;
    return snprintf(resolved,PATH_MAX,"%s/%s",parent,name)<PATH_MAX;
}
static bool publish(const char* path,const char* bytes,char* message,size_t size) {
    char temp[PATH_MAX];if(snprintf(temp,sizeof(temp),"%s.lifecycle-XXXXXX",path)>=PATH_MAX) return false;
    int fd=mkstemp(temp);if(fd<0) return refuse(message,size,"Cannot stage destination scene.");
    bool ok=true;size_t length=strlen(bytes);
    for(size_t at=0;at<length;) {ssize_t n=write(fd,bytes+at,length-at);if(n<0 && errno==EINTR) continue;if(n<=0) {ok=false;break;}at+=n;}
    if(fsync(fd)!=0) ok=false;close(fd);
    /* Atomic no-clobber publication, including racing writers. */
    if(ok && link(temp,path)!=0) ok=false;
    unlink(temp);
    if(!ok) return refuse(message,size,"Destination exists or scene publication failed; nothing overwritten.");
    char parent[PATH_MAX],unused[PATH_MAX];destination(path,unused,parent);
    fd=open(parent,O_RDONLY);bool synced=fd>=0 && fsync(fd)==0;if(fd>=0) close(fd);
    if(!synced) {unlink(path);return refuse(message,size,"Destination directory sync failed.");}
    return true;
}
bool SceneEditorSceneCommand(const char* operation,const char* path,unsigned long long revision,char* message,size_t size) {
    if(!SceneEditorDocumentIsOpen() || revision!=SceneEditorDocumentRevision()) return refuse(message,size,"Read the current scene revision before switching.");
    if(SceneEditorCommandInteractionActive()) return refuse(message,size,"Finish or cancel the active editor operation first.");
    bool opening=!strcmp(operation,"open"),creating=!strcmp(operation,"create"),saving=!strcmp(operation,"save_as");
    if(!opening && !creating && !saving) return refuse(message,size,"Unknown scene lifecycle command.");
    if(!saving && SceneEditorDocumentIsDirty()) return refuse(message,size,"Save the current scene before opening or creating another scene.");
    char target[PATH_MAX],parent[PATH_MAX];
    if(!destination(path,target,parent)) return refuse(message,size,"Supply an absolute scene path with an existing real parent.");
    if(strlen(target)>=sizeof(animSettings.runtimeScenePath)) return refuse(message,size,"Scene path exceeds runtime path capacity.");
    if(opening) {char real[PATH_MAX];struct stat st;if(lstat(target,&st)!=0 || !S_ISREG(st.st_mode) || S_ISLNK(st.st_mode) || !realpath(target,real)) return refuse(message,size,"Open requires a regular saved runtime scene.");snprintf(target,sizeof(target),"%s",real);}
    else {struct stat st;if(lstat(target,&st)==0 || errno!=ENOENT) return refuse(message,size,"Destination already exists; nothing overwritten.");}
    bool relocating=false;char asset_stage[PATH_MAX]="";
    if(saving) {
        char source[PATH_MAX],source_parent[PATH_MAX];
        if(!destination(SceneEditorDocumentPath(),source,source_parent)) return refuse(message,size,"Invalid current scene directory.");
        relocating=strcmp(parent,source_parent)!=0;
    }
    if(!SceneEditorAgentControlReservePath(target,message,size)) return false;
    if(relocating && !SceneEditorSaveAsAssetsBegin(target,asset_stage,sizeof(asset_stage),message,size)) {SceneEditorAgentControlReleaseReservedPath();return false;}
    bool published=false;
    if(!opening) {
        const char* bytes=saving?json_object_to_json_string_ext(document_authoring_root(),JSON_C_TO_STRING_PRETTY|JSON_C_TO_STRING_NOSLASHESCAPE):
            "{\"schema_family\":\"codework_scene\",\"schema_variant\":\"scene_runtime_v1\",\"schema_version\":1,\"scene_id\":\"new_scene\",\"space_mode_default\":\"3d\",\"unit_system\":\"meters\",\"world_scale\":1,\"objects\":[],\"lights\":[{\"light_id\":\"main_light\",\"kind\":\"point\",\"position\":{\"x\":1,\"y\":-2,\"z\":3},\"intensity\":7.5,\"radius\":0}],\"extensions\":{\"ray_tracing\":{\"authoring\":{\"camera_path\":{\"mode\":\"BEZIER_CUBIC\",\"points\":[{\"x\":0,\"y\":-4,\"rotation\":3.141592653589793,\"handleLink\":false}]},\"camera_path_depth\":{\"points\":[{\"z\":2,\"lookPitch\":-0.463647609}]}}}}}";
        json_object* seed=NULL;
        if(creating) {
            seed=json_tokener_parse(bytes);char id[65];ray_tracing_sha256_bytes(target,strlen(target),id);
            json_object_object_add(seed,"scene_id",json_object_new_string(id));
            bytes=json_object_to_json_string_ext(seed,JSON_C_TO_STRING_PRETTY);
        }
        bool saved=publish(target,bytes,message,size);if(seed) json_object_put(seed);
        if(!saved) {
            char recovery[512];if(!SceneEditorSaveAsAssetsFinish(asset_stage,false,recovery,sizeof(recovery))) snprintf(message,size,"%s",recovery);
            SceneEditorAgentControlReleaseReservedPath();return false;
        }published=true;
    }
    if(!SceneEditorDocumentSwitch(target,saving,message,size)) {
        if(published) unlink(target);char recovery[512];
        if(!SceneEditorSaveAsAssetsFinish(asset_stage,false,recovery,sizeof(recovery))) snprintf(message,size,"%s",recovery);
        SceneEditorAgentControlReleaseReservedPath();return false;
    }
    SceneEditorTimelinePause();SceneEditorTimelineClearSelection();SceneEditorRenderPanelReset();
    if(!SceneEditorSaveAsAssetsFinish(asset_stage,true,message,size)) {
        /* Scene and assets remain valid; retained staging is a cleanup condition,
           never report an unapplied command and invite replay. */
        snprintf(message,size,"Scene switched successfully; staging retained: %.300s",asset_stage);return true;
    }
    snprintf(message,size,"Scene %s accepted; previous saved file preserved.",operation);return true;
}
