/* Independent 3D distance contract, including unequal segment lengths. */
static void dm2_sampler_contract(void) {
  MotionPaths d = {.count = 1, .binding_count = 1};
  MotionPath *p = &d.paths[0];
  snprintf(p->id, 64, "route");
  snprintf(p->name, 128, "Vertical then horizontal");
  p->count = 3;
  for (int i = 0; i < 3; ++i) {
    snprintf(p->points[i].id, 64, "p%d", i);
    p->points[i].linear = true;
  }
  p->points[1].position[2] = 3;
  p->points[2].position[2] = 3;
  p->points[2].position[1] = 4;
  snprintf(d.bindings[0].object_id, 64, "mesh");
  snprintf(d.bindings[0].path_id, 64, "route");
  d.bindings[0].enabled = true;
  json_object *author = json_object_new_object();
  json_object_object_add(author, "motion_paths", MotionPathsToJson(&d));
  assert(MotionPathsRuntimeLoad(author, 2));
  TimelineVec3 v;
  assert(MotionPathsRuntimePosition("mesh", 3. / 7, &v));
  assert(fabs(v.z - 6) < 1e-10 && fabs(v.y) < 1e-10);
  assert(MotionPathsRuntimePosition("mesh", .5, &v));
  assert(fabs(v.z - 6) < 1e-10 && fabs(v.y - 1) < 1e-10);
  assert(MotionPathsRuntimePosition("mesh", 1, &v) && fabs(v.y - 8) < 1e-10);
  assert(MotionPathsRuntimePosition("mesh", 0, &v) && fabs(v.z) < 1e-10);
  uint64_t before = MotionPathsRuntimeRevision();
  p->count = 2;
  p->points[0].linear = false;
  p->points[0].outgoing[0] = 3;
  p->points[1].incoming[1] = 4;
  json_object_object_add(author, "motion_paths", MotionPathsToJson(&d));
  assert(MotionPathsRuntimeLoad(author, 1) &&
         MotionPathsRuntimeRevision() != before);
  /* High-resolution reference independently inverts cumulative spatial length.
   */
  double lengths[20001], last[3];
  lengths[0] = 0;
  MotionPathPointAt(p, 0, last);
  for (int i = 1; i <= 20000; ++i) {
    double current[3], length = 0;
    MotionPathPointAt(p, (double)i / 20000, current);
    for (int k = 0; k < 3; ++k) {
      double delta = current[k] - last[k];
      length += delta * delta;
      last[k] = current[k];
    }
    lengths[i] = lengths[i - 1] + sqrt(length);
  }
  for (int j = 1; j < 10; ++j) {
    double progress = j * .1, target = lengths[20000] * progress;
    int i = 1;
    while (lengths[i] < target)
      ++i;
    double t = (i - 1 +
                (target - lengths[i - 1]) / (lengths[i] - lengths[i - 1])) /
               20000,
           expected[3];
    MotionPathPointAt(p, t, expected);
    assert(MotionPathsRuntimePosition("mesh", progress, &v));
    assert(fabs(v.x - expected[0]) < .0002 && fabs(v.y - expected[1]) < .0002 &&
           fabs(v.z - expected[2]) < .0002);
  }
  assert(!MotionPathsRuntimeLoad(author, 0));
  assert(!MotionPathsRuntimePosition("mesh", .5, &v));
  json_object_put(author);
  MotionPathsRuntimeReset();
  fprintf(stderr, "D-M2 sampler PASS: true XYZ distance, unequal segments, "
                  "scale, endpoints, cubic accuracy, cache invalidation.\n");
}
