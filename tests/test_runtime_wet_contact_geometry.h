#include "config/config_manager.h"
#include "material/material_manager.h"
#include "render/runtime_physical_glass_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_triangle_bvh_3d.h"
#include "render/runtime_water_contact_3d.h"
#include "scene/object_manager.h"

static void wet_contact_quad(RuntimeScene3D *s, int axis, int sign,
                             double plane, double a0, double a1, double b0,
                             double b1) {
  int a = (axis + 1) % 3, b = (axis + 2) % 3;
  Vec3 v[4];
  double av[4] = {a0, a1, a1, a0}, bv[4] = {b0, b0, b1, b1};
  for (int i = 0; i < 4; ++i) {
    double c[3];
    c[axis] = plane;
    c[a] = av[i];
    c[b] = bv[i];
    v[i] = vec3(c[0], c[1], c[2]);
  }
  for (int i = 0; i < 2; ++i) {
    RuntimeTriangle3D *t =
        &s->triangleMesh.triangles[s->triangleMesh.triangleCount++];
    t->p0 = v[0];
    t->p1 = v[sign > 0 ? i + 1 : i + 2];
    t->p2 = v[sign > 0 ? i + 2 : i + 1];
    t->normal = vec3_normalize(
        vec3_cross(vec3_sub(t->p1, t->p0), vec3_sub(t->p2, t->p0)));
    t->sceneObjectIndex = 0;
    t->primitiveIndex = 0;
    t->twoSided = true;
  }
}
static void test_runtime_wet_contact_geometry(void) {
  SceneConfig saved = sceneSettings;
  MaterialManagerResetDefaults();
  memset(&sceneSettings, 0, sizeof(sceneSettings));
  sceneSettings.objectCount = 2;
  InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0, 0, 1, 0, NULL,
             0);
  sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
  SceneObjectSeedGlassTransportOverrideFromMaterial(
      &sceneSettings.sceneObjects[0]);
  sceneSettings.sceneObjects[0].glassThinWalled = false;
  sceneSettings.sceneObjects[0].glassTransmission = 1;
  RuntimeScene3D s;
  RuntimeScene3D_Init(&s);
  s.primitiveCount = s.primitiveCapacity = 1;
  s.primitives = calloc(1, sizeof(*s.primitives));
  s.triangleMesh.triangleCapacity = 20;
  s.triangleMesh.triangles = calloc(20, sizeof(*s.triangleMesh.triangles));
  assert_true("wet_geometry_alloc", s.primitives && s.triangleMesh.triangles);
  if (!s.primitives || !s.triangleMesh.triangles) {
    RuntimeScene3D_Free(&s);
    sceneSettings = saved;
    return;
  }
  strcpy(s.primitives[0].source.objectId, "tank");
  s.primitives[0].source.sceneObjectIndex = 0;
  for (int axis = 0; axis < 2; ++axis)
    for (int sign = -1; sign <= 1; sign += 2) {
      if (axis == 0) {
        wet_contact_quad(&s, axis, sign, -sign, -1, 1, 0, 2);
        wet_contact_quad(&s, axis, -sign, -sign * 1.1, -1.1, 1.1, -.1, 2);
      } else {
        wet_contact_quad(&s, axis, sign, -sign, 0, 2, -1, 1);
        wet_contact_quad(&s, axis, -sign, -sign * 1.1, -.1, 2, -1.1, 1.1);
      }
    }
  wet_contact_quad(&s, 2, 1, 0, -1, 1, -1, 1);
  wet_contact_quad(&s, 2, -1, -.1, -1.1, 1.1, -1.1, 1.1);
  float heights[4] = {1, 1.01, 1.02, 1};
  RuntimeWaterBodyMesh3DDesc d = {0};
  d.scene_object_index = 1;
  d.grid_w = d.grid_d = 2;
  d.heights_y = heights;
  d.sample_origin_x = d.sample_origin_z = -.998;
  d.sample_spacing_x = d.sample_spacing_z = 1.996;
  d.bottom_height = .001;
  d.map_y_height_to_scene_z = true;
  RuntimeWaterBodyBoundaryV1 b = {0};
  strcpy(b.container_id, "tank");
  int object;
  char diag[256];
  RuntimeWaterBodyMesh3DDesc original = d;
  assert_true(
      "wet_geometry_separated_unchanged",
      RuntimeWaterContact3D_Prepare(&s, &b, &d, &object, diag, sizeof(diag)) &&
          object == -1 && !memcmp(&d, &original, sizeof(d)));
  b.wet_container_contact = true;
  bool prepared =
      RuntimeWaterContact3D_Prepare(&s, &b, &d, &object, diag, sizeof(diag));
  if (!prepared)
    fprintf(stderr, "wet contact fixture: %s\n", diag);
  assert_true("wet_geometry_valid_contact", prepared && object == 0);
  assert_close("wet_geometry_inner_wall_overlap", d.sample_origin_x, -1.0001,
               1e-12);
  assert_close("wet_geometry_bottom_overlap", d.bottom_height, -.0001, 1e-12);
  assert_close("wet_geometry_height_preserved", d.heights_y[2], heights[2], 0);
  sceneSettings.sceneObjects[0].glassThinWalled = true;
  assert_true(
      "wet_geometry_thin_sheet_refused",
      !RuntimeWaterContact3D_Prepare(&s, &b, &d, &object, diag, sizeof(diag)));
  sceneSettings.sceneObjects[0].glassThinWalled = false;
  d = original;
  d.sample_origin_x = -.9;
  d.sample_spacing_x = 1.8;
  assert_true(
      "wet_geometry_large_gap_refused",
      !RuntimeWaterContact3D_Prepare(&s, &b, &d, &object, diag, sizeof(diag)));
  d = original;
  RuntimeTriangle3D keep = s.triangleMesh.triangles[0];
  s.triangleMesh.triangles[0] = s.triangleMesh.triangles[1];
  assert_true(
      "wet_geometry_duplicate_half_wall_refused",
      !RuntimeWaterContact3D_Prepare(&s, &b, &d, &object, diag, sizeof(diag)));
  s.triangleMesh.triangles[0] = keep;
  RuntimeTriangle3D second = s.triangleMesh.triangles[1];
  s.triangleMesh.triangles[1].p2 = s.triangleMesh.triangles[0].p2;
  assert_true(
      "wet_geometry_overlapping_half_walls_refused",
      !RuntimeWaterContact3D_Prepare(&s, &b, &d, &object, diag, sizeof(diag)));
  s.triangleMesh.triangles[1] = second;
  s.triangleMesh.triangleCount--;
  assert_true(
      "wet_geometry_missing_half_wall_refused",
      !RuntimeWaterContact3D_Prepare(&s, &b, &d, &object, diag, sizeof(diag)));
  RuntimeScene3D_Free(&s);
  sceneSettings = saved;
}

static void test_runtime_wet_contact_camera_null(void) {
  SceneConfig saved = sceneSettings;
  memset(&sceneSettings, 0, sizeof(sceneSettings));
  sceneSettings.objectCount = 2;
  InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 0, 0, 1, 0, NULL,
             0);
  InitObject(&sceneSettings.sceneObjects[1], OBJECT_CIRCLE, 0, 0, 1, 0, NULL,
             0);
  RuntimeScene3D s;
  RuntimeScene3D_Init(&s);
  s.primitiveCount = s.primitiveCapacity = 1;
  s.primitives = calloc(1, sizeof(*s.primitives));
  s.triangleMesh.triangleCount = s.triangleMesh.triangleCapacity = 1;
  s.triangleMesh.triangles = calloc(1, sizeof(*s.triangleMesh.triangles));
  if (!s.primitives || !s.triangleMesh.triangles) {
    assert_true("wet_camera_alloc", false);
    RuntimeScene3D_Free(&s);
    sceneSettings = saved;
    return;
  }
  s.primitives[0].kind = s.primitives[0].source.kind =
      RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
  s.primitives[0].source.sceneObjectIndex = 1;
  RuntimeTriangle3D *t = s.triangleMesh.triangles;
  t->p0 = vec3(-4, -4, 0);
  t->p1 = vec3(4, -4, 0);
  t->p2 = vec3(0, 4, 0);
  t->normal = vec3(0, 0, 1);
  t->twoSided = true;
  t->sceneObjectIndex = 1;
  assert_true("wet_camera_acceleration",
              RuntimeTriangleMesh3D_BuildBVH(&s.triangleMesh) &&
                  RuntimeSceneAcceleration3D_RebuildTLASFromScene(&s));
  RuntimeMaterialPayload3D p = {0};
  p.valid = p.hasGlassTransportOverride = true;
  p.opticalIor = p.bsdf.ior = 1.45;
  p.transparency = .1;
  p.emissive = 10;
  p.baseColorR = p.baseColorG = p.baseColorB = .2;
  p.bsdf.roughness = .5;
  RuntimeMaterialPayload3D water = p;
  water.opticalIor = water.bsdf.ior = 1.333;
  water.materialId = 702;
  water.wetContactContainerIndexPlusOne = 1;
  water.absorptionDistance = 2;
  water.baseColorR = water.baseColorG = water.baseColorB = .5;
  RuntimeCameraMedium3D m;
  RuntimeCameraMedium3D_Init(&m);
  RuntimeCausticPhotonMediumEntry3D entry;
  RuntimeCausticPhotonMediumTransition3D transition;
  RuntimeCausticPhotonMediumEntry3D_FromMaterial(&water, 72, 0, &entry);
  RuntimeCausticPhotonMediumStack3D_ObserveBoundary(&m, &entry, true, false,
                                                    &transition);
  HitInfo3D hit;
  HitInfo3D_Reset(&hit);
  hit.normal = vec3(0, 0, 1);
  hit.position = vec3(0, 0, 1);
  hit.t = 1;
  hit.sceneObjectIndex = 0;
  Ray3D ray = RuntimeRay3D_Make(vec3(0, 0, 2), vec3(0, 0, -1));
  RuntimeNative3DSamplingContext sampling = {123, 0, 1};
  RuntimePhysicalGlass3DContinuation out;
  assert_true("wet_camera_null_advance",
              RuntimePhysicalGlass3D_Advance(&s, &ray, &hit, &p, &sampling, &m,
                                             0, &out));
  assert_true("wet_camera_null_reaches_receiver",
              out.hit && out.hitInfo.sceneObjectIndex == 1 && out.depth == 0 &&
                  out.deterministicStraight);
  assert_close("wet_camera_null_no_tint_or_opacity", out.throughput.x, .5,
               1e-7);
  assert_close("wet_camera_null_no_emission", out.radiance.x, 0, 1e-12);
  assert_true("wet_camera_null_keeps_water",
              RuntimeCausticPhotonMediumStack3D_Top(&out.medium) &&
                  RuntimeCausticPhotonMediumStack3D_Top(&out.medium)
                          ->sceneObjectIndex == 72);
  RuntimeSceneAcceleration3D_ResetTLASForTests();
  RuntimeScene3D_Free(&s);
  sceneSettings = saved;
}

#include "import/water_body_boundary_contract.h"
static void test_runtime_wet_contact_parse(void) {
  FILE *f = fopen(
      "tests/fixtures/water_body_boundary/manifest_dynamic_unified.json", "rb");
  assert_true("wet_parse_fixture", f != NULL);
  if (!f)
    return;
  char text[8192];
  size_t n = fread(text, 1, sizeof(text) - 1, f);
  fclose(f);
  text[n] = 0;
  for (int mode = 0; mode < 5; ++mode) {
    cJSON *root = cJSON_Parse(text);
    cJSON *b = cJSON_GetObjectItem(root, "water_body_boundary_v1");
    if (mode == 1)
      cJSON_AddStringToObject(b, "optical_contact", "separated");
    if (mode == 2)
      cJSON_AddStringToObject(b, "optical_contact", "wet_container");
    if (mode == 3)
      cJSON_AddStringToObject(b, "optical_contact", "automatic");
    if (mode == 4)
      cJSON_AddNumberToObject(b, "optical_contact", 1);
    RuntimeWaterBodyBoundaryV1 out;
    bool closed, dynamic;
    double height, bottom;
    char shell[64], diag[256];
    bool ok = WaterBodyBoundaryContract_Parse(
        root, &out, &closed, &dynamic, &height, &bottom, shell, sizeof(shell),
        diag, sizeof(diag));
    assert_true("wet_parse_explicit_policy", ok == (mode < 3));
    if (ok)
      assert_true("wet_parse_default_separated",
                  out.wet_container_contact == (mode == 2));
    if (mode == 2) {
      cJSON_DeleteItemFromObject(b, "container_inner_bounds_m");
      assert_true("wet_parse_incomplete_contract_refused",
                  !WaterBodyBoundaryContract_Parse(
                      root, &out, &closed, &dynamic, &height, &bottom, shell,
                      sizeof(shell), diag, sizeof(diag)));
    }
    cJSON_Delete(root);
  }
}
