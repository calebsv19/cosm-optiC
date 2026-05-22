#include "material/material_manager.h"
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>

static MaterialLibrary gMatLib;
static bool gInit = false;

void MaterialManagerInit(void) {
    if (gInit) return;
    MaterialLibraryInit(&gMatLib);
    gInit = true;
}

void MaterialManagerResetDefaults(void) {
    gInit = false;
    MaterialLibraryInit(&gMatLib);
    gInit = true;
}

const Material* MaterialManagerGet(int id) {
    if (!gInit) MaterialManagerInit();
    const Material* m = MaterialGet(&gMatLib, id);
    if (!m) {
        int def = MaterialDefaultId();
        return MaterialGet(&gMatLib, def);
    }
    return m;
}

int MaterialManagerDefaultId(void) {
    if (!gInit) MaterialManagerInit();
    return MaterialDefaultId();
}

int MaterialManagerCount(void) {
    if (!gInit) MaterialManagerInit();
    return gMatLib.count;
}

static bool LoadMaterialFromJsonFile(const char* path, Material* out) {
    if (!path || !out) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char* buf = malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    fread(buf, 1, sz, f);
    fclose(f);
    buf[sz] = '\0';
    struct json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root) return false;

    Material m = *out;
    struct json_object *v;
    if (json_object_object_get_ex(root, "diffuse", &v)) m.diffuse = json_object_get_double(v);
    if (json_object_object_get_ex(root, "specular", &v)) m.specular = json_object_get_double(v);
    if (json_object_object_get_ex(root, "reflectivity", &v)) m.reflectivity = json_object_get_double(v);
    if (json_object_object_get_ex(root, "roughness", &v)) m.roughness = json_object_get_double(v);
    if (json_object_object_get_ex(root, "metallic", &v)) m.metallic = json_object_get_double(v);
    if (json_object_object_get_ex(root, "transparency", &v)) m.transparency = json_object_get_double(v);
    if (json_object_object_get_ex(root, "ior", &v)) m.ior = json_object_get_double(v);
    if (json_object_object_get_ex(root, "absorption_distance", &v) ||
        json_object_object_get_ex(root, "absorptionDistance", &v)) {
        m.absorption_distance = json_object_get_double(v);
    }
    if (json_object_object_get_ex(root, "thin_walled", &v) ||
        json_object_object_get_ex(root, "thinWalled", &v)) {
        m.thin_walled = json_object_get_boolean(v);
    }
    struct json_object* base;
    if (json_object_object_get_ex(root, "base_color", &base) && json_object_get_type(base) == json_type_array && json_object_array_length(base) >= 3) {
        m.base_color = vec3((float)json_object_get_double(json_object_array_get_idx(base,0)),
                            (float)json_object_get_double(json_object_array_get_idx(base,1)),
                            (float)json_object_get_double(json_object_array_get_idx(base,2)));
    }
    struct json_object* em;
    if (json_object_object_get_ex(root, "emissive", &em) && json_object_get_type(em) == json_type_array && json_object_array_length(em) >= 3) {
        m.emissive = vec3((float)json_object_get_double(json_object_array_get_idx(em,0)),
                          (float)json_object_get_double(json_object_array_get_idx(em,1)),
                          (float)json_object_get_double(json_object_array_get_idx(em,2)));
    }

    *out = m;
    json_object_put(root);
    return true;
}

static int MaterialManagerPresetIdForFilename(const char* filename) {
    if (!filename) return -1;
    if (strcmp(filename, "default.json") == 0) return MATERIAL_PRESET_DEFAULT;
    if (strcmp(filename, "mirror.json") == 0) return MATERIAL_PRESET_MIRROR;
    if (strcmp(filename, "rough_metal.json") == 0) return MATERIAL_PRESET_ROUGH_METAL;
    if (strcmp(filename, "glossy.json") == 0) return MATERIAL_PRESET_GLOSSY;
    if (strcmp(filename, "emissive.json") == 0) return MATERIAL_PRESET_EMISSIVE;
    if (strcmp(filename, "transparent.json") == 0) return MATERIAL_PRESET_TRANSPARENT;
    return -1;
}

void MaterialManagerLoadDir(const char* dirPath) {
    if (!gInit) MaterialManagerInit();
    if (!dirPath) return;
    DIR* dir = opendir(dirPath);
    if (!dir) return;
    struct dirent* ent = NULL;
    MaterialLibrary lib = {0};
    MaterialLibraryInit(&lib);

    while ((ent = readdir(dir)) != NULL) {
        int preset_id = -1;
        if (ent->d_name[0] == '.') continue;
        const char* dot = strrchr(ent->d_name, '.');
        if (!dot || strcmp(dot, ".json") != 0) continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dirPath, ent->d_name);

        Material m = {
            .diffuse = 0.8f, .specular = 0.05f, .reflectivity = 0.1f, .roughness = 0.6f,
            .base_color = vec3(1,1,1), .emissive = vec3(0,0,0), .metallic = 0.0f,
            .transparency = 0.0f, .ior = 1.0f, .absorption_distance = 1.0f, .thin_walled = false
        };
        if (LoadMaterialFromJsonFile(path, &m)) {
            preset_id = MaterialManagerPresetIdForFilename(ent->d_name);
            if (preset_id >= 0 && preset_id < MAX_MATERIALS) {
                lib.materials[preset_id] = m;
            } else {
                MaterialAdd(&lib, m);
            }
        }
    }
    closedir(dir);
    if (lib.count > 0) {
        gMatLib = lib;
        gInit = true;
    }
}
