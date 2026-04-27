#include "material/material.h"
#include "math/math_utils.h"
#include <stddef.h>

static Material ClampMaterial(Material m) {
    m.diffuse = clampd(m.diffuse, 0.0f, 1.0f);
    m.specular = clampd(m.specular, 0.0f, 1.0f);
    m.reflectivity = clampd(m.reflectivity, 0.0f, 1.0f);
    m.roughness = clampd(m.roughness, 0.0f, 1.0f);
    m.metallic = clampd(m.metallic, 0.0f, 1.0f);
    m.transparency = clampd(m.transparency, 0.0f, 1.0f);
    return m;
}

void MaterialLibraryInit(MaterialLibrary* lib) {
    if (!lib) return;
    lib->count = 0;

    // Preset 0: Default matte
    Material matte = {
        .diffuse = 0.8f,
        .specular = 0.05f,
        .reflectivity = 0.1f,
        .roughness = 0.6f,
        .base_color = vec3(1.0f, 1.0f, 1.0f),
        .emissive = vec3(0.0f, 0.0f, 0.0f),
        .metallic = 0.0f,
        .transparency = 0.0f
    };
    MaterialAdd(lib, matte); // MATERIAL_PRESET_DEFAULT

    // Preset 1: Mirror
    Material mirror = {
        .diffuse = 0.0f,
        .specular = 0.1f,
        .reflectivity = 0.95f,
        .roughness = 0.0f,
        .base_color = vec3(1.0f, 1.0f, 1.0f),
        .emissive = vec3(0.0f, 0.0f, 0.0f),
        .metallic = 1.0f,
        .transparency = 0.0f
    };
    MaterialAdd(lib, mirror); // MATERIAL_PRESET_MIRROR

    // Preset 2: Rough metal
    Material roughMetal = {
        .diffuse = 0.2f,
        .specular = 0.6f,
        .reflectivity = 0.7f,
        .roughness = 0.6f,
        .base_color = vec3(0.8f, 0.8f, 0.8f),
        .emissive = vec3(0.0f, 0.0f, 0.0f),
        .metallic = 1.0f,
        .transparency = 0.0f
    };
    MaterialAdd(lib, roughMetal); // MATERIAL_PRESET_ROUGH_METAL

    // Preset 3: Glossy paint
    Material glossy = {
        .diffuse = 0.6f,
        .specular = 0.25f,
        .reflectivity = 0.4f,
        .roughness = 0.25f,
        .base_color = vec3(1.0f, 1.0f, 1.0f),
        .emissive = vec3(0.0f, 0.0f, 0.0f),
        .metallic = 0.0f,
        .transparency = 0.0f
    };
    MaterialAdd(lib, glossy); // MATERIAL_PRESET_GLOSSY

    // Preset 4: Emissive
    Material emissive = {
        .diffuse = 0.0f,
        .specular = 0.0f,
        .reflectivity = 0.0f,
        .roughness = 1.0f,
        .base_color = vec3(1.0f, 1.0f, 1.0f),
        .emissive = vec3(1.0f, 1.0f, 1.0f),
        .metallic = 0.0f,
        .transparency = 0.0f
    };
    MaterialAdd(lib, emissive); // MATERIAL_PRESET_EMISSIVE

    // Preset 5: Transparent
    Material transparent = {
        .diffuse = 0.05f,
        .specular = 0.0f,
        .reflectivity = 0.0f,
        .roughness = 1.0f,
        .base_color = vec3(1.0f, 1.0f, 1.0f),
        .emissive = vec3(0.0f, 0.0f, 0.0f),
        .metallic = 0.0f,
        .transparency = 0.9f
    };
    MaterialAdd(lib, transparent); // MATERIAL_PRESET_TRANSPARENT
}

int MaterialAdd(MaterialLibrary* lib, Material mat) {
    if (!lib) return -1;
    if (lib->count >= MAX_MATERIALS) return -1;
    lib->materials[lib->count] = ClampMaterial(mat);
    return lib->count++;
}

const Material* MaterialGet(const MaterialLibrary* lib, int id) {
    if (!lib) return NULL;
    if (id < 0 || id >= lib->count) return NULL;
    return &lib->materials[id];
}

int MaterialDefaultId(void) {
    return 0; // preset index 0 is default matte
}
