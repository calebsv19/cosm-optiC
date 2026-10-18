static void test_runtime_wet_contact_photon(void) {
  /* Slabs overlap at one seam: water [1,3], glass [0,1.0001].
   * Forward: water -> null glass entry -> real water/glass -> glass exit.
   * Reverse: glass -> real glass/water -> null glass exit -> water exit. */
  for (int reverse = 0; reverse < 2; ++reverse) {
    RuntimeScene3D scene;
    assert_true("wet_photon_build",
                photon_path_transport_build_nested_media(&scene));
    const double z[4] = {3, 1.0001, 1, 0};
    const int ids[4] = {72, 71, 72, 71};
    for (int i = 0; i < 4; ++i) {
      RuntimeTriangle3D *t = &scene.triangleMesh.triangles[i];
      t->p0.z = t->p1.z = t->p2.z = z[i];
      t->sceneObjectIndex = ids[i];
      t->primitiveIndex = ids[i] - 71;
    }
    assert_true("wet_photon_rebuild",
                RuntimeSceneAcceleration3D_RebuildTLASFromScene(&scene));
    PhotonPathTransportNestedMaterialFixture3D f = {0};
    f.glass = photon_path_transport_glass();
    f.water = photon_path_transport_glass();
    f.glass.hasGlassTransportOverride = f.water.hasGlassTransportOverride =
        true;
    f.glass.materialId = 701;
    f.water.materialId = 702;
    f.water.opticalIor = f.water.bsdf.ior = 1.333;
    f.water.wetContactContainerIndexPlusOne = 72;
    f.water.absorptionDistance = 2;
    f.water.hasGlassAbsorptionColorOverride = true;
    f.water.glassAbsorptionColorR = f.water.glassAbsorptionColorG =
        f.water.glassAbsorptionColorB = .5;
    RuntimeCausticPhotonPathTransportSettings3D settings;
    RuntimeCausticPhotonPathTransport3D_DefaultSettings(&settings);
    settings.sceneTrace.maxDepth = 4;
    settings.applyRoulette = false;
    settings.sceneTrace.materialResolver =
        photon_path_transport_nested_material;
    settings.sceneTrace.materialResolverUserData = &f;
    RuntimeCausticPhotonSample3D sample = photon_path_transport_sample();
    sample.position = vec3(0, 0, reverse ? -1 : 4);
    sample.direction = vec3(0, 0, reverse ? 1 : -1);
    RuntimeCausticPhotonSceneTrace3D trace;
    bool found = false;
    for (int i = 0; i < 256; ++i) {
      sample.photonId = 30000 + i;
      sample.sampleIndex = i;
      f.calls = 0;
      if (RuntimeCausticPhotonPathTransport3D_Trace(&scene, &sample, &settings,
                                                    &trace) &&
          trace.readback.termination ==
              RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_ESCAPED &&
          trace.readback.hitEventCount == 4 &&
          trace.trace.dielectricEventCount == 3) {
        found = true;
        break;
      }
    }
    assert_true("wet_photon_three_physical_interfaces", found);
    if (found) {
      int null_index = reverse ? 2 : 1;
      assert_true("wet_photon_null_boundary_recorded",
                  trace.hitEvents[null_index].nullContactBoundary &&
                      !trace.hitEvents[null_index].bsdfSelection.selected);
      assert_close("wet_photon_null_pdf_preserved",
                   trace.hitEvents[null_index].pathPdfAfter,
                   trace.hitEvents[null_index].pathPdfBefore, 1e-12);
      assert_true("wet_photon_null_segment_flux_recorded",
                  trace.hitEvents[null_index].bsdfSelection.throughputBefore.x > 0);
      assert_true("wet_photon_air_after_exit",
                  trace.finalMediumStack.count == 1 &&
                      trace.readback.mediumTransitionFailureCount == 0);
      assert_close("wet_photon_beer_distance",
                   trace.trace.finalState.throughput.x / sample.flux.x, .5,
                   2e-5);
      assert_true("wet_photon_null_does_not_spend_depth",
                  trace.trace.finalState.depth == 3);
    }
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
  }
}

static void test_runtime_wet_contact_photon_full_budget(void) {
  RuntimeScene3D scene;
  assert_true("wet_budget_build",
              photon_path_transport_build_nested_media(&scene));
  scene.triangleMesh.triangleCount = 3;
  const double z[3] = {1, .9999, 0};
  const int ids[3] = {72, 71, 72};
  for (int i = 0; i < 3; ++i) {
    RuntimeTriangle3D *t = &scene.triangleMesh.triangles[i];
    photon_path_transport_set_large_triangle(t, z[i],
                                             vec3(0, 0, i == 0 ? 1 : -1), i);
    t->sceneObjectIndex = ids[i];
    t->primitiveIndex = ids[i] - 71;
  }
  assert_true("wet_budget_rebuild",
              RuntimeSceneAcceleration3D_RebuildTLASFromScene(&scene));
  PhotonPathTransportNestedMaterialFixture3D f = {0};
  f.glass = photon_path_transport_glass();
  f.water = photon_path_transport_glass();
  f.glass.hasGlassTransportOverride = f.water.hasGlassTransportOverride = true;
  f.glass.opticalIor = f.glass.bsdf.ior = 1;
  f.glass.materialId = 701;
  f.water.materialId = 702;
  f.water.wetContactContainerIndexPlusOne = 72;
  RuntimeCausticPhotonPathTransportSettings3D settings;
  RuntimeCausticPhotonPathTransport3D_DefaultSettings(&settings);
  settings.sceneTrace.maxDepth = 8;
  settings.applyRoulette = false;
  settings.sceneTrace.materialResolver = photon_path_transport_nested_material;
  settings.sceneTrace.materialResolverUserData = &f;
  settings.hasInitialMediumStack = true;
  RuntimeCausticPhotonMediumEntry3D entry;
  RuntimeCausticPhotonMediumTransition3D tr;
  RuntimeCausticPhotonMediumEntry3D_FromMaterial(&f.water, 72, 0, &entry);
  RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
      &settings.initialMediumStack, &entry, true, false, &tr);
  RuntimeCausticPhotonSample3D sample = photon_path_transport_sample();
  sample.position = vec3(0, 0, .5);
  sample.direction = vec3(.8, 0, .6);
  RuntimeCausticPhotonSceneTrace3D trace;
  assert_true("wet_budget_trace_succeeds",
              RuntimeCausticPhotonPathTransport3D_Trace(&scene, &sample,
                                                        &settings, &trace));
  assert_true("wet_budget_eight_real_events_plus_nulls",
              trace.readback.hitEventCount > 8 &&
                  trace.trace.dielectricEventCount == 8 &&
                  trace.readback.mediumTransitionFailureCount == 0 &&
                  trace.readback.termination ==
                      RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_MAX_DEPTH);
  RuntimeCausticPhotonPathPopulationSettings3D population;
  RuntimeCausticPhotonPathPopulation3D_DefaultSettings(&population);
  population.storeDiffuseSurfaces = population.storeTraversedBeams = false;
  RuntimeCausticPhotonPathPopulationReadback3D result;
  RuntimeCausticPhotonPathPopulation3D_PopulateMaps(&trace, &population, NULL,
                                                    NULL, &result);
  assert_true("wet_budget_population_accepts_ledger",
              result.termination !=
                  RUNTIME_CAUSTIC_PHOTON_PATH_POPULATION_INVALID_TRACE);
  RuntimeSceneAcceleration3D_ResetTLASForTests();
  RuntimeScene3D_Free(&scene);
}
