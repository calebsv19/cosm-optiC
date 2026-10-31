/* Actual native volume preparation, shuffled seeking and fail-closed
 * provenance. */
#include "app/evaluated_scene_service.h"
#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_native_3d_render.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t hash(uint64_t h, const void *p, size_t n) {
  const unsigned char *b = p;
  for (size_t i = 0; i < n; i++)
    h = (h ^ b[i]) * 1099511628211ULL;
  return h;
}
int main(int argc, char **argv) {
  if (argc < 3)
    return 2;
  LoadAllSettings();
  RuntimeSceneBridgePreflight p;
  if (!runtime_scene_bridge_apply_file(argv[1], &p)) {
    fprintf(stderr, "admission: %s\n", p.diagnostics);
    return 3;
  }
  if (!strcmp(argv[2], "--replacement")) {
    RayEvaluatedSceneServiceResult a, b;
    if (argc != 4 ||
        !RayEvaluatedSceneCaptureSample((TimelineSample){6, 0, 1}, &a))
      return 20;
    if (runtime_scene_bridge_apply_file(argv[3], &p))
      return 21;
    if (!RayEvaluatedSceneCaptureSample((TimelineSample){6, 0, 1}, &b) ||
        memcmp(&a.snapshot.sequence, &b.snapshot.sequence,
               sizeof(SequenceEvaluation)) ||
        strcmp(a.snapshot.simulation.content_digest,
               b.snapshot.simulation.content_digest))
      return 22;
    puts("VOLUME replacement refused; previous sequence preserved");
    return 0;
  }
  if (!strcmp(argv[2], "--tamper")) {
    RayEvaluatedSceneServiceResult a;
    if (argc != 4 ||
        !RayEvaluatedSceneCaptureSample((TimelineSample){6, 0, 1}, &a))
      return 30;
    FILE *file = fopen(argv[3], "r+b");
    if (!file || fseek(file, 100, SEEK_SET))
      return 31;
    int byte = fgetc(file);
    if (byte == EOF)
      return 32;
    fseek(file, 100, SEEK_SET);
    fputc(byte ^ 1, file);
    fclose(file);
    RuntimeNative3DPreparedFrame f = {0};
    bool accepted = RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
        &f, 48, 32, &a.snapshot, NULL);
    RuntimeNative3DPreparedFrame_Free(&f);
    file = fopen(argv[3], "r+b");
    if (!file)
      return 33;
    fseek(file, 100, SEEK_SET);
    fputc(byte, file);
    fclose(file);
    if (accepted)
      return 34;
    if (!RayEvaluatedSceneCaptureSample((TimelineSample){6, 0, 1}, &a) ||
        !RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
            &f, 48, 32, &a.snapshot, NULL))
      return 35;
    RuntimeNative3DPreparedFrame_Free(&f);
    puts("VOLUME stale evaluated payload refused; restored bytes prepare "
         "successfully");
    return 0;
  }
  for (int i = 2; i < argc; i++) {
    int frame = atoi(argv[i]);
    RayEvaluatedSceneServiceResult s;
    if (!RayEvaluatedSceneCaptureSample((TimelineSample){frame, 0, 1}, &s)) {
      fprintf(stderr, "%s\n", s.status_line);
      return 4;
    }
    RuntimeNative3DPreparedFrame f = {0};
    if (!RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
            &f, 48, 32, &s.snapshot, NULL)) {
      fprintf(stderr, "prepare: %s\n",
              RuntimeNative3DPrepareFrameLastDiagnostics());
      return 5;
    }
    RuntimeVolumeAttachment3D *v = &f.scene.volume;
    if (!v->hasData || v->grid.frameIndex != s.snapshot.sequence.source_tick)
      return 6;
    uint64_t h = 1469598103934665603ULL;
    size_t bytes = v->grid.cellCount * sizeof(float);
    float *fields[] = {v->channels.density, v->channels.velocityX,
                       v->channels.velocityY, v->channels.velocityZ,
                       v->channels.pressure};
    for (int k = 0; k < 5; k++) {
      if (!fields[k])
        return 7;
      h = hash(h, fields[k], bytes);
    }
    if (v->channels.solidMask)
      h = hash(h, v->channels.solidMask, v->grid.cellCount);
    printf("VOLUME "
           "{\"frame\":%d,\"tick\":%llu,\"content\":\"%s\",\"channels\":\"%"
           "016llx\",\"camera\":[%.17g,%.17g,%.17g]}\n",
           frame, (unsigned long long)s.snapshot.sequence.source_tick,
           s.snapshot.simulation.content_digest, (unsigned long long)h,
           s.snapshot.camera.position.x, s.snapshot.camera.position.y,
           s.snapshot.camera.position.z);
    RuntimeNative3DPreparedFrame_Free(&f);
    RayEvaluatedSceneSnapshot forged = s.snapshot;
    forged.simulation.source_frame_index++;
    if (RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(&f, 48, 32,
                                                                 &forged, NULL))
      return 8;
    forged = s.snapshot;
    forged.sequence.source_tick++;
    if (RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(&f, 48, 32,
                                                                 &forged, NULL))
      return 9;
  }
  return 0;
}
