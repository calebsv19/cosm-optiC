/* Light translation ownership. Legacy spatial data and intensity tracks
 * stay authored; the runtime adapter retains independent intensity semantics. */
#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_timeline.h"
#include "scene_editor_motion_paths_internal.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "import/runtime_scene_bridge.h"
#include <stdio.h>
#include <string.h>
static bool driver(const char *p) {
  return !strcmp(p, "light/path_progress");
}
static bool fail(char *message, size_t size, const char *why) {
  if (message && size) snprintf(message, size, "%s", why);
  return false;
}
/* UI setup for an existing camera/object-only timeline. Preserve all its
 * tracks; establish the same unambiguous legacy light source as first setup. */
bool SceneEditorMotionPathPrepareLight(char *message,size_t size) {
  static RuntimeSceneLightTimelineDocument legacy;
  if(RuntimeSceneLightTimelineGetLast(&legacy)) return true;
  static TimelineDocument doc;
  TimelineStatus status=SceneEditorDocumentGetTimeline(&doc);
  if(status==TIMELINE_STATUS_TARGET_NOT_FOUND) {
    if(!SceneEditorTimelineActivate()) return fail(message,size,SceneEditorTimelineStatus());
    return RuntimeSceneLightTimelineGetLast(&legacy) || fail(message,size,"Set up an animated light route first.");
  }
  if(status!=TIMELINE_STATUS_OK) return fail(message,size,"Cannot read the existing timeline.");
  RuntimeSceneBridge3DLightSeedState lights;runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
  if(!lights.valid || lights.light_count!=1 || !lights.lights[0].id[0] || sceneSettings.bezierPath.numPoints<2)
    return fail(message,size,"Choose an unambiguous animated light route before attaching.");
  char target[TIMELINE_ID_CAPACITY],id[TIMELINE_ID_CAPACITY];
  snprintf(target,sizeof(target),"light/%s",lights.lights[0].id);
  unsigned serial=1;bool used;
  do {
    snprintf(id,sizeof(id),"light-progress-%u",serial++);used=false;
    for(size_t i=0;i<doc.track_count;++i) {
      if(!strcmp(doc.tracks[i].track_id,id)) used=true;
      if(!strcmp(doc.tracks[i].target_id,target) && !strcmp(doc.tracks[i].property_id,"light/path_progress"))
        return fail(message,size,"Existing light progress has no spatial source; repair that source first.");
    }
  } while(used);
  TimelineTrack progress;int64_t end;
  if(TimelineRangeEndFrame(doc.range,&end)!=TIMELINE_STATUS_OK ||
      TimelineTrackInit(&progress,id,target,"light/path_progress",TIMELINE_VALUE_SCALAR)!=TIMELINE_STATUS_OK ||
      TimelineTrackSetUnit(&progress,TIMELINE_UNIT_UNITLESS)!=TIMELINE_STATUS_OK ||
      TimelineTrackAddKey(&progress,doc.range.start_frame,TimelineValueScalar(0),TIMELINE_INTERPOLATION_LINEAR)!=TIMELINE_STATUS_OK ||
      TimelineTrackAddKey(&progress,end,TimelineValueScalar(1),TIMELINE_INTERPOLATION_STEP)!=TIMELINE_STATUS_OK)
    return fail(message,size,"Cannot prepare light progress.");
  memset(&legacy,0,sizeof(legacy));legacy.valid=true;legacy.progress_track_index=0;
  legacy.spatial_path=sceneSettings.bezierPath;legacy.spatial_path_3d=sceneSettings.bezierPath3D;
  if(TimelineDocumentAddTrack(&doc,&progress)!=TIMELINE_STATUS_OK) return fail(message,size,"Timeline track capacity reached.");
  if(TimelineDocumentInit(&legacy.timeline,doc.rate,doc.range)!=TIMELINE_STATUS_OK ||
      TimelineDocumentAddTrack(&legacy.timeline,&progress)!=TIMELINE_STATUS_OK)
    return fail(message,size,"Cannot prepare the legacy light source.");
  if(!SceneEditorDocumentSetTimelineWithLight(&doc,&legacy,SceneEditorDocumentRevision(),message,size))
    return fail(message,size,"Cannot retain light setup; the scene was left unchanged.");
  return true;
}
bool SceneEditorMotionPathBindLight(const char *path_id, bool attach,
    unsigned long long revision, char *message, size_t size) {
  static RuntimeSceneLightTimelineDocument spatial;
  if (!RuntimeSceneLightTimelineGetLast(&spatial) || spatial.progress_track_index >= spatial.timeline.track_count)
    return fail(message, size, "Activate the existing light timeline before attaching.");
  const char *target = spatial.timeline.tracks[spatial.progress_track_index].target_id;
  MotionPaths paths;
  static TimelineDocument doc;
  if (revision != SceneEditorDocumentRevision() || !SceneEditorMotionPathsRead(&paths))
    return fail(message, size, "Scene changed; retry light attachment.");
  TimelineStatus status = SceneEditorDocumentGetTimeline(&doc);
  if (status != TIMELINE_STATUS_OK)
    return fail(message, size, "Invalid scene timeline.");
  size_t index = 0;
  for (; index < paths.binding_count; ++index)
    if (!strcmp(paths.bindings[index].target_id, target)) break;
  if (index == paths.binding_count) {
    if (!attach || index == MOTION_BINDING_CAPACITY)
      return fail(message, size, "No light binding or binding capacity reached.");
    memset(&paths.bindings[paths.binding_count++], 0, sizeof(paths.bindings[0]));
    snprintf(paths.bindings[index].target_id, TIMELINE_ID_CAPACITY, "%s", target);
  }
  MotionPathBinding *binding = &paths.bindings[index];
  if (attach) {
    bool found = false;
    for (size_t i = 0; i < paths.count; ++i)
      if (!strcmp(paths.paths[i].id, path_id)) found = true;
    if (!found) return fail(message, size, "Choose a route first.");
    snprintf(binding->path_id, sizeof(binding->path_id), "%s", path_id);
    if (!binding->enabled) {
      binding->restore_known = true;
      binding->restore_count = 0;
      for (size_t i = 0; i < doc.track_count; ++i) {
        const TimelineTrack *t = &doc.tracks[i];
        if (!strcmp(t->target_id, target) && t->enabled && driver(t->property_id)) {
          if (binding->restore_count == 3) return fail(message, size, "Conflicting light sources.");
          snprintf(binding->restore_xyz_tracks[binding->restore_count++], TIMELINE_ID_CAPACITY, "%s", t->track_id);
        }
      }
    }
  } else if (!binding->enabled || !binding->restore_known) {
    return fail(message, size, "No saved light source to restore.");
  }
  /* References are checked even on rebind: detach needs the predecessor. */
  for (size_t j = 0; j < binding->restore_count; ++j) {
    bool found = false;
    for (size_t i = 0; i < doc.track_count; ++i)
      if (!strcmp(doc.tracks[i].target_id, target) && driver(doc.tracks[i].property_id) &&
          !strcmp(doc.tracks[i].track_id, binding->restore_xyz_tracks[j])) found = true;
    if (!found) return fail(message, size, "Prior light source is missing.");
  }
  size_t progress = SIZE_MAX;
  for (size_t i = 0; i < doc.track_count; ++i) {
    TimelineTrack *t = &doc.tracks[i];
    if (strcmp(t->target_id, target)) continue;
    if (driver(t->property_id)) {
      t->enabled = false;
      for (size_t j = 0; !attach && j < binding->restore_count; ++j)
        if (!strcmp(t->track_id, binding->restore_xyz_tracks[j])) t->enabled = true;
    }
    if (!strcmp(t->property_id, MOTION_LIGHT_PROGRESS_PROPERTY)) {
      progress = i;
      t->enabled = attach;
    }
  }
  if (attach && progress == SIZE_MAX) {
    TimelineTrack track;
    char id[64]; unsigned serial = 1; bool used;
    do {
      snprintf(id, sizeof(id), "light-route-%u", serial++); used = false;
      for (size_t i = 0; i < doc.track_count; ++i)
        if (!strcmp(doc.tracks[i].track_id, id)) used = true;
    } while (used);
    TimelineTrackInit(&track, id, target, MOTION_LIGHT_PROGRESS_PROPERTY, TIMELINE_VALUE_SCALAR);
    TimelineTrackSetUnit(&track, TIMELINE_UNIT_UNITLESS);
    TimelineTrackAddKey(&track, doc.range.start_frame, TimelineValueScalar(0), TIMELINE_INTERPOLATION_LINEAR);
    if (doc.range.frame_count > 1)
      TimelineTrackAddKey(&track, doc.range.start_frame + doc.range.frame_count - 1,
          TimelineValueScalar(1), TIMELINE_INTERPOLATION_LINEAR);
    progress = doc.track_count;
    if (TimelineDocumentAddTrack(&doc, &track) != TIMELINE_STATUS_OK)
      return fail(message, size, "Timeline channel capacity reached.");
  }
  if (binding->restore_count != 1)
    return fail(message, size, "Light attachment requires one prior position source.");
  binding->enabled = attach;
  if (!SceneEditorMotionPathsCommit(&paths, &doc, revision, message, size)) return false;
  SceneEditorTimelinePause();
  if (attach) SceneEditorTimelineSelectTrack(progress);
  snprintf(message, size, "%s", attach ? "Light on route; intensity retained." : "Prior light position source restored.");
  return true;
}
