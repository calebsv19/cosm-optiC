#ifndef RAY_CAMERA_SEQUENCE_H
#define RAY_CAMERA_SEQUENCE_H
#include "animation/sequence_clock.h"
#define CAMERA_SEQUENCE_CAPACITY 16u
#define CAMERA_SHOT_CAPACITY 128u
typedef enum CameraClockPolicy { CAMERA_CLOCK_CONTINUOUS=1, CAMERA_CLOCK_SELECTED, CAMERA_CLOCK_CLIP } CameraClockPolicy;
typedef struct CameraClock {
    char id[64];
    CameraClockPolicy policy;
    SequenceTime offset, speed;
    uint64_t duration_frames; /* Explicit hold at final motion frame. */
} CameraClock;
typedef struct CameraShot {
    char id[64], camera_id[64];
    uint64_t start_frame, frame_count; /* Local presentation offsets. */
    SequenceTime clip_start;
} CameraShot;
typedef struct CameraSequence {
    TimelineRate rate; TimelineRange range;
    size_t camera_count, shot_count;
    CameraClock cameras[CAMERA_SEQUENCE_CAPACITY];
    CameraShot shots[CAMERA_SHOT_CAPACITY];
} CameraSequence;
typedef struct CameraSequenceSample {
    size_t camera_index, shot_index;
    SequenceTime presentation_time, motion_time;
    TimelineSample motion_sample; /* Absolute timeline origin + motion offset. */
    bool selected, held_at_end;
} CameraSequenceSample;
bool CameraSequenceValidate(const CameraSequence *sequence);
/* NULL camera_id selects the active shot; a named view never changes selection. */
bool CameraSequenceEvaluate(const CameraSequence *sequence, TimelineSample sample,
                            const char *camera_id, CameraSequenceSample *out);
#endif
