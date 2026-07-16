#include <math.h>
#include <stdio.h>

#include "editor/scene_editor_viewport3d_bridge.h"
#include "editor/scene_editor_viewport_nav_math.h"

static int close_enough(double actual, double expected) {
    return isfinite(actual) && fabs(actual - expected) <= 1e-9;
}

static int basis_matches_oracle(double yaw, double pitch) {
    CoreViewport3DBasis shared_basis = {0};
    SceneEditorViewportNavBasis oracle = {0};
    if (!SceneEditorViewport3DBridgeBuildBasis(yaw, pitch, &shared_basis) ||
        !SceneEditorViewportNavMathBuildBasis(yaw, pitch, &oracle)) return 0;
    return close_enough(shared_basis.right.x, oracle.right.x) &&
           close_enough(shared_basis.right.y, oracle.right.y) &&
           close_enough(shared_basis.right.z, oracle.right.z) &&
           close_enough(shared_basis.screen_down.x, oracle.screen_down.x) &&
           close_enough(shared_basis.screen_down.y, oracle.screen_down.y) &&
           close_enough(shared_basis.screen_down.z, oracle.screen_down.z) &&
           close_enough(shared_basis.forward.x, oracle.forward.x) &&
           close_enough(shared_basis.forward.y, oracle.forward.y) &&
           close_enough(shared_basis.forward.z, oracle.forward.z);
}

int main(void) {
    const double pi = 3.14159265358979323846264338327950288;
    CoreViewport3DVec3d target = {4.0, -3.0, 2.0};
    CoreViewport3DVec3d result = {0};
    CoreViewport3DBasis basis = {0};
    double yaw = 35.0 * pi / 180.0;
    double pitch = -20.0 * pi / 180.0;
    double out_yaw = 0.0;
    double out_pitch = 0.0;
    double out_scale = 0.0;
    if (!basis_matches_oracle(0.0, 0.0) || !basis_matches_oracle(yaw, pitch)) return 1;
    if (!SceneEditorViewport3DBridgeBuildBasis(yaw, pitch, &basis)) return 1;
    if (!SceneEditorViewport3DBridgeApplyPan(target,
                                             yaw,
                                             pitch,
                                             16.0,
                                             32.0,
                                             -16.0,
                                             &result)) return 1;
    {
        CoreViewport3DVec3d delta = {
            result.x - target.x, result.y - target.y, result.z - target.z
        };
        double right_delta = delta.x * basis.right.x + delta.y * basis.right.y +
                             delta.z * basis.right.z;
        double down_delta = delta.x * basis.screen_down.x +
                            delta.y * basis.screen_down.y +
                            delta.z * basis.screen_down.z;
        if (!close_enough(right_delta, -2.0) || !close_enough(down_delta, 1.0)) return 1;
    }
    if (!SceneEditorViewport3DBridgePreserveAnchor(target,
                                                   yaw,
                                                   pitch,
                                                   16.0,
                                                   32.0,
                                                   48.0,
                                                   -24.0,
                                                   &result)) return 1;
    if (!SceneEditorViewport3DBridgeApplyOrbit(yaw,
                                               pitch,
                                               0.2,
                                               -0.1,
                                               &out_yaw,
                                               &out_pitch)) return 1;
    if (!close_enough(out_yaw, yaw + 0.2) || !close_enough(out_pitch, pitch - 0.1)) return 1;
    if (!SceneEditorViewport3DBridgeApplyFrame(target,
                                               yaw,
                                               pitch,
                                               16.0,
                                               1.0,
                                               100.0,
                                               (CoreViewport3DVec3d){8.0, 9.0, 10.0},
                                               32.0,
                                               &result,
                                               &out_scale)) return 1;
    if (!close_enough(result.x, 8.0) || !close_enough(result.y, 9.0) ||
        !close_enough(result.z, 10.0) || !close_enough(out_scale, 32.0)) return 1;
    if (!SceneEditorViewport3DBridgeApplyResize(result,
                                                yaw,
                                                pitch,
                                                out_scale,
                                                1.0,
                                                100.0,
                                                &result,
                                                &out_scale)) return 1;
    if (!close_enough(result.x, 8.0) || !close_enough(result.y, 9.0) ||
        !close_enough(result.z, 10.0) || !close_enough(out_scale, 32.0)) return 1;
    result = (CoreViewport3DVec3d){91.0, 92.0, 93.0};
    if (SceneEditorViewport3DBridgeApplyPan(target,
                                            yaw,
                                            pitch,
                                            0.0,
                                            1.0,
                                            1.0,
                                            &result)) return 1;
    if (!close_enough(result.x, 91.0)) return 1;
    puts("scene editor viewport3d bridge: PASS");
    return 0;
}
