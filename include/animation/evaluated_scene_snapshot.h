#ifndef RAY_TRACING_EVALUATED_SCENE_SNAPSHOT_H
#define RAY_TRACING_EVALUATED_SCENE_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "animation/timeline_document.h"
#include "animation/timeline_light_motion.h"

#define RAY_EVALUATED_SCENE_SNAPSHOT_SCHEMA_VERSION 3u
#define RAY_EVALUATED_SCENE_DIAGNOSTICS_CAPACITY 256u
#define RAY_EVALUATED_OBJECT_TRANSFORM_CAPACITY 64u

typedef enum RayEvaluatedSceneSource {
    RAY_EVALUATED_SCENE_SOURCE_NONE = 0,
    RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE,
    RAY_EVALUATED_SCENE_SOURCE_LEGACY_PREVIEW_FALLBACK
} RayEvaluatedSceneSource;

typedef enum RayEvaluatedPlaybackMode {
    RAY_EVALUATED_PLAYBACK_STOP = 0,
    RAY_EVALUATED_PLAYBACK_LOOP,
    RAY_EVALUATED_PLAYBACK_BOUNCE
} RayEvaluatedPlaybackMode;

typedef enum RayEvaluatedSimulationSource {
    RAY_EVALUATED_SIMULATION_NONE = 0,
    RAY_EVALUATED_SIMULATION_CACHE
} RayEvaluatedSimulationSource;

typedef enum RayEvaluatedObjectTransformSource {
    RAY_EVALUATED_OBJECT_TRANSFORM_NONE = 0,
    RAY_EVALUATED_OBJECT_TRANSFORM_COMPATIBILITY_MOTION
} RayEvaluatedObjectTransformSource;

typedef enum RayEvaluatedSimulationInterpolation {
    RAY_EVALUATED_SIMULATION_INTERPOLATION_NONE = 0,
    RAY_EVALUATED_SIMULATION_INTERPOLATION_STEP,
    RAY_EVALUATED_SIMULATION_INTERPOLATION_LINEAR
} RayEvaluatedSimulationInterpolation;

typedef struct RayEvaluatedSceneIdentity {
    uint64_t scene_revision;
    uint64_t timeline_revision;
} RayEvaluatedSceneIdentity;

typedef struct RayEvaluatedLight {
    bool valid;
    bool enabled;
    char target_id[TIMELINE_ID_CAPACITY];
    char runtime_light_id[TIMELINE_ID_CAPACITY];
    int kind;
    int origin;
    int emission_profile;
    TimelineVec3 position;
    TimelineVec3 axis_u;
    TimelineVec3 axis_v;
    TimelineVec3 normal;
    double radius;
    double width;
    double height;
    TimelineVec3 color;
    double intensity;
    int radiometry_mode;
    double radiance;
    double falloff_distance;
    int falloff_mode;
    double progress;
    double progress_per_frame;
    double path_length_world;
    double world_speed_per_second;
    double global_path_t;
    bool speed_valid;
    bool intensity_authored;
    TimelineEvaluationResult path_progress_provenance;
    TimelineEvaluationResult intensity_provenance;
    /* Compatibility alias for path_progress_provenance. */
    TimelineEvaluationResult property_provenance;
} RayEvaluatedLight;

typedef struct RayEvaluatedCamera {
    bool valid;
    bool uses_authored_path;
    TimelineVec3 position;
    double yaw_radians;
    double pitch_radians;
    double fov_y_degrees;
    double aspect_ratio;
    double zoom;
} RayEvaluatedCamera;

typedef struct RayEvaluatedObjectTransform {
    bool valid;
    char target_id[TIMELINE_ID_CAPACITY];
    RayEvaluatedObjectTransformSource source;
    bool has_position;
    bool has_rotation;
    TimelineVec3 position;
    TimelineVec3 rotation_radians;
    TimelineEvaluationContext frame;
} RayEvaluatedObjectTransform;

typedef struct RayEvaluatedSimulationIdentity {
    RayEvaluatedSimulationSource source;
    bool valid;
    char cache_id[TIMELINE_ID_CAPACITY];
    uint64_t cache_revision;
    int64_t frame_index;
    int64_t source_frame_index;
    TimelineRate source_rate;
    int64_t frame_offset;
    uint32_t frame_stride;
    uint32_t subframe_numerator;
    uint32_t subframe_denominator;
    RayEvaluatedSimulationInterpolation interpolation;
    char content_digest[TIMELINE_ID_CAPACITY];
} RayEvaluatedSimulationIdentity;

typedef struct RayEvaluatedSceneSnapshot {
    uint32_t schema_version;
    bool valid;
    RayEvaluatedSceneSource source;
    RayEvaluatedPlaybackMode playback_mode;
    bool reverse_direction;
    bool clamped;
    TimelineEvaluationContext frame;
    RayEvaluatedSceneIdentity identity;
    RayEvaluatedLight light;
    RayEvaluatedCamera camera;
    size_t object_transform_count;
    RayEvaluatedObjectTransform
        object_transforms[RAY_EVALUATED_OBJECT_TRANSFORM_CAPACITY];
    RayEvaluatedSimulationIdentity simulation;
    uint32_t invalidation_domains;
    char diagnostics[RAY_EVALUATED_SCENE_DIAGNOSTICS_CAPACITY];
} RayEvaluatedSceneSnapshot;

typedef struct RayEvaluatedSceneSnapshotInputs {
    RayEvaluatedSceneSource source;
    RayEvaluatedPlaybackMode playback_mode;
    bool reverse_direction;
    bool clamped;
    TimelineEvaluationContext frame;
    RayEvaluatedSceneIdentity identity;
    RayEvaluatedLight light;
    RayEvaluatedCamera camera;
    const RayEvaluatedObjectTransform* object_transforms;
    size_t object_transform_count;
    RayEvaluatedSimulationIdentity simulation;
    uint32_t invalidation_domains;
    const char* diagnostics;
} RayEvaluatedSceneSnapshotInputs;

const char* RayEvaluatedSceneSourceLabel(RayEvaluatedSceneSource source);
const char* RayEvaluatedPlaybackModeLabel(RayEvaluatedPlaybackMode mode);
TimelineStatus RayEvaluatedTimelineSampleFromElapsed(
    TimelineRate rate,
    TimelineRange range,
    double elapsed_seconds,
    RayEvaluatedPlaybackMode mode,
    TimelineSample* out_sample,
    bool* out_reverse_direction,
    bool* out_clamped);
TimelineStatus RayEvaluatedSceneSnapshotBuild(
    const RayEvaluatedSceneSnapshotInputs* inputs,
    RayEvaluatedSceneSnapshot* out_snapshot);
TimelineStatus RayEvaluatedSceneSnapshotValidate(
    const RayEvaluatedSceneSnapshot* snapshot);
uint64_t RayEvaluatedTimelineFingerprint(const TimelineDocument* timeline,
                                         const Path* spatial_path,
                                         const CameraPath3D* spatial_path_3d);

#endif
