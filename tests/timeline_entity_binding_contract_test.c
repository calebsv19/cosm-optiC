#include "animation/timeline_entity_binding.h"
#include "animation/timeline_camera_channels.h"
#include "app/scene_timeline_session.h"
#include "import/scene_timeline_document_io.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static TimelineTrack scalar(const char* id, const char* target, const char* property,
                            TimelineUnit unit, double a, double b) {
    TimelineTrack track;
    assert(TimelineTrackInit(&track, id, target, property, TIMELINE_VALUE_SCALAR) == TIMELINE_STATUS_OK);
    assert(TimelineTrackSetUnit(&track, unit) == TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(&track, 0, TimelineValueScalar(a), TIMELINE_INTERPOLATION_LINEAR) == TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(&track, 20, TimelineValueScalar(b), TIMELINE_INTERPOLATION_STEP) == TIMELINE_STATUS_OK);
    return track;
}

int main(void) {
    static TimelineDocument document;
    TimelineEntityBindings bindings = {0};
    TimelinePropertyRegistry registry;
    assert(TimelinePropertyRegistryInitFoundationDefaults(&registry) == TIMELINE_STATUS_OK);
    assert(TimelineDocumentInit(&document, (TimelineRate){24,1}, (TimelineRange){0,21}) == TIMELINE_STATUS_OK);
    assert(TimelineEntityBindingsAdd(&bindings, "main-camera", "camera/main", TIMELINE_PROPERTY_TARGET_CAMERA, true, false) == TIMELINE_STATUS_OK);
    assert(TimelineEntityBindingsAdd(&bindings, "lamp", "light/lamp", TIMELINE_PROPERTY_TARGET_LIGHT, true, false) == TIMELINE_STATUS_OK);
    assert(TimelineEntityBindingsAdd(&bindings, "lamp", "object/lamp", TIMELINE_PROPERTY_TARGET_OBJECT, true, false) == TIMELINE_STATUS_OK);
    TimelineEntityBindings before = bindings;
    assert(TimelineEntityBindingsAdd(&bindings, "wrong", "light/lamp", TIMELINE_PROPERTY_TARGET_LIGHT, true, false) == TIMELINE_STATUS_DUPLICATE_ID);
    assert(TimelineEntityBindingsAdd(&bindings, "lamp", "camera/lamp", TIMELINE_PROPERTY_TARGET_CAMERA, false, false) == TIMELINE_STATUS_OWNERSHIP_MISMATCH);
    assert(memcmp(&bindings, &before, sizeof(before)) == 0);
    char too_long[TIMELINE_ENTITY_ID_CAPACITY + 1];
    memset(too_long, 'x', sizeof(too_long)); too_long[sizeof(too_long)-1] = 0;
    assert(TimelineEntityBindingsAdd(&bindings, too_long, "object/long", TIMELINE_PROPERTY_TARGET_OBJECT, true, false) == TIMELINE_STATUS_INVALID_ID);

    TimelineTrack camera = scalar("camera-motion", "camera/main", "camera/path_progress", TIMELINE_UNIT_UNITLESS, 0, 1);
    TimelineTrack light = scalar("light-motion", "light/lamp", "light/path_progress", TIMELINE_UNIT_UNITLESS, 0, 1);
    TimelineTrack intensity = scalar("brightness", "light/lamp", "light/intensity", TIMELINE_UNIT_RELATIVE_INTENSITY, 1, 3);
    assert(TimelineDocumentAddTrack(&document, &camera) == TIMELINE_STATUS_OK);
    assert(TimelineDocumentAddTrack(&document, &light) == TIMELINE_STATUS_OK);
    assert(TimelineDocumentAddTrack(&document, &intensity) == TIMELINE_STATUS_OK);
    assert(TimelineEntityBindingsValidateDocument(&bindings, &registry, &document) == TIMELINE_STATUS_OK);
    TimelineFrameSnapshot camera_snapshot;
    TimelineEvaluationContext camera_context;
    TimelineCameraChannels channels;
    assert(TimelineEvaluationContextBuild(document.rate, document.range,
        (TimelineSample){5,1,2}, &camera_context) == TIMELINE_STATUS_OK);
    assert(TimelineFrameSnapshotBuild(&registry, &document, &camera_context,
        &camera_snapshot) == TIMELINE_STATUS_OK);
    assert(TimelineCameraChannelsResolve(&camera_snapshot, "camera/main", &channels) == TIMELINE_STATUS_OK);
    assert(channels.has_progress && fabs(channels.progress - .275) < 1e-12);
    assert(!channels.has_position && !channels.has_fov);
    assert(TimelineCameraChannelsResolve(&camera_snapshot, "camera/other", &channels) == TIMELINE_STATUS_OK);
    assert(!channels.has_progress);
    TimelineCameraChannels unchanged = channels;
    camera_snapshot.properties[0].unit = TIMELINE_UNIT_DEGREES;
    assert(TimelineCameraChannelsResolve(&camera_snapshot, "camera/main", &channels) == TIMELINE_STATUS_INVALID_SNAPSHOT);
    assert(memcmp(&channels, &unchanged, sizeof(channels)) == 0);
    TimelineTrack camera_position;
    assert(TimelineTrackInit(&camera_position, "camera-pos", "camera/main", "camera/position", TIMELINE_VALUE_VEC3) == TIMELINE_STATUS_OK);
    assert(TimelineTrackSetUnit(&camera_position, TIMELINE_UNIT_WORLD_DISTANCE) == TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(&camera_position, 0, TimelineValueVec3(1,2,3), TIMELINE_INTERPOLATION_STEP) == TIMELINE_STATUS_OK);
    assert(TimelineDocumentAddTrack(&document, &camera_position) == TIMELINE_STATUS_OK);
    assert(TimelineFrameSnapshotBuild(&registry, &document, &camera_context, &camera_snapshot) == TIMELINE_STATUS_DUPLICATE_OWNERSHIP);
    assert(SceneTimelineDocumentToJson(&document)==NULL);
    assert(memcmp(&channels, &unchanged, sizeof(channels)) == 0);
    document.tracks[document.track_count-1].enabled=false;
    assert(TimelinePropertyRegistryValidateDocument(&registry,&document)==TIMELINE_STATUS_OK);
    json_object* disabled_saved=SceneTimelineDocumentToJson(&document);
    assert(disabled_saved);
    /* Imported bytes enabling the alternative cannot bypass ownership. */
    json_object *saved_tracks=NULL,*saved_position=NULL;
    assert(json_object_object_get_ex(disabled_saved,"tracks",&saved_tracks));
    saved_position=json_object_array_get_idx(saved_tracks,document.track_count-1);
    json_object_object_add(saved_position,"enabled",json_object_new_boolean(true));
    static TimelineDocument rejected_document;
    assert(SceneTimelineDocumentFromJson(disabled_saved,&rejected_document)==TIMELINE_STATUS_DUPLICATE_OWNERSHIP);
    json_object_put(disabled_saved);
    document.track_count--;
    /* Round-trip one document containing independent camera and light tracks. */
    static TimelineDocument reopened;
    static TimelineDocument persistence_fixture;
    persistence_fixture = document;
    persistence_fixture.tracks[0].keys[0].interpolation_to_next = TIMELINE_INTERPOLATION_CUBIC_BEZIER;
    assert(TimelineTrackSetScalarTemporalHandles(&persistence_fixture.tracks[0], 0, 0, 0, 5, .1) == TIMELINE_STATUS_OK);
    assert(TimelineTrackSetScalarTemporalHandles(&persistence_fixture.tracks[0], 1, -5, -.1, 0, 0) == TIMELINE_STATUS_OK);
    snprintf(camera_position.target_id, sizeof(camera_position.target_id), "camera/secondary");
    assert(TimelineDocumentAddTrack(&persistence_fixture, &camera_position) == TIMELINE_STATUS_OK);
    TimelineTrack lens = scalar("camera-lens", "camera/main", "camera/fov_y", TIMELINE_UNIT_DEGREES, 45, 70);
    assert(TimelineDocumentAddTrack(&persistence_fixture, &lens) == TIMELINE_STATUS_OK);
    json_object* saved = SceneTimelineDocumentToJson(&persistence_fixture);
    assert(saved);
    json_object* disk = json_tokener_parse(json_object_to_json_string_ext(saved, JSON_C_TO_STRING_PLAIN));
    assert(disk && SceneTimelineDocumentFromJson(disk, &reopened) == TIMELINE_STATUS_OK);
    json_object* saved_again = SceneTimelineDocumentToJson(&reopened);
    assert(saved_again && strcmp(json_object_to_json_string_ext(saved, JSON_C_TO_STRING_PLAIN),
        json_object_to_json_string_ext(saved_again, JSON_C_TO_STRING_PLAIN)) == 0);
    json_object_put(saved_again);
    TimelineFrameSnapshot original_frame, reopened_frame;
    assert(TimelineFrameSnapshotBuild(&registry, &persistence_fixture, &camera_context, &original_frame) == TIMELINE_STATUS_OK);
    assert(TimelineFrameSnapshotBuild(&registry, &reopened, &camera_context, &reopened_frame) == TIMELINE_STATUS_OK);
    assert(original_frame.property_count == reopened_frame.property_count);
    for (size_t i = 0; i < original_frame.property_count; ++i) {
        assert(strcmp(original_frame.properties[i].track.target_id, reopened_frame.properties[i].track.target_id) == 0);
        assert(memcmp(&original_frame.properties[i].track.value,
            &reopened_frame.properties[i].track.value, sizeof(TimelineValue)) == 0);
        assert(original_frame.properties[i].track.derivative_per_frame == reopened_frame.properties[i].track.derivative_per_frame);
    }

    json_object_object_add(disk, "version", json_object_new_int(99));
    static TimelineDocument unchanged_document;
    unchanged_document = reopened;
    assert(SceneTimelineDocumentFromJson(disk, &reopened) == TIMELINE_STATUS_INVALID_ARGUMENT);
    assert(memcmp(&reopened, &unchanged_document, sizeof(reopened)) == 0);
    json_object_put(disk);
    json_object_put(saved);
    SceneTimelineSession session;
    assert(SceneTimelineSessionInit(&session, document.rate, document.range, 7, 9) == TIMELINE_STATUS_OK);
    assert(SceneTimelineSessionSeek(&session, (TimelineSample){5,1,2}) == TIMELINE_STATUS_OK);
    assert(SceneTimelineSessionSelect(&session, &bindings, &registry, "camera/main", "camera/path_progress") == TIMELINE_STATUS_OK);
    assert(SceneTimelineSessionSetPlaying(&session, true) == TIMELINE_STATUS_OK);
    assert(SceneTimelineSessionSelect(&session, &bindings, &registry, "light/lamp", "light/intensity") == TIMELINE_STATUS_OK);
    assert(session.transport.playing && session.transport.sample.absolute_frame == 5 && session.transport.sample.subframe_numerator == 1);
    SceneTimelineSession session_before = session;
    assert(SceneTimelineSessionSelect(&session, &bindings, &registry, "camera/main", "light/intensity") == TIMELINE_STATUS_TARGET_KIND_MISMATCH);
    assert(memcmp(&session_before, &session, sizeof(session)) == 0);
    assert(SceneTimelineSessionBeginEdit(&session, 7, 8) == TIMELINE_STATUS_OWNERSHIP_MISMATCH);
    assert(SceneTimelineSessionBeginEdit(&session, 7, 9) == TIMELINE_STATUS_OK);
    assert(!session.transport.playing);
    assert(SceneTimelineSessionSeek(&session, (TimelineSample){8,0,1}) == TIMELINE_STATUS_OWNERSHIP_MISMATCH);
    assert(SceneTimelineSessionSelect(&session, &bindings, &registry, "camera/main", "") == TIMELINE_STATUS_OWNERSHIP_MISMATCH);
    assert(SceneTimelineSessionReconcile(&session, &bindings, 8, 9) == TIMELINE_STATUS_OK);
    assert(!session.editing);
    assert(SceneTimelineSessionSelect(&session, &bindings, &registry, "camera/main", "camera/path_progress") == TIMELINE_STATUS_OK);
    TimelineEntityBindings replaced = bindings;
    snprintf(replaced.entries[0].entity_id, TIMELINE_ENTITY_ID_CAPACITY, "replacement-camera");
    assert(SceneTimelineSessionReconcile(&session, &replaced, 9, 9) == TIMELINE_STATUS_TARGET_NOT_FOUND);
    assert(session.selection_missing && strcmp(session.selected_entity, "main-camera") == 0);
    assert(SceneTimelineSessionBeginEdit(&session, 9, 9) == TIMELINE_STATUS_TARGET_NOT_FOUND);
    assert(SceneTimelineSessionReconcile(&session, &bindings, 10, 9) == TIMELINE_STATUS_OK);
    assert(SceneTimelineSessionSeek(&session, (TimelineSample){0,0,1}) == TIMELINE_STATUS_OK);
    assert(SceneTimelineSessionSetPlaying(&session, true) == TIMELINE_STATUS_OK);
    assert(SceneTimelineSessionAdvance(&session, .25) == TIMELINE_STATUS_OK);
    assert(session.transport.sample.absolute_frame == 6);
    session_before = session;
    assert(SceneTimelineSessionAdvance(&session, NAN) == TIMELINE_STATUS_INVALID_ARGUMENT);
    assert(memcmp(&session_before, &session, sizeof(session)) == 0);
    /* Target order is a view detail, not identity. */
    TimelineEntityBinding swap = bindings.entries[0];
    bindings.entries[0] = bindings.entries[2]; bindings.entries[2] = swap;
    assert(TimelineEntityBindingsValidateDocument(&bindings, &registry, &document) == TIMELINE_STATUS_OK);
    TimelineEvaluationContext context;
    TimelinePropertyEvaluationResult results[3]; size_t count = 0;
    assert(TimelineEvaluationContextBuild(document.rate, document.range, (TimelineSample){10,0,1}, &context) == TIMELINE_STATUS_OK);
    assert(TimelinePropertyRegistryEvaluateDocument(&registry, &document, &context, results, 3, &count) == TIMELINE_STATUS_OK);
    assert(count == 3 && fabs(results[0].track.value.as.scalar - .5) < 1e-12);
    assert(results[0].invalidation_domains == TIMELINE_INVALIDATION_CAMERA);
    assert(fabs(results[2].track.value.as.scalar - 2) < 1e-12);

    TimelineTrack position;
    assert(TimelineTrackInit(&position, "lamp-position", "object/lamp", "object/transform/position", TIMELINE_VALUE_VEC3) == TIMELINE_STATUS_OK);
    assert(TimelineTrackSetUnit(&position, TIMELINE_UNIT_WORLD_DISTANCE) == TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(&position, 0, TimelineValueVec3(1,2,3), TIMELINE_INTERPOLATION_STEP) == TIMELINE_STATUS_OK);
    assert(TimelineDocumentAddTrack(&document, &position) == TIMELINE_STATUS_OK);
    assert(TimelineEntityBindingsValidateDocument(&bindings, &registry, &document) == TIMELINE_STATUS_DUPLICATE_OWNERSHIP);
    document.tracks[3].enabled = false;
    assert(TimelineEntityBindingsValidateDocument(&bindings, &registry, &document) == TIMELINE_STATUS_OK);
    bindings.entries[1].simulation_owns_transform = true;
    assert(TimelineEntityBindingsValidateDocument(&bindings, &registry, &document) == TIMELINE_STATUS_OWNERSHIP_MISMATCH);
    bindings.entries[1].simulation_owns_transform = false;
    snprintf(document.tracks[0].target_id, TIMELINE_ID_CAPACITY, "camera/missing");
    assert(TimelineEntityBindingsValidateDocument(&bindings, &registry, &document) == TIMELINE_STATUS_TARGET_NOT_FOUND);

    TimelineTrack yaw = scalar("yaw", "camera/main", "camera/yaw", TIMELINE_UNIT_RADIANS, 0, 6.283185307179586);
    assert(TimelinePropertyRegistryValidateTrack(&registry, &yaw, &document.range) == TIMELINE_STATUS_OK);
    yaw.unit = TIMELINE_UNIT_DEGREES;
    assert(TimelinePropertyRegistryValidateTrack(&registry, &yaw, &document.range) == TIMELINE_STATUS_UNIT_MISMATCH);
    TimelineTrack fov = scalar("lens", "camera/main", "camera/fov_y", TIMELINE_UNIT_DEGREES, 45, 180);
    assert(TimelinePropertyRegistryValidateTrack(&registry, &fov, &document.range) == TIMELINE_STATUS_VALUE_OUT_OF_RANGE);
    TimelineTrack pitch = scalar("pitch", "camera/main", "camera/pitch", TIMELINE_UNIT_RADIANS, 0, 2);
    assert(TimelinePropertyRegistryValidateTrack(&registry, &pitch, &document.range) == TIMELINE_STATUS_VALUE_OUT_OF_RANGE);
    static TimelineDocument xyz;
    TimelineDocumentInit(&xyz,(TimelineRate){24,1},(TimelineRange){0,21});
    const char* axes[]={"object/transform/position_x","object/transform/position_y","object/transform/position_z"};
    for(int axis=0;axis<3;++axis) {
        TimelineTrack t=scalar(axes[axis],"object/lamp",axes[axis],TIMELINE_UNIT_WORLD_DISTANCE,0,10);
        assert(TimelineDocumentAddTrack(&xyz,&t)==TIMELINE_STATUS_OK);
    }
    assert(TimelineEntityBindingsValidateDocument(&before,&registry,&xyz)==TIMELINE_STATUS_OK);
    assert(TimelineDocumentAddTrack(&xyz,&light)==TIMELINE_STATUS_OK);
    assert(TimelineEntityBindingsValidateDocument(&before,&registry,&xyz)==TIMELINE_STATUS_DUPLICATE_OWNERSHIP);
    TimelineEntityBinding simulated={.editable=true,.simulation_owns_transform=true};
    assert(!TimelineEntityBindingCanAuthor(&simulated,axes[0]));
    puts("timeline entity/camera/object contracts: PASS");
    return 0;
}
