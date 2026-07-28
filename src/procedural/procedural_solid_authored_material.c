#include "procedural/procedural_solid_authored_material.h"

#include "app/ray_tracing_sha256.h"
#include "core_io.h"
#include <json-c/json.h>

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_report(
    ProceduralSolidAuthoredMaterialReport *report,
    ProceduralSolidAuthoredMaterialStatus status,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = status;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(
        report->message, sizeof(report->message), "%s",
        message ? message : "");
}

static bool id_valid(const char *text, size_t capacity) {
    size_t length;
    if (!text || !text[0]) return false;
    length = strlen(text);
    if (length >= capacity) return false;
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char c = (unsigned char)text[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static bool finite_range(double value, double minimum, double maximum) {
    return isfinite(value) && value >= minimum && value <= maximum;
}

static bool texture_kind_supported(const char *kind) {
    static const char *kinds[] = {
        "solid", "rust", "fog", "grime", "oil", "brushed_metal",
        "wood", "brick", "concrete", "stone", "scratches", "edge_wear"};
    if (!kind) return false;
    for (size_t i = 0u; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        if (strcmp(kind, kinds[i]) == 0) return true;
    }
    return false;
}

const char *ProceduralSolidAuthoredMaterialStatus_Name(
    ProceduralSolidAuthoredMaterialStatus status) {
    switch (status) {
        case PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_OK: return "ok";
        case PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_ARGUMENT:
            return "argument";
        case PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_IO: return "io";
        case PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_JSON: return "json";
        case PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_SCHEMA:
            return "schema";
        case PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_IDENTITY:
            return "identity";
        case PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_PARAMETER:
            return "parameter";
        case PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_STALE_BASE:
            return "stale_base";
    }
    return "unknown";
}

void ProceduralSolidAuthoredMaterialV1_Init(
    ProceduralSolidAuthoredMaterialV1 *material) {
    ProceduralSolidAuthoredMaterialSurfaceV1 *surface;
    if (!material) return;
    memset(material, 0, sizeof(*material));
    material->schema_version =
        PROCEDURAL_SOLID_AUTHORED_MATERIAL_SCHEMA_VERSION;
    snprintf(material->template_id, sizeof(material->template_id), "custom");
    surface = &material->surface;
    surface->base_color_r = 0.5;
    surface->base_color_g = 0.5;
    surface->base_color_b = 0.5;
    surface->roughness = 0.6;
    surface->metallic = 0.0;
    surface->reflectivity = 0.08;
    surface->specular = 0.18;
    surface->emission_color_r = 1.0;
    surface->emission_color_g = 1.0;
    surface->emission_color_b = 1.0;
    surface->emission_strength = 0.0;
    surface->transparency = 0.0;
    surface->ior = 1.5;
    surface->texture.enabled = false;
    snprintf(
        surface->texture.kind, sizeof(surface->texture.kind), "solid");
    surface->texture.scale_units = 0.2;
    surface->texture.strength = 0.5;
    surface->texture.coverage = 0.5;
    surface->texture.grain = 0.5;
    surface->texture.edge_softness = 0.5;
    surface->texture.contrast = 0.5;
    surface->texture.flow = 0.5;
    surface->texture.color_depth = 0.5;
    surface->texture.surface_damage = 0.0;
    surface->texture.microdetail_normal_strength = 0.0;
    surface->texture.seed = 1;
}

static void apply_template(
    ProceduralSolidAuthoredMaterialV1 *material,
    const char *template_id) {
    ProceduralSolidAuthoredMaterialSurfaceV1 *s = &material->surface;
    snprintf(material->template_id, sizeof(material->template_id), "%s",
             template_id);
    if (strcmp(template_id, "pitted_concrete") == 0) {
        s->base_color_r = 0.48; s->base_color_g = 0.50; s->base_color_b = 0.52;
        s->roughness = 0.86; s->reflectivity = 0.035; s->specular = 0.10;
        s->texture.enabled = true;
        snprintf(s->texture.kind, sizeof(s->texture.kind), "concrete");
        s->texture.scale_units = 0.08; s->texture.strength = 0.72;
        s->texture.coverage = 0.58; s->texture.grain = 0.78;
        s->texture.edge_softness = 0.32; s->texture.contrast = 0.66;
        s->texture.color_depth = 0.34; s->texture.surface_damage = 0.62;
        s->texture.seed = 104729;
    } else if (strcmp(template_id, "weathered_rock") == 0) {
        s->base_color_r = 0.34; s->base_color_g = 0.31; s->base_color_b = 0.27;
        s->roughness = 0.76; s->reflectivity = 0.06; s->specular = 0.15;
        s->texture.enabled = true;
        snprintf(s->texture.kind, sizeof(s->texture.kind), "stone");
        s->texture.scale_units = 0.22; s->texture.strength = 0.82;
        s->texture.coverage = 0.70; s->texture.grain = 0.70;
        s->texture.edge_softness = 0.46; s->texture.contrast = 0.72;
        s->texture.color_depth = 0.52; s->texture.surface_damage = 0.48;
        s->texture.seed = 130363;
    } else if (strcmp(template_id, "wind_sand") == 0) {
        s->base_color_r = 0.72; s->base_color_g = 0.58; s->base_color_b = 0.34;
        s->roughness = 0.68; s->reflectivity = 0.04; s->specular = 0.11;
        s->texture.enabled = true;
        snprintf(s->texture.kind, sizeof(s->texture.kind), "stone");
        s->texture.scale_units = 0.12; s->texture.strength = 0.46;
        s->texture.coverage = 0.42; s->texture.grain = 0.84;
        s->texture.edge_softness = 0.70; s->texture.contrast = 0.38;
        s->texture.flow = 0.82; s->texture.color_depth = 0.24;
        s->texture.seed = 155921;
    } else if (strcmp(template_id, "snow") == 0) {
        s->base_color_r = 0.90; s->base_color_g = 0.94; s->base_color_b = 0.98;
        s->roughness = 0.74; s->reflectivity = 0.16; s->specular = 0.30;
        s->texture.enabled = true;
        snprintf(s->texture.kind, sizeof(s->texture.kind), "fog");
        s->texture.scale_units = 0.06; s->texture.strength = 0.26;
        s->texture.coverage = 0.64; s->texture.grain = 0.66;
        s->texture.edge_softness = 0.78; s->texture.contrast = 0.20;
        s->texture.seed = 196613;
    } else if (strcmp(template_id, "polished_stone") == 0) {
        s->base_color_r = 0.18; s->base_color_g = 0.25; s->base_color_b = 0.30;
        s->roughness = 0.20; s->reflectivity = 0.48; s->specular = 0.62;
        s->texture.enabled = true;
        snprintf(s->texture.kind, sizeof(s->texture.kind), "stone");
        s->texture.scale_units = 0.30; s->texture.strength = 0.32;
        s->texture.coverage = 0.54; s->texture.grain = 0.36;
        s->texture.edge_softness = 0.64; s->texture.contrast = 0.42;
        s->texture.color_depth = 0.40; s->texture.seed = 228017;
    } else if (strcmp(template_id, "emissive_crystal") == 0) {
        s->base_color_r = 0.18; s->base_color_g = 0.52; s->base_color_b = 0.74;
        s->roughness = 0.16; s->reflectivity = 0.38; s->specular = 0.72;
        s->emission_color_r = 0.12; s->emission_color_g = 0.62;
        s->emission_color_b = 1.0; s->emission_strength = 2.4;
        s->texture.enabled = true;
        snprintf(s->texture.kind, sizeof(s->texture.kind), "edge_wear");
        s->texture.scale_units = 0.18; s->texture.strength = 0.48;
        s->texture.coverage = 0.52; s->texture.grain = 0.46;
        s->texture.edge_softness = 0.38; s->texture.contrast = 0.74;
        s->texture.color_depth = 0.62; s->texture.seed = 263167;
    }
}

bool ProceduralSolidAuthoredMaterialV1_FromTemplate(
    const char *template_id,
    const char *material_id,
    ProceduralSolidAuthoredMaterialV1 *out_material,
    ProceduralSolidAuthoredMaterialReport *report) {
    static const char *templates[] = {
        "pitted_concrete", "weathered_rock", "wind_sand", "snow",
        "polished_stone", "emissive_crystal"};
    bool supported = false;
    ProceduralSolidAuthoredMaterialV1 material;
    if (!template_id || !material_id || !out_material) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_ARGUMENT,
                   "template", "template, material id, and output are required");
        return false;
    }
    for (size_t i = 0u; i < sizeof(templates) / sizeof(templates[0]); ++i) {
        if (strcmp(template_id, templates[i]) == 0) supported = true;
    }
    if (!supported) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_PARAMETER,
                   "template_id", "unsupported material template");
        return false;
    }
    ProceduralSolidAuthoredMaterialV1_Init(&material);
    snprintf(material.material_id, sizeof(material.material_id), "%s",
             material_id);
    apply_template(&material, template_id);
    if (!ProceduralSolidAuthoredMaterialV1_Validate(&material, report)) {
        return false;
    }
    *out_material = material;
    return true;
}

bool ProceduralSolidAuthoredMaterialV1_Validate(
    const ProceduralSolidAuthoredMaterialV1 *material,
    ProceduralSolidAuthoredMaterialReport *report) {
    const ProceduralSolidAuthoredMaterialSurfaceV1 *s;
    const ProceduralSolidAuthoredTextureV1 *t;
    set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_OK, "", "ok");
    if (!material) return false;
    if (material->schema_version !=
            PROCEDURAL_SOLID_AUTHORED_MATERIAL_SCHEMA_VERSION) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_SCHEMA,
                   "schema_version", "unsupported authored material schema");
        return false;
    }
    if (!id_valid(material->material_id, sizeof(material->material_id)) ||
        !id_valid(material->template_id, sizeof(material->template_id))) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_IDENTITY,
                   "material_id", "material and template ids must be stable ids");
        return false;
    }
    s = &material->surface;
    if (!finite_range(s->base_color_r, 0.0, 1.0) ||
        !finite_range(s->base_color_g, 0.0, 1.0) ||
        !finite_range(s->base_color_b, 0.0, 1.0) ||
        !finite_range(s->roughness, 0.02, 1.0) ||
        !finite_range(s->metallic, 0.0, 1.0) ||
        !finite_range(s->reflectivity, 0.0, 1.0) ||
        !finite_range(s->specular, 0.0, 1.0) ||
        !finite_range(s->emission_color_r, 0.0, 1.0) ||
        !finite_range(s->emission_color_g, 0.0, 1.0) ||
        !finite_range(s->emission_color_b, 0.0, 1.0) ||
        !finite_range(s->emission_strength, 0.0, 100.0) ||
        !finite_range(s->transparency, 0.0, 1.0) ||
        !finite_range(s->ior, 1.0, 3.0)) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_PARAMETER,
                   "surface", "surface parameter is outside its typed range");
        return false;
    }
    t = &s->texture;
    if (!id_valid(t->kind, sizeof(t->kind)) ||
        !texture_kind_supported(t->kind) ||
        !finite_range(t->scale_units, 0.001, 1000.0) ||
        !finite_range(t->strength, 0.0, 1.0) ||
        !finite_range(t->coverage, 0.0, 1.0) ||
        !finite_range(t->grain, 0.0, 1.0) ||
        !finite_range(t->edge_softness, 0.0, 1.0) ||
        !finite_range(t->contrast, 0.0, 1.0) ||
        !finite_range(t->flow, 0.0, 1.0) ||
        !finite_range(t->color_depth, 0.0, 1.0) ||
        !finite_range(t->surface_damage, 0.0, 1.0) ||
        !finite_range(t->microdetail_normal_strength, 0.0, 1.0)) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_PARAMETER,
                   "texture", "texture node parameter is invalid");
        return false;
    }
    return true;
}

static json_object *material_json(
    const ProceduralSolidAuthoredMaterialV1 *material) {
    const ProceduralSolidAuthoredMaterialSurfaceV1 *s = &material->surface;
    const ProceduralSolidAuthoredTextureV1 *t = &s->texture;
    json_object *root = json_object_new_object();
    json_object *surface = json_object_new_object();
    json_object *color = json_object_new_object();
    json_object *emission = json_object_new_object();
    json_object *emission_color = json_object_new_object();
    json_object *texture = json_object_new_object();
    if (!root || !surface || !color || !emission || !emission_color ||
        !texture) {
        if (root) json_object_put(root);
        return NULL;
    }
    json_object_object_add(root, "schema",
        json_object_new_string(PROCEDURAL_SOLID_AUTHORED_MATERIAL_SCHEMA));
    json_object_object_add(root, "schema_version",
        json_object_new_int(PROCEDURAL_SOLID_AUTHORED_MATERIAL_SCHEMA_VERSION));
    json_object_object_add(root, "material_id",
        json_object_new_string(material->material_id));
    json_object_object_add(root, "template_id",
        json_object_new_string(material->template_id));
    json_object_object_add(color, "r", json_object_new_double(s->base_color_r));
    json_object_object_add(color, "g", json_object_new_double(s->base_color_g));
    json_object_object_add(color, "b", json_object_new_double(s->base_color_b));
    json_object_object_add(surface, "base_color", color);
    json_object_object_add(surface, "roughness", json_object_new_double(s->roughness));
    json_object_object_add(surface, "metallic", json_object_new_double(s->metallic));
    json_object_object_add(surface, "reflectivity", json_object_new_double(s->reflectivity));
    json_object_object_add(surface, "specular", json_object_new_double(s->specular));
    json_object_object_add(emission_color, "r", json_object_new_double(s->emission_color_r));
    json_object_object_add(emission_color, "g", json_object_new_double(s->emission_color_g));
    json_object_object_add(emission_color, "b", json_object_new_double(s->emission_color_b));
    json_object_object_add(emission, "color", emission_color);
    json_object_object_add(emission, "strength", json_object_new_double(s->emission_strength));
    json_object_object_add(surface, "emission", emission);
    json_object_object_add(surface, "transparency", json_object_new_double(s->transparency));
    json_object_object_add(surface, "ior", json_object_new_double(s->ior));
    json_object_object_add(texture, "enabled", json_object_new_boolean(t->enabled));
    json_object_object_add(texture, "kind", json_object_new_string(t->kind));
    json_object_object_add(texture, "scale_units", json_object_new_double(t->scale_units));
    json_object_object_add(texture, "strength", json_object_new_double(t->strength));
    json_object_object_add(texture, "coverage", json_object_new_double(t->coverage));
    json_object_object_add(texture, "grain", json_object_new_double(t->grain));
    json_object_object_add(texture, "edge_softness", json_object_new_double(t->edge_softness));
    json_object_object_add(texture, "contrast", json_object_new_double(t->contrast));
    json_object_object_add(texture, "flow", json_object_new_double(t->flow));
    json_object_object_add(texture, "color_depth", json_object_new_double(t->color_depth));
    json_object_object_add(texture, "surface_damage", json_object_new_double(t->surface_damage));
    if (t->microdetail_normal_strength > 0.0) {
        json_object_object_add(
            texture,
            "microdetail_normal_strength",
            json_object_new_double(t->microdetail_normal_strength));
    }
    json_object_object_add(texture, "seed", json_object_new_int(t->seed));
    json_object_object_add(surface, "texture_node", texture);
    json_object_object_add(root, "surface", surface);
    return root;
}

bool ProceduralSolidAuthoredMaterialV1_Digest(
    const ProceduralSolidAuthoredMaterialV1 *material,
    char out_digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY],
    ProceduralSolidAuthoredMaterialReport *report) {
    json_object *root;
    const char *text;
    bool ok;
    if (!out_digest ||
        !ProceduralSolidAuthoredMaterialV1_Validate(material, report)) {
        return false;
    }
    root = material_json(material);
    if (!root) return false;
    text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    ok = ray_tracing_sha256_bytes(text, strlen(text), out_digest);
    json_object_put(root);
    if (!ok) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_IO,
                   "digest", "unable to hash authored material");
        return false;
    }
    if (report) snprintf(report->material_digest_sha256,
                         sizeof(report->material_digest_sha256), "%s",
                         out_digest);
    return true;
}

bool ProceduralSolidAuthoredMaterialV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralSolidAuthoredMaterialV1 *material,
    ProceduralSolidAuthoredMaterialReport *report) {
    json_object *root;
    const char *text;
    CoreResult result;
    char digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
    if (!path || !ProceduralSolidAuthoredMaterialV1_Digest(
                     material, digest, report)) return false;
    root = material_json(material);
    if (!root) return false;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    json_object_put(root);
    if (result.code != CORE_OK) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_IO,
                   "path", result.message);
        return false;
    }
    if (report) snprintf(report->material_digest_sha256,
                         sizeof(report->material_digest_sha256), "%s", digest);
    return true;
}

static bool get_object(json_object *root, const char *key, json_object **out) {
    return json_object_object_get_ex(root, key, out) &&
           json_object_get_type(*out) == json_type_object;
}

static bool get_text(
    json_object *root, const char *key, char *out, size_t capacity) {
    json_object *value = NULL;
    const char *text;
    if (!json_object_object_get_ex(root, key, &value) ||
        json_object_get_type(value) != json_type_string) return false;
    text = json_object_get_string(value);
    if (!text || strlen(text) >= capacity) return false;
    snprintf(out, capacity, "%s", text);
    return true;
}

static bool get_double(json_object *root, const char *key, double *out) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(root, key, &value) ||
        (json_object_get_type(value) != json_type_double &&
         json_object_get_type(value) != json_type_int)) return false;
    *out = json_object_get_double(value);
    return true;
}

bool ProceduralSolidAuthoredMaterialV1_LoadJsonFile(
    const char *path,
    ProceduralSolidAuthoredMaterialV1 *out_material,
    ProceduralSolidAuthoredMaterialReport *report) {
    json_object *root = NULL, *value = NULL, *surface = NULL, *color = NULL;
    json_object *emission = NULL, *emission_color = NULL, *texture = NULL;
    ProceduralSolidAuthoredMaterialV1 material;
    if (!path || !out_material) return false;
    root = json_object_from_file(path);
    if (!root || json_object_get_type(root) != json_type_object) {
        if (root) json_object_put(root);
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_IO,
                   "path", "unable to parse authored material JSON");
        return false;
    }
    ProceduralSolidAuthoredMaterialV1_Init(&material);
    if (!json_object_object_get_ex(root, "schema", &value) ||
        json_object_get_type(value) != json_type_string ||
        strcmp(json_object_get_string(value),
               PROCEDURAL_SOLID_AUTHORED_MATERIAL_SCHEMA) != 0 ||
        !json_object_object_get_ex(root, "schema_version", &value) ||
        json_object_get_int64(value) !=
            PROCEDURAL_SOLID_AUTHORED_MATERIAL_SCHEMA_VERSION ||
        !get_text(root, "material_id", material.material_id,
                  sizeof(material.material_id)) ||
        !get_text(root, "template_id", material.template_id,
                  sizeof(material.template_id)) ||
        !get_object(root, "surface", &surface) ||
        !get_object(surface, "base_color", &color) ||
        !get_double(color, "r", &material.surface.base_color_r) ||
        !get_double(color, "g", &material.surface.base_color_g) ||
        !get_double(color, "b", &material.surface.base_color_b) ||
        !get_double(surface, "roughness", &material.surface.roughness) ||
        !get_double(surface, "metallic", &material.surface.metallic) ||
        !get_double(surface, "reflectivity", &material.surface.reflectivity) ||
        !get_double(surface, "specular", &material.surface.specular) ||
        !get_object(surface, "emission", &emission) ||
        !get_object(emission, "color", &emission_color) ||
        !get_double(emission_color, "r", &material.surface.emission_color_r) ||
        !get_double(emission_color, "g", &material.surface.emission_color_g) ||
        !get_double(emission_color, "b", &material.surface.emission_color_b) ||
        !get_double(emission, "strength", &material.surface.emission_strength) ||
        !get_double(surface, "transparency", &material.surface.transparency) ||
        !get_double(surface, "ior", &material.surface.ior) ||
        !get_object(surface, "texture_node", &texture) ||
        !get_text(texture, "kind", material.surface.texture.kind,
                  sizeof(material.surface.texture.kind)) ||
        !get_double(texture, "scale_units",
                    &material.surface.texture.scale_units) ||
        !get_double(texture, "strength", &material.surface.texture.strength) ||
        !get_double(texture, "coverage", &material.surface.texture.coverage) ||
        !get_double(texture, "grain", &material.surface.texture.grain) ||
        !get_double(texture, "edge_softness",
                    &material.surface.texture.edge_softness) ||
        !get_double(texture, "contrast", &material.surface.texture.contrast) ||
        !get_double(texture, "flow", &material.surface.texture.flow) ||
        !get_double(texture, "color_depth",
                    &material.surface.texture.color_depth) ||
        !get_double(texture, "surface_damage",
                    &material.surface.texture.surface_damage) ||
        !json_object_object_get_ex(texture, "enabled", &value) ||
        json_object_get_type(value) != json_type_boolean) {
        json_object_put(root);
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_JSON,
                   "material", "authored material JSON is incomplete");
        return false;
    }
    material.surface.texture.enabled = json_object_get_boolean(value);
    if (json_object_object_get_ex(texture, "microdetail_normal_strength", &value)) {
        if ((json_object_get_type(value) != json_type_double &&
             json_object_get_type(value) != json_type_int)) {
            json_object_put(root);
            set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_JSON,
                       "texture.microdetail_normal_strength",
                       "microdetail normal strength must be numeric");
            return false;
        }
        material.surface.texture.microdetail_normal_strength =
            json_object_get_double(value);
    }
    if (!json_object_object_get_ex(texture, "seed", &value) ||
        json_object_get_type(value) != json_type_int) {
        json_object_put(root);
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_JSON,
                   "texture.seed", "texture seed is required");
        return false;
    }
    material.surface.texture.seed = json_object_get_int(value);
    json_object_put(root);
    if (!ProceduralSolidAuthoredMaterialV1_Validate(&material, report)) {
        return false;
    }
    *out_material = material;
    return true;
}

static bool parse_number(const char *text, double *out) {
    char *end = NULL;
    double value;
    if (!text || !out) return false;
    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || !end || *end != '\0' ||
        !isfinite(value)) return false;
    *out = value;
    return true;
}

bool ProceduralSolidAuthoredMaterialV1_SetParameter(
    const ProceduralSolidAuthoredMaterialV1 *base,
    const char *expected_base_digest,
    const char *parameter_id,
    const char *value,
    ProceduralSolidAuthoredMaterialV1 *out_material,
    ProceduralSolidAuthoredMaterialReport *report) {
    ProceduralSolidAuthoredMaterialV1 edited;
    char digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
    double number = 0.0;
    if (!base || !expected_base_digest || !parameter_id || !value ||
        !out_material ||
        !ProceduralSolidAuthoredMaterialV1_Digest(base, digest, report)) {
        return false;
    }
    if (strcmp(digest, expected_base_digest) != 0) {
        set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_STALE_BASE,
                   "expected_base_digest", "authored material changed");
        return false;
    }
    edited = *base;
    if (strcmp(parameter_id, "texture.kind") == 0) {
        snprintf(edited.surface.texture.kind,
                 sizeof(edited.surface.texture.kind), "%s", value);
    } else if (strcmp(parameter_id, "texture.enabled") == 0) {
        if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            edited.surface.texture.enabled = true;
        } else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            edited.surface.texture.enabled = false;
        } else {
            set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_PARAMETER,
                       parameter_id, "boolean value must be true or false");
            return false;
        }
    } else {
        if (!parse_number(value, &number)) {
            set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_PARAMETER,
                       parameter_id, "numeric parameter value is invalid");
            return false;
        }
#define SET_PARAM(name, member) \
        if (strcmp(parameter_id, name) == 0) edited.surface.member = number
        SET_PARAM("base_color.r", base_color_r);
        else SET_PARAM("base_color.g", base_color_g);
        else SET_PARAM("base_color.b", base_color_b);
        else SET_PARAM("roughness", roughness);
        else SET_PARAM("metallic", metallic);
        else SET_PARAM("reflectivity", reflectivity);
        else SET_PARAM("specular", specular);
        else SET_PARAM("emission.color.r", emission_color_r);
        else SET_PARAM("emission.color.g", emission_color_g);
        else SET_PARAM("emission.color.b", emission_color_b);
        else SET_PARAM("emission.strength", emission_strength);
        else SET_PARAM("transparency", transparency);
        else SET_PARAM("ior", ior);
        else SET_PARAM("texture.scale_units", texture.scale_units);
        else SET_PARAM("texture.strength", texture.strength);
        else SET_PARAM("texture.coverage", texture.coverage);
        else SET_PARAM("texture.grain", texture.grain);
        else SET_PARAM("texture.edge_softness", texture.edge_softness);
        else SET_PARAM("texture.contrast", texture.contrast);
        else SET_PARAM("texture.flow", texture.flow);
        else SET_PARAM("texture.color_depth", texture.color_depth);
        else SET_PARAM("texture.surface_damage", texture.surface_damage);
        else SET_PARAM("texture.microdetail_normal_strength",
                       texture.microdetail_normal_strength);
        else if (strcmp(parameter_id, "texture.seed") == 0) {
            edited.surface.texture.seed = (int)number;
            if ((double)edited.surface.texture.seed != number) {
                set_report(report,
                           PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_PARAMETER,
                           parameter_id, "texture seed must be an integer");
                return false;
            }
        } else {
            set_report(report, PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_PARAMETER,
                       parameter_id, "unknown authored material parameter");
            return false;
        }
#undef SET_PARAM
    }
    snprintf(edited.template_id, sizeof(edited.template_id), "custom");
    if (!ProceduralSolidAuthoredMaterialV1_Validate(&edited, report)) {
        return false;
    }
    *out_material = edited;
    return true;
}
