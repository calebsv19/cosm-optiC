#pragma once
#include "core_authored_surface_graph.h"
#include "render/runtime_material_payload_3d.h"
#include <json-c/json.h>
bool RuntimeSurfaceGraphParse(json_object *source, CoreSurfaceGraph *out, char *diagnostic,
                              size_t size);
bool RuntimeSurfaceGraphActive(int index);
bool RuntimeSurfaceGraphCompositionActive(int index);
bool RuntimeSurfaceGraphResolve(const HitInfo3D *hit, RuntimeMaterialPayload3D *out);
const char *RuntimeSurfaceGraphCapabilities(void);

/* Stable graph failure identity; empty object/node means that scope is unavailable.
 * Property is a JSON-style path relative to the graph, or the enclosing scene/object.
 * Existing string diagnostics format these fields and may truncate to caller capacity. */
typedef struct {
    char code[40];
    char object_id[64];
    char node_id[64];
    char property[96];
    char message[160];
} RuntimeSurfaceGraphDiagnostic;
/* Same parser/compiler as RuntimeSurfaceGraphParse, with machine-readable failure fields.
 * Failure clears the output program; success clears the diagnostic. */
bool RuntimeSurfaceGraphParseDetailed(json_object *source, const char *object_id,
                                      CoreSurfaceGraph *out,
                                      RuntimeSurfaceGraphDiagnostic *diagnostic);
