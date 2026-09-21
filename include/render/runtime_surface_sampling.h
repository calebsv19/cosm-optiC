#pragma once
#include "render/runtime_material_payload_3d.h"
/* Immutable resources prepared with the retained scene. No shading-time IO. */
bool RuntimeSurfaceSamplingActive(int index);
bool RuntimeSurfaceSamplingResolve(const HitInfo3D *hit, RuntimeMaterialPayload3D *payload);
bool RuntimeSurfaceSamplingApply(const HitInfo3D *hit, RuntimeMaterialPayload3D *payload);
bool RuntimeSurfaceSamplingCoordinates(const HitInfo3D *hit, double uv[2], double dx[2],
                                       double dy[2], Vec3 *du, Vec3 *dv);
unsigned long long RuntimeSurfaceSamplingBuildCount(void);
size_t RuntimeSurfaceSamplingPreparedBytes(void);
