#include "render/ray_tracing2_preview.h"

#include <math.h>
#include <stdlib.h>

#include "render/integrators/integrator_common.h"

static void BuildGaussianKernel(float* kernel, int radius) {
    float sigma = (float)radius * 0.5f + 0.5f;
    float sum = 0.0f;
    for (int i = -radius; i <= radius; i++) {
        float value = expf(-(i * i) / (2.0f * sigma * sigma));
        kernel[i + radius] = value;
        sum += value;
    }
    if (sum > 0.0f) {
        for (int i = 0; i < (2 * radius + 1); i++) {
            kernel[i] /= sum;
        }
    }
}

void RayTracingPreview_ApplySeparableBlur(Uint8* buffer, int width, int height, int radius) {
    if (radius <= 0 || !buffer) return;
    int kernelSize = radius * 2 + 1;
    float* kernel = (float*)malloc((size_t)kernelSize * sizeof(float));
    if (!kernel) return;
    BuildGaussianKernel(kernel, radius);

    size_t total = (size_t)width * (size_t)height;
    float* temp = (float*)malloc(total * sizeof(float));
    float* output = (float*)malloc(total * sizeof(float));
    if (!temp || !output) {
        free(kernel);
        free(temp);
        free(output);
        return;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            for (int k = -radius; k <= radius; k++) {
                int sx = x + k;
                if (sx < 0) sx = 0;
                if (sx >= width) sx = width - 1;
                accum += kernel[k + radius] * buffer[y * width + sx];
            }
            temp[y * width + x] = accum;
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            for (int k = -radius; k <= radius; k++) {
                int sy = y + k;
                if (sy < 0) sy = 0;
                if (sy >= height) sy = height - 1;
                accum += kernel[k + radius] * temp[sy * width + x];
            }
            output[y * width + x] = accum;
        }
    }

    for (size_t i = 0; i < total; i++) {
        buffer[i] = (Uint8)Clamp(output[i], 0, 255);
    }

    free(kernel);
    free(temp);
    free(output);
}
