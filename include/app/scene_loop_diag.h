#ifndef SCENE_LOOP_DIAG_H
#define SCENE_LOOP_DIAG_H

#include <stdint.h>

void scene_loop_diag_tick(double frame_elapsed_sec,
                          uint32_t wait_blocked_ms,
                          uint32_t wait_call_count);

#endif // SCENE_LOOP_DIAG_H
