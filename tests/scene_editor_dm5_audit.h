/* Fail-closed persistence and unit/clock/cache audit using isolated JSON
 * copies. No audit mutation reaches the retained editor document. */
static void dm5_audit(const char *scene) {
  json_object *saved = json_object_from_file(scene);
  assert(saved);
  char message[512];
  for (int scale_case = 0; scale_case < 3; ++scale_case) {
    json_object *copy = NULL;
    assert(json_object_deep_copy(saved, &copy, NULL) == 0);
    double scale = (double[]){.5, 2, 1000}[scale_case];
    json_object_object_add(copy, "world_scale", json_object_new_double(scale));
    json_object *a = MotionPlansAuthor(copy),
                *timeline = dm5_get(a, "scene_timeline"),
                *rate = dm5_get(timeline, "rate"),
                *range = dm5_get(timeline, "range");
    json_object_object_del(a, "motion_plans");
    json_object_object_add(rate, "numerator", json_object_new_int(30000));
    json_object_object_add(rate, "denominator", json_object_new_int(1001));
    json_object_object_add(range, "start_frame", json_object_new_int(100));
    json_object_object_add(range, "frame_count", json_object_new_int(300));
    json_object *tracks = dm5_get(timeline, "tracks");
    for (size_t i = 0; i < json_object_array_length(tracks); ++i) {
      json_object *keys = dm5_get(json_object_array_get_idx(tracks, i), "keys");
      for (size_t k = 0; k < json_object_array_length(keys); ++k) {
        json_object *key = json_object_array_get_idx(keys, k);
        json_object_object_add(
            key, "frame",
            json_object_new_int64(json_object_get_int64(dm5_get(key, "frame")) +
                                  100));
      }
    }
    MotionTimingScheduleRequest r = {.start_time = .2,
                                     .max_speed = 100 * scale,
                                     .acceleration = 10000 * scale,
                                     .braking = 5000 * scale,
                                     .count = 2};
    r.points[1] = (MotionTimingWaypoint){
        .position = 1, .fixed_arrival = true, .arrival = 4};
    MotionRouteSchedule planned;
    json_object *e = MotionPlanCreate(a, scale, "camera/main", &r, &planned,
                                      message, sizeof(message));
    if (!e)
      fprintf(stderr, "M5 unit audit: %s\n", message);
    assert(e);
    json_object *list = json_object_new_array();
    json_object_array_add(list, e);
    json_object_object_add(a, "motion_plans", list);
    assert(RuntimeSceneTimelineValidateScene(copy, message, sizeof(message)));
    assert(MotionPlansRuntimeLoad(a, scale));
    TimelineEvaluationContext c;
    assert(TimelineEvaluationContextBuild(
               (TimelineRate){30000, 1001}, (TimelineRange){100, 300},
               (TimelineSample){122, 1, 2}, &c) == TIMELINE_STATUS_OK);
    assert(fabs(c.local_time_seconds - .75075) < 1e-12);
    TimelineEvaluationResult value = {.valid = true,
                                      .status = TIMELINE_STATUS_OK};
    snprintf(value.target_id, sizeof(value.target_id), "camera/main");
    snprintf(value.property_id, sizeof(value.property_id),
             "camera/route_progress");
    assert(MotionPlansRuntimeEvaluate(&c, &value));
    MotionTimingScheduleSample expected;
    assert(MotionTimingScheduleSampleAt(&planned.timeline, c.local_time_seconds,
                                        &expected) == MOTION_TIMING_OK);
    TimelineVec3 pos;
    double length;
    assert(MotionPlansRuntimeGeometry(value.target_id, value.value.as.scalar,
                                      &pos, &length, NULL));
    assert(fabs(value.value.as.scalar - expected.position / length) < 1e-12);
    assert(fabs(value.derivative_per_frame -
                expected.velocity / length * 1001 / 30000) < 1e-12);
    double original = value.value.as.scalar;
    c.local_time_seconds = 0;
    assert(MotionPlansRuntimeEvaluate(&c, &value));
    assert(value.value.as.scalar == 0 && value.derivative_per_frame == 0);
    c.local_time_seconds = 5;
    assert(MotionPlansRuntimeEvaluate(&c, &value));
    assert(fabs(value.value.as.scalar - 1) < 1e-12 &&
           value.derivative_per_frame == 0);
    c.local_time_seconds = .75075;
    assert(MotionPlansRuntimeEvaluate(&c, &value));
    assert(value.value.as.scalar == original);
    json_object_object_add(a, "motion_plans", NULL);
    assert(!RuntimeSceneTimelineValidateScene(copy, message, sizeof(message)));
    assert(!MotionPlansRuntimeLoad(a, scale));
    assert(RuntimeSceneTimelineLoad(a,scale)==TIMELINE_STATUS_INVALID_ARGUMENT);
    assert(!MotionPathsRuntimeBinding("camera/main",NULL));
    assert(!MotionPlansRuntimeActive("camera/main"));
    json_object_put(copy);
  }
  assert(RuntimeSceneTimelineLoad(MotionPlansAuthor(saved),2)==TIMELINE_STATUS_OK);
  json_object_put(saved);
  fprintf(stderr, "D-M5 audit PASS: rational rate, nonzero range origin, "
                  "subframes, three world scales, outside-plan rests, seek "
                  "independence, malformed-list refusal and cache reset\n");
}
