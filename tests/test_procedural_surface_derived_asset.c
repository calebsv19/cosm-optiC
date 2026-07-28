#include "procedural/procedural_surface_derived_asset.h"

#include <json-c/json.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;

static void expect_true(const char *label, bool value) {
    if (value) return;
    fprintf(stderr, "FAIL: %s\n", label);
    failures += 1;
}

static ProceduralSurfaceDerivedAssetManifest make_manifest(void) {
    ProceduralSurfaceDerivedAssetManifest manifest;
    ProceduralSurfaceDerivedAssetReport report;
    memset(&manifest, 0, sizeof(manifest));
    manifest.schema_version =
        PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA_VERSION_V1;
    snprintf(manifest.asset_id, sizeof(manifest.asset_id), "asset_psg5");
    snprintf(manifest.source_asset_id, sizeof(manifest.source_asset_id),
             "cage_psg5");
    snprintf(manifest.recipe_path, sizeof(manifest.recipe_path), "recipe.json");
    snprintf(manifest.mesh_path, sizeof(manifest.mesh_path),
             "asset_psg5.runtime.json");
    snprintf(manifest.material_path, sizeof(manifest.material_path),
             "asset_psg5.material.json");
    manifest.quality = PROCEDURAL_SURFACE_PLANE_QUALITY_PREVIEW;
    manifest.cage_kind = PROCEDURAL_SURFACE_CAGE_RECTANGULAR_PRISM;
    manifest.cage_width_units = 4.0;
    manifest.cage_height_units = 3.0;
    manifest.cage_depth_units = 2.0;
    snprintf(manifest.recipe_digest_sha256,
             sizeof(manifest.recipe_digest_sha256),
             "563d838258da20c7c9a106323470fedc642558d90154ce62f25f2a006fe99525");
    snprintf(manifest.shell_digest_sha256,
             sizeof(manifest.shell_digest_sha256),
             "f6fd32de40f0e0ceccfde8d70678cdd076acaba23d5b9510b69a23702e9f7a1f");
    snprintf(manifest.material_digest_sha256,
             sizeof(manifest.material_digest_sha256),
             "694ca67f570cb52c7b5009b24922914c415ea50830c624884f79ba94b96583bd");
    snprintf(manifest.collision_owner, sizeof(manifest.collision_owner),
             "semantic_cage");
    expect_true(
        "cage digest",
        ProceduralSurfaceDerivedAsset_CageDigest(
            manifest.cage_kind, manifest.cage_width_units,
            manifest.cage_height_units, manifest.cage_depth_units,
            manifest.cage_digest_sha256, &report));
    expect_true(
        "cache identity",
        ProceduralSurfaceDerivedAsset_CacheIdentity(
            manifest.recipe_digest_sha256, manifest.cage_digest_sha256,
            manifest.quality, manifest.shell_digest_sha256,
            manifest.material_digest_sha256,
            manifest.cache_identity_sha256, &report));
    return manifest;
}

static int write_material_fixture(
    const char *path,
    const ProceduralSurfaceDerivedAssetManifest *manifest) {
    json_object *root = json_object_new_object();
    json_object *vertices = json_object_new_array();
    json_object *triangles = json_object_new_array();
    if (!root || !vertices || !triangles) return 0;
    for (int i = 0; i < 3; ++i) {
        json_object *vertex = json_object_new_object();
        json_object *color = json_object_new_array();
        json_object_array_add(color, json_object_new_double(0.2 + (0.1 * i)));
        json_object_array_add(color, json_object_new_double(0.3 + (0.1 * i)));
        json_object_array_add(color, json_object_new_double(0.4 + (0.1 * i)));
        json_object_object_add(vertex, "color", color);
        json_object_object_add(vertex, "roughness",
                               json_object_new_double(0.7 + (0.05 * i)));
        json_object_object_add(vertex, "snow_likelihood",
                               json_object_new_double(0.1 * i));
        json_object_array_add(vertices, vertex);
    }
    {
        json_object *triangle = json_object_new_array();
        json_object_array_add(triangle, json_object_new_int(0));
        json_object_array_add(triangle, json_object_new_int(1));
        json_object_array_add(triangle, json_object_new_int(2));
        json_object_array_add(triangles, triangle);
    }
    json_object_object_add(root, "schema_version",
                           json_object_new_string(
                               "procedural_surface_material_artifact_v1"));
    json_object_object_add(root, "recipe_digest_sha256",
                           json_object_new_string(
                               manifest->recipe_digest_sha256));
    json_object_object_add(root, "shell_digest_sha256",
                           json_object_new_string(
                               manifest->shell_digest_sha256));
    json_object_object_add(root, "material_digest_sha256",
                           json_object_new_string(
                               manifest->material_digest_sha256));
    json_object_object_add(root, "vertices", vertices);
    json_object_object_add(root, "triangles", triangles);
    if (json_object_to_file_ext(
            path, root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED) != 0) {
        json_object_put(root);
        return 0;
    }
    json_object_put(root);
    return 1;
}

int main(void) {
    char temp_root[256];
    char manifest_path[512];
    char material_path[512];
    ProceduralSurfaceDerivedAssetManifest manifest = make_manifest();
    ProceduralSurfaceDerivedAssetManifest loaded;
    ProceduralSurfaceDerivedAssetManifest sentinel;
    ProceduralSurfaceDerivedAssetMaterial material;
    ProceduralSurfaceDerivedAssetReport report;
    char original_identity[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];

    snprintf(temp_root, sizeof(temp_root),
             "/tmp/ray_tracing_psg5_contract_%ld", (long)getpid());
    expect_true("temporary directory",
                mkdir(temp_root, 0700) == 0);
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", temp_root);
    snprintf(material_path, sizeof(material_path), "%s/material.json", temp_root);
    expect_true(
        "manifest validates",
        ProceduralSurfaceDerivedAssetManifest_Validate(&manifest, &report));
    expect_true(
        "manifest saves",
        ProceduralSurfaceDerivedAssetManifest_SaveJsonFile(
            manifest_path, &manifest, &report));
    memset(&loaded, 0, sizeof(loaded));
    expect_true(
        "manifest reloads",
        ProceduralSurfaceDerivedAssetManifest_LoadJsonFile(
            manifest_path, &loaded, &report));
    expect_true(
        "manifest round trip identity",
        strcmp(loaded.cache_identity_sha256,
               manifest.cache_identity_sha256) == 0);
    expect_true("material fixture writes",
                write_material_fixture(material_path, &manifest));
    ProceduralSurfaceDerivedAssetMaterial_Init(&material);
    expect_true(
        "material reloads",
        ProceduralSurfaceDerivedAssetMaterial_LoadJsonFile(
            material_path, &manifest, 3u, 1u, &material, &report));
    expect_true("material topology retained",
                material.valid && material.vertex_count == 3u &&
                material.triangle_count == 1u &&
                material.triangle_indices[2] == 2u);
    ProceduralSurfaceDerivedAssetMaterial_Free(&material);

    snprintf(original_identity, sizeof(original_identity), "%s",
             manifest.cache_identity_sha256);
    manifest.cage_width_units = 5.0;
    expect_true(
        "stale cage rejected",
        !ProceduralSurfaceDerivedAssetManifest_Validate(&manifest, &report) &&
        report.status == PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_STALE);
    sentinel = loaded;
    expect_true(
        "invalid load remains transactional",
        !ProceduralSurfaceDerivedAssetManifest_LoadJsonFile(
            "/tmp/does-not-exist-psg5.json", &sentinel, &report) &&
        memcmp(&sentinel, &loaded, sizeof(loaded)) == 0);
    if (strcmp(original_identity,
               "60e315e7db7616692572d6f664500893c97c9c8e1ec963844125c28cb238f470") !=
        0) {
        fprintf(stderr, "identity observed: %s\n", original_identity);
        expect_true("identity frozen", false);
    }
    if (failures == 0) {
        printf("procedural surface derived asset contract passed "
               "(cache_identity=%s)\n",
               original_identity);
    }
    return failures == 0 ? 0 : 1;
}
