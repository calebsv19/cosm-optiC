#include "app/evaluated_scene_service.h"
#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"
#include <stdio.h>
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  LoadAllSettings();
  RuntimeSceneBridgePreflight preflight;
  if (!runtime_scene_bridge_apply_file(argv[1], &preflight)) {
    fprintf(stderr, "%s\n", preflight.diagnostics);
    return 3;
  }
  RayEvaluatedSceneServiceResult sample;
  if (!RayEvaluatedSceneCaptureSample((TimelineSample){48, 1, 2}, &sample))
    return 4;
  const RayEvaluatedCamera *camera = &sample.snapshot.camera;
  if (!camera->inspection_id[0] || camera->inspection_time.numerator != 43 ||
      camera->inspection_time.denominator != 48 ||
      sample.snapshot.sequence.source_tick != 150)
    return 5;
  if (!RayEvaluatedSceneCaptureSample((TimelineSample){98, 1, 2}, &sample) ||
      sample.snapshot.camera.inspection_time.numerator != 71 ||
      sample.snapshot.camera.inspection_time.denominator != 24)
    return 6;
  puts("Inspection exact subframe 43/48 seconds, endpoint clamp and immutable "
       "source tick 150 PASS");
  return 0;
}
