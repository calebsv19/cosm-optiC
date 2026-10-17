/* Compare the real accelerated scene route with its independent flat route.
 * The cases exercise signed/near-zero components and changing distance limits. */
static void test_bvh_query_edges(void) {
    enum { count = 256 };
    RuntimeScene3D scene;
    Ray3D rays[count];
    HitInfo3D *flat = calloc(count, sizeof(*flat));
    bool found[count];
    assert_true("query_edges_storage", flat != NULL);
    if (!flat) return;
    assert_true("query_edges_scene", build_test_scene(&scene));
    RuntimeTriangleMesh3D_ClearBVH(&scene.triangleMesh);
    const double component[] = {0.0, -0.0, 1e-13, -1e-13, 1e-12, -1e-12, 1.001e-12, -1.001e-12};
    const double far_limit[] = {4.0, 5.0, 6.0, 7.0, 100.0, DBL_MAX, HUGE_VAL, 6.0 + 1e-9};
    for (int i = 0; i < count; ++i) {
        rays[i] = RuntimeRay3D_Make(vec3(-3.5 + (i % 8), 0.0, 1.0),
                                   vec3(component[(i / 8) % 8], 1.0, component[i % 8]));
        found[i] = RuntimeRay3D_TraceSceneFirstHit(&scene, &rays[i],
                         i % 3 ? 0.0 : 5.5, far_limit[(i / 32) % 8], &flat[i]);
    }
    assert_true("query_edges_bvh", RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh));
    for (int i = 0; i < count; ++i) {
        HitInfo3D hit = {0};
        bool accelerated = RuntimeRay3D_TraceSceneFirstHit(&scene, &rays[i],
                             i % 3 ? 0.0 : 5.5, far_limit[(i / 32) % 8], &hit);
        assert_true("query_edges_hit_state", accelerated == found[i]);
        if (accelerated && found[i]) {
            assert_hit_matches("query_edges_attributes", &flat[i], &hit);
            assert_near("query_edges_exact_t", hit.t, flat[i].t, 0.0);
            assert_near("query_edges_bary_u", hit.baryU, flat[i].baryU, 0.0);
            assert_near("query_edges_bary_v", hit.baryV, flat[i].baryV, 0.0);
            assert_near("query_edges_bary_w", hit.baryW, flat[i].baryW, 0.0);
            assert_near("query_edges_position", vec3_length(vec3_sub(hit.position, flat[i].position)), 0.0, 0.0);
            assert_near("query_edges_normal", vec3_length(vec3_sub(hit.shadingNormal, flat[i].shadingNormal)), 0.0, 0.0);
        }
    }
    RuntimeTriangle3D triangle = scene.triangleMesh.triangles[0];
    Ray3D miss = RuntimeRay3D_Make(vec3(100, 0, 100), vec3(0, 1, 0));
    HitInfo3D untouched, out;
    memset(&untouched, 0x5a, sizeof(untouched)); memcpy(&out, &untouched, sizeof(out));
    assert_true("triangle_miss", !RuntimeRay3D_IntersectTriangle(&miss, &triangle, 0, 0, 100, &out));
    assert_true("triangle_miss_output_untouched", memcmp(&out, &untouched, sizeof(out)) == 0);
    RuntimeScene3D_Free(&scene);
    free(flat);
}
