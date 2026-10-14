#include "import/runtime_scene_lighting_writeback.h"
static json_object* get(json_object* p,const char* key) {json_object* v=NULL;if(json_object_is_type(p,json_type_object)) json_object_object_get_ex(p,key,&v);return v;}
static json_object* author(json_object* p) {return get(get(get(p,"extensions"),"ray_tracing"),"authoring");}
void RuntimeSceneLightingPreserveOverlay(json_object* scene,json_object* overlay) {
    const char* keys[]={"environment","light_settings"};
    for(int i=0;i<2;++i) {
        json_object* retained=get(author(scene),keys[i]),*generated=get(author(overlay),keys[i]);
        if(!json_object_is_type(retained,json_type_object) || !json_object_is_type(generated,json_type_object)) continue;
        json_object_object_foreach(retained,key,v) {if(!get(generated,key)) json_object_object_add(generated,key,json_object_get(v));}
    }
}
