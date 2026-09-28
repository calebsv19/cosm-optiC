#include "import/runtime_scene_timeline.h"
#include "import/scene_timeline_document_io.h"
static void test_object_timeline_admission(void) {
    static TimelineDocument doc;
    TimelineDocumentInit(&doc,(TimelineRate){24,1},(TimelineRange){100,21});
    const char* axes[]={"object/transform/position_x","object/transform/position_y","object/transform/position_z"};
    for(int i=0;i<3;++i) {
        TimelineTrack track;TimelineTrackInit(&track,axes[i],"object/subject",axes[i],TIMELINE_VALUE_SCALAR);
        TimelineTrackSetUnit(&track,TIMELINE_UNIT_WORLD_DISTANCE);
        TimelineTrackAddKey(&track,100,TimelineValueScalar(i),TIMELINE_INTERPOLATION_LINEAR);
        TimelineTrackAddKey(&track,120,TimelineValueScalar(i+10),TIMELINE_INTERPOLATION_LINEAR);
        TimelineDocumentAddTrack(&doc,&track);
    }
    json_object* root=json_tokener_parse("{\"world_scale\":2,\"objects\":[{\"object_id\":\"subject\",\"object_type\":\"mesh_asset_instance\"}],\"extensions\":{\"ray_tracing\":{\"authoring\":{}}}}");
    json_object *extensions=NULL,*ray=NULL,*authoring=NULL,*objects=NULL;
    json_object_object_get_ex(root,"extensions",&extensions);json_object_object_get_ex(extensions,"ray_tracing",&ray);
    json_object_object_get_ex(ray,"authoring",&authoring);json_object_object_get_ex(root,"objects",&objects);
    json_object_object_add(authoring,"scene_timeline",SceneTimelineDocumentToJson(&doc));char error[256];
    assert_true("object_timeline_admit_scaled_nonzero_range",RuntimeSceneTimelineValidateScene(root,error,sizeof(error)));
    json_object* motion=json_tokener_parse("[{\"object_id\":\"subject\",\"mode\":\"physics\",\"enabled\":true}]");
    json_object_object_add(authoring,"object_motion_tracks",motion);
    assert_true("object_timeline_reject_simulation_owner",!RuntimeSceneTimelineValidateScene(root,error,sizeof(error)));
    json_object_object_del(authoring,"object_motion_tracks");
    json_object_array_add(objects,json_object_get(json_object_array_get_idx(objects,0)));
    assert_true("object_timeline_reject_duplicate_identity",!RuntimeSceneTimelineValidateScene(root,error,sizeof(error)));
    json_object_array_del_idx(objects,1,1);
    json_object_object_add(json_object_array_get_idx(objects,0),"object_type",json_object_new_string("curve_path"));
    assert_true("object_timeline_reject_unsupported_geometry",!RuntimeSceneTimelineValidateScene(root,error,sizeof(error)));
    json_object_put(root);
}
