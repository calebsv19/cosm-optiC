#ifndef MATERIAL_H
#define MATERIAL_H

#include <stdbool.h>

#include "math/vec3.h"

typedef struct {
    float diffuse;       // 0..1
    float specular;      // 0..1
    float reflectivity;  // 0..1
    float roughness;     // 0..1 (0 mirror, 1 diffuse)
    Vec3  base_color;    // albedo
    Vec3  emissive;      // emissive contribution (optional)
    float metallic;      // 0..1 (reserved for future use)
    float transparency;  // 0..1 (reserved for future use)
    float ior;           // >= 1.0 for dielectric transmission
    float absorption_distance; // > 0.0 reference distance for transmittance color
    bool thin_walled;    // thin sheet tinting without solid-thickness buildup
} Material;

#define MAX_MATERIALS 16

enum {
    MATERIAL_PRESET_DEFAULT = 0,
    MATERIAL_PRESET_MIRROR = 1,
    MATERIAL_PRESET_ROUGH_METAL = 2,
    MATERIAL_PRESET_GLOSSY = 3,
    MATERIAL_PRESET_EMISSIVE = 4,
    MATERIAL_PRESET_TRANSPARENT = 5
};

typedef struct {
    Material materials[MAX_MATERIALS];
    int count;
} MaterialLibrary;

void MaterialLibraryInit(MaterialLibrary* lib);
int  MaterialAdd(MaterialLibrary* lib, Material mat);
const Material* MaterialGet(const MaterialLibrary* lib, int id);
int  MaterialDefaultId(void);

#endif // MATERIAL_H
