#include "editor/scene_editor_document_rigid.h"
#include "editor/scene_editor_document.h"
#include "scene_editor_document_transaction.h"
#include "import/runtime_rigid_result.h"
#include "import/runtime_mesh_asset_loader.h"
#include <stdio.h>
#include <string.h>
static json_object *get(json_object *o,const char *key) {
    json_object *v=NULL;if(o) json_object_object_get_ex(o,key,&v);return v;
}
static json_object *ensure(json_object *o,const char *key) {
    json_object *v=get(o,key);
    if(!v) {v=json_object_new_object();json_object_object_add(o,key,v);}
    return json_object_is_type(v,json_type_object)?v:NULL;
}
bool SceneEditorDocumentValidateRigidResult(char *d,size_t n) {
    RuntimeRigidResult *candidate=NULL;
    if(!SceneEditorDocumentIsOpen() || !RuntimeRigidResultPrepare(document_authoring_root(),SceneEditorDocumentPath(),&candidate,d,n)) return false;
    bool ok=candidate && RuntimeRigidResultMeshesMatch(candidate,ray_tracing_runtime_mesh_assets_last(),d,n);
    bool bound=candidate!=NULL;
    RuntimeRigidResultFree(candidate);
    if(ok) snprintf(d,n,"Verified result, physical input and loaded geometry match.");
    else if(!bound) snprintf(d,n,"No rigid result is bound to this scene.");
    return ok;
}
bool SceneEditorDocumentSelectRigidResult(const char *bundle,const char *revision,
    unsigned long long expected,char *d,size_t n) {
    if(!SceneEditorDocumentIsOpen() || expected!=SceneEditorDocumentRevision() ||
       !bundle || !bundle[0] || strlen(bundle)>=3500 || !revision || strlen(revision)!=64 ||
       strspn(revision,"0123456789abcdef")!=64) {
        snprintf(d,n,"Result selection refused: invalid identity/path or scene changed.");return false;
    }
    if(!document_begin_command(d,n)) return false;
    json_object *ext=ensure(document_authoring_root(),"extensions"),*ray=ext?ensure(ext,"ray_tracing"):NULL;
    if(!ray) {document_rollback_command();snprintf(d,n,"Scene extensions must be objects.");return false;}
    json_object *ref=json_object_new_object();
    json_object_object_add(ref,"schema",json_object_new_string("ray_rigid_result_binding_v1"));
    json_object_object_add(ref,"bundle_path",json_object_new_string(bundle));
    json_object_object_add(ref,"result_revision",json_object_new_string(revision));
    json_object_object_add(ray,"rigid_result",ref);
    if(!document_finish_command(d,n)) return false;
    snprintf(d,n,"Result selected. Undo is available; save the scene to retain it.");return true;
}
