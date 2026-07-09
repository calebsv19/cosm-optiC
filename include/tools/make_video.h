#ifndef MAKE_VIDEO_H
#define MAKE_VIDEO_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*MakeVideoProgressCallback)(size_t current_frame,
                                          size_t total_frames,
                                          int percent_complete,
                                          void *user_data);

bool ResolveFFmpegBinary(char *out, size_t out_size);

int MakeVideoFromFrames(const char *frameDir,
                        const char *outputFile,
                        int fps);
int MakeVideoFromFramesWithProgress(const char *frameDir,
                                    const char *outputFile,
                                    int fps,
                                    MakeVideoProgressCallback progress_cb,
                                    void *user_data);

#endif // MAKE_VIDEO_H
