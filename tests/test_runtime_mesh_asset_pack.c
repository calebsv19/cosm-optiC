#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "core_mesh_asset.h"
#include "import/runtime_mesh_asset_pack.h"

static const char* kMrt13PressureAssetPath =
    "tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_256x128.runtime.json";
static const char* kMrt13PackPath =
    "/private/tmp/ray_tracing_mrt13_asset_sphere_256x128.rtmeshpack";

static int g_runtime_mesh_asset_pack_failures = 0;

static void assert_true(const char* name, bool cond) {
    if (!cond) {
        printf("FAIL %-56s condition=false\n", name);
        g_runtime_mesh_asset_pack_failures += 1;
    }
}

static void assert_near(const char* name, double actual, double expected, double epsilon) {
    if (fabs(actual - expected) > epsilon) {
        printf("FAIL %-56s actual=%f expected=%f\n", name, actual, expected);
        g_runtime_mesh_asset_pack_failures += 1;
    }
}

static long long file_size_or_negative(const char* path) {
    struct stat st;
    if (!path || stat(path, &st) != 0) return -1;
    return (long long)st.st_size;
}

static double elapsed_ms(clock_t start, clock_t end) {
    return ((double)(end - start) * 1000.0) / (double)CLOCKS_PER_SEC;
}

static double measure_json_load_ms(const char* path, int repetitions) {
    double total_ms = 0.0;
    for (int i = 0; i < repetitions; ++i) {
        CoreMeshAssetRuntimeDocument document;
        CoreResult result = core_result_ok();
        clock_t start = clock();
        result = core_mesh_asset_runtime_document_load_file(path, &document);
        total_ms += elapsed_ms(start, clock());
        if (result.code != CORE_OK) {
            printf("FAIL %-56s %s\n",
                   "mrt13_json_measure_load",
                   result.message ? result.message : "load failed");
            g_runtime_mesh_asset_pack_failures += 1;
            return total_ms;
        }
        core_mesh_asset_runtime_document_free(&document);
    }
    return repetitions > 0 ? total_ms / (double)repetitions : 0.0;
}

static double measure_pack_read_ms(const char* path, int repetitions) {
    double total_ms = 0.0;
    for (int i = 0; i < repetitions; ++i) {
        CoreMeshAssetRuntimeDocument document;
        char diagnostics[256] = {0};
        clock_t start = clock();
        bool ok = ray_tracing_runtime_mesh_asset_pack_read_file(path,
                                                               &document,
                                                               diagnostics,
                                                               sizeof(diagnostics));
        total_ms += elapsed_ms(start, clock());
        if (!ok) {
            printf("FAIL %-56s %s\n",
                   "mrt13_pack_measure_read",
                   diagnostics[0] ? diagnostics : "read failed");
            g_runtime_mesh_asset_pack_failures += 1;
            return total_ms;
        }
        core_mesh_asset_runtime_document_free(&document);
    }
    return repetitions > 0 ? total_ms / (double)repetitions : 0.0;
}

static void compare_sample_vertex(const CoreMeshAssetRuntimeDocument* a,
                                  const CoreMeshAssetRuntimeDocument* b,
                                  size_t index,
                                  const char* label) {
    char name[128] = {0};
    snprintf(name, sizeof(name), "%s_x", label);
    assert_near(name, b->vertices[index].position.x, a->vertices[index].position.x, 0.0);
    snprintf(name, sizeof(name), "%s_y", label);
    assert_near(name, b->vertices[index].position.y, a->vertices[index].position.y, 0.0);
    snprintf(name, sizeof(name), "%s_z", label);
    assert_near(name, b->vertices[index].position.z, a->vertices[index].position.z, 0.0);
}

static void compare_sample_triangle(const CoreMeshAssetRuntimeDocument* a,
                                    const CoreMeshAssetRuntimeDocument* b,
                                    size_t index,
                                    const char* label) {
    char name[128] = {0};
    snprintf(name, sizeof(name), "%s_a", label);
    assert_true(name, b->triangles[index].a == a->triangles[index].a);
    snprintf(name, sizeof(name), "%s_b", label);
    assert_true(name, b->triangles[index].b == a->triangles[index].b);
    snprintf(name, sizeof(name), "%s_c", label);
    assert_true(name, b->triangles[index].c == a->triangles[index].c);
    snprintf(name, sizeof(name), "%s_group", label);
    assert_true(name,
                strcmp(b->triangles[index].surface_group_id,
                       a->triangles[index].surface_group_id) == 0);
}

static int test_runtime_mesh_asset_pack_pressure_fixture(void) {
    CoreMeshAssetRuntimeDocument json_document;
    CoreMeshAssetRuntimeDocument packed_document;
    CoreResult load_result = core_result_ok();
    CoreResult validate_result = core_result_ok();
    char diagnostics[256] = {0};
    clock_t write_start = 0;
    double pack_write_ms = 0.0;
    double json_load_ms = 0.0;
    double pack_read_ms = 0.0;
    long long json_bytes = 0;
    long long pack_bytes = 0;
    bool ok = false;

    remove(kMrt13PackPath);
    load_result = core_mesh_asset_runtime_document_load_file(kMrt13PressureAssetPath, &json_document);
    assert_true("mrt13_json_pressure_load", load_result.code == CORE_OK);
    if (load_result.code != CORE_OK) {
        printf("mrt13 load diagnostic: %s\n",
               load_result.message ? load_result.message : "load failed");
        return 0;
    }

    write_start = clock();
    ok = ray_tracing_runtime_mesh_asset_pack_write_file(kMrt13PackPath,
                                                       &json_document,
                                                       diagnostics,
                                                       sizeof(diagnostics));
    pack_write_ms = elapsed_ms(write_start, clock());
    assert_true("mrt13_pack_write_pressure", ok);
    if (!ok) {
        printf("mrt13 pack diagnostic: %s\n", diagnostics[0] ? diagnostics : "write failed");
        core_mesh_asset_runtime_document_free(&json_document);
        return 0;
    }

    ok = ray_tracing_runtime_mesh_asset_pack_read_file(kMrt13PackPath,
                                                      &packed_document,
                                                      diagnostics,
                                                      sizeof(diagnostics));
    assert_true("mrt13_pack_read_pressure", ok);
    if (!ok) {
        printf("mrt13 pack diagnostic: %s\n", diagnostics[0] ? diagnostics : "read failed");
        core_mesh_asset_runtime_document_free(&json_document);
        remove(kMrt13PackPath);
        return 0;
    }
    validate_result = core_mesh_asset_runtime_document_validate(&packed_document);
    assert_true("mrt13_pack_validate_pressure", validate_result.code == CORE_OK);

    assert_true("mrt13_pack_asset_id",
                strcmp(packed_document.contract.asset_id, json_document.contract.asset_id) == 0);
    assert_true("mrt13_pack_source_asset_id",
                strcmp(packed_document.contract.source_asset_id,
                       json_document.contract.source_asset_id) == 0);
    assert_true("mrt13_pack_vertex_count",
                packed_document.vertex_count == json_document.vertex_count);
    assert_true("mrt13_pack_triangle_count",
                packed_document.triangle_count == json_document.triangle_count);
    assert_true("mrt13_pack_surface_group_count",
                packed_document.surface_group_count == json_document.surface_group_count);
    assert_true("mrt13_pack_surface_group_id",
                strcmp(packed_document.surface_groups[0].group_id,
                       json_document.surface_groups[0].group_id) == 0);
    compare_sample_vertex(&json_document, &packed_document, 0u, "mrt13_pack_vertex_first");
    compare_sample_vertex(&json_document,
                          &packed_document,
                          json_document.vertex_count / 2u,
                          "mrt13_pack_vertex_mid");
    compare_sample_vertex(&json_document,
                          &packed_document,
                          json_document.vertex_count - 1u,
                          "mrt13_pack_vertex_last");
    compare_sample_triangle(&json_document, &packed_document, 0u, "mrt13_pack_triangle_first");
    compare_sample_triangle(&json_document,
                            &packed_document,
                            json_document.triangle_count / 2u,
                            "mrt13_pack_triangle_mid");
    compare_sample_triangle(&json_document,
                            &packed_document,
                            json_document.triangle_count - 1u,
                            "mrt13_pack_triangle_last");

    json_bytes = file_size_or_negative(kMrt13PressureAssetPath);
    pack_bytes = file_size_or_negative(kMrt13PackPath);
    assert_true("mrt13_json_file_size_positive", json_bytes > 0);
    assert_true("mrt13_pack_file_size_positive", pack_bytes > 0);
    assert_true("mrt13_pack_smaller_than_json", pack_bytes > 0 && json_bytes > pack_bytes);

    json_load_ms = measure_json_load_ms(kMrt13PressureAssetPath, 3);
    pack_read_ms = measure_pack_read_ms(kMrt13PackPath, 3);

    printf("MRT13 packed mesh load measurement asset=%s vertices=%zu triangles=%zu json_bytes=%lld pack_bytes=%lld pack_ratio=%.3f json_load_avg_ms=%.3f pack_read_avg_ms=%.3f pack_write_ms=%.3f\n",
           json_document.contract.asset_id,
           json_document.vertex_count,
           json_document.triangle_count,
           json_bytes,
           pack_bytes,
           json_bytes > 0 ? (double)pack_bytes / (double)json_bytes : 0.0,
           json_load_ms,
           pack_read_ms,
           pack_write_ms);

    core_mesh_asset_runtime_document_free(&packed_document);
    core_mesh_asset_runtime_document_free(&json_document);
    remove(kMrt13PackPath);
    return 0;
}

int run_test_runtime_mesh_asset_pack_tests(void) {
    int before = g_runtime_mesh_asset_pack_failures;
    test_runtime_mesh_asset_pack_pressure_fixture();
    return g_runtime_mesh_asset_pack_failures - before;
}

#ifdef RAY_TRACING_RUNTIME_MESH_ASSET_PACK_STANDALONE
int main(void) {
    return run_test_runtime_mesh_asset_pack_tests() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
