#include "app/scene_atmosphere_runtime.h"

bool SceneAtmosphereRuntimeRequiresSynchronousSelection(bool legacy_enabled) {
    const SceneAtmosphereAttachment* v=SceneAtmosphereRuntimeRead();
    if(v->present) return v->enabled && v->animated;
    return legacy_enabled;
}
