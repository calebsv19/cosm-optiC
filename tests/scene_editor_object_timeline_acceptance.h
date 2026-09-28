#include "editor/scene_editor_object_timeline.h"
#include "editor/scene_editor_object_commands.h"
#include "import/runtime_scene_object_timeline.h"
#include "import/runtime_scene_motion_bridge.h"
#include "editor/scene_editor_object_transform_preview.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_3d_builder_internal.h"
static void object_timeline_acceptance(SceneEditor* editor,const char* path) {
    SceneEditorDocumentObjectInfo object;bool found=false;
    for(int i=0;i<SceneEditorDocumentObjectCount();++i) if(SceneEditorDocumentObjectAt(i,&object) &&
        !strcmp(object.type,"mesh_asset_instance") && object.runtime_index>=0) {found=true;break;}
    assert(found);char diagnostics[256];SceneEditorObjectReadback readback;
    assert(SceneEditorObjectExecute(SCENE_OBJECT_SELECT,object.id,NULL,false,SceneEditorDocumentRevision(),&readback,diagnostics,sizeof(diagnostics)));
    SceneEditorDocumentTransform base;assert(SceneEditorDocumentGetTransformForSceneIndex(object.runtime_index,&base,diagnostics,sizeof(diagnostics)));
    choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);authoring_control(editor,"setup");authoring_control(editor,"frame_paths");
    unsigned long long revision=SceneEditorDocumentRevision();authoring_control(editor,"animate_object");
    assert(SceneEditorDocumentRevision()==revision+1);
    TimelineTrack track;TimelineRate rate;TimelineRange range;TimelineSample sample;
    assert(SceneEditorTimelineSelectedTrack(&track,&rate,&range,&sample) && RuntimeObjectTimelineAxis(track.property_id)==0);
    double scale=SceneEditorDocumentWorldScale();double targets[]={base.position[0]+10,base.position[0]+10,base.position[0]+20};
    const char* frames[]={"20","40","60"};
    for(int i=0;i<3;++i) {char value[64];snprintf(value,sizeof(value),"%.17g",targets[i]);
        authoring_control(editor,"frame");authoring_text(editor,frames[i]);authoring_control(editor,"value");authoring_text(editor,value);}
    const int times[]={0,10,20,30,40,50,60,10};const double offsets[]={0,5,10,10,10,15,20,5};
    for(int i=0;i<8;++i) {
        assert(SceneEditorTimelineSeek(times[i]));RayEvaluatedSceneSnapshot frame;assert(SceneEditorTimelineCopyEvaluated(&frame));
        const RayEvaluatedObjectTransform* transform=NULL;
        for(size_t j=0;j<frame.object_transform_count;++j) if(!strcmp(frame.object_transforms[j].target_id,object.id)) transform=&frame.object_transforms[j];
        assert(transform && fabs(transform->position.x-(base.position[0]+offsets[i])*scale)<1e-8);
        assert(fabs(transform->position.y-base.position[1]*scale)<1e-8 && fabs(transform->position.z-base.position[2]*scale)<1e-8);
        RuntimeMotionTrack3DSample geometry;assert(runtime_scene_motion_bridge_sample_object(object.id,frame.frame.normalized_t,&geometry));
        assert(fabs(geometry.position_x-transform->position.x)<1e-8);
        double viewport[3];assert(SceneEditorObjectTimelinePosition(object.id,viewport) && fabs(viewport[0]-geometry.position_x)<1e-8);
        SceneEditorDocumentTransform unchanged;assert(SceneEditorDocumentGetTransformForSceneIndex(object.runtime_index,&unchanged,diagnostics,sizeof(diagnostics)));
        assert(!memcmp(&base,&unchanged,sizeof(base)));
    }
    assert(SceneEditorTimelineSeekSample((TimelineSample){10,1,2}));
    RayEvaluatedSceneSnapshot subframe;assert(SceneEditorTimelineCopyEvaluated(&subframe));
    assert(fabs(subframe.object_transforms[subframe.object_transform_count-1].position.x-(base.position[0]+5.25)*scale)<1e-8);
    RayTracingRuntimeMeshAssetSet* assets=calloc(1,sizeof(*assets));assert(assets);
    ray_tracing_runtime_mesh_asset_set_init(assets);
    assert(ray_tracing_runtime_mesh_assets_load_scene_file(path,assets,diagnostics,sizeof(diagnostics)));
    RuntimeScene3D geometry;RuntimeScene3D_Init(&geometry);Vec3 start={0};bool triangle_found=false;
    for(int pass=0;pass<2;++pass) {
        assert(SceneEditorTimelineSeek(pass?60:0));RayEvaluatedSceneSnapshot frame;assert(SceneEditorTimelineCopyEvaluated(&frame));
        assert(runtime_scene_3d_builder_append_mesh_asset_set_at_t(&geometry,assets,false,frame.frame.normalized_t));
        bool found_triangle=false;
        for(int j=0;j<geometry.triangleMesh.triangleCount;++j) if(geometry.triangleMesh.triangles[j].sceneObjectIndex==object.runtime_index) {
            Vec3 point=geometry.triangleMesh.triangles[j].p0;
            if(!pass) {start=point;triangle_found=true;}
            else assert(triangle_found && fabs(point.x-start.x-20*scale)<1e-7 && fabs(point.y-start.y)<1e-7 && fabs(point.z-start.z)<1e-7);
            found_triangle=true;break;
        }
        assert(found_triangle);RuntimeScene3D_Free(&geometry);RuntimeScene3D_Init(&geometry);
    }
    ray_tracing_runtime_mesh_asset_set_free(assets);free(assets);
    assert(SceneEditorTimelineSeek(0));SceneEditorSessionRuntimeRender(editor);capture(editor,"object_start.ppm");
    assert(SceneEditorTimelineSeek(60));SceneEditorSessionRuntimeRender(editor);capture(editor,"object_moved.ppm");
    choose_menu(editor,1,0);assert(SceneEditorTimelineSelectedTrack(&track,&rate,&range,&sample) && track.key_count==3);
    choose_menu(editor,1,1);assert(SceneEditorTimelineSelectedTrack(&track,&rate,&range,&sample) && track.key_count==4);
    choose_menu(editor,0,0);assert(SceneEditorDocumentOpen(path,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorTimelineSeek(30));RayEvaluatedSceneSnapshot reopened;assert(SceneEditorTimelineCopyEvaluated(&reopened));
    assert(fabs(reopened.object_transforms[reopened.object_transform_count-1].position.x-(base.position[0]+10)*scale)<1e-8);
    static TimelineDocument good,bad;
    assert(SceneEditorDocumentGetTimeline(&good)==TIMELINE_STATUS_OK);bad=good;
    size_t axis=SIZE_MAX;for(size_t i=0;i<good.track_count;++i) if(RuntimeObjectTimelineAxis(good.tracks[i].property_id)==0) {axis=i;break;}
    assert(axis!=SIZE_MAX);revision=SceneEditorDocumentRevision();bad.tracks[axis].enabled=false;
    assert(!SceneEditorDocumentSetTimeline(&bad,revision,diagnostics,sizeof(diagnostics)) && SceneEditorDocumentRevision()==revision);
    bad=good;snprintf(bad.tracks[axis].target_id,sizeof(bad.tracks[axis].target_id),"object/missing");
    assert(!SceneEditorDocumentSetTimeline(&bad,revision,diagnostics,sizeof(diagnostics)) && SceneEditorDocumentRevision()==revision);
    assert(SceneEditorDocumentSetFlag(object.id,"locked",true,revision,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorTimelineSelectTrack(axis));revision=SceneEditorDocumentRevision();
    assert(!SceneEditorTimelineSetKey(999) && SceneEditorDocumentRevision()==revision);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    assert(SceneEditorTimelineSelectTrack(axis));
    FILE* expected=fopen("object_expected.json","w");assert(expected);
    fprintf(expected,"{\"object_id\":\"%s\",\"base_x\":%.17g,\"scale\":%.17g}\n",object.id,base.position[0],scale);fclose(expected);
    fprintf(stderr,"Object timeline native PASS: select/add XYZ, move-hold-resume, backwards seek/subframe, geometry/viewport agreement, base immutability, undo/redo and save/reopen\n");
}
