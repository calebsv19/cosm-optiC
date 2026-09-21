#include <json-c/json.h>
#include <stdbool.h>
#include <string.h>

static json_object* member(json_object* o,const char* key) {
    json_object* v=NULL;if(o) json_object_object_get_ex(o,key,&v);return v;
}
/* A mapped object and its source stack form one authoring binding. Preserve the
 * complete source row on duplication, including optional producer metadata. */
bool SceneEditorDocumentCloneMappedMaterial(json_object* root,json_object* source,const char* new_id) {
    json_object* mapping=member(member(member(source,"extensions"),"ray_tracing"),"surface_mapping");
    if(!mapping) return true;
    const char* old_id=json_object_get_string(member(source,"object_id"));
    json_object* rows=member(member(member(member(root,"extensions"),"ray_tracing"),"authoring"),"object_materials");
    for(size_t i=0;rows && i<json_object_array_length(rows);++i) {
        json_object* row=json_object_array_get_idx(rows,i);
        const char* id=json_object_get_string(member(row,"object_id"));
        if(!id || !old_id || strcmp(id,old_id)) continue;
        json_object* copy=json_tokener_parse(json_object_to_json_string_ext(row,JSON_C_TO_STRING_PLAIN));
        if(!copy) return false;
        json_object_object_add(copy,"object_id",json_object_new_string(new_id));
        return json_object_array_add(rows,copy)==0;
    }
    return false;
}
