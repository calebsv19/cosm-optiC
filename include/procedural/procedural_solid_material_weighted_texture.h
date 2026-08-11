#ifndef PROCEDURAL_SOLID_MATERIAL_WEIGHTED_TEXTURE_H
#define PROCEDURAL_SOLID_MATERIAL_WEIGHTED_TEXTURE_H

#include "procedural/procedural_solid_authored_material.h"

#include <stddef.h>

typedef struct ProceduralSolidMaterialWeightedTextureV1 {
    ProceduralSolidAuthoredTextureV1 texture;
    double weight;
    size_t graph_layer_index;
} ProceduralSolidMaterialWeightedTextureV1;

#endif
