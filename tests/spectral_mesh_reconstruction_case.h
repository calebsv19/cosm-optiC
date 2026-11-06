/* Independent composite tensor Gauss integral, not the clipped-tetrahedron
 * quadrature used by the implementation. Axis-aligned faces and nodal knots
 * land on this oracle's cell boundaries. */
static RuntimeScene3D mesh_cube(RuntimePrimitive3D *primitive,
                                RuntimeTriangle3D triangles[12]) {
  *primitive =
      (RuntimePrimitive3D){.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH};
  const int face[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1},
                          {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
  Vec3 vertex[8];
  for (int i = 0; i < 8; i++)
    vertex[i] = vec3(i & 1 ? .65 : .45, i & 2 ? .65 : .45, i & 4 ? .65 : .45);
  for (int i = 0; i < 6; i++)
    for (int j = 0; j < 2; j++)
      triangles[2 * i + j] = (RuntimeTriangle3D){.p0 = vertex[face[i][0]],
                                                 .p1 = vertex[face[i][j + 1]],
                                                 .p2 = vertex[face[i][j + 2]],
                                                 .primitiveIndex = 0};
  return (RuntimeScene3D){
      .primitives = primitive,
      .primitiveCount = 1,
      .triangleMesh = {.triangles = triangles, .triangleCount = 12}};
}
static double mesh_integral_oracle(RuntimeSpectralVolume *v) {
  const double q[] = {.1127016653792583, .5, .8872983346207417},
               w[] = {.2777777777777778, .4444444444444444, .2777777777777778};
  double sum = 0, h = .025;
  for (int z = 0; z < 40; z++)
    for (int y = 0; y < 40; y++)
      for (int x = 0; x < 40; x++)
        for (int a = 0; a < 3; a++)
          for (int b = 0; b < 3; b++)
            for (int c = 0; c < 3; c++) {
              Vec3 p = vec3((x + q[a]) * h, (y + q[b]) * h, (z + q[c]) * h);
              sum += sample(v, p) * w[a] * w[b] * w[c] * h * h * h;
            }
  return sum * 4 * M_PI;
}
static void mesh_boundaries(void) {
  RuntimePrimitive3D primitive;
  RuntimeTriangle3D triangles[12];
  RuntimeScene3D s = mesh_cube(&primitive, triangles);
  Vec3 positions[] = {{.449, .53, .53}, {.449, .449, .53}, {.449, .449, .449}};
  for (int mode = 1; mode <= 2; mode++)
    for (int case_id = 0; case_id < 3; case_id++) {
      RuntimeSpectralVolume *v = volume(mode);
      bind(v, &s);
      assert(v->reconstruction->domain.mesh.count == 6);
      event(v, positions[case_id]);
      bool prepared = RuntimeSpectralVolume_PrepareReconstruction(v);
      if (!prepared)
        fprintf(stderr, "mesh prepare: %s\n", RuntimeSpectralVolume_Error(v));
      assert(prepared);
      begin(v);
      double total = mesh_integral_oracle(v);
      printf("mesh mode=%d boundary=%d integral=%.17g relative=%.9g\n", mode,
             case_id, total, fabs(total - 1));
      fflush(stdout);
      assert(fabs(total - 1) < 2e-4);
      /* Exact zero inside the solid and behind the opposite face, even when an
       * adaptive tent spans the entire obstacle. */
      for (int i = 0; i < 20; i++)
        assert(sample(v, vec3(.451 + i * .009, .53, .53)) == 0);
      assert(sample(v, vec3(.651, .53, .53)) == 0);
      assert(sample(v, vec3(.44, .53, .53)) > 0);
      /* Extended supporting planes must not create a discontinuity in the
       * exterior. */
      Vec3 a = {.42, .45 - 1e-9, .53}, b = {.42, .45 + 1e-9, .53};
      double x = sample(v, a), y = sample(v, b);
      assert(fabs(x - y) < 1e-5 * fmax(1, fmax(x, y)));
      RuntimeSpectralVolume_Free(v);
    }
  char diag[192];
  RuntimeSpectralVolume *v = volume(1);
  s.triangleMesh.triangleCount = 11;
  assert(!RuntimeSpectralVolume_BindScene(v, &s, diag, sizeof(diag)));
  assert(strstr(diag, "closed"));
  RuntimeSpectralVolume_Free(v);
  s = mesh_cube(&primitive, triangles);
  for (int i = 0; i < 12; i++) {
    Vec3 *q[] = {&triangles[i].p0, &triangles[i].p1, &triangles[i].p2};
    for (int j = 0; j < 3; j++)
      if (q[j]->x == .65 && q[j]->y == .65 && q[j]->z == .65)
        *q[j] = vec3(.55, .55, .55);
  }
  v = volume(1);
  assert(!RuntimeSpectralVolume_BindScene(v, &s, diag, sizeof(diag)));
  RuntimeSpectralVolume_Free(v);
  puts("PASS mesh faces, edges, corners: independent normalization, zero "
       "solid/opposite-face leakage, exterior continuity and open/nonconvex "
       "refusal");
}
static RuntimeScene3D mesh_prism(RuntimePrimitive3D *primitive,
                                 RuntimeTriangle3D triangles[12]) {
  *primitive =
      (RuntimePrimitive3D){.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH};
  Vec3 p[] = {{.4, .4, .4}, {.6, .4, .4}, {.4, .6, .4},
              {.4, .4, .6}, {.6, .4, .6}, {.4, .6, .6}};
  const int ids[8][3] = {{0, 2, 1}, {3, 4, 5}, {0, 1, 4}, {0, 4, 3},
                         {1, 2, 5}, {1, 5, 4}, {2, 0, 3}, {2, 3, 5}};
  for (int i = 0; i < 8; i++)
    triangles[i] = (RuntimeTriangle3D){.p0 = p[ids[i][0]],
                                       .p1 = p[ids[i][1]],
                                       .p2 = p[ids[i][2]],
                                       .primitiveIndex = 0};
  return (RuntimeScene3D){
      .primitives = primitive,
      .primitiveCount = 1,
      .triangleMesh = {.triangles = triangles, .triangleCount = 8}};
}
static double prism_oracle(RuntimeSpectralVolume *v) {
  /* Independent triangle x interval integration. Each XY cell is bisected
   * along x+y=constant, aligning the oblique prism face without 3D clipping. */
  const double q[] = {.046910077030668, .23076534494715845, .5,
                      .7692346550528415, .953089922969332},
               w[] = {.11846344252809455, .23931433524968325,
                      .28444444444444444, .23931433524968325,
                      .11846344252809455};
  double sum = 0, h = .05;
  for (int z = 0; z < 20; z++)
    for (int y = 0; y < 20; y++)
      for (int x = 0; x < 20; x++)
        for (int half = 0; half < 2; half++)
          for (int a = 0; a < 5; a++)
            for (int b = 0; b < 5; b++)
              for (int c = 0; c < 5; c++) {
                double u = q[a], vv = (1 - u) * q[b];
                if (half) {
                  u = 1 - u;
                  vv = 1 - vv;
                }
                Vec3 p = vec3((x + u) * h, (y + vv) * h, (z + q[c]) * h);
                sum +=
                    sample(v, p) * w[a] * w[b] * w[c] * (1 - q[a]) * h * h * h;
              }
  return sum * 4 * M_PI;
}
static void prism_boundaries(void) {
  RuntimePrimitive3D primitive;
  RuntimeTriangle3D triangles[12];
  RuntimeScene3D scene = mesh_prism(&primitive, triangles);
  Vec3 deposit[] = {{.501, .501, .5}, {.501, .501, .399}, {.601, .399, .399}};
  for (int mode = 1; mode <= 2; mode++)
    for (int k = 0; k < 3; k++) {
      RuntimeSpectralVolume *v = volume(mode);
      bind(v, &scene);
      assert(v->reconstruction->domain.mesh.count == 5);
      event(v, deposit[k]);
      bool ok = RuntimeSpectralVolume_PrepareReconstruction(v);
      if (!ok)
        fprintf(stderr, "prism prepare: %s\n", RuntimeSpectralVolume_Error(v));
      assert(ok);
      begin(v);
      double total = prism_oracle(v);
      printf("prism mode=%d boundary=%d integral=%.17g relative=%.9g\n", mode,
             k, total, fabs(total - 1));
      fflush(stdout);
      assert(fabs(total - 1) < 2e-4);
      for (int j = 0; j < 15; j++)
        assert(sample(v, vec3(.41 + j * .005, .41, .5)) == 0);
      if (k == 0) {
        assert(sample(v, vec3(.399, .5, .5)) == 0);
        assert(sample(v, vec3(.5, .399, .5)) == 0);
      }
      RuntimeSpectralVolume_Free(v);
    }
  puts("PASS prism oblique face, edge and acute corner normalization and solid "
       "leakage");
}
static void mesh_camera_quadrature(void) {
  RuntimePrimitive3D primitive;
  RuntimeTriangle3D triangles[12];
  RuntimeScene3D scene = mesh_cube(&primitive, triangles);
  const double extinction[] = {.01, 1, 100};
  for (int test = 0; test < 3; test++) {
    double sigma = extinction[test];
    RuntimeSpectralVolume *v = volume(2);
    v->config.absorption =
        (RuntimeSpectralCurve){.count = 2, .points = {{380, 0}, {780, 0}}};
    v->config.scattering = (RuntimeSpectralCurve){
        .count = 2, .points = {{380, sigma}, {780, sigma}}};
    bind(v, &scene);
    event(v, vec3(.449, .449, .53));
    Ray3D ray = RuntimeRay3D_Make(vec3(.44, .2, .53), vec3(0, 1, 0));
    RuntimeSpectralPencilLine line = {.vacuum_nm = 550, .band = 0};
    RuntimeSpectralPathReadout out = {.weight = 1};
    bool restart;
    assert(RuntimeSpectralVolume_Segment(v, &line, &ray, .6, true, 0, &out,
                                         &restart));
    double expected = 0;
    const int n = 200000;
    for (int i = 0; i < n; i++) {
      double t = (i + .5) * .6 / n;
      expected += sample(v, vec3(.44, .2 + t, .53)) * exp(-sigma * t) * .6 / n;
    }
    double error = fabs(out.inscattered - expected) / expected;
    printf("mesh camera extinction=%.3g relative=%.9g\n", sigma, error);
    fflush(stdout);
    assert(error < 2e-4);
    RuntimeSpectralVolume_Free(v);
  }
}
