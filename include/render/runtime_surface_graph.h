#pragma once
#include "core_authored_surface_graph.h"
#include "render/runtime_material_payload_3d.h"
#include <json-c/json.h>
bool RuntimeSurfaceGraphParse(json_object *source, CoreSurfaceGraph *out, char *diagnostic,
                              size_t size);
bool RuntimeSurfaceGraphActive(int index);
bool RuntimeSurfaceGraphResolve(const HitInfo3D *hit, RuntimeMaterialPayload3D *out);
const char *RuntimeSurfaceGraphCapabilities(void);
