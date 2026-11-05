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
  puts("PASS balanced four-quadrant coverage in both independent convergence "
       "groups");
}
