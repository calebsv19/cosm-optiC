#include "core_scene_compile.h"
#include "core_scene_rigid_result.h"
static bool digest_bytes(const void *p, size_t n, char hex[65]) {
  return core_scene_compile_sha256(p, n, hex).code == CORE_OK;
}
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f)
    return 2;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  char *bytes = n > 0 && n < CORE_SCENE_RIGID_RESULT_JSON_BYTES ? malloc((size_t)n) : NULL;
  bool ok = bytes && fread(bytes, 1, (size_t)n, f) == (size_t)n;
  fclose(f);
  CoreSceneRigidResult r = {0};
  ok = ok &&
       core_scene_rigid_result_read(bytes, (size_t)n, &r, digest_bytes).code ==
           CORE_OK;
  free(bytes);
  if (ok) {
    uint32_t tick;
    ok = core_scene_rigid_result_tick(&r, 1, 0, 1, 30, 1, &tick).code ==
             CORE_OK &&
         tick == 8 &&
         core_scene_rigid_result_tick(&r, 0, 1, 2, 240, 1, &tick).code !=
             CORE_OK &&
         core_scene_rigid_result_tick(&r, INT64_MAX, 0, UINT32_MAX, UINT32_MAX,
                                      UINT32_MAX, &tick)
                 .code != CORE_OK;
  }
  core_scene_rigid_result_free(&r);
  return ok ? 0 : 3;
}
