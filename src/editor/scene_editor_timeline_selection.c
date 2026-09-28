#include "editor/scene_editor_timeline_selection.h"
#include "scene_editor_timeline_commands.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_document.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
static SceneEditorTimelineKeySelection selection;
static struct {TimelineKeyframe keys[TIMELINE_TRACK_KEY_CAPACITY];size_t count;char property[TIMELINE_ID_CAPACITY];TimelineUnit unit;} clipboard;
static char feedback[256];
static bool fail(const char* message) {snprintf(feedback,sizeof(feedback),"%s",message);return false;}
const char* SceneEditorTimelineSelectionStatus(void) {return feedback;}
void SceneEditorTimelineClearKeys(void) {memset(&selection,0,sizeof(selection));feedback[0]=0;}
void SceneEditorTimelineSelectionReset(void) {SceneEditorTimelineClearKeys();memset(&clipboard,0,sizeof(clipboard));}
static const TimelineTrack* current(TimelineRange* range) {
    size_t index;const TimelineDocument* doc=SceneEditorTimelineDocumentView(&index);
    if(!doc || index>=doc->track_count) {SceneEditorTimelineClearKeys();return NULL;}
    const TimelineTrack* track=&doc->tracks[index];
    if(selection.count && (selection.revision!=SceneEditorDocumentRevision() || strcmp(selection.track_id,track->track_id))) {
        /* Undo/redo or an external document edit invalidates frame identities.
         * Clear conservatively instead of silently selecting replacement keys. */
        memset(&selection,0,sizeof(selection));
    }
    if(range) *range=doc->range;
    return track;
}
static bool contains(int64_t frame) {
    for(size_t i=0;i<selection.count;++i) if(selection.frames[i]==frame) return true;
    return false;
}
bool SceneEditorTimelineSelectionRead(SceneEditorTimelineKeySelection* out) {
    if(!out || !current(NULL)) return false;
    *out=selection;return selection.count>0;
}
bool SceneEditorTimelineKeySelected(const char* id,int64_t frame) {
    return current(NULL) && id && !strcmp(selection.track_id,id) && contains(frame);
}
bool SceneEditorTimelineSelectKey(int64_t frame,bool toggle) {
    const TimelineTrack* t=current(NULL);if(!t) return false;
    size_t k=SIZE_MAX;for(size_t i=0;i<t->key_count;++i) if(t->keys[i].frame==frame) k=i;
    if(k==SIZE_MAX) return fail("No key at that frame. Use Add key at the playhead.");
    if(toggle && contains(frame)) {
        for(size_t i=0;i<selection.count;++i) if(selection.frames[i]==frame) {memmove(&selection.frames[i],&selection.frames[i+1],(--selection.count-i)*sizeof(int64_t));break;}
        if(selection.count) for(size_t i=0;i<t->key_count;++i) if(t->keys[i].frame==selection.frames[selection.count-1]) selection.primary=t->keys[i];
    } else {
        if(!toggle) selection.count=0;
        if(!contains(frame)) selection.frames[selection.count++]=frame;
        selection.primary=t->keys[k];
    }
    snprintf(selection.track_id,sizeof(selection.track_id),"%s",t->track_id);
    selection.revision=SceneEditorDocumentRevision();feedback[0]=0;return true;
}
bool SceneEditorTimelineSelectAllKeys(void) {
    const TimelineTrack* t=current(NULL);if(!t) return false;
    selection.count=0;
    for(size_t i=0;i<t->key_count;++i) selection.frames[selection.count++]=t->keys[i].frame;
    selection.primary=t->keys[0];selection.revision=SceneEditorDocumentRevision();
    snprintf(selection.track_id,sizeof(selection.track_id),"%s",t->track_id);feedback[0]=0;return true;
}
bool SceneEditorTimelineNavigateKey(int direction) {
    const TimelineTrack* t=current(NULL);TimelineSample sample;
    if(!t || !SceneEditorTimelineCurrentSample(&sample)) return false;
    int64_t origin=selection.count?selection.primary.frame:sample.absolute_frame;
    size_t found=SIZE_MAX;
    if(direction>0) {for(size_t k=0;k<t->key_count;++k) if(t->keys[k].frame>origin) {found=k;break;}}
    else {for(size_t k=t->key_count;k>0;--k) if(t->keys[k-1].frame<origin) {found=k-1;break;}}
    if(found==SIZE_MAX) return fail(direction>0?"Already at the last key.":"Already at the first key.");
    int64_t frame=t->keys[found].frame;SceneEditorTimelinePause();
    return SceneEditorTimelineSelectKey(frame,false) && SceneEditorTimelineSeek(frame);
}
static int compare_keys(const void* a,const void* b) {int64_t x=((const TimelineKeyframe*)a)->frame,y=((const TimelineKeyframe*)b)->frame;return (x>y)-(x<y);}
static bool add_frame(int64_t a,int64_t b,int64_t* out) {
    if((b>0 && a>INT64_MAX-b) || (b<0 && a<INT64_MIN-b)) return false;
    *out=a+b;return true;
}
static bool delta_frame(int64_t to,int64_t from,int64_t* out) {
    if((from<0 && to>INT64_MAX+from) || (from>0 && to<INT64_MIN+from)) return false;
    *out=to-from;return true;
}
static bool commit(TimelineTrack* t,TimelineRange range,const int64_t* frames,size_t count,int64_t primary) {
    int64_t end;if(TimelineRangeEndFrame(range,&end)!=TIMELINE_STATUS_OK) return false;
    if(!t->key_count) return fail("Keep at least one key in the channel.");
    qsort(t->keys,t->key_count,sizeof(t->keys[0]),compare_keys);
    for(size_t i=0;i<t->key_count;++i) {
        if(t->keys[i].frame<range.start_frame || t->keys[i].frame>end) return fail("Edit rejected: keys must stay inside the animation range.");
        if(i && t->keys[i].frame==t->keys[i-1].frame) return fail("Edit rejected: a key already occupies that frame. Nothing changed.");
    }
    if(!SceneEditorTimelineCommitTrack(t,SceneEditorDocumentRevision())) return fail(SceneEditorTimelineStatus());
    /* Re-resolve after commit; the document owner may fit temporal handles. */
    memset(&selection,0,sizeof(selection));const TimelineTrack* saved=current(NULL);
    if(!saved) return false;
    snprintf(selection.track_id,sizeof(selection.track_id),"%s",saved->track_id);
    selection.count=count;selection.revision=SceneEditorDocumentRevision();
    if(count) memcpy(selection.frames,frames,count*sizeof(int64_t));
    for(size_t i=0;i<saved->key_count;++i) if(saved->keys[i].frame==primary) selection.primary=saved->keys[i];
    snprintf(feedback,sizeof(feedback),"Applied. Undo restores the whole edit.");return true;
}
/* Batch edits stage one complete channel and commit once, never key-by-key. */
static bool edit(int operation,int64_t destination,double value,TimelineInterpolation mode,const double* handles) {
    TimelineRange range;const TimelineTrack* source=current(&range);
    if(!source || !selection.count) return fail("Select a key first. Shift-click extends the selection.");
    TimelineTrack candidate=*source;int64_t frames[TIMELINE_TRACK_KEY_CAPACITY];size_t count=0;
    int64_t delta=0,primary=selection.primary.frame;
    if((operation==1 || operation==4) && !delta_frame(destination,primary,&delta)) return fail("Frame offset is too large.");
    for(size_t i=0;i<candidate.key_count;++i) {
        TimelineKeyframe* k=&candidate.keys[i];if(!contains(k->frame)) continue;
        if(operation==1 || operation==4) {if(!add_frame(k->frame,delta,&k->frame)) return fail("Frame offset is too large.");}
        if(operation==2 || operation==4) k->value=TimelineValueScalar(value);
        if(operation==3) {
            k->interpolation_to_next=mode;
            if(mode==TIMELINE_INTERPOLATION_CUBIC_BEZIER && i+1<candidate.key_count) {
                double span=(candidate.keys[i+1].frame-k->frame)/3.0;
                k->outgoing_frame_offset=span;k->outgoing_value_offset=0;
                candidate.keys[i+1].incoming_frame_offset=-span;candidate.keys[i+1].incoming_value_offset=0;
            }
        }
        if(operation==5) {
            if(selection.count!=1) return fail("Select one key to edit its curve handles.");
            k->incoming_frame_offset=handles[0];k->incoming_value_offset=handles[1];
            k->outgoing_frame_offset=handles[2];k->outgoing_value_offset=handles[3];
        }
        frames[count++]=k->frame;
    }
    if(operation==0) {
        size_t kept=0;for(size_t i=0;i<candidate.key_count;++i) if(!contains(candidate.keys[i].frame)) candidate.keys[kept++]=candidate.keys[i];
        candidate.key_count=kept;count=0;
    }
    if(operation==1 || operation==4) primary=destination;
    return commit(&candidate,range,frames,count,primary);
}
bool SceneEditorTimelineMoveSelectedKeys(int64_t frame) {return edit(1,frame,0,0,NULL);}
bool SceneEditorTimelineSetSelectedValue(double value) {return isfinite(value) && edit(2,0,value,0,NULL);}
bool SceneEditorTimelineMoveSelectedValue(int64_t frame,double value) {
    SceneEditorTimelineKeySelection s;if(!SceneEditorTimelineSelectionRead(&s) || s.count!=1) return fail("Select one curve key to edit time and value.");
    return isfinite(value) && edit(4,frame,value,0,NULL);
}
bool SceneEditorTimelineDeleteSelectedKeys(void) {return edit(0,0,0,0,NULL);}
bool SceneEditorTimelineSelectedInterpolation(TimelineInterpolation mode) {return edit(3,0,0,mode,NULL);}
bool SceneEditorTimelineSelectedHandles(double fi,double vi,double fo,double vo) {double h[]={fi,vi,fo,vo};return edit(5,0,0,0,h);}
bool SceneEditorTimelineCopyKeys(void) {
    const TimelineTrack* t=current(NULL);if(!t || !selection.count) return fail("Select keys to copy.");
    clipboard.count=0;for(size_t i=0;i<t->key_count;++i) if(contains(t->keys[i].frame)) clipboard.keys[clipboard.count++]=t->keys[i];
    snprintf(clipboard.property,sizeof(clipboard.property),"%s",t->property_id);clipboard.unit=t->unit;
    snprintf(feedback,sizeof(feedback),"Copied %zu keys. Move the playhead, then Paste.",clipboard.count);return true;
}
bool SceneEditorTimelineCanPasteKeys(void) {
    const TimelineTrack* t=current(NULL);return t && clipboard.count && t->unit==clipboard.unit && !strcmp(t->property_id,clipboard.property);
}
bool SceneEditorTimelinePasteKeys(void) {
    TimelineRange range;const TimelineTrack* source=current(&range);TimelineSample sample;
    if(!source || !SceneEditorTimelineCanPasteKeys() || !SceneEditorTimelineCurrentSample(&sample)) return fail("Copy keys from a matching channel first.");
    if(source->key_count+clipboard.count>TIMELINE_TRACK_KEY_CAPACITY) return fail("Channel key capacity exceeded. Nothing changed.");
    TimelineTrack t=*source;int64_t delta,frames[TIMELINE_TRACK_KEY_CAPACITY];
    if(!delta_frame(sample.absolute_frame,clipboard.keys[0].frame,&delta)) return fail("Frame offset is too large.");
    for(size_t i=0;i<clipboard.count;++i) {
        TimelineKeyframe k=clipboard.keys[i];if(!add_frame(k.frame,delta,&k.frame)) return fail("Frame offset is too large.");
        frames[i]=k.frame;t.keys[t.key_count++]=k;
    }
    return commit(&t,range,frames,clipboard.count,frames[0]);
}
bool SceneEditorTimelineDuplicateKeys(void) {return SceneEditorTimelineCopyKeys() && SceneEditorTimelinePasteKeys();}
