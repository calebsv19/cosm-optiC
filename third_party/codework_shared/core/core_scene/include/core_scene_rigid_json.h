#ifndef CORE_SCENE_RIGID_JSON_H
#define CORE_SCENE_RIGID_JSON_H
#include "core_scene_rigid.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional cJSON adapter, compiled separately from the default library.
 * Both byte sequences are bounded, strict JSON. Output is assigned only after
 * complete validation. Catalog consistency is not payload authentication. */
CoreResult core_scene_rigid_json_read(const char *scene, size_t scene_bytes,
                                      const char *catalog, size_t catalog_bytes,
                                      CoreSceneRigidLimits limits,
                                      CoreSceneRigidContract *output,
                                      CoreSceneRigidDiagnostic *diagnostic);
#ifdef __cplusplus
}
#endif
#endif
