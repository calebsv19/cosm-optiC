#define _DARWIN_C_SOURCE
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_rigid.h"
#include "editor/scene_editor_timeline.h"
#include "../src/editor/scene_editor_timeline_ui.h"
#include "../src/editor/scene_editor_timeline_commands.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_shared_ui.h"
#include "render/font_runtime.h"
#include "import/runtime_rigid_result.h"
#include "../src/editor/scene_editor_rigid_panel.h"
#include "../src/editor/scene_editor_rigid_job.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"BR5 line %d: %s: %s\n",__LINE__,#x,d);return 3;}} while(0)
int main(int argc,char **argv) {
    if(argc!=6) return 2;
    char d[2048]={0};LoadAllSettings();
    CHECK(SceneEditorDocumentSwitch(argv[1],false,d,sizeof(d)));
    CHECK(SceneEditorDocumentValidateRigidResult(d,sizeof(d)));
    CHECK(!SceneEditorDocumentIsDirty());
    CHECK(SceneEditorTimelineActivate());
    size_t selected;const TimelineDocument *doc=SceneEditorTimelineDocumentView(&selected);
    CHECK(doc && doc->track_count==0 && doc->range.frame_count==181 && doc->rate.frames_per_second_numerator==240);
    CHECK(!SceneEditorDocumentIsDirty());
    CHECK(SceneEditorTimelineSeek(154));
    static RayEvaluatedSceneSnapshot before,after;
    CHECK(SceneEditorTimelineCopyEvaluated(&before));
    CHECK(before.object_transform_count==4 && before.object_transforms[3].source_tick==154);
    unsigned long long revision=SceneEditorDocumentRevision();
    CHECK(!SceneEditorDocumentSelectRigidResult(argv[2],argv[3],revision+1,d,sizeof(d)));
    CHECK(!SceneEditorDocumentSelectRigidResult("missing",argv[3],revision,d,sizeof(d)));
    CHECK(SceneEditorDocumentRevision()==revision && !SceneEditorDocumentIsDirty());
    CHECK(SceneEditorTimelineCopyEvaluated(&after));
    CHECK(!memcmp(before.object_transforms,after.object_transforms,4*sizeof(before.object_transforms[0])));
    CHECK(SceneEditorDocumentSelectRigidResult(argv[2],argv[3],revision,d,sizeof(d)));
    CHECK(SceneEditorDocumentIsDirty() && SceneEditorDocumentCanUndo());
    CHECK(SceneEditorDocumentUndo(d,sizeof(d)));CHECK(SceneEditorDocumentRedo(d,sizeof(d)));
    CHECK(SceneEditorDocumentSave(d,sizeof(d)));
    CHECK(SceneEditorDocumentSwitch(argv[1],false,d,sizeof(d)));
    CHECK(SceneEditorDocumentValidateRigidResult(d,sizeof(d)));
    CHECK(SceneEditorTimelineSeek(154));
    CHECK(SceneEditorTimelineCopyEvaluated(&after));
    CHECK(!memcmp(before.object_transforms,after.object_transforms,4*sizeof(before.object_transforms[0])));
    CHECK(!SceneEditorTimelineSeek(181));
    CHECK(SceneEditorTimelineCopyEvaluated(&after));
    CHECK(after.object_transforms[3].source_tick==154);
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0) {fprintf(stderr,"SDL video: %s\n",SDL_GetError());return 4;}
    SDL_Window *window=SDL_CreateWindow("BR5 local inspection proof",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,600,700,SDL_WINDOW_VULKAN);
    CHECK(window);static VkRenderer renderer_storage;SDL_Renderer *renderer=&renderer_storage;
    VkRendererConfig cfg;vk_renderer_config_set_defaults(&cfg);cfg.enable_validation=VK_FALSE;
    CHECK(vk_renderer_init(renderer,window,&cfg)==VK_SUCCESS);
    CHECK(ray_tracing_font_runtime_init());ray_tracing_font_runtime_attach_renderer(renderer);
    SceneEditor editor={.renderer=renderer,.window=window};
    SceneEditorWorkspaceProfileSelect(&editor,SCENE_WORKSPACE_RENDER);
    SceneEditorPaneLayout layout={.timeline_visible=true,.timeline_rect={0,500,600,200}};
    VkCommandBuffer cmd;VkFramebuffer fb;VkExtent2D extent;
    CHECK(vk_renderer_begin_frame(renderer,&cmd,&fb,&extent)==VK_SUCCESS);
    SceneEditorTimelineRender(renderer,&layout);
    SDL_Rect control;CHECK(SceneEditorTimelineControl("play",&control));
    SDL_Event event={0};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;event.button.x=control.x+2;event.button.y=control.y+2;
    CHECK(SceneEditorTimelineHandleEvent(&event,&layout));CHECK(SceneEditorTimelinePlaying());
    CHECK(SceneEditorTimelineHandleEvent(&event,&layout));CHECK(!SceneEditorTimelinePlaying());
    CHECK(SceneEditorTimelineControl("next",&control));event.button.x=control.x+2;event.button.y=control.y+2;
    CHECK(SceneEditorTimelineHandleEvent(&event,&layout));TimelineSample at;CHECK(SceneEditorTimelineCurrentSample(&at));CHECK(at.absolute_frame==155);
    CHECK(SceneEditorTimelineSeek(0));CHECK(SceneEditorTimelineSeek(180));CHECK(SceneEditorTimelineSeek(154));
    SceneEditorSharedUiBegin(&editor);
    SDL_Rect pane={0,0,600,490};SDL_RenderSetClipRect(renderer,&pane);
    SceneEditorRigidPanelDraw(renderer,pane,5);
    SDL_RenderSetClipRect(renderer,NULL);
    SceneEditorSharedUiEnd();
    CHECK(vk_renderer_request_capture(renderer,argv[4])==VK_SUCCESS);
    CHECK(vk_renderer_end_frame(renderer,cmd)==VK_SUCCESS);
    for(int i=0;i<3;i++) {CHECK(vk_renderer_begin_frame(renderer,&cmd,&fb,&extent)==VK_SUCCESS);CHECK(vk_renderer_end_frame(renderer,cmd)==VK_SUCCESS);}
    CHECK(SceneEditorRigidPanelControl("rigid_validate",&control));
    event.button.x=control.x+2;event.button.y=control.y+2;CHECK(SceneEditorRigidPanelEvent(&event));
    CHECK(access(argv[4],R_OK)==0);
    /* Real native producer job, not a replacement script. */
    CHECK(SceneEditorRigidJobBegin(argv[5],d,sizeof(d)));
    CHECK(!SceneEditorRigidJobBegin(argv[5],d,sizeof(d)));
    for(unsigned i=0;SceneEditorRigidJobBusy() && i<9000;i++) {SceneEditorRigidJobPoll(d,sizeof(d));SDL_Delay(10);}
    CHECK(!SceneEditorRigidJobBusy());CHECK(strstr(d,"Prepared result opened"));
    CHECK(SceneEditorDocumentValidateRigidResult(d,sizeof(d)));
    CHECK(SceneEditorTimelineSeek(154));CHECK(SceneEditorTimelineCopyEvaluated(&after));
    CHECK(!memcmp(before.object_transforms,after.object_transforms,4*sizeof(before.object_transforms[0])));
    CHECK(SceneEditorDocumentSave(d,sizeof(d)));
    /* The same create-only destination must fail without replacing playback. */
    char retained_path[4096];snprintf(retained_path,sizeof(retained_path),"%s",SceneEditorDocumentPath());
    CHECK(SceneEditorRigidJobBegin(argv[5],d,sizeof(d)));
    for(unsigned i=0;SceneEditorRigidJobBusy() && i<9000;i++) {SceneEditorRigidJobPoll(d,sizeof(d));SDL_Delay(10);}
    CHECK(!SceneEditorRigidJobBusy() && strstr(d,"Preparation failed"));
    CHECK(!strcmp(retained_path,SceneEditorDocumentPath()));
    CHECK(SceneEditorTimelineCopyEvaluated(&after));
    CHECK(!memcmp(before.object_transforms,after.object_transforms,4*sizeof(before.object_transforms[0])));
    printf("BR5 {\"ok\":true,\"zero_authored_tracks\":true,\"rollback\":true,\"undo_redo\":true,\"save_reopen\":true,\"transport_events\":true,\"native_producer_job\":true,\"failed_job_preserves_playback\":true}\n");
    SceneEditorRigidPanelReset();SceneEditorDocumentClose();
    ray_tracing_font_runtime_detach_renderer(renderer);ray_tracing_font_runtime_shutdown();
    vk_renderer_shutdown(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
