#include "editor/scene_editor_material_perf.h"
#include "editor/scene_editor_mesh_preview_surface.h"
#include "core_time.h"
#include "engine/Render/render_pipeline.h"
#include <sys/resource.h>
#include <inttypes.h>
static uint64_t material_perf_t4_rss(void) {
    struct rusage usage;assert(getrusage(RUSAGE_SELF,&usage)==0);
#ifdef __APPLE__
    return (uint64_t)usage.ru_maxrss;
#else
    return (uint64_t)usage.ru_maxrss*1024;
#endif
}
static double material_perf_t4_frame(SceneEditor *editor,SceneEditorDigestOverlayProjector *projector) {
    uint64_t begin=core_time_now_ns();assert(render_begin_frame());
    SDL_RenderSetClipRect(editor->renderer,NULL);
    SceneEditorMeshPreviewFrameStats stats={0};
    assert(SceneEditorMeshPreviewSurfaceRender(editor->renderer,projector,editor->currentMode,-1,-1,SCENE_EDITOR_MESH_DISPLAY_MATERIAL,&stats));
    render_end_frame();
    return (double)core_time_diff_ns(core_time_now_ns(),begin)/1e6;
}
static void material_perf_t4_sample(FILE *out,const char *phase,int iteration,double frame_ms,double command_ms,double refresh_ms) {
    SceneEditorMaterialPerfSample s=SceneEditorMaterialPerfRead();
    fprintf(out,"{\"phase\":\"%s\",\"iteration\":%d,\"frame_ms\":%.9g,\"document_command_ms\":%.9g,\"preview_refresh_ms\":%.9g,\"stage_ms\":[",phase,iteration,frame_ms,command_ms,refresh_ms);
    for(int i=0;i<SCENE_MATERIAL_PERF_STAGE_COUNT;++i)fprintf(out,"%s%.9g",i?",":"",s.ns[i]/1e6);
    fprintf(out,"],\"rasterized\":%s,\"interactive\":%s,\"width\":%d,\"height\":%d,\"submitted_triangles\":%zu,\"rendered_triangles\":%zu,\"instances\":%d,\"pixel_fnv64\":\"%016" PRIx64 "\",\"peak_rss_bytes\":%" PRIu64 "}",s.rasterized?"true":"false",s.interactive?"true":"false",s.width,s.height,s.submitted_triangles,s.rendered_triangles,s.instances,SceneEditorMaterialPerfPixelHash(),material_perf_t4_rss());
}
static void material_performance_t4_probe(SceneEditor *editor) {
    const char *requested=getenv("OPTIC_T4_TRIANGLES"),*instances=getenv("OPTIC_T4_INSTANCES"),*count_text=getenv("OPTIC_T4_SAMPLES");
    assert(requested && instances);int count=count_text?atoi(count_text):20;assert(count>=5 && count<=100);
    SceneEditorMaterialPerfEnable(true,true);SceneEditorMaterialPerfBeginSample();
    uint64_t initial_rss=material_perf_t4_rss();
    SceneEditorMeshPreviewStorePrepare(ray_tracing_runtime_mesh_assets_last());
    SceneEditorMaterialPerfSample preparation=SceneEditorMaterialPerfRead();
    assert(SceneEditorMeshPreviewStoreInstanceCount()==atoi(instances));
    const CoreMeshPreviewLodMesh *lod=SceneEditorMeshPreviewStoreGetForQuality(0,false);assert(lod && lod->attribute_protected && lod->surface_corners && lod->surface_corner_count==lod->triangle_count*3 && !strcmp(lod->uv_set_id,"paint_uv"));
    RuntimeSceneBridge3DDigestState digest={0};runtime_scene_bridge_get_last_3d_digest_state(&digest);
    SDL_Rect viewport={0,0,640,480};SceneEditorDigestOverlayProjector projector;
    assert(SceneEditorDigestOverlayBuildProjectorWithView(&digest,&viewport,25,35,1,&projector));
    for(int i=0;i<3;++i){projector.yaw_rad+=.02;material_perf_t4_frame(editor,&projector);}
    FILE *out=fopen("material_performance_t4.json","w");assert(out);
    fprintf(out,"{\"schema\":\"optic.material_preview_baseline_v1\",\"fixed_viewport\":[640,480],\"quality\":\"test_forced_settled\",\"requested_asset_triangles\":%s,\"requested_instances\":%s,\"actual_asset_triangles\":%zu,\"attribute_protected\":%s,\"geometry_prepare_ms\":%.9g,\"initial_peak_rss_bytes\":%" PRIu64 ",\"stage_names\":[\"geometry_prepare\",\"signature\",\"buffer_prepare\",\"raster_shade\",\"outline\",\"texture_upload\"],\"samples\":[",requested,instances,lod->triangle_count,lod->attribute_protected?"true":"false",preparation.ns[SCENE_MATERIAL_PERF_GEOMETRY]/1e6,initial_rss);
    bool comma=false;
    for(int i=0;i<count;++i){SceneEditorMaterialPerfBeginSample();projector.yaw_rad+=.013;
        double frame=material_perf_t4_frame(editor,&projector);if(comma)fputc(',',out);comma=true;
        material_perf_t4_sample(out,"orbit",i,frame,0,0);assert(SceneEditorMaterialPerfRead().width==640 && SceneEditorMaterialPerfRead().height==480);}
    for(int i=0;i<count;++i){SceneEditorMaterialPerfBeginSample();double frame=material_perf_t4_frame(editor,&projector);fputc(',',out);material_perf_t4_sample(out,"cache_hit",i,frame,0,0);assert(!SceneEditorMaterialPerfRead().rasterized);}
    json_object *row=authoring_t1_row(0),*graph=authoring_t1_member(row,"surface_graph"),*scalar=authoring_t1_node(graph,"low");
    char diagnostic[1024];
    RuntimeSurfaceMeshSamplePrepared stale_triangle;
    assert(RuntimeSurfaceMaterialPrepareMeshTriangle(0,0,0,lod,NULL,NULL,&stale_triangle));
    for(int i=0;i<count;++i){SceneEditorMaterialPerfBeginSample();json_object_object_add(scalar,"value",json_object_new_double(i%2?.21:.2));
        uint64_t begin=core_time_now_ns();
        assert(SceneEditorDocumentSetSurfaceGraph(0,json_object_to_json_string(graph),SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
        double command=(double)core_time_diff_ns(core_time_now_ns(),begin)/1e6;
        if(!i){RuntimeMaterialSurfaceEval stale_eval;double weights[]={.2,.3,.5};
            assert(!RuntimeSurfaceMaterialSamplePreparedMesh(&stale_triangle,weights,vec3(0,0,0),vec3(0,0,1),&stale_eval));}
        begin=core_time_now_ns();SceneEditorMeshPreviewSurfaceReset(editor->renderer);SceneEditorMeshPreviewStorePrepare(ray_tracing_runtime_mesh_assets_last());
        double refresh=(double)core_time_diff_ns(core_time_now_ns(),begin)/1e6;
        double frame=material_perf_t4_frame(editor,&projector);fputc(',',out);material_perf_t4_sample(out,"edit",i,frame,command,refresh);
    }
    json_object_put(row);
    SceneEditorMaterialPerfEnable(true,false);SceneEditorMaterialPerfBeginSample();projector.yaw_rad+=.013;
    double natural=material_perf_t4_frame(editor,&projector);fputc(',',out);material_perf_t4_sample(out,"production_interactive",0,natural,0,0);
    fprintf(out," ],\"uv_set_id\":\"paint_uv\",\"per_corner_uv_normals_tangents\":true,\"peak_rss_bytes\":%" PRIu64 ",\"document_command_scope\":\"snapshot+serialize+temporary_file+runtime_reapply+history\",\"frame_scope\":\"native_surface_begin_draw_present_no_inspector\"}\n",material_perf_t4_rss());assert(!fclose(out));
    SceneEditorMaterialPerfEnable(false,false);
}
