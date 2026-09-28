#include "editor/scene_editor_material_perf.h"
static void motion_interaction_acceptance(SceneEditor* editor,const char* path) {
    key_insert_acceptance(editor,path);
    SceneEditorMaterialPerfEnable(true,true);
    uint64_t hashes[3];const int frames[]={0,40,80};
    unsigned long long revision=SceneEditorDocumentRevision();
    for(int mode=SCENE_EDITOR_MESH_DISPLAY_SOLID;mode<=SCENE_EDITOR_MESH_DISPLAY_MATERIAL;++mode) {
        SceneEditorMeshPreviewModeSet(mode);
        for(int i=0;i<3;++i) {
            assert(SceneEditorTimelineSeek(frames[i]));SceneEditorMaterialPerfBeginSample();
            SceneEditorSessionRuntimeRender(editor);
            assert(SceneEditorMaterialPerfRead().rasterized);
            hashes[i]=SceneEditorMaterialPerfPixelHash();assert(hashes[i]);
            char name[64];snprintf(name,sizeof(name),"motion_%d_frame_%d.ppm",mode,frames[i]);capture(editor,name);
            SceneEditorMaterialPerfBeginSample();SceneEditorSessionRuntimeRender(editor);
            assert(!SceneEditorMaterialPerfRead().rasterized);
        }
        assert(hashes[0]!=hashes[1] && hashes[1]!=hashes[2]);
        for(int i=2;i>=0;--i) {
            assert(SceneEditorTimelineSeek(frames[i]));SceneEditorSessionRuntimeRender(editor);
            assert(SceneEditorMaterialPerfPixelHash()==hashes[i]);
        }
    }
    assert(SceneEditorDocumentRevision()==revision);
    SceneEditorMeshPreviewModeSet(SCENE_EDITOR_MESH_DISPLAY_SOLID);
    SceneEditorMaterialPerfEnable(true,false);assert(SceneEditorTimelineSeek(10));
    SceneEditorSessionRuntimeRender(editor);SceneEditorTimelineTogglePlaying();
    uint64_t previous=0;unsigned changed=0;
    for(int i=0;i<4;++i) {
        SDL_Delay(45);assert(SceneEditorTimelineAdvance());SceneEditorMaterialPerfBeginSample();
        SceneEditorSessionRuntimeRender(editor);uint64_t pixels=SceneEditorMaterialPerfPixelHash();
        if(previous && pixels!=previous) ++changed;previous=pixels;
        SceneEditorMaterialPerfSample timing=SceneEditorMaterialPerfRead();
        fprintf(stderr,"Playback frame: raster=%d interactive=%d size=%dx%d raster_ms=%.2f\n",timing.rasterized,timing.interactive,timing.width,timing.height,timing.ns[SCENE_MATERIAL_PERF_RASTER_SHADE]/1e6);
    }
    SceneEditorTimelinePause();assert(changed==3);
    SceneEditorMaterialPerfEnable(false,false);
    static TimelineDocument doc;assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    size_t z=SIZE_MAX;for(size_t i=0;i<doc.track_count;++i) if(RuntimeObjectTimelineAxis(doc.tracks[i].property_id)==2) {z=i;break;}
    assert(z!=SIZE_MAX);assert(SceneEditorTimelineSelectTrack(z));assert(SceneEditorTimelineSeek(10));
    SceneEditorSessionRuntimeRender(editor);SDL_Rect row;assert(SceneEditorTimelineTrackRect(z,&row));
    SDL_Event event={0};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_RIGHT;
    event.button.x=SceneEditorTimelineFrameX(60);event.button.y=row.y+row.h/2;
    SceneEditorSessionRuntimeHandleEvent(editor,&event);event.type=SDL_MOUSEBUTTONUP;SceneEditorSessionRuntimeHandleEvent(editor,&event);
    SceneEditorSessionRuntimeRender(editor);TimelineSample sample;SceneEditorTimelineKeySelection keys;
    assert(SceneEditorTimelineCurrentSample(&sample) && sample.absolute_frame==10);
    assert(SceneEditorDocumentRevision()==revision+1);
    assert(SceneEditorTimelineSelectionRead(&keys) && keys.primary.frame==60);
    double expected=(doc.tracks[z].keys[1].value.as.scalar+doc.tracks[z].keys[2].value.as.scalar)/2;
    assert(fabs(keys.primary.value.as.scalar-expected)<1e-8);
    revision=SceneEditorDocumentRevision();event.type=SDL_MOUSEBUTTONDOWN;SceneEditorSessionRuntimeHandleEvent(editor,&event);
    event.type=SDL_MOUSEBUTTONUP;SceneEditorSessionRuntimeHandleEvent(editor,&event);
    assert(SceneEditorDocumentRevision()==revision); /* Existing key is selected, never duplicated. */
    selection_click_key(editor,z,40,false);selection_click_key(editor,z,60,true);
    assert(SceneEditorTimelineSelectionRead(&keys) && keys.count==2);
    assert(SceneEditorTimelineCurrentSample(&sample) && sample.absolute_frame==10);
    assert(SceneEditorTimelineTrackRect(z,&row));SDL_WarpMouseInWindow(editor->window,SceneEditorTimelineFrameX(60),row.y+row.h/2);SDL_PumpEvents();
    capture(editor,"key_hover.ppm");
    SDL_WarpMouseInWindow(editor->window,SceneEditorTimelineFrameX(70),row.y+row.h/2);SDL_PumpEvents();capture(editor,"key_create_guide.ppm");
    choose_menu(editor,1,0);assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[z].key_count==4);
    choose_menu(editor,1,1);assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[z].key_count==5);
    char diagnostic[256];assert(SceneEditorDocumentSave(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentOpen(path,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[z].key_count==5);
    fprintf(stderr,"Motion/interaction PASS: surface pixel hashes change forward, restore backward, reuse identical poses; right-click insertion, duplicate guard, independent playhead, Shift selection, undo/redo, save/reopen.\n");
}
