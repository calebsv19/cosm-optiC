#include "scene_editor_retained_light_save.h"
#include "scene_editor_document_transaction.h"
#include "import/runtime_scene_light_timeline_io.h"
#include <stdlib.h>
static json_object* member(json_object* o,const char* key) {json_object* v=NULL;if(o) json_object_object_get_ex(o,key,&v);return v;}
json_object* SceneEditorRetainedLightForSave(json_object* encoded,double scale) {
    json_object* author=member(member(member(document_authoring_root(),"extensions"),"ray_tracing"),"authoring");
    json_object* retained=member(author,"light_timeline");
    if(!retained || !encoded) return json_object_get(encoded);
    RuntimeSceneLightTimelineDocument* parsed=malloc(sizeof(*parsed));if(!parsed) return json_object_get(encoded);
    json_object* canonical=NULL;
    if(RuntimeSceneLightTimelineParseAuthoring(author,scale,parsed,NULL,0)==TIMELINE_STATUS_OK)
        canonical=RuntimeSceneLightTimelineToJsonObject(parsed,scale);
    free(parsed);
    /* Legacy UI adapters can still edit their runtime carrier. Preserve raw
     * metadata only when its parsed meaning equals the current export. */
    bool same=canonical && json_object_equal(canonical,encoded);if(canonical) json_object_put(canonical);
    return json_object_get(same?retained:encoded);
}
