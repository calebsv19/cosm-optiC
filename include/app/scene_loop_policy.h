#ifndef SCENE_LOOP_POLICY_H
#define SCENE_LOOP_POLICY_H

#include <stdbool.h>

typedef struct SceneLoopWaitPolicyInput {
    bool high_intensity_mode;
    bool interaction_active;
    bool background_busy;
    bool resize_pending;
} SceneLoopWaitPolicyInput;

int scene_loop_compute_wait_timeout_ms(const SceneLoopWaitPolicyInput *input);

#endif // SCENE_LOOP_POLICY_H
