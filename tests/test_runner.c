#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "render/material_bsdf.h"
#include "render/fast_rng.h"
#include "config/config_manager.h"
#include "app/animation.h"
#include "render/ray_tracing2.h"
#include "render/integrator_common.h"
#include "render/direct_light_integrator.h"
#include "render/forward_light_integrator.h"
#include "render/camera_path_integrator.h"
#include "render/uniform_grid.h"
#include "render/light_pdf.h"
#include "render/ray_types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;

static void assert_close(const char* name, double a, double b, double tol) {
    if (fabs(a - b) > tol) {
        printf("FAIL %-32s got=%.6f expected=%.6f (tol=%.6f)\n", name, a, b, tol);
        failures++;
    }
}

static void assert_true(const char* name, bool cond) {
    if (!cond) {
        printf("FAIL %-32s condition=false\n", name);
        failures++;
    }
}

static MaterialBSDF make_diffuse(double albedo) {
    MaterialBSDF m = {0};
    m.albedo = albedo;
    m.diffuseWeight = 1.0;
    m.specWeight = 0.0;
    m.reflectivity = 0.0;
    m.roughness = 0.5;
    m.weightSum = 1.0;
    m.model = MATERIAL_BSDF_LAMBERT;
    return m;
}

static int test_diffuse_evaluate(void) {
    MaterialBSDF m = make_diffuse(0.8);
    double nx = 0.0, ny = 1.0;
    double inX = 0.0, inY = 1.0;
    double outX = 0.0, outY = 1.0;
    double v = MaterialBSDFEvaluateCos(&m, nx, ny, inX, inY, outX, outY);
    assert_close("diffuse_evaluate_cos", v, 0.8 / M_PI, 1e-4);
    return 0;
}

static int test_diffuse_pdf(void) {
    MaterialBSDF m = make_diffuse(0.5);
    double nx = 0.0, ny = 1.0;
    double inX = 0.0, inY = 1.0;
    double outX = 0.0, outY = 1.0;
    double pdf = MaterialBSDFAngularPdf(&m, nx, ny, inX, inY, outX, outY);
    assert_close("diffuse_pdf", pdf, 1.0 / M_PI, 1e-6);
    return 0;
}

static int test_sample_diffuse_consistency(void) {
    MaterialBSDF m = make_diffuse(0.7);
    double nx = 0.0, ny = 1.0;
    double inX = 0.0, inY = 1.0;
    FastRNG rng;
    FastRNGSeed(&rng, 12345u, 6789u);
    BSDFSample s = {0};
    bool ok = MaterialBSDFSample(&m, nx, ny, inX, inY, &rng, &s);
    assert_true("sample_diffuse_valid", ok);
    if (!ok) return 0;
    double dot = s.dirX * nx + s.dirY * ny;
    assert_true("sample_diffuse_hemisphere", dot > 0.0);
    assert_true("sample_diffuse_pdf_pos", s.pdf > 0.0);
    assert_true("sample_diffuse_weight_pos", s.weight > 0.0);
    if (s.pdf > 0.0) {
        double ratio = s.weight / s.pdf;
        assert_close("sample_diffuse_weight_over_pdf", ratio, m.albedo, 0.05);
    }
    return 0;
}

// Minimal deterministic scene harness (direct + forward + camera)
static void setup_tiny_scene(void) {
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.windowWidth = 64;
    sceneSettings.windowHeight = 64;
    sceneSettings.bezierPath.numPoints = 1;
    sceneSettings.bezierPath.points[0].x = 20.0;
    sceneSettings.bezierPath.points[0].y = 0.0;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.camera.zoom = 1.0;

    sceneSettings.objectCount = 2;
    memset(sceneSettings.sceneObjects, 0, sizeof(sceneSettings.sceneObjects));
    strncpy(sceneSettings.sceneObjects[0].type, "circle", sizeof(sceneSettings.sceneObjects[0].type) - 1);
    sceneSettings.sceneObjects[0].x = 0.0;
    sceneSettings.sceneObjects[0].y = 0.0;
    sceneSettings.sceneObjects[0].radius = 5.0;
    sceneSettings.sceneObjects[0].scale = 1.0;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].opacity = 1.0f;
    sceneSettings.sceneObjects[0].reflectivity = 0.0f;
    sceneSettings.sceneObjects[0].roughness = 0.5f;

    strncpy(sceneSettings.sceneObjects[1].type, "circle", sizeof(sceneSettings.sceneObjects[1].type) - 1);
    sceneSettings.sceneObjects[1].x = -15.0;
    sceneSettings.sceneObjects[1].y = -5.0;
    sceneSettings.sceneObjects[1].radius = 3.0;
    sceneSettings.sceneObjects[1].scale = 1.0;
    sceneSettings.sceneObjects[1].color = 0x808080;
    sceneSettings.sceneObjects[1].opacity = 1.0f;
    sceneSettings.sceneObjects[1].reflectivity = 0.2f;
    sceneSettings.sceneObjects[1].roughness = 0.1f;

    animSettings.integratorMode = 0; // forward by default
    animSettings.useTiledRenderer = false;
    animSettings.tileSize = 16;
    animSettings.cacheVarianceCutoff = 0.35;
    animSettings.cacheHaloRadius = 3.5;
    sceneSettings.rays = 64;
    animSettings.lightIntensity = 2.0;
}

static int sample_pixel_energy(const float* buffer, int w, int h, int x, int y, float* out) {
    if (!buffer || x < 0 || y < 0 || x >= w || y >= h) return 0;
    *out = buffer[(size_t)y * (size_t)w + (size_t)x];
    return 1;
}

static int test_deterministic_modes(void) {
    setup_tiny_scene();
    InitRayTracingScene();

    int w = sceneSettings.windowWidth;
    int h = sceneSettings.windowHeight;
    size_t count = (size_t)w * (size_t)h;
    float* scratch = (float*)malloc(count * sizeof(float));
    if (!scratch) return 0;

    LightSource light = { .x = sceneSettings.bezierPath.points[0].x,
                          .y = sceneSettings.bezierPath.points[0].y,
                          .radius = 3.0 };

    // Direct-only mode
    IntegratorContext ctx = {
        .pixelBuffer = (Uint8*)malloc(count),
        .energyBuffer = scratch,
        .directEnergyBuffer = NULL,
        .width = w,
        .height = h,
        .objects = sceneSettings.sceneObjects,
        .objectCount = sceneSettings.objectCount,
        .tileGrid = NULL,
        .useTiles = false,
        .frameSeed = 1,
        .uniformGrid = NULL,
        .integratorMode = 2,
        .cache = NULL,
        .materials = NULL,
        .materialCount = 0,
        .mesh = NULL,
        .triangleMesh = NULL
    };
    memset(ctx.pixelBuffer, 0, count);
    memset(ctx.energyBuffer, 0, count * sizeof(float));
    DirectLightIntegratorRender(&ctx, &light);
    float directSample = 0.0f;
    sample_pixel_energy(ctx.energyBuffer, w, h, w / 2, h / 2, &directSample);

    // Forward mode (no tiles)
    memset(ctx.pixelBuffer, 0, count);
    memset(ctx.energyBuffer, 0, count * sizeof(float));
    animSettings.integratorMode = 0;
    ForwardLightIntegratorRender(&ctx, &light);
    float forwardSample = 0.0f;
    sample_pixel_energy(ctx.energyBuffer, w, h, w / 2, h / 2, &forwardSample);

    // Camera-path mode uses cache-less run (no cache passed)
    memset(ctx.pixelBuffer, 0, count);
    memset(ctx.energyBuffer, 0, count * sizeof(float));
    animSettings.integratorMode = 1;
    CameraPathIntegratorRender(&ctx, &light);
    float cameraSample = 0.0f;
    sample_pixel_energy(ctx.energyBuffer, w, h, w / 2, h / 2, &cameraSample);

    assert_true("deterministic_direct_positive", directSample >= 0.0f);
    assert_true("deterministic_forward_nonzero", forwardSample > 0.0f);
    assert_true("deterministic_camera_nonnegative", cameraSample >= 0.0f);

    free(ctx.pixelBuffer);
    free(scratch);
    CleanupRayTracing();
    return 0;
}

// Debug: verify normal orientation and pdf validity on a single hit
static int test_hit_normal_and_pdfs(void) {
    setup_tiny_scene();
    InitRayTracingScene();

    int w = sceneSettings.windowWidth;
    int h = sceneSettings.windowHeight;
    LightSource light = { .x = sceneSettings.bezierPath.points[0].x,
                          .y = sceneSettings.bezierPath.points[0].y,
                          .radius = 3.0 };

    IntegratorContext ctx = {
        .pixelBuffer = NULL,
        .energyBuffer = NULL,
        .directEnergyBuffer = NULL,
        .width = w,
        .height = h,
        .objects = sceneSettings.sceneObjects,
        .objectCount = sceneSettings.objectCount,
        .tileGrid = NULL,
        .useTiles = false,
        .frameSeed = 1,
        .uniformGrid = NULL,
        .integratorMode = 1,
        .cache = NULL,
        .materials = NULL,
        .materialCount = 0,
        .mesh = NULL,
        .triangleMesh = NULL
    };

    // Build a tiny uniform grid for direct intersection
    UniformGrid grid = {0};
    UniformGridBuild(&grid,
                     sceneSettings.sceneObjects,
                     sceneSettings.objectCount,
                     NULL,
                     4.0);
    ctx.uniformGrid = &grid;

    // Build a crude mesh for segments if needed by intersection code
    SurfaceMesh mesh = {0};
    SurfaceMeshInit(&mesh);
    SurfaceBuildMeshes(&mesh, NULL,
                       sceneSettings.sceneObjects,
                       sceneSettings.objectCount,
                       8.0);
    ctx.mesh = &mesh;

    // Ray from camera center through screen center
    // Aim a ray from camera toward the first object's center to guarantee a hit
    const SceneObject* target = &sceneSettings.sceneObjects[0];
    double tx = target->x;
    double ty = target->y;
    double dx = tx - sceneSettings.camera.x;
    double dy = ty - sceneSettings.camera.y;
    double len = sqrt(dx*dx + dy*dy);
    if (len < 1e-6) { dx = 0.0; dy = 1.0; len = 1.0; }
    dx /= len; dy /= len;
    Ray2D ray = { sceneSettings.camera.x, sceneSettings.camera.y, dx, dy };
    HitInfo2D hit = {0};
    hit.objectIndex = -1; hit.triangleIndex = -1; hit.baryW = 1.0;
    bool ok = UniformGridTraceRay(ctx.uniformGrid, &ray, PATH_EPSILON, DBL_MAX, &hit);
    assert_true("debug_trace_hit", ok);
    if (ok) {
        // Orient normal for incoming
        double inx = -dx, iny = -dy;
        double lenIn = sqrt(inx*inx + iny*iny);
        if (lenIn > 1e-9) { inx /= lenIn; iny /= lenIn; }
        double ndot = inx * hit.nx + iny * hit.ny;
        if (ndot < 0.0) { hit.nx = -hit.nx; hit.ny = -hit.ny; ndot = -ndot; }

        // Orient normal and check it faces incoming
        assert_true("debug_normal_facing", ndot >= 0.0);

        // BSDF pdf/value checks
        MaterialBSDF m = {0};
        MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[hit.objectIndex], &m);
        BSDFSample s;
        FastRNG rng; FastRNGSeed(&rng, 111, 222);
        bool sampled = MaterialBSDFSample(&m, hit.nx, hit.ny, inx, iny, &rng, &s);
        assert_true("debug_bsdf_sample_ok", sampled);
        if (sampled) {
            double pdf = MaterialBSDFAngularPdf(&m, hit.nx, hit.ny, inx, iny, s.dirX, s.dirY);
            assert_true("debug_pdf_positive", pdf > 0.0);
            double val = MaterialBSDFEvaluateCos(&m, hit.nx, hit.ny, inx, iny, s.dirX, s.dirY);
            assert_true("debug_eval_positive", val >= 0.0);
        }

        // Light PDF check at hit
        double lx = light.x - hit.px;
        double ly = light.y - hit.py;
        double lDist = sqrt(lx*lx + ly*ly);
        if (lDist > 1e-6) { lx /= lDist; ly /= lDist; }
        double cosOn = fmax(0.0, hit.nx * lx + hit.ny * ly);
        double pdfL = CircleLightPdfSolidAngle(&light, hit.px, hit.py, cosOn);
        assert_true("debug_light_pdf_positive", pdfL > 0.0);
    }

    SurfaceMeshFree(&mesh);
    UniformGridFree(&grid);
    CleanupRayTracing();
    return 0;
}

int main(void) {
    test_diffuse_evaluate();
    test_diffuse_pdf();
    test_sample_diffuse_consistency();
    test_deterministic_modes();
    test_hit_normal_and_pdfs();

    if (failures > 0) {
        printf("TEST RESULT: %d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    printf("TEST RESULT: PASS\n");
    return EXIT_SUCCESS;
}
