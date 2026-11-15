#include "render/runtime_spectral_sampling.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
  for (unsigned seed = 0; seed < 7; ++seed)
    for (int batch = 0; batch < 64; ++batch) {
      for (int group = 0; group < 2; ++group) {
        int quadrants[4] = {0};
        for (int j = 0; j < 4; ++j) {
          double x, y, a, b;
          int pass = 8 * batch + 4 * group + j;
          RuntimeSpectralSampling_Jitter(seed, pass, &x, &y);
          RuntimeSpectralSampling_Jitter(seed, pass, &a, &b);
          assert(x == a && y == b && x >= -.5 && x < .5 && y >= -.5 && y < .5);
          ++quadrants[(x >= 0) + 2 * (y >= 0)];
        }
        for (int j = 0; j < 4; ++j)
          assert(quadrants[j] == 1);
      }
    }
  for (unsigned seed = 0; seed < 5; ++seed)
    for (int side = 2; side <= 8; side *= 2)
      for (int group = 0; group < 2; ++group) {
        int cells[64] = {0};
        for (int j = 0; j < side * side; ++j) {
          int pass = (j / 4) * 8 + group * 4 + j % 4;
          RuntimeNative3DSamplingContext c =
              RuntimeSpectralSampling_Camera(seed, pass);
          double x, y, a, b;
          RuntimeNative3DSampling_PixelPosition(&c, 17, 23, &x, &y);
          RuntimeNative3DSampling_PixelPosition(&c, 17, 23, &a, &b);
          assert(x == a && y == b);
          int ix = (int)((x - 16.5) * side), iy = (int)((y - 22.5) * side);
          assert(ix >= 0 && ix < side && iy >= 0 && iy < side);
          ++cells[iy * side + ix];
        }
        for (int j = 0; j < side * side; ++j)
          assert(cells[j] == 1);
      }
  RuntimeNative3DSamplingContext c = RuntimeSpectralSampling_Camera(19, 0);
  RuntimeNative3DSamplingContext epoch =
      RuntimeSpectralSampling_Camera(19, 131072);
  RuntimeNative3DSamplingContext other = RuntimeSpectralSampling_Camera(19, 4);
  assert(c.cameraSequenceSeed != epoch.cameraSequenceSeed &&
         c.cameraSequenceSeed != other.cameraSequenceSeed);
  double x, y, a, b;
  RuntimeNative3DSampling_PixelPosition(&c, 17, 23, &x, &y);
  RuntimeNative3DSampling_PixelPosition(&c, 18, 23, &a, &b);
  assert(x - 17 != a - 18 || y != b);
  for (unsigned bundle = 1; bundle <= 8; bundle *= 2)
    for (int pass = 0; pass < 64; ++pass)
      for (unsigned ray = 0; ray < bundle; ++ray) {
        unsigned index = ((unsigned)pass / 8 * 4 + pass % 4) * bundle + ray;
        int equivalent = (int)(index / 4 * 8 + (pass % 8 / 4) * 4 + index % 4);
        RuntimeNative3DSamplingContext bundled =
            RuntimeSpectralSampling_CameraForMap(19, pass, ray, bundle);
        RuntimeNative3DSamplingContext flat = RuntimeSpectralSampling_Camera(19, equivalent);
        assert(bundled.temporalSubpassIndex == flat.temporalSubpassIndex &&
               bundled.cameraSequenceSeed == flat.cameraSequenceSeed);
      }
  RuntimeSpectralSamplingConfig config;
  const char *valid[] = {
      "{\"mode\":\"converge\"}",
      "{\"mode\":\"converge\",\"camera_samples_per_map\":1}",
      "{\"mode\":\"converge\",\"camera_sequence\":\"progressive\",\"camera_samples_per_map\":8}"};
  for (unsigned i=0; i<sizeof(valid)/sizeof(*valid); ++i) {
    json_object *j=json_tokener_parse(valid[i]);
    assert(RuntimeSpectralSampling_Parse(j,&config));
    assert(config.camera_samples_per_map == (i==2 ? 8 : 1));
    json_object_put(j);
  }
  const char *bad_values[] = {"0", "-1", "3", "9", "4294967297", "1.0", "true", "null", "\"4\""};
  for (unsigned i=0; i<sizeof(bad_values)/sizeof(*bad_values); ++i) {
    char text[256];
    snprintf(text,sizeof(text),"{\"mode\":\"converge\",\"camera_sequence\":\"progressive\",\"camera_samples_per_map\":%s}",bad_values[i]);
    json_object *j=json_tokener_parse(text);assert(j);
    assert(!RuntimeSpectralSampling_Parse(j,&config));json_object_put(j);
  }
  json_object *j=json_tokener_parse("{\"mode\":\"converge\",\"camera_samples_per_map\":4}");
  assert(!RuntimeSpectralSampling_Parse(j,&config));json_object_put(j);
  puts("PASS bounded bundle parsing and exact independent-group camera sequence extension");
  puts("PASS progressive per-pixel replay, independent groups, epoch renewal "
       "and 2x2/4x4/8x8 coverage");
  puts("PASS balanced four-quadrant coverage in both independent convergence "
       "groups");
}
