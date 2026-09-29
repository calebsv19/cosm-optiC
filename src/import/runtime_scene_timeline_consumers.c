#include "runtime_scene_timeline_consumers.h"
#include "motion/scene_motion_paths.h"
#include "import/runtime_scene_object_timeline.h"
#include "import/runtime_scene_light_timeline_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool refuse(const TimelineTrack* track, const char* reason,
                   char* diagnostics, size_t size) {
    if (diagnostics && size) snprintf(diagnostics,size,
        "scene_timeline consumer unavailable: %s %s: %s",
        track?track->target_id:"light",track?track->property_id:"path_progress",reason);
    return false;
}

bool RuntimeSceneTimelineValidateConsumers(json_object* authoring, double world_scale,
    const TimelineDocument* document, char* diagnostics, size_t size) {
    RuntimeSceneLightTimelineDocument* spatial=malloc(sizeof(*spatial));
    if (!spatial) return refuse(NULL,"allocation failed",diagnostics,size);
    TimelineStatus status=RuntimeSceneLightTimelineParseAuthoring(authoring,world_scale,spatial,NULL,0);
    bool has_spatial=status==TIMELINE_STATUS_OK;
    char light_target[TIMELINE_ID_CAPACITY]={0};
    if (has_spatial) snprintf(light_target,sizeof(light_target),"%s",
        spatial->timeline.tracks[spatial->progress_track_index].target_id);
    free(spatial);
    bool has_progress=false;
    for (size_t i=0;i<document->track_count;++i) {
        const TimelineTrack* track=&document->tracks[i];
        if (!track->enabled) continue;
        if (!strncmp(track->property_id,"camera/",7) || RuntimeObjectTimelineAxis(track->property_id)>=0 || !strcmp(track->property_id,MOTION_PROGRESS_PROPERTY)) continue;
        if (!strcmp(track->property_id,"light/path_progress") ||
            !strcmp(track->property_id,"light/intensity")) {
            if (!has_spatial || strcmp(light_target,track->target_id))
                return refuse(track,"no matching animated light spatial binding",diagnostics,size);
            if (!strcmp(track->property_id,"light/path_progress")) has_progress=true;
        } else {
            return refuse(track,"property has no scene runtime adapter; keep track disabled",diagnostics,size);
        }
    }
    if (has_spatial && !has_progress)
        return refuse(NULL,"animated light requires an enabled path-progress track",diagnostics,size);
    return true;
}
