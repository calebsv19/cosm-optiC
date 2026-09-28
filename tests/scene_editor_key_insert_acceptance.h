#include "../src/editor/scene_editor_object_timeline_panel.h"

static void insert_position(SceneEditor* editor,const char* frame,double z) {
    char value[64];snprintf(value,sizeof(value),"%.17g",z);
    authoring_control(editor,"frame");authoring_text(editor,frame);
    authoring_control(editor,"position_z");authoring_text(editor,value);
    assert(SceneEditorObjectTimelinePanelPending());
    unsigned long long revision=SceneEditorDocumentRevision();
    authoring_control(editor,"set_position_key");
    assert(SceneEditorDocumentRevision()==revision+1);
    assert(!SceneEditorObjectTimelinePanelPending());
}
static void key_insert_reopen_acceptance(SceneEditor* editor) {
    static TimelineDocument doc;assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    size_t track=doc.track_count;
    for(size_t i=0;i<doc.track_count;++i) if(RuntimeObjectTimelineAxis(doc.tracks[i].property_id)==2 &&
        doc.tracks[i].key_count==4 && doc.tracks[i].keys[2].frame==80 && doc.tracks[i].keys[3].frame==81) {track=i;break;}
    assert(track<doc.track_count);choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
    assert(SceneEditorTimelineSelectTrack(track));assert(SceneEditorTimelineSeek(80));double xyz[3];
    assert(SceneEditorObjectTimelinePosition(doc.tracks[track].target_id+7,xyz));
    assert(fabs(xyz[2]-doc.tracks[track].keys[2].value.as.scalar*SceneEditorDocumentWorldScale())<1e-8);
    assert(!SceneEditorDocumentIsDirty());capture(editor,"reopened_frame_80.ppm");
    fprintf(stderr,"Frame-80 insertion fresh-process reopen PASS.\n");
}
static void key_insert_acceptance(SceneEditor* editor,const char* path) {
    SceneEditorDocumentObjectInfo object;bool found=false;
    for(int i=0;i<SceneEditorDocumentObjectCount();++i)
        if(SceneEditorDocumentObjectAt(i,&object) && !strcmp(object.type,"mesh_asset_instance") && object.runtime_index>=0) {found=true;break;}
    assert(found);char diagnostics[256];SceneEditorObjectReadback readback;
    assert(SceneEditorObjectExecute(SCENE_OBJECT_SELECT,object.id,NULL,false,SceneEditorDocumentRevision(),&readback,diagnostics,sizeof(diagnostics)));
    SceneEditorDocumentTransform base;
    assert(SceneEditorDocumentGetTransformForSceneIndex(object.runtime_index,&base,diagnostics,sizeof(diagnostics)));
    choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);authoring_control(editor,"animate_object");
    insert_position(editor,"81",base.position[2]);
    insert_position(editor,"80",base.position[2]+10); /* Reported failure: earlier than the last key. */
    insert_position(editor,"40",base.position[2]+5);
    insert_position(editor,"80",base.position[2]+12); /* Update, not duplicate. */
    static TimelineDocument doc,saved;
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    unsigned axes=0;size_t z_track=0;
    for(size_t i=0;i<doc.track_count;++i) {
        TimelineTrack* t=&doc.tracks[i];int axis=RuntimeObjectTimelineAxis(t->property_id);
        if(axis<0 || strcmp(t->target_id+7,object.id)) continue;
        assert(t->key_count==4 && t->keys[0].frame==0 && t->keys[1].frame==40 &&
            t->keys[2].frame==80 && t->keys[3].frame==81);
        assert(fabs(t->keys[2].value.as.scalar-(base.position[axis]+(axis==2?12:0)))<1e-8);
        if(axis==2) z_track=i;
        axes|=1u<<axis;
    }
    assert(axes==7);
    TimelineTrack selected;TimelineRate rate;TimelineRange range;TimelineSample sample;
    assert(SceneEditorTimelineSelectedTrack(&selected,&rate,&range,&sample));
    SceneEditorTimelineKeySelection selection;
    assert(SceneEditorTimelineSelectionRead(&selection) && selection.count==1);
    const int frames[]={0,20,40,60,80,81};const double dz[]={0,2.5,5,8.5,12,0};
    for(size_t i=0;i<6;++i) {
        assert(SceneEditorTimelineSeek(frames[i]));double xyz[3];
        assert(SceneEditorObjectTimelinePosition(object.id,xyz));
        assert(fabs(xyz[2]-(base.position[2]+dz[i])*SceneEditorDocumentWorldScale())<1e-7);
    }
    choose_menu(editor,1,0); /* Undo the update at 80. */
    assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    assert(fabs(doc.tracks[z_track].keys[2].value.as.scalar-base.position[2]-10)<1e-8);
    choose_menu(editor,1,1);
    authoring_control(editor,"save_animation");
    assert(!SceneEditorDocumentIsDirty());
    assert(SceneEditorDocumentOpen(path,diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentGetTimeline(&saved)==TIMELINE_STATUS_OK);
    assert(saved.tracks[z_track].key_count==4 && saved.tracks[z_track].keys[2].frame==80);
    assert(SceneEditorTimelineSelectTrack(z_track));assert(SceneEditorTimelineSeek(80));
    SceneEditorSessionRuntimeRender(editor);capture(editor,"inserted_frame_80.ppm");
    /* Selected buttons must visibly react to hover, as must unselected ones. */
    authoring_control(editor,"position_z");authoring_text(editor,"110");
    SDL_Rect apply;assert(SceneEditorRenderAuthoringControl("set_position_key",&apply));
    SDL_WarpMouseInWindow(editor->window,1,1);SDL_PumpEvents();
    capture(editor,"apply_ready.ppm");
    SDL_WarpMouseInWindow(editor->window,apply.x+apply.w/2,apply.y+apply.h/2);SDL_PumpEvents();
    capture(editor,"apply_hover.ppm");
    key(editor,SDLK_ESCAPE);
    /* Fill only Z so X/Y would insert first: refusal must remain atomic. */
    doc=saved;TimelineTrack* full=&doc.tracks[z_track];full->key_count=0;
    for(unsigned i=0;i<TIMELINE_TRACK_KEY_CAPACITY;++i)
        assert(TimelineTrackAddKey(full,i,TimelineValueScalar(base.position[2]),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);
    assert(SceneEditorDocumentSetTimeline(&doc,SceneEditorDocumentRevision(),diagnostics,sizeof(diagnostics)));
    assert(SceneEditorTimelineSeek(160));SceneEditorSessionRuntimeRender(editor);
    authoring_control(editor,"position_z");authoring_text(editor,"110");
    unsigned long long revision=SceneEditorDocumentRevision();
    authoring_control(editor,"set_position_key");
    assert(SceneEditorDocumentRevision()==revision && SceneEditorObjectTimelinePanelPending());
    static TimelineDocument after;assert(SceneEditorDocumentGetTimeline(&after)==TIMELINE_STATUS_OK);
    assert(!memcmp(&doc,&after,sizeof(doc)));
    capture(editor,"real_capacity_refusal.ppm");key(editor,SDLK_ESCAPE);
    assert(SceneEditorDocumentSetTimeline(&saved,SceneEditorDocumentRevision(),diagnostics,sizeof(diagnostics)));
    assert(SceneEditorDocumentSave(diagnostics,sizeof(diagnostics)));
    fprintf(stderr,"Object key insertion PASS: 81 then 80, interior 40, update 80, evaluation, undo/redo, save/reopen, atomic capacity refusal.\n");
}
