#ifndef RUNTIME_WATER_CONTACT_3D_H
#define RUNTIME_WATER_CONTACT_3D_H
#include "import/water_surface_import.h"
#include "render/runtime_water_body_mesh_3d.h"
/* Opt-in, axis-aligned rectangular closed container contact. Adjust only the
 * generated closure; preserve imported samples and all authored scene assets.
 */
bool RuntimeWaterContact3D_Prepare(const RuntimeScene3D *scene,
                                   const RuntimeWaterBodyBoundaryV1 *boundary,
                                   RuntimeWaterBodyMesh3DDesc *desc,
                                   int *out_container_index, char *diagnostics,
                                   size_t diagnostics_size);
#endif
