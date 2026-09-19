#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document_internal.h"
#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
static void document_diag(char* out,size_t size,const char* text) { if(out && size) snprintf(out,size,"%s",text); }
static json_object* document_object_by_id(const char* id) {
    json_object* objects = NULL;
    if (!id || !SceneEditorDocumentRetainedRoot() || !json_object_object_get_ex(SceneEditorDocumentRetainedRoot(),"objects",&objects)) return NULL;
    for (size_t i=0;i<json_object_array_length(objects);++i) {
        json_object* object=json_object_array_get_idx(objects,i), *value=NULL;
        if (json_object_object_get_ex(object,"object_id",&value) && strcmp(json_object_get_string(value),id)==0) return object;
    }
    return NULL;
}
static bool document_object_flag(json_object* object,const char* key,bool fallback) {
    json_object *flags=NULL,*value=NULL;
    if (object && json_object_object_get_ex(object,"flags",&flags) &&
        json_object_object_get_ex(flags,key,&value) && json_object_is_type(value,json_type_boolean))
        return json_object_get_boolean(value);
    return fallback;
}
int SceneEditorDocumentObjectCount(void) {
    json_object* objects=NULL;
    return SceneEditorDocumentRetainedRoot() && json_object_object_get_ex(SceneEditorDocumentRetainedRoot(),"objects",&objects)
        ? (int)json_object_array_length(objects) : 0;
}
bool SceneEditorDocumentObjectAt(int ordinal,SceneEditorDocumentObjectInfo* out) {
    json_object *objects=NULL,*value=NULL;
    if (!out || ordinal<0 || ordinal>=SceneEditorDocumentObjectCount()) return false;
    memset(out,0,sizeof(*out)); out->runtime_index=-1;
    json_object_object_get_ex(SceneEditorDocumentRetainedRoot(),"objects",&objects);
    json_object* object=json_object_array_get_idx(objects,ordinal);
    if (!json_object_object_get_ex(object,"object_id",&value)) return false;
    snprintf(out->id,sizeof(out->id),"%s",json_object_get_string(value));
    if (json_object_object_get_ex(object,"object_type",&value)) snprintf(out->type,sizeof(out->type),"%s",json_object_get_string(value));
    if (json_object_object_get_ex(object,"display_name",&value)) snprintf(out->name,sizeof(out->name),"%s",json_object_get_string(value));
    bool generated_name=!out->name[0] || strcmp(out->name,out->id)==0;
    if (generated_name) {
        snprintf(out->name,sizeof(out->name),"%s %d",strstr(out->type,"mesh") ? "Mesh" : strstr(out->type,"plane") ? "Plane" : strstr(out->type,"sphere") ? "Sphere" : "Object",ordinal+1);
    }
    if(generated_name && strncmp(out->id,"obj_",4)==0 && out->id[4]) {
        snprintf(out->name,sizeof(out->name),"%s",out->id+4);
        for(char* p=out->name;*p;++p) if(*p=='_') *p=' ';
        out->name[0]=(char)toupper((unsigned char)out->name[0]);
    }
    out->visible=document_object_flag(object,"visible",true);
    out->locked=document_object_flag(object,"locked",false);
    for (int i=0;i<sceneSettings.objectCount;++i) {
        char id[128];
        if (runtime_scene_bridge_get_last_object_id_for_scene_index(i,id,sizeof(id)) && strcmp(id,out->id)==0) { out->runtime_index=i;break; }
    }
    return true;
}
bool SceneEditorDocumentObjectById(const char* id,SceneEditorDocumentObjectInfo* out) {
    SceneEditorDocumentObjectInfo info;
    if (!id) return false;
    for (int i=0;i<SceneEditorDocumentObjectCount();++i) if (SceneEditorDocumentObjectAt(i,&info) && strcmp(info.id,id)==0) {
        if(out) *out=info;
        return true;
    }
    return false;
}
bool SceneEditorDocumentObjectEditable(int index,char* diagnostics,size_t size) {
    json_object* object=document_object_for_scene_index(index,NULL,0);
    if (object && document_object_flag(object,"locked",false)) {
        document_diag(diagnostics,size,"Unlock this object to edit"); return false;
    }
    return true;
}
bool SceneEditorDocumentSetFlag(const char* id,const char* flag,bool value,
    unsigned long long revision,char* diagnostics,size_t size) {
    if (!flag || (strcmp(flag,"visible") && strcmp(flag,"locked"))) return false;
    if (revision!=SceneEditorDocumentRevision()) { document_diag(diagnostics,size,"Scene changed; inspect and retry");return false; }
    json_object* object=document_object_by_id(id),*flags=NULL;
    if (!object) { document_diag(diagnostics,size,"Object ID not found");return false; }
    if (strcmp(flag,"locked") && document_object_flag(object,"locked",false)) {
        document_diag(diagnostics,size,"Unlock this object to edit");return false;
    }
    if (document_object_flag(object,flag,strcmp(flag,"visible")==0)==value) return true;
    if (json_object_object_get_ex(object,"flags",&flags) && !json_object_is_type(flags,json_type_object)) {
        document_diag(diagnostics,size,"Object flags are invalid; no changes made");return false;
    }
    if (!document_begin_command(diagnostics,size)) return false;
    if (!json_object_object_get_ex(object,"flags",&flags)) {
        flags=json_object_new_object();json_object_object_add(object,"flags",flags);
    }
    json_object_object_add(flags,flag,json_object_new_boolean(value));
    return document_finish_command(diagnostics,size);
}


bool SceneEditorDocumentRequireEditable(int index) {
    char reason[160];
    if(SceneEditorDocumentObjectEditable(index,reason,sizeof(reason))) return true;
    SceneEditorChromeShellSetActionFeedback(reason,4000);return false;
}

bool SceneEditorDocumentRenameById(const char* id,const char* name,unsigned long long revision,char* diagnostics,size_t size) {
    json_object* object=document_object_by_id(id),*old=NULL;
    size_t length=name ? strlen(name) : 0;
    if(!object || revision!=SceneEditorDocumentRevision() || !length || length>96) {
        document_diag(diagnostics,size,"Rename requires a current object and a name of 1 to 96 bytes");return false;
    }
    if(document_object_flag(object,"locked",false)) {document_diag(diagnostics,size,"Unlock this object to edit");return false;}
    if(json_object_object_get_ex(object,"display_name",&old) && strcmp(json_object_get_string(old),name)==0) return true;
    if(!document_begin_command(diagnostics,size)) return false;
    json_object_object_add(object,"display_name",json_object_new_string(name));
    return document_finish_command(diagnostics,size);
}

const char* SceneEditorDocumentTypeLabel(const char* type) {
    if(!type) return "Object";
    if(strstr(type,"mesh")) return "Mesh";
    if(strstr(type,"curve")) return "Curve";
    if(strstr(type,"plane")) return "Plane";
    if(strstr(type,"sphere")) return "Sphere";
    if(strstr(type,"box") || strstr(type,"prism")) return "Box";
    return "Object";
}
