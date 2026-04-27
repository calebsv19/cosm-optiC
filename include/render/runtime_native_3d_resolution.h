#ifndef RUNTIME_NATIVE_3D_RESOLUTION_H
#define RUNTIME_NATIVE_3D_RESOLUTION_H

#include <stdbool.h>
#include <stdint.h>

int RuntimeNative3DClampRenderScale(int value);
bool RuntimeNative3DResolveScaledDimensions(int host_width,
                                            int host_height,
                                            int scale,
                                            int* out_width,
                                            int* out_height);
bool RuntimeNative3DResolveUpscaledRect(int src_x,
                                        int src_y,
                                        int src_width,
                                        int src_height,
                                        int src_frame_width,
                                        int src_frame_height,
                                        int dst_frame_width,
                                        int dst_frame_height,
                                        int* out_x,
                                        int* out_y,
                                        int* out_width,
                                        int* out_height);
void RuntimeNative3DUpscaleNearest(const uint8_t* src,
                                   int src_width,
                                   int src_height,
                                   uint8_t* dst,
                                   int dst_width,
                                   int dst_height);

#endif
