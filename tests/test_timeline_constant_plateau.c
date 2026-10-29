#include "animation/timeline_property_registry.h"

#include <assert.h>
#include <stdio.h>

/* A bounded progress hold must stay exactly at its authored endpoint, including
 * scalar builds without fused floating-point operations used by Linux workers. */
static void check_hold(const char* track_id, const char* target,
                       const char* property, int first_frame) {
    TimelineTrack track;
    TimelinePropertyRegistry registry;
    const TimelinePropertyDescriptor* descriptor = NULL;
    TimelineTrackInit(&track, track_id, target, property, TIMELINE_VALUE_SCALAR);
    TimelineTrackSetUnit(&track, TIMELINE_UNIT_UNITLESS);
    TimelineTrackAddKey(&track, first_frame, TimelineValueScalar(1.0),
                        TIMELINE_INTERPOLATION_CUBIC_BEZIER);
    TimelineTrackAddKey(&track, 199, TimelineValueScalar(1.0),
                        TIMELINE_INTERPOLATION_STEP);
    track.keys[0].outgoing_frame_offset = (199.0 - first_frame) / 3.0;
    track.keys[1].incoming_frame_offset = -(199.0 - first_frame) / 3.0;
    TimelinePropertyRegistryInitFoundationDefaults(&registry);
    TimelinePropertyRegistryFind(&registry, property, &descriptor);
    assert(descriptor != NULL);
    for (int frame = first_frame; frame <= 199; ++frame) {
        TimelineEvaluationContext context;
        TimelineEvaluationResult result;
        TimelineEvaluationContextBuild((TimelineRate){24, 1},
                                       (TimelineRange){0, 200},
                                       (TimelineSample){frame, 0, 1}, &context);
        assert(TimelineTrackEvaluate(&track, &context, &result) == TIMELINE_STATUS_OK);
        assert(result.value.as.scalar == 1.0);
        assert(TimelinePropertyDescriptorValidateValue(descriptor, result.value)
               == TIMELINE_STATUS_OK);
    }
}

int main(void) {
    check_hold("skull_descent-progress", "object/falling_skull",
               "object/path_progress", 170);
    check_hold("camera_aquarium_reveal-progress", "camera/main",
               "camera/route_progress", 185);
    puts("PASS bounded object and camera constant cubic holds");
    return 0;
}
