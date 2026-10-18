#include "render/runtime_camera_dielectric_3d.h"
#include "render/runtime_physical_glass_3d.h"

static void test_runtime_wet_contact_medium(void) {
  RuntimeMaterialPayload3D water = photon_medium_material(702, 1.333, 8);
  RuntimeMaterialPayload3D glass = photon_medium_material(701, 1.45, 2);
  water.hasGlassTransportOverride = glass.hasGlassTransportOverride = true;
  water.transparency = glass.transparency = 1;
  water.wetContactContainerIndexPlusOne = 72; /* object 71 */
  RuntimeCausticPhotonMediumEntry3D w, g,
      other = photon_medium_entry(703, 73, 1.5);
  RuntimeCausticPhotonMediumEntry3D_FromMaterial(&water, 72, 0, &w);
  RuntimeCausticPhotonMediumEntry3D_FromMaterial(&glass, 71, 0, &g);
  RuntimeCameraMedium3D m;
  RuntimeCausticPhotonMediumTransition3D t;
  double from, to;
  RuntimeCameraMedium3D_Init(&m);
  RuntimeCausticPhotonMediumStack3D_ObserveBoundary(&m, &w, true, false, &t);
  assert_true("wet_contact_water_air_control",
              RuntimeCausticPhotonMediumStack3D_ResolveInterface(&m, &w, false,
                                                                 &from, &to) &&
                  from == 1.333 && to == 1);
  RuntimeCausticPhotonMediumStack3D_ResolveInterface(&m, &g, true, &from, &to);
  assert_true("wet_contact_enter_container_null", from == 1.333 && to == 1.333);
  assert_true("wet_contact_insert_container",
              RuntimeCausticPhotonMediumStack3D_ObserveBoundary(&m, &g, true,
                                                                false, &t) &&
                  m.count == 3 && m.entries[1].sceneObjectIndex == 71 &&
                  m.entries[2].sceneObjectIndex == 72);
  assert_true("wet_contact_duplicate_container_not_null",
              !RuntimeCausticPhotonMediumStack3D_IsNullContact(&m, &g, true));
  assert_true("wet_contact_unrelated_exit_refused",
              !RuntimeCausticPhotonMediumStack3D_ResolveInterface(
                  &m, &other, false, &from, &to));
  RuntimeCausticPhotonMediumStack3D_ResolveInterface(&m, &w, false, &from, &to);
  assert_true("wet_contact_water_glass_pair", from == 1.333 && to == 1.45);
  HitInfo3D hit;
  HitInfo3D_Reset(&hit);
  hit.sceneObjectIndex = 72;
  hit.normal = vec3(0, 0, 1);
  Vec3 incoming = vec3(sqrt(1 - .523138 * .523138), 0, .523138);
  RuntimeDielectricTransport3D d;
  assert_true(
      "wet_contact_camera_resolves",
      RuntimeCameraDielectric3D_Resolve(&m, &water, &hit, incoming, &d));
  assert_true("wet_contact_no_spurious_water_air_tir",
              !d.totalInternalReflection && d.hasRefraction);
  assert_close("wet_contact_fresnel", d.fresnel, .00908870, 1e-6);
  RuntimeCausticPhotonMediumStack3D saved = m;
  RuntimeCausticPhotonMediumStack3D_ObserveBoundary(&m, &w, false, true, &t);
  assert_true("wet_contact_reflection_keeps_membership",
              m.count == saved.count && m.entries[2].sceneObjectIndex == 72);
  assert_true("wet_contact_camera_transmits",
              RuntimeCameraMedium3D_Transmit(&m, &water, &hit, &d) &&
                  m.count == 2 && m.entries[1].sceneObjectIndex == 71);
  RuntimeCausticPhotonMediumStack3D_ResolveInterface(&m, &w, true, &from, &to);
  assert_true("wet_contact_reverse_glass_water_pair",
              from == 1.45 && to == 1.333);
  RuntimeCausticPhotonMediumStack3D_ObserveBoundary(&m, &w, true, false, &t);
  RuntimeCausticPhotonMediumStack3D_ResolveInterface(&m, &g, false, &from, &to);
  assert_true("wet_contact_exit_container_null", from == 1.333 && to == 1.333);
  RuntimeCausticPhotonMediumStack3D_ObserveBoundary(&m, &g, false, false, &t);
  assert_true("wet_contact_remove_underlay",
              m.count == 2 && m.entries[1].sceneObjectIndex == 72);
  RuntimeCausticPhotonMediumStack3D_ResolveInterface(&m, &w, false, &from, &to);
  assert_true("wet_contact_exposed_top_still_air", to == 1);
  assert_true(
      "wet_contact_air_control_tir",
      RuntimeCameraDielectric3D_Resolve(&m, &water, &hit, incoming, &d) &&
          d.totalInternalReflection);
}
