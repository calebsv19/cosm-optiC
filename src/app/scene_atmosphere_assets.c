#define _XOPEN_SOURCE 700
#include "app/scene_atmosphere_assets.h"
#include "import/fluid_volume_import_3d.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool fail(char* d,size_t n,const char* message) {
    if(d && n) snprintf(d,n,"%s",message);
    return false;
}
static json_object* member(json_object* root,const char* name) {
    json_object* out=NULL;
    if(root) json_object_object_get_ex(root,name,&out);
    return out;
}
static bool directory(const char* path,char out[PATH_MAX]) {
    if(!path || strlen(path)>=PATH_MAX) return false;
    strcpy(out,path);
    char* slash=strrchr(out,'/');
    if(slash) {if(slash==out) slash[1]=0;else *slash=0;}
    else strcpy(out,".");
    return true;
}
bool SceneAtmosphereAssetPath(const char* owner,const char* relative,const char* root,
    char* out,size_t size,char* d,size_t n) {
    char base[PATH_MAX],project[PATH_MAX],candidate[PATH_MAX],resolved[PATH_MAX];
    if(!relative || !relative[0] || relative[0]=='/' || strchr(relative,'\\') ||
       strchr(relative,':') || !directory(owner,base) || !realpath(root,project))
        return fail(d,n,"cache dependency needs a contained relative path");
    for(const char* p=relative;*p;) {
        const char* end=strchr(p,'/');size_t len=end?(size_t)(end-p):strlen(p);
        if(!len || (len==1 && *p=='.') || (len==2 && p[0]=='.' && p[1]=='.'))
            return fail(d,n,"cache dependency path contains traversal");
        if(!end) break;
        p=end+1;if(!*p) return fail(d,n,"cache dependency path is empty");
    }
    char canonical_base[PATH_MAX];
    if(!realpath(base,canonical_base)) return fail(d,n,"cache owner directory is missing");
    if(snprintf(candidate,sizeof(candidate),"%s/%s",canonical_base,relative)>=(int)sizeof(candidate))
        return fail(d,n,"cache dependency path exceeds capacity");
    /* Check each component, rather than accepting a symlink whose target happens
       to be inside the project. The same policy applies after relocation. */
    for(char* p=candidate+1;;++p) {
        if(*p && *p!='/') continue;
        char saved=*p;*p=0;struct stat st;
        bool safe=lstat(candidate,&st)==0 && !S_ISLNK(st.st_mode);
        *p=saved;
        if(!safe) return fail(d,n,"cache dependency is missing or is a symlink");
        if(!saved) break;
    }
    if(!realpath(candidate,resolved)) return fail(d,n,"cache dependency is missing");
    size_t len=strlen(project);struct stat st;
    if(strncmp(project,resolved,len) || resolved[len]!='/' ||
       stat(resolved,&st)!=0 || !S_ISREG(st.st_mode))
        return fail(d,n,"cache dependency escapes its project or is not a file");
    if(strlen(resolved)>=size) return fail(d,n,"cache dependency path exceeds capacity");
    strcpy(out,resolved);return true;
}
static bool dependencies(const char* path,const char* root,int depth,char* d,size_t n) {
    if(depth>4) return fail(d,n,"cache manifest nesting exceeds supported depth");
    json_object* doc=json_object_from_file(path);
    if(!doc) return fail(d,n,"cannot read cache manifest");
    bool ok=true;char child[PATH_MAX];
    const char* sources[]={"fluid_source","water_source"};
    for(size_t i=0;i<2 && ok;++i) {
        json_object* source=member(doc,sources[i]);
        if(!source) continue;
        json_object* p=member(source,"path");
        ok=p && json_object_is_type(p,json_type_string) &&
            SceneAtmosphereAssetPath(path,json_object_get_string(p),root,child,sizeof(child),d,n);
        if(ok && (!strrchr(child,'.') || !strcmp(strrchr(child,'.'),".json")))
            ok=dependencies(child,root,depth+1,d,n);
    }
    json_object* frames=member(doc,"frames");
    if(ok && frames) {
        size_t count=json_object_is_type(frames,json_type_array)?json_object_array_length(frames):0;
        if(!count || count>1000000) ok=false;
        int64_t last=-1;
        for(size_t i=0;i<count && ok;++i) {
            json_object* frame=json_object_array_get_idx(frames,i);
            json_object* index=member(frame,"frame_index"),*p=member(frame,"path");
            ok=index && json_object_is_type(index,json_type_int) &&
                json_object_get_int64(index)>last && json_object_get_int64(index)<=INT_MAX &&
                p && json_object_is_type(p,json_type_string) &&
                SceneAtmosphereAssetPath(path,json_object_get_string(p),root,child,sizeof(child),d,n);
            if(ok) last=json_object_get_int64(index);
        }
    }
    json_object_put(doc);
    return ok || fail(d,n,"cache manifest needs sorted actual frame IDs and contained dependencies");
}
/* Resolve the 3D retained bundle directly. The older shared bundle resolver
 * recognizes VF2D sources; no shared or PhysicsSim schema change is needed. */
bool SceneAtmosphereAssetVolumeSource(const char* scene,const char* source,char* out,size_t size,
    int* out_kind,char* d,size_t n) {
    char root[PATH_MAX],path[PATH_MAX];
    if(!directory(scene,root) || strlen(source)>=sizeof(path)) return fail(d,n,"Invalid cache source path.");
    strcpy(path,source);
    for(int depth=0;depth<=4;++depth) {
        RuntimeVolume3DSourceKind kind;
        if(!fluid_volume_import_3d_classify_path(path,&kind)) return fail(d,n,"Unsupported volume source.");
        if(kind==RUNTIME_VOLUME_3D_SOURCE_MANIFEST) {
            json_object* doc=json_object_from_file(path);
            json_object* fluid=member(doc,"fluid_source");
            if(fluid) {
                json_object* rel=member(fluid,"path");char child[PATH_MAX];
                bool ok=rel && json_object_is_type(rel,json_type_string) &&
                    SceneAtmosphereAssetPath(path,json_object_get_string(rel),root,child,sizeof(child),d,n);
                json_object_put(doc);if(!ok) return false;
                strcpy(path,child);continue;
            }
            if(doc) json_object_put(doc);
        }
        if(strlen(path)>=size) return fail(d,n,"Volume source path exceeds capacity.");
        strcpy(out,path);*out_kind=kind;return true;
    }
    return fail(d,n,"Volume bundle nesting exceeds supported depth.");
}
bool SceneAtmosphereAssetsValidate(const SceneAtmosphereAttachment* value,
    const char* scene_path,char* d,size_t n) {
    if(!SceneAtmosphereAttachmentValidate(value,d,n)) return false;
    if(!value->present) return true;
    char root[PATH_MAX],path[PATH_MAX];
    if(!directory(scene_path,root) || !SceneAtmosphereAssetPath(scene_path,value->source_path,
        root,path,sizeof(path),d,n)) return false;
    bool single=!strcmp(value->source_kind,"vf3d") || !strcmp(value->source_kind,"pack");
    if(!single && !dependencies(path,root,0,d,n)) return false;
    RuntimeVolumeAttachment3D probe;
    RuntimeVolumeAttachment3D_Init(&probe);
    char selected[PATH_MAX];int resolved_kind;
    bool ok=SceneAtmosphereAssetVolumeSource(scene_path,path,selected,sizeof(selected),&resolved_kind,d,n) &&
        fluid_volume_import_3d_load_source_at_frame(selected,(RuntimeVolume3DSourceKind)resolved_kind,(int)value->source_start,&probe,d,n);
    RuntimeVolumeAttachment3D_Free(&probe);
    return ok;
}
