#include "editor/scene_editor_atmosphere_preview.h"
#include "editor/scene_editor_atmosphere.h"
#include "editor/scene_editor_atmosphere_panel.h"
#include "editor/scene_editor_document_timeline.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_scatter.h"
#include "render/runtime_volume_3d_sampling.h"
#include "render/runtime_volume_3d_transform.h"
#include "render/runtime_native_3d_render.h"
#include "app/evaluated_scene_service.h"
#include "editor/scene_editor_agent_control.h"
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
extern char** environ;

static void atmosphere_mcp_native(SceneEditor* editor,const char* scene) {
    const char* script=getenv("OPTIC_ATMOSPHERE_MCP_TEST");assert(script);
    char* args[]={"python3",(char*)script,(char*)scene,NULL};pid_t pid;
    assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);
    int status=0;bool finished=false;
    for(int i=0;i<2000;++i) {
        (void)SceneEditorAgentControlPoll();SDL_PumpEvents();
        SceneEditorSessionRuntimeRender(editor);
        if(waitpid(pid,&status,WNOHANG)==pid) {finished=true;break;}
        SDL_Delay(10);
    }
    if(!finished) {kill(pid,SIGTERM);waitpid(pid,&status,0);}
    assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
    SceneAtmosphereAttachment v;assert(SceneEditorAtmosphereRead(&v) && v.present && !v.animated);
    assert(!SceneEditorDocumentIsDirty());capture(editor,"atmosphere_mcp_shared_state.ppm");
}

static void atmosphere_control(SceneEditor* editor,const char* name) {
    SceneEditorSessionRuntimeRender(editor);SDL_Rect rect;
    /* Find controls through the same sidebar scrolling as a normal user. */
    if(!SceneEditorAtmospherePanelControl(name,&rect)) {
        SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
        SDL_Event e={0};e.type=SDL_MOUSEWHEEL;e.wheel.windowID=SDL_GetWindowID(editor->window);
        e.wheel.mouseX=layout.left_content_rect.x+20;e.wheel.mouseY=layout.left_content_rect.y+20;
        e.wheel.y=100;e.wheel.preciseY=100;SceneEditorSessionRuntimeHandleEvent(editor,&e);
        SceneEditorSessionRuntimeRender(editor);
        for(int i=0;i<120 && !SceneEditorAtmospherePanelControl(name,&rect);++i) {
            e.wheel.y=-10;e.wheel.preciseY=-10;SceneEditorSessionRuntimeHandleEvent(editor,&e);
            SceneEditorSessionRuntimeRender(editor);
        }
    }
    assert(SceneEditorAtmospherePanelControl(name,&rect));click(editor,rect);
    SceneEditorSessionRuntimeRender(editor);
}
static void atmosphere_preview_pixels(SceneEditor* editor,unsigned char* pixels) {
    if(!SceneEditorAtmospherePreviewEnabled()) atmosphere_control(editor,"preview");
    int width=0,height=0;
    for(int i=0;i<1000;++i) {
        SceneEditorSessionRuntimeRender(editor);
        if(SceneEditorAtmospherePreviewCopyPixels(pixels,160*100*4,&width,&height)) {
            assert(width==160 && height==100);return;
        }
        SDL_Delay(1);
    }
    fprintf(stderr,"Preview timeout: %s\n",SceneEditorAtmospherePreviewStatus());assert(false);
}
static void atmosphere_native(SceneEditor* editor,const char* scene,bool reopen) {
    char message[512];SceneAtmosphereAttachment value;
    if(reopen) {
        assert(SceneEditorAtmosphereRead(&value) && value.present && value.animated && value.density_scale==2);
        assert(value.source_rate.frames_per_second_numerator==24);
        fprintf(stderr,"Atmosphere fresh reopen PASS\n");return;
    }
    assert(SceneEditorAtmosphereRead(&value));
    if(value.present) assert(SceneEditorAtmosphereRemove(SceneEditorDocumentRevision(),message,sizeof(message)));
    static TimelineDocument timeline;
    assert(TimelineDocumentInit(&timeline,(TimelineRate){24,1},(TimelineRange){0,8})==TIMELINE_STATUS_OK);
    assert(SceneEditorDocumentSetTimeline(&timeline,SceneEditorDocumentRevision(),message,sizeof(message)));
    RuntimeVolume3DMaterial_SetDensityScale(3);
    RuntimeVolume3DScatter_SetStrengthGain(4);
    SceneAtmosphereAttachmentDefaults(&value);value.present=true;value.animated=true;
    strcpy(value.source_path,"assets/cache/manifest.json");
    assert(SceneEditorAtmosphereSet(&value,SceneEditorDocumentRevision(),message,sizeof(message)));
    assert(RuntimeVolume3DMaterial_GetTuning().density_scale==1);
    unsigned long long revision=SceneEditorDocumentRevision();
    SceneAtmosphereAttachment invalid=value;strcpy(invalid.source_path,"assets/missing.json");
    assert(!SceneEditorAtmosphereSet(&invalid,revision,message,sizeof(message)));
    assert(SceneEditorDocumentRevision()==revision);
    assert(!SceneEditorAtmosphereSet(&value,revision-1,message,sizeof(message)));
    assert(SceneEditorDocumentUndo(message,sizeof(message)));
    assert(RuntimeVolume3DMaterial_GetTuning().density_scale==3);
    assert(RuntimeVolume3DScatter_GetTuning().strength==4);
    assert(SceneEditorDocumentRedo(message,sizeof(message)));
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_ENVIRONMENT);
    atmosphere_control(editor,"appearance");
    atmosphere_control(editor,"density");
    SDL_Event input={0};input.type=SDL_TEXTINPUT;strcpy(input.text.text,"2");
    SceneEditorSessionRuntimeHandleEvent(editor,&input);key(editor,SDLK_RETURN);
    assert(SceneEditorAtmosphereRead(&value) && value.density_scale==2);
    atmosphere_control(editor,"enabled");
    atmosphere_control(editor,"appearance");atmosphere_control(editor,"playback");
    atmosphere_control(editor,"rate");
    memset(&input,0,sizeof(input));input.type=SDL_TEXTINPUT;strcpy(input.text.text,"30000/1001");
    SceneEditorSessionRuntimeHandleEvent(editor,&input);key(editor,SDLK_RETURN);
    assert(SceneEditorAtmosphereRead(&value) && value.source_rate.frames_per_second_numerator==30000 && value.source_rate.frames_per_second_denominator==1001);
    assert(SceneEditorDocumentUndo(message,sizeof(message)));
    assert(SceneEditorAtmosphereRead(&value) && !value.enabled);
    assert(RuntimeVolume3DMaterial_GetTuning().density_scale==3);
    atmosphere_control(editor,"enabled");
    static unsigned char first[160*100*4],last[160*100*4],reference[160*100*4];
    assert(SceneEditorTimelineSeek(0));atmosphere_preview_pixels(editor,first);
    assert(SceneEditorTimelineSeek(4));atmosphere_preview_pixels(editor,last);
    assert(memcmp(first,last,sizeof(first))!=0);
    RayEvaluatedSceneSnapshot displayed;assert(SceneEditorTimelineCopyEvaluated(&displayed));
    RuntimeNative3DPreparedFrame* exact=calloc(1,sizeof(*exact));assert(exact);
    assert(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(exact,160,100,&displayed,NULL));
    assert(RuntimeNative3DRenderPreparedRegion(reference,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,exact,0,0,160,100,NULL));
    assert(memcmp(last,reference,sizeof(last))==0);
    RuntimeNative3DPreparedFrame_Free(exact);free(exact);
    atmosphere_control(editor,"appearance");atmosphere_control(editor,"density");
    memset(&input,0,sizeof(input));input.type=SDL_TEXTINPUT;strcpy(input.text.text,"0");
    SceneEditorSessionRuntimeHandleEvent(editor,&input);key(editor,SDLK_RETURN);
    atmosphere_preview_pixels(editor,first);assert(memcmp(first,last,sizeof(first))!=0);
    assert(SceneEditorDocumentUndo(message,sizeof(message)));atmosphere_preview_pixels(editor,first);
    assert(memcmp(first,last,sizeof(first))==0);
    atmosphere_control(editor,"appearance");
    capture(editor,"atmosphere_environment.ppm");
    assert(SceneEditorGetPaneLayout(&(SceneEditorPaneLayout){0}));
    SDL_Rect playback;assert(SceneEditorTimelineControl("play",&playback));
    atmosphere_control(editor,"preview");assert(!SceneEditorAtmospherePreviewEnabled());
    assert(!SceneEditorAtmospherePreviewCopyPixels(first,sizeof(first),NULL,NULL));
    atmosphere_control(editor,"preview");assert(SceneEditorAtmospherePreviewEnabled());
    atmosphere_preview_pixels(editor,last);
    assert(SceneEditorDocumentGetTimeline(&timeline)==TIMELINE_STATUS_OK);
    timeline.range.frame_count=240;
    assert(SceneEditorDocumentSetTimeline(&timeline,SceneEditorDocumentRevision(),message,sizeof(message)));
    assert(SceneEditorTimelineSeek(0));SceneEditorTimelineTogglePlaying();
    bool completed_while_playing=false;int images=0;
    for(int i=0;i<80;++i) {
        SDL_Delay(20);SceneEditorSessionRuntimeRender(editor);
        if(SceneEditorAtmospherePreviewCopyPixels(first,sizeof(first),NULL,NULL)) {
            ++images;if(SceneEditorTimelinePlaying()) completed_while_playing=true;
        }
        if(images>=2) break;
    }
    assert(completed_while_playing && images>=2);
    TimelineSample advanced;assert(SceneEditorTimelineCurrentSample(&advanced) && advanced.absolute_frame>0);
    SceneEditorTimelinePause();assert(SceneEditorDocumentUndo(message,sizeof(message)));
    assert(SceneEditorTimelineSeek(4));atmosphere_preview_pixels(editor,last);
    fprintf(stderr,"Atmosphere progressive playback PASS: images complete while the shared clock advances\n");
    fprintf(stderr,"Atmosphere native preview PASS: seek image changes, exact native pixels, timeline and toggle\n");
    const int samples[]={0,4,1,3,7,2,4};const int selected[]={0,4,0,2,4,2,4};
    for(int i=0;i<7;++i) {
        assert(SceneEditorTimelineSeek(samples[i]));RayEvaluatedSceneSnapshot snapshot;
        assert(SceneEditorTimelineCopyEvaluated(&snapshot));
        assert(snapshot.simulation.valid && snapshot.simulation.source_frame_index==selected[i]);
        RuntimeNative3DPreparedFrame* prepared=calloc(1,sizeof(*prepared));assert(prepared);
        if(i==0) {
            RayEvaluatedSceneSnapshot stale=snapshot;stale.simulation.content_digest[0]=stale.simulation.content_digest[0]=='0'?'1':'0';
            assert(!RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(prepared,64,64,&stale,NULL));
            assert(strstr(RuntimeNative3DPrepareFrameLastDiagnostics(),"Cache changed after scene evaluation"));
        }
        assert(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(prepared,64,64,&snapshot,NULL));
        assert(prepared->scene.volume.grid.frameIndex==(uint64_t)selected[i]);
        assert(RuntimeVolume3D_SetPlacement(&prepared->scene.volume,vec3(2,3,4),vec3(0,0,90),vec3(2,3,4)));
        Vec3 origin=prepared->scene.volume.grid.origin;
        Vec3 point=RuntimeVolume3D_ToCachePoint(&prepared->scene.volume,vec3_add(origin,vec3(2,5,4)));
        assert(fabs(point.x-origin.x-1)<1e-8 && fabs(point.y-origin.y)<1e-8);
        RuntimeVolumeAttachment3D original=prepared->scene.volume;original.hasPlacement=false;
        double voxel=original.grid.voxelSize;
        Vec3 cached=vec3_add(origin,vec3(voxel*.5,voxel*.5,voxel*.5));
        Vec3 moved=vec3_add(origin,vec3(2-voxel*1.5,3+voxel,4+voxel*2));
        assert(fabs(RuntimeVolume3D_SampleDensityAtPosition(&original,cached)-
            RuntimeVolume3D_SampleDensityAtPosition(&prepared->scene.volume,moved))<1e-6);
        Ray3D ray={.origin=vec3_add(origin,vec3(2-voxel*1.5,1,4+voxel*2)),.direction=vec3(0,2,0)};
        double enter,exit;
        assert(RuntimeVolume3D_ClipRayToBounds(&prepared->scene.volume,&ray,0,1e6,&enter,&exit));
        assert(fabs(enter-1)<1e-8 && exit>enter);
        RuntimeNative3DPreparedFrame_Free(prepared);free(prepared);
    }
    choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty());
    assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));
    assert(SceneEditorAtmosphereRead(&value) && value.density_scale==2 && value.animated);
    SceneAtmosphereAttachment prior=value;
    atmosphere_control(editor,"generate");
    assert(SceneEditorAtmosphereRead(&value) && value.present && !value.animated);
    atmosphere_preview_pixels(editor,last);
    choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty());
    assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));
    assert(SceneEditorAtmosphereRead(&value) && value.present && !value.animated);
    assert(SceneEditorAtmosphereSet(&prior,SceneEditorDocumentRevision(),message,sizeof(message)));
    choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty());
    fprintf(stderr,"Atmosphere UI static generation PASS: actual button, preview, File Save and reopen\n");
    assert(SceneEditorAtmosphereRemove(SceneEditorDocumentRevision(),message,sizeof(message)));
    assert(SceneEditorAtmosphereRead(&value) && !value.present);
    assert(SceneEditorDocumentUndo(message,sizeof(message)));
    assert(SceneEditorAtmosphereRead(&value) && value.present);
    choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty());
    fprintf(stderr,"Atmosphere native PASS: controls, stale/missing refusal, undo/redo/restoration, sparse seeks, placement, actual File Save/reopen\n");
}
