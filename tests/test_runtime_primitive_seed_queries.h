#include <assert.h>
#include <string.h>
#include "import/runtime_scene_bridge.h"
static void test_runtime_primitive_seed_queries(void) {
    RuntimeSceneBridge3DPrimitiveSeedState reference;
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&reference);
    int count=runtime_scene_bridge_last_3d_primitive_seed_count();
    assert(count==(reference.valid ? reference.primitive_count : 0));
    for(int i=0;i<count;i++) {
        RuntimeSceneBridgePrimitiveSeed seed;
        assert(runtime_scene_bridge_get_last_3d_primitive_seed(i,&seed));
        assert(!memcmp(&seed,&reference.primitives[i],sizeof(seed)));
    }
    RuntimeSceneBridgePrimitiveSeed sentinel,before;
    memset(&sentinel,0x5a,sizeof(sentinel));before=sentinel;
    assert(!runtime_scene_bridge_get_last_3d_primitive_seed(-1,&sentinel));
    assert(!runtime_scene_bridge_get_last_3d_primitive_seed(count,&sentinel));
    assert(!runtime_scene_bridge_get_last_3d_primitive_seed(0,NULL));
    assert(!memcmp(&sentinel,&before,sizeof(sentinel)));
}
