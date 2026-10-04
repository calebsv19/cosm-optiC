#include "editor/scene_editor_atmosphere_preview.h"
#include "editor/scene_editor_atmosphere_panel.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_workspace_profile.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "vk_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native render state is global. Prepare and small row batches belong to the
 * editor thread, never a second concurrent renderer or a save-to-disk proxy. */
enum {WIDTH=160,HEIGHT=100};
static struct {
    bool enabled,valid,failed,uploaded,texture_valid;
    uint64_t requested,completed;
    int row;
    int64_t rendered_frame;
    unsigned long long document_revision;
    unsigned char work[WIDTH*HEIGHT*4],pixels[WIDTH*HEIGHT*4];
    RuntimeNative3DPreparedFrame* frame;
    VkRenderer* owner;
    VkRendererTexture texture;
    char status[512];
} preview={.enabled=true};
static void release_frame(void) {
    if(preview.frame) {RuntimeNative3DPreparedFrame_Free(preview.frame);free(preview.frame);preview.frame=NULL;}
}
static uint64_t hash_bytes(uint64_t hash,const void* data,size_t size) {
    const unsigned char* p=data;
    while(size--) {hash^=*p++;hash*=UINT64_C(1099511628211);}return hash;
}
static uint64_t signature(const RayEvaluatedSceneSnapshot* s) {
    uint64_t h=UINT64_C(1469598103934665603),revision=SceneEditorDocumentRevision();
    h=hash_bytes(h,&revision,sizeof(revision));
    h=hash_bytes(h,&s->identity,sizeof(s->identity));
    h=hash_bytes(h,&s->frame,sizeof(s->frame));
    h=hash_bytes(h,&s->camera,sizeof(s->camera));
    h=hash_bytes(h,&s->light,sizeof(s->light));
    h=hash_bytes(h,&s->simulation,sizeof(s->simulation));
    return hash_bytes(h,s->object_transforms,s->object_transform_count*sizeof(s->object_transforms[0]));
}
void SceneEditorAtmospherePreviewReset(void) {
    bool enabled=preview.enabled;release_frame();
    if(preview.texture_valid && preview.owner) {
        vk_renderer_wait_idle(preview.owner);
        vk_renderer_texture_destroy(preview.owner,&preview.texture);
    }
    memset(&preview,0,sizeof(preview));preview.enabled=enabled;
}
void SceneEditorAtmospherePreviewSetEnabled(bool enabled) {
    if(preview.enabled!=enabled) {SceneEditorAtmospherePreviewReset();preview.enabled=enabled;}
}
bool SceneEditorAtmospherePreviewEnabled(void) {return preview.enabled;}
bool SceneEditorAtmospherePreviewPending(void) {return preview.frame!=NULL;}
const char* SceneEditorAtmospherePreviewStatus(void) {return preview.status;}
void SceneEditorAtmospherePreviewTick(void) {
    if(!preview.enabled || SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_ENVIRONMENT || !SceneEditorDocumentIsOpen()) {
        release_frame();preview.requested=0;return;
    }
    if(SceneEditorAtmospherePanelInteractionActive()) return;
    static RayEvaluatedSceneSnapshot snapshot;
    if(!SceneEditorTimelineCopyEvaluated(&snapshot)) {
        release_frame();preview.valid=false;preview.requested=0;
        snprintf(preview.status,sizeof(preview.status),"Preview unavailable: %s",SceneEditorTimelineStatus());return;
    }
    uint64_t requested=signature(&snapshot);
    /* During playback complete the bounded in-flight image, then sample the
     * newest playhead. Cancelling on every advancing frame would starve output.
     * Direct seeks and document edits replace the image immediately. */
    bool finish_playback=preview.frame && SceneEditorTimelinePlaying() &&
        preview.document_revision==SceneEditorDocumentRevision();
    if(requested!=preview.requested && !finish_playback) {
        release_frame();preview.requested=requested;preview.failed=false;preview.row=0;
        preview.rendered_frame=snapshot.frame.sample.absolute_frame;preview.document_revision=SceneEditorDocumentRevision();
        preview.frame=calloc(1,sizeof(*preview.frame));
        if(!preview.frame || !RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(preview.frame,WIDTH,HEIGHT,&snapshot,NULL)) {
            snprintf(preview.status,sizeof(preview.status),"Preview failed: %.450s",RuntimeNative3DPrepareFrameLastDiagnostics());
            release_frame();preview.failed=true;preview.valid=false;return;
        }
        memset(preview.work,0,sizeof(preview.work));
        snprintf(preview.status,sizeof(preview.status),"Updating scene-camera preview at frame %lld...",(long long)snapshot.frame.sample.absolute_frame);
    }
    if(!preview.frame || preview.failed) return;
    /* Editor evaluation/picking can rebind the process-wide acceleration owner
     * between batches. Restore this exact prepared scene before every batch. */
    if(!RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(&preview.frame->scene)) {
        snprintf(preview.status,sizeof(preview.status),"Native preview acceleration binding failed.");
        release_frame();preview.failed=true;preview.valid=false;return;
    }
    /* One row is the atomic renderer unit; stop after 4 ms or eight rows. */
    Uint64 started=SDL_GetPerformanceCounter(),budget=SDL_GetPerformanceFrequency()/250;
    for(int rows=0;rows<8 && preview.row<HEIGHT;++rows) {
        if(!RuntimeNative3DRenderPreparedRegion(preview.work,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
            preview.frame,0,preview.row,WIDTH,preview.row+1,NULL)) {
            snprintf(preview.status,sizeof(preview.status),"Native preview rendering failed.");
            release_frame();preview.failed=true;preview.valid=false;return;
        }
        ++preview.row;if(SDL_GetPerformanceCounter()-started>=budget) break;
    }
    if(preview.row==HEIGHT) {
        memcpy(preview.pixels,preview.work,sizeof(preview.pixels));preview.valid=true;preview.uploaded=false;
        preview.completed=preview.requested;release_frame();
        snprintf(preview.status,sizeof(preview.status),"Scene camera | Direct light preview | Frame %lld",(long long)preview.rendered_frame);
    }
}
bool SceneEditorAtmospherePreviewCopyPixels(unsigned char* out,size_t size,int* width,int* height) {
    if(!preview.valid || preview.completed!=preview.requested || !out || size<sizeof(preview.pixels)) return false;
    memcpy(out,preview.pixels,sizeof(preview.pixels));if(width) *width=WIDTH;if(height) *height=HEIGHT;return true;
}
bool SceneEditorAtmospherePreviewDraw(SDL_Renderer* renderer,SDL_Rect viewport) {
    if(!preview.enabled || SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_ENVIRONMENT) return false;
    VkRenderer* vk=(VkRenderer*)renderer;
    if(preview.texture_valid && preview.owner!=vk) SceneEditorAtmospherePreviewReset();
    if(preview.valid && !preview.uploaded) {
        if(!preview.texture_valid) {
            preview.texture_valid=vk_renderer_texture_create_from_rgba(vk,preview.pixels,WIDTH,HEIGHT,VK_FILTER_LINEAR,&preview.texture)==VK_SUCCESS;
            preview.owner=vk;preview.uploaded=preview.texture_valid;
        } else preview.uploaded=vk_renderer_texture_update_rgba_subrect(vk,&preview.texture,preview.pixels,WIDTH*4,0,0,WIDTH,HEIGHT)==VK_SUCCESS;
    }
    SDL_SetRenderDrawColor(renderer,12,14,18,255);SDL_RenderFillRect(renderer,&viewport);
    if(preview.valid && preview.texture_valid) {
        SDL_Rect dst=viewport;
        if(dst.w*HEIGHT>dst.h*WIDTH) {dst.w=dst.h*WIDTH/HEIGHT;dst.x+=(viewport.w-dst.w)/2;}
        else {dst.h=dst.w*HEIGHT/WIDTH;dst.y+=(viewport.h-dst.h)/2;}
        vk_renderer_draw_texture(vk,&preview.texture,NULL,&dst);
    }
    ray_tracing_text_draw_utf8_at(renderer,ray_tracing_font_runtime_get_ui_regular(renderer,11,8),preview.status,
        viewport.x+8,viewport.y+8,(SDL_Color){230,235,245,255});
    return true;
}
