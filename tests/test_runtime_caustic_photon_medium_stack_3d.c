#include "test_runtime_caustic_photon_medium_stack_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_medium_stack_3d.h"
#include "test_support.h"

static RuntimeMaterialPayload3D photon_medium_material(int material_id,
                                                       double ior,
                                                       double absorption_distance) {
    RuntimeMaterialPayload3D material;
    RuntimeMaterialPayload3D_Reset(&material);
    material.valid = true;
    material.materialId = material_id;
    material.opticalIor = ior;
    material.bsdf.ior = ior;
    material.baseColorR = 0.7;
    material.baseColorG = 0.8;
    material.baseColorB = 0.9;
    material.absorptionDistance = absorption_distance;
    return material;
}

static RuntimeCausticPhotonMediumEntry3D photon_medium_entry(int material_id,
                                                             int object_id,
                                                             double ior) {
    RuntimeMaterialPayload3D material =
        photon_medium_material(material_id, ior, 3.5);
    RuntimeCausticPhotonMediumEntry3D entry;
    memset(&entry, 0, sizeof(entry));
    RuntimeCausticPhotonMediumEntry3D_FromMaterial(
        &material, object_id, 0.25, &entry);
    return entry;
}

static int test_runtime_caustic_photon_medium_stack_initial_air(void) {
    RuntimeCausticPhotonMediumStack3D stack;
    const RuntimeCausticPhotonMediumEntry3D* top;
    RuntimeCausticPhotonMediumStack3D_Init(&stack);
    top = RuntimeCausticPhotonMediumStack3D_Top(&stack);
    assert_true("runtime_caustic_photon_medium_stack_initial_air",
                top && top->valid && top->isAir && top->ior == 1.0 &&
                    top->sceneObjectIndex == -1 && top->materialId == -1 &&
                    stack.count == 1u &&
                    RuntimeCausticPhotonMediumStack3D_Depth(&stack) == 0u);
    return 0;
}

static int test_runtime_caustic_photon_medium_stack_material_entry(void) {
    RuntimeMaterialPayload3D material = photon_medium_material(17, 1.42, 6.0);
    RuntimeCausticPhotonMediumEntry3D entry;
    assert_true("runtime_caustic_photon_medium_stack_material_entry_build",
                RuntimeCausticPhotonMediumEntry3D_FromMaterial(
                    &material, 91, 0.4, &entry));
    assert_true("runtime_caustic_photon_medium_stack_material_entry_fields",
                entry.valid && !entry.isAir && entry.mediumId == 17 &&
                    entry.sceneObjectIndex == 91 && entry.materialId == 17 &&
                    entry.ior == 1.42 && entry.absorptionColor.x == 0.7 &&
                    entry.absorptionColor.y == 0.8 &&
                    entry.absorptionColor.z == 0.9 &&
                    entry.absorptionDistance == 6.0 && entry.density == 0.4);
    return 0;
}

static int test_runtime_caustic_photon_medium_segment_transmittance(void) {
    RuntimeCausticPhotonMediumEntry3D medium =
        photon_medium_entry(18, 92, 1.42);
    RuntimeCausticPhotonMediumStack3D stack;
    Vec3 transmittance;
    medium.absorptionColor = vec3(0.25, 0.5, 1.0);
    medium.absorptionDistance = 2.0;
    assert_true("runtime_caustic_photon_medium_segment_beer",
                RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
                    &medium, 1.0, &transmittance));
    assert_close("runtime_caustic_photon_medium_segment_beer_r",
                 transmittance.x,
                 0.5,
                 1.0e-12);
    assert_close("runtime_caustic_photon_medium_segment_beer_g",
                 transmittance.y,
                 sqrt(0.5),
                 1.0e-12);
    assert_close("runtime_caustic_photon_medium_segment_beer_b",
                 transmittance.z,
                 1.0,
                 1.0e-12);
    RuntimeCausticPhotonMediumStack3D_Init(&stack);
    assert_true("runtime_caustic_photon_medium_segment_air",
                RuntimeCausticPhotonMediumEntry3D_SegmentTransmittance(
                    RuntimeCausticPhotonMediumStack3D_Top(&stack),
                    100.0,
                    &transmittance) &&
                    transmittance.x == 1.0 && transmittance.y == 1.0 &&
                    transmittance.z == 1.0);
    return 0;
}

static int test_runtime_caustic_photon_medium_stack_nested_transitions(void) {
    RuntimeCausticPhotonMediumStack3D stack;
    RuntimeCausticPhotonMediumEntry3D glass = photon_medium_entry(21, 101, 1.5);
    RuntimeCausticPhotonMediumEntry3D water = photon_medium_entry(22, 102, 1.33);
    RuntimeCausticPhotonMediumTransition3D transition;
    RuntimeCausticPhotonMediumStack3D_Init(&stack);

    assert_true("runtime_caustic_photon_medium_stack_push_glass",
                RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &glass, true, false, &transition) &&
                    transition.succeeded && transition.stackChanged &&
                    transition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED &&
                    transition.depthBefore == 0u && transition.depthAfter == 1u &&
                    transition.topBefore.isAir &&
                    transition.topAfter.mediumId == glass.mediumId);
    assert_true("runtime_caustic_photon_medium_stack_push_water",
                RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &water, true, false, &transition) &&
                    transition.depthBefore == 1u && transition.depthAfter == 2u &&
                    transition.topBefore.mediumId == glass.mediumId &&
                    transition.topAfter.mediumId == water.mediumId &&
                    stack.maxDepth == 2u);
    assert_true("runtime_caustic_photon_medium_stack_pop_water",
                RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &water, false, false, &transition) &&
                    transition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED &&
                    transition.depthBefore == 2u && transition.depthAfter == 1u &&
                    transition.topAfter.mediumId == glass.mediumId);
    assert_true("runtime_caustic_photon_medium_stack_pop_glass",
                RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &glass, false, false, &transition) &&
                    transition.depthBefore == 1u && transition.depthAfter == 0u &&
                    transition.topAfter.isAir && stack.pushCount == 2u &&
                    stack.popCount == 2u);
    return 0;
}

static int test_runtime_caustic_photon_medium_stack_nested_interfaces(void) {
    RuntimeCausticPhotonMediumStack3D stack;
    RuntimeCausticPhotonMediumEntry3D glass = photon_medium_entry(21, 101, 1.5);
    RuntimeCausticPhotonMediumEntry3D water = photon_medium_entry(22, 102, 1.33);
    RuntimeCausticPhotonMediumTransition3D transition;
    double eta_from = 0.0;
    double eta_to = 0.0;
    RuntimeCausticPhotonMediumStack3D_Init(&stack);

    assert_true("runtime_caustic_photon_medium_interface_air_glass",
                RuntimeCausticPhotonMediumStack3D_ResolveInterface(
                    &stack, &glass, true, &eta_from, &eta_to) &&
                    eta_from == 1.0 && eta_to == 1.5 &&
                    RuntimeCausticPhotonMediumStack3D_Depth(&stack) == 0u);
    RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
        &stack, &glass, true, false, &transition);
    assert_true("runtime_caustic_photon_medium_interface_glass_water",
                RuntimeCausticPhotonMediumStack3D_ResolveInterface(
                    &stack, &water, true, &eta_from, &eta_to) &&
                    eta_from == 1.5 && eta_to == 1.33 &&
                    RuntimeCausticPhotonMediumStack3D_Depth(&stack) == 1u);
    RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
        &stack, &water, true, false, &transition);
    assert_true("runtime_caustic_photon_medium_interface_water_glass",
                RuntimeCausticPhotonMediumStack3D_ResolveInterface(
                    &stack, &water, false, &eta_from, &eta_to) &&
                    eta_from == 1.33 && eta_to == 1.5 &&
                    RuntimeCausticPhotonMediumStack3D_Depth(&stack) == 2u);
    RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
        &stack, &water, false, false, &transition);
    assert_true("runtime_caustic_photon_medium_interface_glass_air",
                RuntimeCausticPhotonMediumStack3D_ResolveInterface(
                    &stack, &glass, false, &eta_from, &eta_to) &&
                    eta_from == 1.5 && eta_to == 1.0 &&
                    RuntimeCausticPhotonMediumStack3D_Depth(&stack) == 1u);
    return 0;
}

static int test_runtime_caustic_photon_medium_stack_tir_and_mismatch(void) {
    RuntimeCausticPhotonMediumStack3D stack;
    RuntimeCausticPhotonMediumEntry3D glass = photon_medium_entry(31, 201, 1.5);
    RuntimeCausticPhotonMediumEntry3D wrong = photon_medium_entry(32, 202, 1.33);
    RuntimeCausticPhotonMediumTransition3D transition;
    RuntimeCausticPhotonMediumStack3D_Init(&stack);
    RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
        &stack, &glass, true, false, &transition);

    assert_true("runtime_caustic_photon_medium_stack_tir_no_change",
                RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &glass, false, true, &transition) &&
                    transition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_TIR_NO_CHANGE &&
                    !transition.stackChanged && transition.depthBefore == 1u &&
                    transition.depthAfter == 1u && stack.tirNoChangeCount == 1u);
    assert_true("runtime_caustic_photon_medium_stack_duplicate_enter",
                !RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &glass, true, false, &transition) &&
                    transition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_MISMATCH &&
                    RuntimeCausticPhotonMediumStack3D_Depth(&stack) == 1u);
    assert_true("runtime_caustic_photon_medium_stack_wrong_exit",
                !RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &wrong, false, false, &transition) &&
                    transition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_MISMATCH &&
                    stack.mismatchCount == 2u);
    return 0;
}

static int test_runtime_caustic_photon_medium_stack_failure_records(void) {
    RuntimeCausticPhotonMediumStack3D stack;
    RuntimeCausticPhotonMediumEntry3D entries[8];
    RuntimeCausticPhotonMediumEntry3D invalid;
    RuntimeCausticPhotonMediumTransition3D transition;
    RuntimeCausticPhotonMediumStack3D_Init(&stack);

    entries[0] = photon_medium_entry(40, 300, 1.1);
    assert_true("runtime_caustic_photon_medium_stack_underflow",
                !RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &entries[0], false, false, &transition) &&
                    transition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_UNDERFLOW &&
                    stack.underflowCount == 1u);
    for (uint32_t i = 0u;
         i < RUNTIME_CAUSTIC_PHOTON_MEDIUM_STACK_CAPACITY - 1u;
         ++i) {
        entries[i] = photon_medium_entry(40 + (int)i, 300 + (int)i, 1.1);
        assert_true("runtime_caustic_photon_medium_stack_fill",
                    RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                        &stack, &entries[i], true, false, &transition));
    }
    entries[7] = photon_medium_entry(47, 307, 1.1);
    assert_true("runtime_caustic_photon_medium_stack_overflow",
                !RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &entries[7], true, false, &transition) &&
                    transition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_OVERFLOW &&
                    stack.overflowCount == 1u &&
                    RuntimeCausticPhotonMediumStack3D_Depth(&stack) == 7u);

    memset(&invalid, 0, sizeof(invalid));
    assert_true("runtime_caustic_photon_medium_stack_invalid",
                !RuntimeCausticPhotonMediumStack3D_ObserveBoundary(
                    &stack, &invalid, true, false, &transition) &&
                    transition.reason ==
                        RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_INVALID_ENTRY &&
                    stack.invalidEntryCount == 1u);
    assert_true("runtime_caustic_photon_medium_stack_labels",
                strcmp(RuntimeCausticPhotonMediumTransitionReason3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_OVERFLOW),
                       "overflow") == 0 &&
                    strcmp(RuntimeCausticPhotonMediumTransitionReason3D_Label(
                               RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_NONE),
                           "none") == 0);
    return 0;
}

int run_test_runtime_caustic_photon_medium_stack_3d_tests(void) {
    int result = 0;
    result |= test_runtime_caustic_photon_medium_stack_initial_air();
    result |= test_runtime_caustic_photon_medium_stack_material_entry();
    result |= test_runtime_caustic_photon_medium_segment_transmittance();
    result |= test_runtime_caustic_photon_medium_stack_nested_transitions();
    result |= test_runtime_caustic_photon_medium_stack_nested_interfaces();
    result |= test_runtime_caustic_photon_medium_stack_tir_and_mismatch();
    result |= test_runtime_caustic_photon_medium_stack_failure_records();
    return result;
}
