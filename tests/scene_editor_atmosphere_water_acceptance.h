#include "app/scene_atmosphere_runtime.h"
#include "import/water_surface_import.h"

static void atmosphere_water_native(SceneEditor* editor,const char* scene,bool reopen) {
    char diagnostics[512],path[4096];SceneAtmosphereAttachment value;
    if(!reopen) {
        snprintf(path,sizeof(path),"%s/assets/bundle/scene_bundle.json",getenv("RAY_TRACING_PROGRAM_ROOT"));
        bool imported=SceneEditorAtmosphereImport(path,false,SceneEditorDocumentRevision(),diagnostics,sizeof(diagnostics));
        if(!imported) fprintf(stderr,"Water bundle import: %s\n",diagnostics);
        assert(imported);
        assert(SceneEditorAtmosphereRead(&value) && !strcmp(value.source_kind,"scene_bundle"));
        value.animated=true;
        assert(SceneEditorAtmosphereSet(&value,SceneEditorDocumentRevision(),diagnostics,sizeof(diagnostics)));
    } else assert(SceneEditorAtmosphereRead(&value) && value.present && value.animated && !strcmp(value.source_kind,"scene_bundle"));
    const int frames[]={4,1,3,0,7,2};const int expected[]={4,0,2,0,4,2};
    for(int i=0;i<6;++i) {
        assert(SceneEditorTimelineSeek(frames[i]));RayEvaluatedSceneSnapshot snapshot;
        assert(SceneEditorTimelineCopyEvaluated(&snapshot));int selected=frames[i];
        assert(SceneAtmosphereRuntimeWater(scene,&snapshot.frame,path,sizeof(path),&selected,diagnostics,sizeof(diagnostics))==1);
        assert(selected==expected[i]);
        RuntimeWaterSurfaceFrame water={0};bool found=false;
        assert(RuntimeWaterSurfaceImport_LoadSourceAtFrame(path,selected,&water,&found,diagnostics,sizeof(diagnostics)));
        assert(found && water.valid && water.frame_index==(uint64_t)expected[i]);
        RuntimeWaterSurfaceFrame_Free(&water);
        RuntimeNative3DPreparedFrame* prepared=calloc(1,sizeof(*prepared));assert(prepared);
        assert(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(prepared,64,64,&snapshot,NULL));
        int triangles=0;
        for(int t=0;t<prepared->scene.triangleMesh.triangleCount;++t) {
            int index=prepared->scene.triangleMesh.triangles[t].sceneObjectIndex;
            if(index>=0 && index<sceneSettings.objectCount && !strcmp(sceneSettings.sceneObjects[index].type,"water_surface")) ++triangles;
        }
        assert(triangles>0 && prepared->scene.volume.grid.frameIndex==(uint64_t)expected[i]);
        RuntimeNative3DPreparedFrame_Free(prepared);free(prepared);
    }
    value.enabled=false;
    assert(SceneEditorAtmosphereSet(&value,SceneEditorDocumentRevision(),diagnostics,sizeof(diagnostics)));
    int selected=4;assert(SceneAtmosphereRuntimeWater(scene,NULL,path,sizeof(path),&selected,diagnostics,sizeof(diagnostics))==1 && !path[0]);
    RayEvaluatedSceneSnapshot disabled;assert(SceneEditorTimelineCopyEvaluated(&disabled));
    RuntimeNative3DPreparedFrame* empty=calloc(1,sizeof(*empty));assert(empty);
    assert(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(empty,64,64,&disabled,NULL));
    assert(!empty->scene.volume.enabled);
    for(int t=0;t<empty->scene.triangleMesh.triangleCount;++t) {
        int index=empty->scene.triangleMesh.triangles[t].sceneObjectIndex;
        assert(index<0 || index>=sceneSettings.objectCount || strcmp(sceneSettings.sceneObjects[index].type,"water_surface"));
    }
    RuntimeNative3DPreparedFrame_Free(empty);free(empty);
    assert(SceneEditorDocumentUndo(diagnostics,sizeof(diagnostics)));
    if(!reopen) {
        choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty());
        assert(SceneEditorDocumentOpen(scene,diagnostics,sizeof(diagnostics)));
        assert(SceneEditorAtmosphereRead(&value) && value.enabled && value.animated && !strcmp(value.source_kind,"scene_bundle"));
    }
    fprintf(stderr,"Atmosphere water PASS: imported existing bundle, actual sparse sidecar IDs, reverse/direct seeks, prepared water triangles, disable/undo, File Save/reopen; no solver\n");
}
