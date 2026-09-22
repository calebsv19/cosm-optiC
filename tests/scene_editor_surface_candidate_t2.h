#include "app/ray_tracing_sha256.h"

/* Run only against a disposable copy of tools-03/uv_project. Candidate assets
 * remain relative to that directory; the original tool proof stays immutable. */
static char* candidate_t2_read(const char* path,size_t* length) {
    FILE* f=fopen(path,"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long size=ftell(f);assert(size>=0);rewind(f);
    char* bytes=malloc((size_t)size+1);assert(bytes);
    assert(fread(bytes,1,(size_t)size,f)==(size_t)size);assert(!fclose(f));
    bytes[size]=0;*length=(size_t)size;return bytes;
}
static void candidate_t2_write(const char* path,const char* bytes,size_t size) {
    FILE* f=fopen(path,"wb");assert(f);assert(fwrite(bytes,1,size,f)==size);assert(!fclose(f));
}
static void candidate_t2_reject(const char* candidate,const char* receipt,const char* scene) {
    char before[65],after[65],diagnostic[512];
    unsigned long long revision=SceneEditorDocumentRevision();
    bool dirty=SceneEditorDocumentIsDirty(),undo=SceneEditorDocumentCanUndo(),redo=SceneEditorDocumentCanRedo();
    char mapping_before[8192],mapping_after[8192];
    assert(SceneEditorDocumentGetSurfaceMappingJSON(0,mapping_before,sizeof(mapping_before)));
    assert(ray_tracing_sha256_file(scene,before));
    assert(!SceneEditorDocumentAdoptSurfaceCandidate(candidate,receipt,diagnostic,sizeof(diagnostic)));
    assert(diagnostic[0]);
    assert(SceneEditorDocumentRevision()==revision && SceneEditorDocumentIsDirty()==dirty);
    assert(SceneEditorDocumentCanUndo()==undo && SceneEditorDocumentCanRedo()==redo);
    assert(ray_tracing_sha256_file(scene,after) && !strcmp(before,after));
    assert(SceneEditorDocumentGetSurfaceMappingJSON(0,mapping_after,sizeof(mapping_after)));
    assert(!strcmp(mapping_before,mapping_after));
}
static void surface_candidate_t2_probe(SceneEditor* editor,const char* scene_path) {
    (void)editor;
    char parent[PATH_MAX],candidate[PATH_MAX],receipt[PATH_MAX],bad_receipt[PATH_MAX],diagnostic[512];
    snprintf(parent,sizeof(parent),"%s",scene_path);char* slash=strrchr(parent,'/');assert(slash);*slash=0;
    assert(snprintf(candidate,sizeof(candidate),"%s/uv_candidate.json",parent)<(int)sizeof(candidate));
    assert(snprintf(receipt,sizeof(receipt),"%s/uv_candidate.json.receipt.json",parent)<(int)sizeof(receipt));
    assert(snprintf(bad_receipt,sizeof(bad_receipt),"%s/rejected-receipt.json",parent)<(int)sizeof(bad_receipt));
    size_t scene_size,candidate_size;
    char* scene_bytes=candidate_t2_read(scene_path,&scene_size);
    char* candidate_bytes=candidate_t2_read(candidate,&candidate_size);
    json_object* review=json_object_from_file(receipt);assert(review);
    json_object* altered=json_tokener_parse(json_object_to_json_string(review));assert(altered);
    json_object_object_add(altered,"state",json_object_new_string("preflight_only_candidate_not_adopted"));
    assert(!json_object_to_file_ext(bad_receipt,altered,JSON_C_TO_STRING_PRETTY));
    candidate_t2_reject(candidate,bad_receipt,scene_path);
    json_object_object_add(altered,"state",json_object_new_string("reviewed_candidate_not_adopted"));
    json_object* validation=NULL;assert(json_object_object_get_ex(altered,"validation",&validation));
    json_object_object_add(validation,"preview_render",json_object_new_string("true"));
    assert(!json_object_to_file_ext(bad_receipt,altered,JSON_C_TO_STRING_PRETTY));
    candidate_t2_reject(candidate,bad_receipt,scene_path);
    json_object_put(altered);

    altered=json_tokener_parse(json_object_to_json_string(review));assert(altered);
    json_object_object_add(altered,"geometry_unchanged",json_object_new_boolean(false));
    assert(!json_object_to_file_ext(bad_receipt,altered,JSON_C_TO_STRING_PRETTY));
    candidate_t2_reject(candidate,bad_receipt,scene_path);json_object_put(altered);
    json_object *review_root=NULL,*preview_frame=NULL;
    assert(json_object_object_get_ex(review,"review_root",&review_root));
    assert(json_object_object_get_ex(review,"validation",&validation));
    assert(json_object_object_get_ex(validation,"preview_frame",&preview_frame));
    char preview_path[PATH_MAX];
    assert(snprintf(preview_path,sizeof(preview_path),"%s/%s/%s",parent,
        json_object_get_string(review_root),json_object_get_string(preview_frame))<(int)sizeof(preview_path));
    size_t preview_size;char* preview_bytes=candidate_t2_read(preview_path,&preview_size);assert(preview_size);
    preview_bytes[0]^=1;candidate_t2_write(preview_path,preview_bytes,preview_size);
    candidate_t2_reject(candidate,receipt,scene_path);
    preview_bytes[0]^=1;candidate_t2_write(preview_path,preview_bytes,preview_size);free(preview_bytes);

    /* Identical parsed JSON with different bytes is still a stale proof. */
    FILE* f=fopen(candidate,"ab");assert(f);assert(fputc('\n',f)!=EOF);assert(!fclose(f));
    candidate_t2_reject(candidate,receipt,scene_path);
    candidate_t2_write(candidate,candidate_bytes,candidate_size);
    f=fopen(scene_path,"ab");assert(f);assert(fputc('\n',f)!=EOF);assert(!fclose(f));
    candidate_t2_reject(candidate,receipt,scene_path); /* disk differs from baseline */
    assert(SceneEditorDocumentOpen(scene_path,diagnostic,sizeof(diagnostic)));
    candidate_t2_reject(candidate,receipt,scene_path); /* baseline itself is stale */
    candidate_t2_write(scene_path,scene_bytes,scene_size);
    assert(SceneEditorDocumentOpen(scene_path,diagnostic,sizeof(diagnostic)));

    SceneEditorDocumentTransform transform;
    assert(SceneEditorDocumentGetTransformForSceneIndex(0,&transform,diagnostic,sizeof(diagnostic)));
    transform.position[0]+=.125;
    assert(SceneEditorDocumentSetTransformForSceneIndex(0,&transform,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentIsDirty());candidate_t2_reject(candidate,receipt,scene_path);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentOpen(scene_path,diagnostic,sizeof(diagnostic)));

    char mapping_before[8192],mapping_after[8192];
    assert(SceneEditorDocumentGetSurfaceMappingJSON(0,mapping_before,sizeof(mapping_before)));
    unsigned long long revision=SceneEditorDocumentRevision();
    assert(SceneEditorDocumentAdoptSurfaceCandidate(candidate,receipt,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision()==revision+1 && !SceneEditorDocumentIsDirty());
    assert(SceneEditorDocumentCanUndo());
    assert(SceneEditorDocumentGetSurfaceMappingJSON(0,mapping_after,sizeof(mapping_after)));
    assert(strcmp(mapping_before,mapping_after));
    assert(strstr(mapping_after,"authored_uv"));
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentGetSurfaceMappingJSON(0,mapping_after,sizeof(mapping_after)));
    assert(!strcmp(mapping_before,mapping_after));
    assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentGetSurfaceMappingJSON(0,mapping_after,sizeof(mapping_after)));
    assert(strstr(mapping_after,"authored_uv"));
    json_object* result=json_object_new_object();
    json_object_object_add(result,"schema",json_object_new_string("optic_surface_candidate_native_acceptance_v1"));
    json_object_object_add(result,"passed",json_object_new_boolean(true));
    json_object_object_add(result,"checks",json_object_new_string("preflight-only; typed preview flag; unchanged geometry flag; preview bytes pin; candidate pin; current disk pin; baseline pin; dirty guard; adoption save; undo; redo"));
    assert(!json_object_to_file_ext("surface-candidate-t2-receipt.json",result,JSON_C_TO_STRING_PRETTY));
    json_object_put(result);json_object_put(review);free(scene_bytes);free(candidate_bytes);
}
