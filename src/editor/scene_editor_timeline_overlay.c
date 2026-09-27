#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_digest_overlay.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <math.h>
#include <stdio.h>

/* Evaluated markers are presentation only: scrubbing never changes authored
 * path points or the base camera used by editing tools. */
void SceneEditorTimelineRenderEvaluatedMarkers(SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector) {
    RayEvaluatedSceneSnapshot frame;
    if(!renderer || !projector || SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_RENDER ||
        !SceneEditorTimelineCopyEvaluated(&frame)) return;
    static char camera_label[64],light_label[64];
    TTF_Font* font=ray_tracing_font_runtime_get_ui_regular(renderer,10,8);
    int x,y;
    if(frame.camera.valid && SceneEditorDigestOverlayProjectPoint(projector,
        frame.camera.position.x,frame.camera.position.y,frame.camera.position.z,&x,&y)) {
        SDL_Rect body={x-7,y-5,14,10};
        SDL_SetRenderDrawColor(renderer,255,194,91,255);SDL_RenderFillRect(renderer,&body);
        double distance=projector->span_max*.06;
        double horizontal=cos(frame.camera.pitch_radians)*distance;
        int end_x,end_y;
        if(SceneEditorDigestOverlayProjectPoint(projector,
            frame.camera.position.x+sin(frame.camera.yaw_radians)*horizontal,
            frame.camera.position.y-cos(frame.camera.yaw_radians)*horizontal,
            frame.camera.position.z+sin(frame.camera.pitch_radians)*distance,&end_x,&end_y))
            SDL_RenderDrawLine(renderer,x,y,end_x,end_y);
        snprintf(camera_label,sizeof(camera_label),"Camera @ %lld",(long long)frame.frame.sample.absolute_frame);
        ray_tracing_text_draw_utf8_at(renderer,font,camera_label,x+10,y-16,(SDL_Color){255,214,151,255});
    }
    if(frame.light.valid && SceneEditorDigestOverlayProjectPoint(projector,
        frame.light.position.x,frame.light.position.y,frame.light.position.z,&x,&y)) {
        SDL_SetRenderDrawColor(renderer,255,247,157,255);
        SDL_RenderDrawLine(renderer,x-8,y,x+8,y);SDL_RenderDrawLine(renderer,x,y-8,x,y+8);
        snprintf(light_label,sizeof(light_label),"Light %.3g",frame.light.intensity);
        ray_tracing_text_draw_utf8_at(renderer,font,light_label,x+10,y+4,(SDL_Color){255,247,157,255});
    }
}
