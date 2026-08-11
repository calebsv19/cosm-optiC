#ifndef PROCEDURAL_SURFACE_PRISM_BINDING_H
#define PROCEDURAL_SURFACE_PRISM_BINDING_H

#include "procedural/procedural_surface_binding.h"
#include "procedural/procedural_surface_prism_mesh.h"

#include <stdbool.h>

typedef struct ProceduralSurfacePrismBindingContext {
    const ProceduralSurfaceCageContract *cage;
    const ProceduralSurfaceBindingV1 *binding;
    const ProceduralSurfaceFieldGraphV1 *graph;
} ProceduralSurfacePrismBindingContext;

bool ProceduralSurfacePrismBindingContext_Init(
    ProceduralSurfacePrismBindingContext *context,
    const ProceduralSurfaceCageContract *cage,
    const ProceduralSurfaceBindingV1 *binding,
    const ProceduralSurfaceFieldGraphV1 *graph,
    ProceduralSurfaceBindingReport *report);

bool ProceduralSurfacePrismBinding_EvaluateLegacy(
    const void *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceFieldOutput *out_field,
    ProceduralSurfaceFieldReport *report);

bool ProceduralSurfacePrismBinding_EvaluateSample(
    const ProceduralSurfacePrismBindingContext *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldBudget *budget,
    ProceduralSurfaceBoundSample *out_sample,
    ProceduralSurfaceBindingReport *report);

bool ProceduralSurfacePrismBinding_ResolveDisplacementDirection(
    const void *context,
    ProceduralSurfaceFieldPoint3D point,
    ProceduralSurfaceFieldPoint3D source_normal,
    ProceduralSurfaceFieldPoint3D *out_direction);

ProceduralSurfaceFieldPoint3D ProceduralSurfacePrismBinding_NominalNormal(
    const ProceduralSurfaceCageContract *cage,
    ProceduralSurfaceFieldPoint3D point,
    const char **out_surface_group_id);

#endif
