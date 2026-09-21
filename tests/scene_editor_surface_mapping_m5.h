#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_surface_sampling.h"

static void surface_mapping_m5_probe(SceneEditor *editor) {
    const char *kind = getenv("OPTIC_M5_CASE");
    assert(kind);
    bool variance = !strcmp(kind, "normal_variance"), height = !strcmp(kind, "height"),
         zero = !strcmp(kind, "normal_zero"), linear = !strcmp(kind, "linear"),
         alpha = !strcmp(kind, "alpha");
    assert(RuntimeSurfaceSamplingActive(0));
    CoreAuthoredSurfaceMapping map;
    assert(RuntimeSurfaceMappingDefinition(0, &map));
    RuntimeScene3D scene;
    RuntimeScene3D_Init(&scene);
    assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    const CoreMeshPreviewLodMesh *lod = SceneEditorMeshPreviewStoreGetForQuality(0, true);
    fprintf(stderr, "M5 LOD: %p protected=%d preview=%zu runtime=%d\n", (void *)lod,
            lod ? lod->attribute_protected : 0, lod ? lod->triangle_count : 0,
            scene.triangleMesh.triangleCount);
    assert(lod && lod->attribute_protected &&
           lod->triangle_count == (size_t)scene.triangleMesh.triangleCount);
    const double coherence = hypot(1, 205) / sqrt(129. * 129 + 1 + 205. * 205);
    double far_rough = sqrt(.5 + (variance ? 1 - coherence : 0));
    double max_error = 0, filtered_variation = 0, point_variation = 0;
    size_t samples = 0, frames = 0, degenerate = 0;
    HitInfo3D first = {0};
    for (int j = 0; j < scene.triangleMesh.triangleCount;
         j += scene.triangleMesh.triangleCount > 100 ? 31 : 1) {
        const RuntimeTriangle3D *tri = &scene.triangleMesh.triangles[j];
        const double w[3] = {.23, .31, .46};
        Vec3 point = vec3_add(vec3_add(vec3_scale(tri->p0, w[0]), vec3_scale(tri->p1, w[1])),
                              vec3_scale(tri->p2, w[2]));
        Vec3 n = vec3_normalize(vec3_cross(vec3_sub(tri->p1, tri->p0), vec3_sub(tri->p2, tri->p0)));
        Ray3D ray = RuntimeRay3D_Make(vec3_add(point, vec3_scale(n, 2)), vec3_scale(n, -1));
        HitInfo3D hit;
        assert(RuntimeRay3D_TraceSceneFirstHit(&scene, &ray, 1e-5, 4, &hit));
        if (j == 0)
            first = hit;
        hit.hasPixelFootprint = true;
        hit.pixelDpDx = vec3_scale(hit.surfaceDpDu, .3);
        hit.pixelDpDy = vec3_scale(hit.surfaceDpDv, .0001);
        RuntimeMaterialPayload3D p;
        assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit, &p));
        RuntimeMaterialSurfaceEval expected = RuntimeMaterialSurfaceEvalMakeBase(
                                       p.baseColorR, p.baseColorG, p.baseColorB, p.bsdf.roughness,
                                       p.bsdf.reflectivity, p.bsdf.specWeight, p.bsdf.diffuseWeight,
                                       p.transparency),
                                   preview;
        assert(RuntimeSurfaceMaterialSampleMeshFootprint(0, 0, tri->localTriangleIndex, w, point,
                                                         hit.shadingNormal, lod, &hit.pixelDpDx,
                                                         &hit.pixelDpDy, &preview));
        max_error = fmax(max_error, m4_compare(&preview, &expected));
        assert(preview.linearColor);
        double color = linear ? 128. / 255 : .21586050011389926;
        if (!alpha && strcmp(kind, "procedural"))
            assert(fabs(p.baseColorR - color) < 1e-6);
        else if (alpha) {
            RuntimeMaterialPayload3D base;
            assert(RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &base));
            HitInfo3D average = hit;
            average.footprintUnbounded = true;
            RuntimeMaterialPayload3D ap;
            assert(RuntimeMaterialPayload3D_ResolveFromHit(&average, &ap));
            assert(fabs(ap.baseColorR - (.5 * color + .5 * base.baseColorR)) < 1e-6);
            assert(fabs(ap.baseColorG - (.5 * color + .5 * base.baseColorG)) < 1e-6);
        }
        if (hit.hasSurfaceDifferentials) {
            frames++;
            assert(fabs(p.bsdf.roughness - far_rough) < 1e-5);
            if (!height) {
                /* Independent chart frame: inverse UV rotation/scaling, then
                 * Gram-Schmidt around the interpolated shading normal. */
                double co = cos(map.rotation_rad), si = sin(map.rotation_rad);
                Vec3 u = vec3_sub(vec3_scale(hit.surfaceDpDu, co / map.uv_scale[0]),
                                  vec3_scale(hit.surfaceDpDv, si / map.uv_scale[1]));
                Vec3 v = vec3_add(vec3_scale(hit.surfaceDpDu, si / map.uv_scale[0]),
                                  vec3_scale(hit.surfaceDpDv, co / map.uv_scale[1]));
                Vec3 t = vec3_normalize(
                    vec3_sub(u, vec3_scale(hit.shadingNormal, vec3_dot(u, hit.shadingNormal))));
                double sign = vec3_dot(vec3_cross(hit.shadingNormal, t), v) < 0 ? -1 : 1;
                Vec3 b = vec3_scale(vec3_cross(hit.shadingNormal, t), sign);
                Vec3 oracle = vec3_normalize(vec3_add(
                    vec3_add(vec3_scale(t, zero || variance ? 0 : 129. / 255),
                             vec3_scale(b, zero       ? 0
                                           : variance ? 1. / 255
                                                      : 65. / 255)),
                    vec3_scale(hit.shadingNormal, zero || variance ? 205. / 255 : 221. / 255)));
                assert(p.hasMicrodetailNormal && preview.worldNormalActive);
                assert(vec3_length(vec3_sub(p.microdetailShadingNormal, oracle)) < 1e-6);
                Vec3 vp =
                    vec3(preview.worldNormal[0], preview.worldNormal[1], preview.worldNormal[2]);
                assert(vec3_length(vec3_sub(vp, oracle)) < 1e-6);
                HitInfo3D applied = hit;
                assert(RuntimeMaterialPayload3D_ApplyShadingNormal(&p, &applied));
                RuntimeMaterialPayload3D again;
                assert(RuntimeMaterialPayload3D_ResolveFromHit(&applied, &again));
                assert(vec3_length(vec3_sub(again.microdetailShadingNormal, oracle)) < 1e-6);
                /* Directional illumination must follow chart U, including mirrors. */
                if (!zero && !variance)
                    assert(vec3_dot(applied.shadingNormal, t) > .45);
            }
        } else {
            degenerate++;
            assert(!p.hasMicrodetailNormal);
        }
        samples++;
    }
    assert(frames > 0);
    /* Camera pixel differentials and the unbounded grazing/secondary policy. */
    Vec3 n = first.geometricNormal, u = vec3_normalize(first.surfaceDpDu),
         v = vec3_normalize(vec3_cross(n, u));
    RuntimeCameraProjector3D projector = {.viewportWidth = 128,
                                          .viewportHeight = 128,
                                          .origin = vec3_add(first.position, vec3_scale(n, 2)),
                                          .forward = vec3_scale(n, -1),
                                          .right = u,
                                          .up = v,
                                          .nearPlane = .001,
                                          .tanHalfFovX = .5,
                                          .tanHalfFovY = .5};
    Ray3D camera = RuntimeCameraProjector3D_MakePrimaryRay(&projector, 63.5, 63.5);
    HitInfo3D camera_hit;
    assert(camera.hasDifferentials &&
           RuntimeRay3D_TraceSceneFirstHit(&scene, &camera, .001, 4, &camera_hit));
    assert(camera_hit.hasPixelFootprint && vec3_length(camera_hit.pixelDpDx) > 0);
    camera.directionDx = vec3_scale(camera.direction, -1);
    assert(RuntimeRay3D_TraceSceneFirstHit(&scene, &camera, .001, 4, &camera_hit) &&
           camera_hit.footprintUnbounded);
    RuntimeMaterialPayload3D p;
    assert(RuntimeMaterialPayload3D_ResolveFromHit(&camera_hit, &p));
    assert(fabs(p.bsdf.roughness - far_rough) < 1e-5);
    Ray3D secondary =
        RuntimeRay3D_MakeOffset(vec3_add(first.position, n), n, vec3_scale(n, -1), .0001);
    assert(RuntimeRay3D_TraceSceneFirstHit(&scene, &secondary, .001, 4, &camera_hit) &&
           camera_hit.footprintUnbounded);
    /* Move sub-texel phases through a one-axis grazing footprint. */
    double last_filtered = 0, last_point = 0;
    for (int i = 0; i < 128; ++i) {
        HitInfo3D h = first;
        h.surfaceUV[0] += (i + .31) * .0037;
        h.surfaceUV[1] += .0091;
        assert(RuntimeMaterialPayload3D_ResolveFromHit(&h, &p));
        double point = p.bsdf.roughness;
        h.hasPixelFootprint = true;
        h.pixelDpDx = vec3_scale(h.surfaceDpDu, .3);
        h.pixelDpDy = vec3_scale(h.surfaceDpDv, .0001);
        assert(RuntimeMaterialPayload3D_ResolveFromHit(&h, &p));
        if (i) {
            point_variation += fabs(point - last_point);
            filtered_variation += fabs(p.bsdf.roughness - last_filtered);
        }
        last_point = point;
        last_filtered = p.bsdf.roughness;
    }
    assert(point_variation > .1 && filtered_variation < point_variation * .01);
    if (!strcmp(kind, "procedural")) {
        double pf = 0, pp = 0, last_color_f = 0, last_color_p = 0;
        for (int i = 0; i < 128; ++i) {
            HitInfo3D h = first;
            h.surfaceUV[0] += (i + .31) * .017;
            h.surfaceUV[1] += .123;
            assert(RuntimeMaterialPayload3D_ResolveFromHit(&h, &p));
            double point_color = p.baseColorR;
            h.footprintUnbounded = true;
            assert(RuntimeMaterialPayload3D_ResolveFromHit(&h, &p));
            if (i) {
                pf += fabs(p.baseColorR - last_color_f);
                pp += fabs(point_color - last_color_p);
            }
            last_color_f = p.baseColorR;
            last_color_p = point_color;
        }
        assert(pp > .01 && pf < pp * .01);
    }
    if (height) {
        HitInfo3D h = first;
        h.surfaceUV[0] = .2;
        h.surfaceUV[1] = .2;
        assert(RuntimeMaterialPayload3D_ResolveFromHit(&h, &p) && p.hasMicrodetailNormal);
        assert(vec3_dot(p.microdetailShadingNormal, vec3_normalize(h.surfaceDpDu)) < -.001);
    }
    char diagnostic[512];
    ObjectEditorSetSelectedObjectIndex(0);
    assert(SceneEditorFrameViewport(true));
    choose_menu(editor, 3, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    m2_control(editor, "expand");
    m2_edit(editor, "offset_u", "0.23");
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    assert(SceneEditorDocumentRedo(diagnostic, sizeof(diagnostic)));
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    assert(SceneEditorRuntimeScenePersistAuthoring(diagnostic, sizeof(diagnostic)));
    choose_menu(editor, 3, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    assert(SceneEditorMeshPreviewModeGet() == SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor, "m5-material.ppm");
    m2_control(editor, "expand");
    ObjectEditorSetSelectedObjectIndex(-1);
    SceneEditorDigestOverlayNavState nav = *SceneEditorGetViewportNavState();
    unsigned long long builds = RuntimeSurfaceSamplingBuildCount();
    Uint64 start = SDL_GetPerformanceCounter();
    for (int i = 0; i < 12; ++i) {
        SceneEditorDigestOverlayNavState orbit = nav;
        orbit.orbit_yaw_deg += i * 3;
        SceneEditorRestoreViewportNav(&orbit);
        SceneEditorSessionRuntimeRender(editor);
    }
    double ms = (SDL_GetPerformanceCounter() - start) * 1000. / SDL_GetPerformanceFrequency() / 12;
    assert(ms < 250 && RuntimeSurfaceSamplingBuildCount() == builds);
    capture(editor, "m5-orbit.ppm");
    nav.overlay_zoom *= .25;
    nav.orbit_pitch_deg = 80;
    SceneEditorRestoreViewportNav(&nav);
    capture(editor, "m5-distant.ppm");
    assert(RuntimeSurfaceSamplingBuildCount() == builds);
    FILE *f = fopen("mapping_m5.json", "w");
    assert(f);
    fprintf(f,
            "{\"samples\":%zu,\"valid_frames\":%zu,\"degenerate_frames\":%zu,\"triangles\":%d,"
            "\"max_material_error\":%.9g,\"filtered_variation\":%.9g,\"point_variation\":%.9g,"
            "\"orbit_ms\":%.6f,\"orbit_rebuilds\":0,\"prepared_bytes\":%zu}\n",
            samples, frames, degenerate, scene.triangleMesh.triangleCount, max_error,
            filtered_variation, point_variation, ms, RuntimeSurfaceSamplingPreparedBytes());
    fclose(f);
    RuntimeScene3D_Free(&scene);
}

static void surface_sampling_curved_m5_probe(SceneEditor *editor) {
    CoreAuthoredSurfaceMapping map;
    assert(RuntimeSurfaceMappingDefinition(0, &map));
    assert(map.version == 1 || map.version == 2);
    RuntimeScene3D scene;
    RuntimeScene3D_Init(&scene);
    assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    size_t samples = 0;
    double max_normal_error = 0, filtered_variation = 0, point_variation = 0, last_f = 0,
           last_p = 0;
    for (int i = 0; i < 80; ++i) {
        double angle = .21 + i * .019, z = .13 + i * .0037;
        Vec3 position = map.version == 2 ? vec3(3 * cos(angle), 3 * sin(angle), z)
                                         : vec3(-1.4 + i * .031, -.17, 2);
        Vec3 direction = map.version == 2 ? vec3(-cos(angle), -sin(angle), 0) : vec3(0, 0, -1);
        Ray3D ray = RuntimeRay3D_Make(position, direction);
        HitInfo3D hit;
        assert(RuntimeRay3D_TraceSceneFirstHit(&scene, &ray, .0001, 6, &hit));
        RuntimeMaterialPayload3D p;
        assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit, &p));
        double point = p.bsdf.roughness;
        double a = atan2(hit.position.y, hit.position.x);
        Vec3 u = map.version == 2 ? vec3(-sin(a), cos(a), 0) : vec3(1, 0, 0);
        Vec3 v = map.version == 2 ? vec3(0, 0, 1) : vec3(0, 1, 0);
        hit.hasPixelFootprint = true;
        hit.pixelDpDx = vec3_scale(u, 2);
        hit.pixelDpDy = vec3_scale(v, .01);
        assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit, &p) && p.hasMicrodetailNormal);
        assert(fabs(p.baseColorR - .21586050011389926) < 1e-6 &&
               fabs(p.bsdf.roughness - sqrt(.5)) < 1e-5);
        Vec3 t = vec3_normalize(vec3_cross(v, hit.shadingNormal));
        if (vec3_dot(t, u) < 0)
            t = vec3_scale(t, -1);
        double sign = vec3_dot(vec3_cross(hit.shadingNormal, t), v) < 0 ? -1 : 1;
        Vec3 b = vec3_scale(vec3_cross(hit.shadingNormal, t), sign);
        Vec3 oracle =
            vec3_normalize(vec3_add(vec3_add(vec3_scale(t, 129. / 255), vec3_scale(b, 65. / 255)),
                                    vec3_scale(hit.shadingNormal, 221. / 255)));
        double error = vec3_length(vec3_sub(oracle, p.microdetailShadingNormal));
        max_normal_error = fmax(error, max_normal_error);
        assert(error < 2e-5);
        if (i) {
            point_variation += fabs(point - last_p);
            filtered_variation += fabs(p.bsdf.roughness - last_f);
        }
        last_p = point;
        last_f = p.bsdf.roughness;
        samples++;
    }
    assert(point_variation > .1 && filtered_variation < point_variation * .01);
    if (map.version == 2) {
        HitInfo3D pole;
        HitInfo3D_Reset(&pole);
        pole.sceneObjectIndex = 0;
        pole.position = vec3(0, 0, 1);
        pole.shadingNormal = pole.normal = pole.geometricNormal = vec3(0, 0, 1);
        RuntimeMaterialPayload3D p, base;
        assert(RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0, &base));
        assert(RuntimeMaterialPayload3D_ResolveFromHit(&pole, &p));
        assert(fabs(p.baseColorR - base.baseColorR) < 1e-12 && !p.hasMicrodetailNormal);
    }
    ObjectEditorSetSelectedObjectIndex(0);
    assert(SceneEditorFrameViewport(true));
    choose_menu(editor, 3, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor, "m5-material.ppm");
    ObjectEditorSetSelectedObjectIndex(-1);
    SceneEditorDigestOverlayNavState nav = *SceneEditorGetViewportNavState();
    unsigned long long builds = RuntimeSurfaceSamplingBuildCount();
    Uint64 start = SDL_GetPerformanceCounter();
    for (int i = 0; i < 12; ++i) {
        SceneEditorDigestOverlayNavState orbit = nav;
        orbit.orbit_yaw_deg += i * 3;
        SceneEditorRestoreViewportNav(&orbit);
        SceneEditorSessionRuntimeRender(editor);
    }
    double ms = (SDL_GetPerformanceCounter() - start) * 1000. / SDL_GetPerformanceFrequency() / 12;
    assert(ms < 250 && RuntimeSurfaceSamplingBuildCount() == builds);
    nav.overlay_zoom *= .25;
    nav.orbit_pitch_deg = 80;
    SceneEditorRestoreViewportNav(&nav);
    capture(editor, "m5-distant.ppm");
    char diagnostic[512];
    assert(SceneEditorRuntimeScenePersistAuthoring(diagnostic, sizeof(diagnostic)));
    FILE *f = fopen("mapping_m5.json", "w");
    assert(f);
    fprintf(f,
            "{\"samples\":%zu,\"max_normal_error\":%.9g,\"filtered_variation\":%.9g,\"point_"
            "variation\":%.9g,\"orbit_ms\":%.6f,\"orbit_rebuilds\":0}\n",
            samples, max_normal_error, filtered_variation, point_variation, ms);
    fclose(f);
    RuntimeScene3D_Free(&scene);
}

static void surface_sampling_ray_motion_m5_probe(void) {
    RuntimeScene3D scene;
    RuntimeScene3D_Init(&scene);
    assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    const RuntimeTriangle3D *tri = &scene.triangleMesh.triangles[0];
    Vec3 center = vec3_scale(vec3_add(vec3_add(tri->p0, tri->p1), tri->p2), 1. / 3);
    Vec3 n = vec3_normalize(vec3_cross(vec3_sub(tri->p1, tri->p0), vec3_sub(tri->p2, tri->p0)));
    Vec3 u = vec3_normalize(vec3_sub(tri->p1, tri->p0)), v = vec3_cross(n, u);
    double variations[2][2] = {{0}};
    int counts[2] = {0};
    for (int grazing = 0; grazing < 2; ++grazing) {
        double last_f = 0, last_p = 0;
        for (int i = 0; i < 64; ++i) {
            Vec3 target = vec3_add(center, vec3_scale(u, (i - 32) * .0023));
            Vec3 origin = vec3_add(target, grazing ? vec3_sub(vec3_scale(n, .02), vec3_scale(u, 2))
                                                   : vec3_scale(n, 50));
            Vec3 forward = vec3_normalize(vec3_sub(target, origin)),
                 right = vec3_normalize(vec3_cross(forward, v));
            RuntimeCameraProjector3D projector = {.viewportWidth = 128,
                                                  .viewportHeight = 128,
                                                  .origin = origin,
                                                  .forward = forward,
                                                  .right = right,
                                                  .up = v,
                                                  .nearPlane = .001,
                                                  .tanHalfFovX = .5,
                                                  .tanHalfFovY = .5};
            Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(&projector, 63.5, 63.5);
            HitInfo3D hit;
            assert(RuntimeRay3D_TraceSceneFirstHit(&scene, &ray, .001, 100, &hit));
            assert(hit.hasPixelFootprint || hit.footprintUnbounded);
            RuntimeMaterialPayload3D p;
            assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit, &p));
            double filtered = p.bsdf.roughness;
            hit.hasPixelFootprint = hit.footprintUnbounded = false;
            assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit, &p));
            if (i) {
                variations[grazing][0] += fabs(filtered - last_f);
                variations[grazing][1] += fabs(p.bsdf.roughness - last_p);
            }
            last_f = filtered;
            last_p = p.bsdf.roughness;
            counts[grazing]++;
        }
        assert(variations[grazing][1] > .1 &&
               variations[grazing][0] < variations[grazing][1] * .01);
    }
    FILE *f = fopen("mapping_m5_ray_motion.json", "w");
    assert(f);
    fprintf(f,
            "{\"far_rays\":%d,\"grazing_rays\":%d,\"far_filtered_variation\":%.12g,\"far_point_"
            "variation\":%.12g,\"grazing_filtered_variation\":%.12g,\"grazing_point_variation\":%."
            "12g}\n",
            counts[0], counts[1], variations[0][0], variations[0][1], variations[1][0],
            variations[1][1]);
    fclose(f);
    RuntimeScene3D_Free(&scene);
}
