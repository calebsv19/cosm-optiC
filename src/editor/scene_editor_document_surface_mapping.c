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

#include "config/config_manager.h"
/* Creating an explicit mapping on a plain object creates an editable brick
 * source. Existing graph/image/stack documents are left for validation. */
static json_object* ensure_object(json_object* parent,const char* key) {
    json_object* value=member(parent,key);
    if(value) return json_object_is_type(value,json_type_object)?value:NULL;
    value=json_object_new_object();json_object_object_add(parent,key,value);return value;
}
bool SceneEditorDocumentEnsureMappingSource(json_object* root,json_object* object,int index) {
    if(index<0 || index>=sceneSettings.objectCount) return false;
    const char* id=json_object_get_string(member(object,"object_id"));
    json_object* ext=ensure_object(root,"extensions");if(!ext) return false;
    json_object* ray=ensure_object(ext,"ray_tracing");if(!ray) return false;
    json_object* authoring=ensure_object(ray,"authoring");if(!authoring) return false;
    json_object* rows=member(authoring,"object_materials");
    if(!rows) {rows=json_object_new_array();json_object_object_add(authoring,"object_materials",rows);}
    if(!json_object_is_type(rows,json_type_array)) return false;
    json_object* row=NULL;
    for(size_t i=0;i<json_object_array_length(rows);++i) {
        json_object* candidate=json_object_array_get_idx(rows,i);
        const char* key=json_object_get_string(member(candidate,"object_id"));
        if(key && id && !strcmp(key,id)) {row=candidate;break;}
    }
    if(row) {
        if(member(row,"material_texture_stack") || member(row,"materialTextureStack") ||
           member(row,"material_graph") || member(row,"materialGraph") || member(row,"authored_texture") ||
           member(row,"procedural_texture")) return true;
        json_object_object_add(row,"material_texture_stack",json_tokener_parse("{\"layers\":[{\"id\":\"brick\",\"name\":\"Brick\",\"kind\":\"brick\",\"enabled\":true}]}"));
        return true;
    }
    SceneObject* source=&sceneSettings.sceneObjects[index];
    if(source->textureId!=0) return false;
    row=json_object_new_object();json_object_object_add(row,"object_id",json_object_new_string(id));
    json_object_object_add(row,"material_id",json_object_new_int(source->material_id));
    json_object_object_add(row,"object_color",json_object_new_int(source->color));
    json_object_object_add(row,"roughness",json_object_new_double(source->roughness));
    json_object_object_add(row,"reflectivity",json_object_new_double(source->reflectivity));
    json_object_object_add(row,"material_texture_stack",json_tokener_parse("{\"layers\":[{\"id\":\"brick\",\"name\":\"Brick\",\"kind\":\"brick\",\"enabled\":true}]}"));
    return json_object_array_add(rows,row)==0;
}
