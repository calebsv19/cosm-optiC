#include "editor/scene_editor_material_perf.h"
#include "core_time.h"
#include <string.h>
static bool active, settled;
static SceneEditorMaterialPerfSample sample;
static const unsigned char *frame_pixels;
static size_t frame_bytes;
void SceneEditorMaterialPerfEnable(bool enabled,bool force_settled) {
    active=enabled;settled=enabled && force_settled;
    if(!enabled){frame_pixels=NULL;frame_bytes=0;}
}
bool SceneEditorMaterialPerfForceSettled(void) {return settled;}
uint64_t SceneEditorMaterialPerfNow(void) {return active?core_time_now_ns():0;}
void SceneEditorMaterialPerfAdd(SceneEditorMaterialPerfStage stage,uint64_t start) {
    if(active && start && stage>=0 && stage<SCENE_MATERIAL_PERF_STAGE_COUNT)
        sample.ns[stage]+=core_time_diff_ns(core_time_now_ns(),start);
}
void SceneEditorMaterialPerfBeginSample(void) {memset(&sample,0,sizeof(sample));}
void SceneEditorMaterialPerfFrame(bool rasterized,bool interactive,int width,int height,
                                  size_t submitted,size_t rendered,int instances) {
    if(!active)return;
    sample.rasterized=rasterized;sample.interactive=interactive;sample.width=width;sample.height=height;
    sample.submitted_triangles=submitted;sample.rendered_triangles=rendered;sample.instances=instances;
}
SceneEditorMaterialPerfSample SceneEditorMaterialPerfRead(void) {return sample;}

void SceneEditorMaterialPerfPixels(const unsigned char *pixels,size_t size) {if(active || !pixels){frame_pixels=pixels;frame_bytes=size;}}
uint64_t SceneEditorMaterialPerfPixelHash(void) {
    if(!active)return 0;
    uint64_t hash=UINT64_C(1469598103934665603);
    for(size_t i=0;frame_pixels && i<frame_bytes;++i){hash^=frame_pixels[i];hash*=UINT64_C(1099511628211);}
    return hash;
}
