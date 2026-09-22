/* Independent preparation accounting: many objects share content, not path names. */
#include "render/runtime_surface_sampling.h"
static double t2_clock_ms(void) {return 1000.0*(double)SDL_GetPerformanceCounter()/(double)SDL_GetPerformanceFrequency();}
static int t2_double_compare(const void* a,const void* b) {double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static void surface_resources_t2_cache_probe(SceneEditor* editor,const char* scene_path) {
    char diagnostic[512];RuntimeSceneBridgePreflight summary;
    assert(sceneSettings.objectCount==100);
    RuntimeSurfaceMappingReset();assert(RuntimeSurfaceSamplingCacheResetForTests());
    RuntimeSurfaceSamplingResetCacheStats();
    double cold=t2_clock_ms();
    assert(runtime_scene_bridge_preflight_file(scene_path,&summary));
    assert(runtime_scene_bridge_apply_file(scene_path,&summary));
    assert(SceneEditorDocumentOpen(scene_path,diagnostic,sizeof(diagnostic)));
    cold=t2_clock_ms()-cold;
    RuntimeSurfaceSamplingCacheStats initial;
    RuntimeSurfaceSamplingGetCacheStats(&initial);
    fprintf(stderr,"T2 cold: decodes=%llu images=%llu programs=%llu bytes=%zu hits=%llu\n",initial.decodes,initial.image_builds,initial.program_builds,initial.bytes,initial.cache_hits);
    assert(initial.decodes==1 && initial.image_builds==1 && initial.program_builds==1);
    assert(initial.live_entries==2 && initial.live_bytes<128u*1024u*1024u);
    for(int i=0;i<100;++i)assert(RuntimeSurfaceSamplingActive(i));
    RuntimeSurfaceSamplingResetCacheStats();double warm=t2_clock_ms();
    assert(runtime_scene_bridge_preflight_file(scene_path,&summary));
    assert(runtime_scene_bridge_apply_file(scene_path,&summary));warm=t2_clock_ms()-warm;
    RuntimeSurfaceSamplingCacheStats stats;RuntimeSurfaceSamplingGetCacheStats(&stats);
    assert(stats.decodes==0 && stats.image_builds==0 && stats.program_builds==0);
    SceneEditorDocumentTransform transform;
    assert(SceneEditorDocumentGetTransformForSceneIndex(0,&transform,diagnostic,sizeof(diagnostic)));
    double edits[20];
    for(int i=0;i<20;++i) {
        transform.position[0]+=.001;double start=t2_clock_ms();
        assert(SceneEditorDocumentSetTransformForSceneIndex(0,&transform,diagnostic,sizeof(diagnostic)));
        edits[i]=t2_clock_ms()-start;
    }
    RuntimeSurfaceSamplingGetCacheStats(&stats);
    assert(stats.decodes==0 && stats.program_builds==0);
    /* Pixel-footprint evaluation and ordinary viewport frames never prepare resources. */
    SceneEditorMeshPreviewModeSet(SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    for(int i=0;i<3;++i)SceneEditorSessionRuntimeRender(editor);
    RuntimeSurfaceSamplingGetCacheStats(&stats);assert(stats.decodes==0 && stats.program_builds==0);
    const char* image_path=getenv("OPTIC_T2_CACHE_IMAGE");assert(image_path);
    FILE* image=fopen(image_path,"r+b");assert(image);int first=fgetc(image);assert(first!=EOF);
    rewind(image);assert(fputc(first^1,image)!=EOF);assert(!fclose(image));
    assert(!runtime_scene_bridge_preflight_file(scene_path,&summary));
    for(int i=0;i<100;++i)assert(RuntimeSurfaceSamplingActive(i));
    image=fopen(image_path,"r+b");assert(image);assert(fputc(first,image)!=EOF);assert(!fclose(image));
    assert(runtime_scene_bridge_preflight_file(scene_path,&summary));
    /* Same bytes, another interpretation require another immutable pyramid. */
    RuntimeSurfaceSamplingResetCacheStats();
    assert(SceneEditorDocumentSurfaceSamplingSetChannel(0,"base_color",image_path,"linear",SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
    RuntimeSurfaceSamplingGetCacheStats(&stats);assert(stats.decodes==1 && stats.image_builds==1 && stats.program_builds==0);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    RuntimeSurfaceSamplingPurgeUnusedResources();RuntimeSurfaceSamplingGetCacheStats(&stats);
    assert(stats.evictions>=1 && stats.entries==stats.live_entries);
    size_t resident=stats.live_bytes;
    assert(RuntimeSurfaceSamplingSetCacheBudgetForTests(resident));
    unsigned long long revision=SceneEditorDocumentRevision();
    assert(!SceneEditorDocumentSurfaceSamplingSetChannel(0,"base_color",getenv("OPTIC_T2_RELINK"),"srgb",revision,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision()==revision && RuntimeSurfaceSamplingActive(0));
    assert(RuntimeSurfaceSamplingSetCacheBudgetForTests(128u*1024u*1024u));
    assert(SceneEditorDocumentSurfaceSamplingSetChannel(0,"base_color",getenv("OPTIC_T2_RELINK"),"srgb",revision,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    qsort(edits,20,sizeof(double),t2_double_compare);
    RuntimeSurfaceSamplingGetCacheStats(&stats);
    FILE* receipt=fopen("resources_t2_cache.json","w");assert(receipt);
    fprintf(receipt,"{\"objects\":100,\"cold_decodes\":%llu,\"cold_image_builds\":%llu,\"cold_program_builds\":%llu,\"cold_ms\":%.6f,\"warm_ms\":%.6f,\"frame_edit_p50_ms\":%.6f,\"frame_edit_p95_ms\":%.6f,\"live_bytes\":%zu,\"resident_bytes\":%zu,\"evictions\":%llu,\"warm_and_frame_zero_decode\":true,\"corrupt_pin_rejected\":true,\"interpretation_separated\":true,\"budget_recovered\":true}\n",initial.decodes,initial.image_builds,initial.program_builds,cold,warm,edits[9],edits[18],stats.live_bytes,stats.bytes,stats.evictions);
    assert(!fclose(receipt));
}
