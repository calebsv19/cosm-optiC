NATIVE3D_AUDIT_DEPS = \
	$(BUILD_DIR)/render/materials/material_bsdf.o \
	$(BUILD_DIR)/material/material_manager.o \
	$(BUILD_DIR)/material/material.o \
	$(BUILD_DIR)/render/integrators/integrator_common.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_tonemap.o \
	$(BUILD_DIR)/render/adapters/space_mode_adapter.o \
	$(BUILD_DIR)/render/helpers/ray_tracing_integrator_catalog.o \
	$(BUILD_DIR)/render/backend/ray_tracing_mode_backend.o \
	$(BUILD_DIR)/render/runtime_camera_3d_rays.o \
	$(BUILD_DIR)/render/runtime_direct_light_3d.o \
	$(BUILD_DIR)/render/runtime_direct_light_source_3d.o \
	$(BUILD_DIR)/render/runtime_diffuse_bounce_3d.o \
	$(BUILD_DIR)/render/runtime_dielectric_transport_3d.o \
	$(BUILD_DIR)/render/runtime_emissive_direct_3d.o \
	$(BUILD_DIR)/render/runtime_emissive_light_set_3d.o \
	$(BUILD_DIR)/render/runtime_light_set_3d.o \
	$(BUILD_DIR)/render/runtime_light_emitter_3d.o \
	$(BUILD_DIR)/render/runtime_specular_reflection_3d.o \
	$(BUILD_DIR)/render/runtime_disney_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_result_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_stochastic_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_bootstrap_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_beam_map_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_lens_transport_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_lens_transport_shapes_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_emit_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_integration_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_lifecycle_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_map_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_map_store_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_beam_contribution_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_bsdf_direction_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_bsdf_policy_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_bsdf_sampling_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_receiver_contribution_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_receiver_policy_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_scene_descriptor_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_scene_population_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_scene_trace_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_settings_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_photon_trace_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_settings_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_sphere_lens_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_surface_cache_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_analytic_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_analytic_sphere_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_lens_path_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_mesh_dielectric_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_debug_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_provider_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_shapes_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_surface_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_triangle_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_utils_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_transport_volume_3d.o \
	$(BUILD_DIR)/render/runtime_caustic_volume_cache_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_caustic_sidecar_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_estimator_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_transmission_policy_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_transport_emissive_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_transport_sampling_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_transport_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_transport_utils_3d.o \
	$(BUILD_DIR)/render/runtime_disney_v2_transmission_3d.o \
	$(BUILD_DIR)/render/runtime_emission_transparency_3d.o \
	$(BUILD_DIR)/render/runtime_emission_transparency_3d_support.o \
	$(BUILD_DIR)/render/runtime_emission_transparency_3d_sampling.o \
	$(BUILD_DIR)/render/runtime_path_depth_policy_3d.o \
	$(BUILD_DIR)/render/runtime_native_3d_feature_buffer.o \
	$(BUILD_DIR)/render/runtime_native_3d_denoise.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_unit.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler_diag.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler_metrics.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler_policy.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler_utils.o \
	$(BUILD_DIR)/render/runtime_native_3d_blue_noise.o \
	$(BUILD_DIR)/render/runtime_native_3d_sampling.o \
	$(BUILD_DIR)/render/runtime_native_3d_adaptive_sampling_state_mask.o \
	$(BUILD_DIR)/render/materials/runtime_material_authored_texture_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_authored_texture_3d_manifest.o \
	$(BUILD_DIR)/render/materials/runtime_material_graph_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_graph_3d_json.o \
	$(BUILD_DIR)/render/materials/runtime_material_payload_3d.o \
	$(BUILD_DIR)/render/materials/runtime_water_material_3d.o \
	$(BUILD_DIR)/render/materials/runtime_principled_bsdf_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_texture_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_texture_stack_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_texture_stack_3d_metadata.o \
	$(BUILD_DIR)/render/materials/runtime_material_response_3d.o \
	$(BUILD_DIR)/editor/scene_editor_material_face_metrics.o \
	$(BUILD_DIR)/editor/scene_editor_material_face_placement.o \
	$(BUILD_DIR)/editor/scene_editor_material_graph.o \
	$(BUILD_DIR)/editor/scene_editor_material_stack.o \
	$(BUILD_DIR)/render/runtime_native_3d_adaptive_sampling.o \
	$(BUILD_DIR)/render/runtime_native_3d_async_render_bridge.o \
	$(BUILD_DIR)/render/runtime_native_3d_async_render_job.o \
	$(BUILD_DIR)/render/runtime_native_3d_render.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_output.o \
	$(BUILD_DIR)/render/runtime_native_3d_prepared_scene_cache.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_prepare.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_request_snapshot.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_stats.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading_basic.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading_background.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading_disney_v2.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading_shared.o \
	$(BUILD_DIR)/render/runtime_native_3d_preview_reconstruction.o \
	$(BUILD_DIR)/render/runtime_native_3d_prepare_diagnostics.o \
	$(BUILD_DIR)/render/runtime_native_3d_resolution.o \
	$(BUILD_DIR)/render/runtime_native_3d_temporal_accum.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_occupancy.o \
	$(BUILD_DIR)/render/runtime_mesh_accel_pack_3d.o \
	$(BUILD_DIR)/render/runtime_mesh_blas_cache_3d.o \
	$(BUILD_DIR)/render/runtime_dynamic_geometry_accel_3d.o \
	$(BUILD_DIR)/render/runtime_scene_accel_3d_instances.o \
	$(BUILD_DIR)/render/runtime_scene_accel_3d.o \
	$(BUILD_DIR)/render/runtime_ray_3d.o \
	$(BUILD_DIR)/render/runtime_frame_dataflow_ledger_3d.o \
	$(BUILD_DIR)/render/runtime_render_trace_cost_ledger_3d.o \
	$(BUILD_DIR)/render/runtime_scene_3d.o \
	$(BUILD_DIR)/render/runtime_environment_3d.o \
	$(BUILD_DIR)/render/runtime_scene_3d_capabilities.o \
	$(BUILD_DIR)/render/runtime_triangle_bvh_3d.o \
	$(BUILD_DIR)/render/runtime_triangle_bvh_cache_3d.o \
	$(BUILD_DIR)/render/runtime_triangle_bvh_trace_3d.o \
	$(BUILD_DIR)/render/runtime_volume_3d.o \
	$(BUILD_DIR)/render/runtime_volume_3d_sampling.o \
	$(BUILD_DIR)/render/runtime_volume_3d_integrate.o \
	$(BUILD_DIR)/render/runtime_volume_3d_scatter.o \
	$(BUILD_DIR)/render/runtime_volume_3d_debug.o \
	$(BUILD_DIR)/render/runtime_scene_3d_samples.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder_geometry.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder_heightfield.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder_mesh.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder_shared.o \
	$(BUILD_DIR)/render/runtime_visibility_3d.o \
	$(BUILD_DIR)/scene/object_manager.o \
	$(BUILD_DIR)/geo/shape_adapter.o \
	$(BUILD_DIR)/geo/geolib/shape_asset.o \
	$(BUILD_DIR)/geo/geolib/shape_library.o \
	$(BUILD_DIR)/import/shape_import.o \
	$(BUILD_DIR)/import/fluid_import.o \
	$(BUILD_DIR)/import/fluid_volume_import_3d.o \
	$(BUILD_DIR)/import/fluid_volume_source_import_3d.o \
	$(BUILD_DIR)/import/fluid_volume_pack_import_3d.o \
	$(BUILD_DIR)/import/water_surface_import.o \
	$(BUILD_DIR)/import/fluid_pack_import.o \
	$(BUILD_DIR)/import/scene_bundle_import.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_json_utils.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_authoring.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_authoring_environment.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_authoring_paths.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_file.o \
	$(BUILD_DIR)/import/runtime_scene_motion_bridge.o \
	$(BUILD_DIR)/import/runtime_scene_volume_defaults.o \
	$(BUILD_DIR)/import/runtime_mesh_asset_pack.o \
	$(BUILD_DIR)/import/runtime_mesh_asset_loader.o \
	$(BUILD_DIR)/import/runtime_mesh_asset_loader_cache.o \
	$(BUILD_DIR)/import/runtime_mesh_asset_loader_utils.o \
	$(BUILD_DIR)/import/runtime_scene_bridge.o \
	$(BUILD_DIR)/path/path_system.o \
	$(BUILD_DIR)/path/path_arc_length.o \
	$(BUILD_DIR)/camera/camera.o \
	$(BUILD_DIR)/camera/camera_path_3d.o \
	$(BUILD_DIR)/motion/runtime_motion_track_3d.o \
	$(BUILD_DIR)/app/animation_fluid_scene.o \
	$(BUILD_DIR)/app/data_paths.o \
	$(BUILD_DIR)/config/core/config_runtime_paths.o \
	$(BUILD_DIR)/config/core/config_animation_runtime3d.o \
	$(BUILD_DIR)/config/core/config_animation_persistence.o \
	$(BUILD_DIR)/config/scene/config_scene_material_persistence.o \
	$(BUILD_DIR)/config/io/config_file_io.o \
	$(BUILD_DIR)/config/scene/config_scene_path_io.o \
	$(BUILD_DIR)/config/core/config_manager.o \
	$(BUILD_DIR)/render/fluid/fluid_state.o \
	$(BUILD_DIR)/render/pipeline/ray_tracing2_native3d_overlay.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_core.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_json.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_flatten.o \
	$(BUILD_DIR)/timer_hud_external/cJSON.o \
	$(CORE_BASE_OBJS) \
	$(CORE_IO_OBJS) \
	$(CORE_DATA_OBJS) \
	$(CORE_PACK_OBJS) \
	$(CORE_QUEUE_OBJS) \
	$(CORE_TIME_OBJS) \
	$(CORE_WORKERS_OBJS) \
	$(CORE_SCENE_OBJS) \
	$(CORE_SCENE_VIEW_OBJS) \
	$(CORE_SCENE_COMPILE_OBJS) \
	$(CORE_MESH_ASSET_OBJS) \
	$(CORE_MESH_PREVIEW_OBJS) \
	$(CORE_AUTHORED_TEXTURE_OBJS) \
	$(CORE_OBJECT_OBJS) \
	$(CORE_UNITS_OBJS) \
	$(CORE_SPACE_OBJS) \
	$(CORE_THEME_OBJS) \
	$(CORE_FONT_OBJS) \
	$(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS)) \
	$(KIT_VIZ_OBJS) \
	$(KIT_RUNTIME_DIAG_OBJS)

RAY_TRACING_RENDER_HEADLESS_DEPS = \
	$(BUILD_DIR)/app/ray_tracing_request_utils.o \
	$(BUILD_DIR)/app/agent_render_request.o \
	$(BUILD_DIR)/app/agent_render_request_defaults.o \
	$(BUILD_DIR)/app/agent_render_request_helpers.o \
	$(BUILD_DIR)/app/agent_render_request_labels.o \
	$(BUILD_DIR)/app/agent_render_request_validate.o \
	$(BUILD_DIR)/render/adapters/timer_hud_headless_stub.o \
	$(BUILD_DIR)/tools/make_video.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_caustic_photon_probe.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_caustic_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_dynamic_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_frame_dataflow_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_frame_analysis.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_frame_output.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_inspection_overrides.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_motion_accel_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_object_audit_prepare.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_object_audit_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_progress_callbacks.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_progress_status.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_render_budget.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_render_completion.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_render_failures.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_render_setup.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_render_stats_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_startup_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_trace_cost_direct_light_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_trace_cost_summary.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_usage.o \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless_volume_water_selection.o \
	$(NATIVE3D_AUDIT_DEPS)

RAY_TRACING_JOB_RUNNER_DEPS = \
	$(BUILD_DIR)/app/ray_tracing_job_runner.o \
	$(BUILD_DIR)/app/ray_tracing_headless_job_bundle.o \
	$(BUILD_DIR)/app/ray_tracing_job_runner_paths.o \
	$(BUILD_DIR)/app/ray_tracing_job_runner_status.o \
	$(patsubst $(CORE_HEADLESS_JOB_DIR)/src/%.c,$(BUILD_DIR)/core_headless_job/%.o,$(CORE_HEADLESS_JOB_SRCS)) \
	$(RAY_TRACING_RENDER_HEADLESS_DEPS)

RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_DEPS = \
	$(BUILD_DIR)/app/ray_tracing_request_utils.o \
	$(BUILD_DIR)/app/material_preview_request.o \
	$(BUILD_DIR)/app/material_preview_headless.o \
	$(BUILD_DIR)/render/adapters/timer_hud_headless_stub.o \
	$(BUILD_DIR)/editor/material_preview_surface_eval.o \
	$(NATIVE3D_AUDIT_DEPS)


TEST_OBJ := $(BUILD_DIR)/tests/test_runner.o $(BUILD_DIR)/tests/test_runner_registry.o \
	$(BUILD_DIR)/tests/test_support.o $(BUILD_DIR)/tests/test_config_animation.o \
	$(BUILD_DIR)/tests/test_config_animation_source_volume_suite.o \
	$(BUILD_DIR)/tests/test_config_animation_settings_export_suite.o \
	$(BUILD_DIR)/tests/test_ui_menu_contracts.o \
	$(BUILD_DIR)/tests/test_runtime_scene_bridge_core.o \
	$(BUILD_DIR)/tests/test_runtime_scene_bridge_writeback.o \
	$(BUILD_DIR)/tests/test_runtime_object_motion_tracks.o \
	$(BUILD_DIR)/tests/test_runtime_scene_3d_geometry.o \
	$(BUILD_DIR)/tests/test_runtime_scene_3d_geometry_builder_suite.o \
	$(BUILD_DIR)/tests/test_runtime_scene_3d_geometry_trace_suite.o \
	$(BUILD_DIR)/tests/test_runtime_mesh_asset_loader.o \
	$(BUILD_DIR)/tests/test_water_surface_runtime.o \
	$(BUILD_DIR)/tests/test_runtime_volume_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_bootstrap_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_beam_map_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_lens_transport_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_photon_emit_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_photon_integration_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_photon_map_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_photon_bsdf_policy_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_photon_bsdf_sampling_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_photon_scene_population_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_photon_scene_trace_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_photon_trace_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_sphere_lens_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_surface_cache_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_transport_3d.o \
	$(BUILD_DIR)/tests/test_runtime_caustic_volume_cache_3d.o \
	$(BUILD_DIR)/tests/test_runtime_lighting_materials.o \
	$(BUILD_DIR)/tests/test_runtime_light_set_3d.o \
	$(BUILD_DIR)/tests/test_runtime_material_authored_texture_validation_suite.o \
	$(BUILD_DIR)/tests/test_runtime_lighting_materials_payload_suite.o \
	$(BUILD_DIR)/tests/test_runtime_lighting_materials_direct_light_suite.o \
	$(BUILD_DIR)/tests/test_runtime_lighting_materials_transport_suite.o \
	$(BUILD_DIR)/tests/test_runtime_diffuse_temporal.o \
	$(BUILD_DIR)/tests/test_runtime_emission_transparency.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_denoise.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_async_render_bridge.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_async_render_job.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_render.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_render_live_suite.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_render_prepared_suite.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_render_prepared_parity_volume_suite.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_render_prepared_scatter_preview_suite.o \
	$(BUILD_DIR)/tests/test_runtime_render_metrics_export.o \
	$(BUILD_DIR)/tests/test_runtime_preview_editor.o \
	$(BUILD_DIR)/tests/test_runtime_scene_editor.o \
	$(BUILD_DIR)/tests/test_runtime_path_policy.o \
	$(BUILD_DIR)/tests/test_runtime_mode_backend_policy.o \
	$(BUILD_DIR)/tests/test_fluid_volume_import_3d.o \
	$(BUILD_DIR)/tests/test_fluid_volume_pack_import_3d.o \
	$(BUILD_DIR)/tests/test_stubs.o \
	$(BUILD_DIR)/tests/fluid_pack_import_test.o \
	$(BUILD_DIR)/tests/kit_viz_fluid_overlay_adapter_test.o \
	$(BUILD_DIR)/tests/render_metrics_dataset_test.o

ifeq ($(UNAME_S),Darwin)
CORE_TIME_TEST_DEPS := $(BUILD_DIR)/core_time/core_time.o $(BUILD_DIR)/core_time/core_time_mac.o
else
CORE_TIME_TEST_DEPS := $(BUILD_DIR)/core_time/core_time.o $(BUILD_DIR)/core_time/core_time_posix.o
endif

TEST_DEPS := \
	$(BUILD_DIR)/render/materials/material_bsdf.o \
	$(BUILD_DIR)/material/material_manager.o \
	$(BUILD_DIR)/material/material.o \
	$(BUILD_DIR)/render/integrators/direct_light_integrator.o \
	$(BUILD_DIR)/render/integrators/forward_light_integrator.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_energy.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_cache.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_direct.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_indirect.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_sampling.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_tonemap.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_visibility.o \
	$(BUILD_DIR)/render/integrators/hybrid/camera_path_integrator.o \
	$(BUILD_DIR)/render/integrators/camera_path_integrator_disney.o \
	$(BUILD_DIR)/render/integrators/integrator_common.o \
	$(BUILD_DIR)/render/accel/irradiance_cache.o \
	$(BUILD_DIR)/render/adapters/space_mode_adapter.o \
	$(BUILD_DIR)/render/helpers/ray_tracing_integrator_catalog.o \
	$(BUILD_DIR)/render/backend/ray_tracing_mode_backend.o \
		$(BUILD_DIR)/render/runtime_camera_3d_rays.o \
		$(BUILD_DIR)/render/runtime_direct_light_3d.o \
		$(BUILD_DIR)/render/runtime_direct_light_source_3d.o \
		$(BUILD_DIR)/render/runtime_diffuse_bounce_3d.o \
		$(BUILD_DIR)/render/runtime_dielectric_transport_3d.o \
		$(BUILD_DIR)/render/runtime_emissive_direct_3d.o \
		$(BUILD_DIR)/render/runtime_emissive_light_set_3d.o \
		$(BUILD_DIR)/render/runtime_light_set_3d.o \
		$(BUILD_DIR)/render/runtime_light_emitter_3d.o \
		$(BUILD_DIR)/render/runtime_specular_reflection_3d.o \
		$(BUILD_DIR)/render/runtime_disney_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_result_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_stochastic_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_bootstrap_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_beam_map_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_lens_transport_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_lens_transport_shapes_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_emit_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_integration_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_lifecycle_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_map_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_map_store_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_beam_contribution_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_receiver_contribution_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_receiver_policy_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_scene_descriptor_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_scene_population_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_scene_trace_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_settings_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_photon_trace_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_settings_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_sphere_lens_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_surface_cache_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_analytic_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_analytic_sphere_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_lens_path_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_debug_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_provider_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_shapes_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_surface_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_triangle_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_utils_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_transport_volume_3d.o \
		$(BUILD_DIR)/render/runtime_caustic_volume_cache_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_caustic_sidecar_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_estimator_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_transmission_policy_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_transport_emissive_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_transport_sampling_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_transport_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_transport_utils_3d.o \
		$(BUILD_DIR)/render/runtime_disney_v2_transmission_3d.o \
		$(BUILD_DIR)/render/runtime_emission_transparency_3d.o \
		$(BUILD_DIR)/render/runtime_emission_transparency_3d_support.o \
		$(BUILD_DIR)/render/runtime_path_depth_policy_3d.o \
	$(BUILD_DIR)/render/runtime_native_3d_feature_buffer.o \
	$(BUILD_DIR)/render/runtime_native_3d_denoise.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_unit.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler_diag.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler_metrics.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler_policy.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_scheduler_utils.o \
	$(BUILD_DIR)/render/runtime_native_3d_blue_noise.o \
	$(BUILD_DIR)/render/runtime_native_3d_sampling.o \
	$(BUILD_DIR)/render/runtime_native_3d_adaptive_sampling_state_mask.o \
	$(BUILD_DIR)/render/materials/runtime_material_authored_texture_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_graph_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_graph_3d_json.o \
	$(BUILD_DIR)/render/materials/runtime_material_payload_3d.o \
	$(BUILD_DIR)/render/materials/runtime_water_material_3d.o \
	$(BUILD_DIR)/render/materials/runtime_principled_bsdf_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_texture_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_texture_stack_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_response_3d.o \
	$(BUILD_DIR)/render/runtime_native_3d_adaptive_sampling.o \
	$(BUILD_DIR)/render/runtime_native_3d_render.o \
	$(BUILD_DIR)/render/runtime_native_3d_prepared_scene_cache.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading_basic.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading_disney_v2.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading_shared.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_output.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_request_snapshot.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_stats.o \
	$(BUILD_DIR)/render/runtime_native_3d_progress_hud.o \
	$(BUILD_DIR)/render/runtime_native_3d_preview_reconstruction.o \
	$(BUILD_DIR)/render/runtime_native_3d_resolution.o \
	$(BUILD_DIR)/render/runtime_native_3d_temporal_accum.o \
		$(BUILD_DIR)/render/runtime_native_3d_tile_occupancy.o \
		$(BUILD_DIR)/render/runtime_mesh_accel_pack_3d.o \
		$(BUILD_DIR)/render/runtime_mesh_blas_cache_3d.o \
	$(BUILD_DIR)/render/runtime_dynamic_geometry_accel_3d.o \
	$(BUILD_DIR)/render/runtime_scene_accel_3d_instances.o \
	$(BUILD_DIR)/render/runtime_scene_accel_3d.o \
	$(BUILD_DIR)/render/runtime_ray_3d.o \
	$(BUILD_DIR)/render/runtime_frame_dataflow_ledger_3d.o \
	$(BUILD_DIR)/render/runtime_render_trace_cost_ledger_3d.o \
	$(BUILD_DIR)/render/runtime_scene_3d.o \
	$(BUILD_DIR)/render/runtime_environment_3d.o \
	$(BUILD_DIR)/render/runtime_scene_3d_capabilities.o \
	$(BUILD_DIR)/render/runtime_triangle_bvh_cache_3d.o \
	$(BUILD_DIR)/render/runtime_triangle_bvh_trace_3d.o \
	$(BUILD_DIR)/render/runtime_volume_3d.o \
	$(BUILD_DIR)/render/runtime_volume_3d_sampling.o \
	$(BUILD_DIR)/render/runtime_volume_3d_integrate.o \
	$(BUILD_DIR)/render/runtime_volume_3d_scatter.o \
	$(BUILD_DIR)/render/runtime_volume_3d_debug.o \
	$(BUILD_DIR)/render/runtime_scene_3d_samples.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder_geometry.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder_heightfield.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder_mesh.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder_shared.o \
	$(BUILD_DIR)/render/runtime_visibility_3d.o \
	$(BUILD_DIR)/editor/editor_mode_router.o \
	$(BUILD_DIR)/editor/material_editor_knob_control.o \
	$(BUILD_DIR)/editor/material_editor_layer_model.o \
	$(BUILD_DIR)/editor/material_editor_authored_texture_binding.o \
	$(BUILD_DIR)/editor/material_editor_face_preview.o \
	$(BUILD_DIR)/editor/scene_editor_material_graph.o \
	$(BUILD_DIR)/editor/material_editor_recipe.o \
	$(BUILD_DIR)/editor/material_editor_graph_actions.o \
	$(BUILD_DIR)/editor/material_editor_graph_readback.o \
	$(BUILD_DIR)/editor/material_editor_material_readback.o \
	$(BUILD_DIR)/editor/material_editor_response_readback.o \
	$(BUILD_DIR)/editor/material_editor_texture_channel_readback.o \
	$(BUILD_DIR)/editor/material_editor_face_region_readback.o \
	$(BUILD_DIR)/editor/material_editor_compact_layout.o \
	$(BUILD_DIR)/editor/material_editor_compact_response_render.o \
	$(BUILD_DIR)/editor/material_editor_compact_render.o \
	$(BUILD_DIR)/editor/material_editor.o \
	$(BUILD_DIR)/editor/material_preview_surface_eval.o \
	$(BUILD_DIR)/editor/scene_editor_material_face_metrics.o \
	$(BUILD_DIR)/editor/object_editor_object_ops.o \
	$(BUILD_DIR)/editor/object_editor_selection_tracker.o \
	$(BUILD_DIR)/editor/scene_editor_control_surface.o \
	$(BUILD_DIR)/editor/scene_editor_digest_overlay_projector.o \
	$(BUILD_DIR)/editor/scene_editor_material_face_placement.o \
	$(BUILD_DIR)/editor/scene_editor_material_stack.o \
	$(BUILD_DIR)/editor/scene_editor_material_preview.o \
	$(BUILD_DIR)/editor/scene_editor_tool_state.o \
	$(BUILD_DIR)/editor/scene_editor_viewport_nav_zoom.o \
	$(BUILD_DIR)/editor/scene_editor_runtime_scene_persistence.o \
	$(BUILD_DIR)/path/path_system.o \
	$(BUILD_DIR)/path/path_arc_length.o \
		$(BUILD_DIR)/render/accel/uniform_grid.o \
		$(BUILD_DIR)/render/accel/surface_mesh.o \
	$(BUILD_DIR)/render/helpers/render_helper.o \
	$(BUILD_DIR)/render/font_bridge.o \
	$(BUILD_DIR)/render/font_runtime.o \
	$(BUILD_DIR)/render/text_font_cache.o \
	$(BUILD_DIR)/render/text_draw.o \
	$(BUILD_DIR)/render/text_upload_policy.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2_native3d_overlay.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2_preview.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2_preview_present_capture.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2_preview_present_history.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2_preview_present.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2.o \
	$(BUILD_DIR)/render/fluid/fluid_state.o \
	$(BUILD_DIR)/render/fluid/fluid_overlay.o \
	$(BUILD_DIR)/scene/object_manager.o \
	$(BUILD_DIR)/geo/shape_adapter.o \
	$(BUILD_DIR)/geo/geolib/shape_asset.o \
	$(BUILD_DIR)/geo/geolib/shape_library.o \
	$(BUILD_DIR)/import/shape_import.o \
	$(BUILD_DIR)/timer_hud_external/cJSON.o \
	$(BUILD_DIR)/camera/camera.o \
	$(BUILD_DIR)/camera/camera_path_3d.o \
	$(BUILD_DIR)/app/preview_mode_route.o \
	$(BUILD_DIR)/app/preview_playback.o \
	$(BUILD_DIR)/app/preview_camera_sample.o \
	$(BUILD_DIR)/app/preview_camera_projector.o \
	$(BUILD_DIR)/app/preview_retained_scene_mesh.o \
	$(BUILD_DIR)/app/preview_retained_scene_renderer.o \
	$(BUILD_DIR)/app/animation_output.o \
	$(BUILD_DIR)/app/render_export_batch.o \
	$(BUILD_DIR)/app/animation_fluid_scene.o \
	$(BUILD_DIR)/app/data_paths.o \
	$(BUILD_DIR)/config/core/config_runtime_paths.o \
	$(BUILD_DIR)/config/core/config_animation_runtime3d.o \
	$(BUILD_DIR)/config/core/config_animation_persistence.o \
		$(BUILD_DIR)/config/io/config_file_io.o \
	$(BUILD_DIR)/config/scene/config_scene_path_io.o \
	$(BUILD_DIR)/config/core/config_manager.o \
	$(BUILD_DIR)/ui/menu/scene_source_ui_labels.o \
	$(BUILD_DIR)/ui/menu/volume_source_ui_labels.o \
	$(BUILD_DIR)/ui/menu/shared_theme_font_adapter.o \
	$(BUILD_DIR)/tools/make_video.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_core.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_json.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_flatten.o \
	$(BUILD_DIR)/import/fluid_import.o \
	$(BUILD_DIR)/import/fluid_volume_import_3d.o \
	$(BUILD_DIR)/import/fluid_volume_source_import_3d.o \
	$(BUILD_DIR)/import/fluid_volume_pack_import_3d.o \
	$(BUILD_DIR)/import/water_surface_import.o \
	$(BUILD_DIR)/import/fluid_pack_import.o \
	$(BUILD_DIR)/import/scene_bundle_import.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_json_utils.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_authoring.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_authoring_environment.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_file.o \
	$(BUILD_DIR)/import/runtime_scene_motion_bridge.o \
	$(BUILD_DIR)/import/runtime_scene_volume_defaults.o \
	$(BUILD_DIR)/import/runtime_mesh_asset_pack.o \
	$(BUILD_DIR)/import/runtime_mesh_asset_loader.o \
	$(BUILD_DIR)/import/runtime_mesh_asset_loader_cache.o \
	$(BUILD_DIR)/import/runtime_mesh_asset_loader_utils.o \
	$(BUILD_DIR)/import/runtime_scene_bridge.o \
	$(BUILD_DIR)/motion/runtime_motion_track_3d.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_render.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_render_controls.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_render_manifest.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_render_volume.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_render_sliders.o \
	$(BUILD_DIR)/ui/menu/menu_layout.o \
	$(BUILD_DIR)/ui/menu/menu_panel_chrome.o \
	$(BUILD_DIR)/ui/menu/menu_batch_panel.o \
	$(BUILD_DIR)/ui/menu/menu_resume_panel.o \
	$(BUILD_DIR)/ui/menu/menu_scene_project_summary.o \
	$(BUILD_DIR)/ui/menu/menu_worker_export.o \
	$(BUILD_DIR)/ui/menu/scene_source_catalog.o \
	$(BUILD_DIR)/ui/menu/volume_source_catalog.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_state.o \
	$(BUILD_DIR)/render/adapters/kit_viz_fluid_overlay_adapter.o \
	$(BUILD_DIR)/render/runtime_triangle_bvh_3d.o \
	$(BUILD_DIR)/core_base/core_base.o \
	$(BUILD_DIR)/core_io/core_io.o \
	$(BUILD_DIR)/core_data/core_data.o \
	$(BUILD_DIR)/core_pack/core_pack.o \
	$(BUILD_DIR)/core_queue/core_queue.o \
	$(CORE_TIME_TEST_DEPS) \
	$(BUILD_DIR)/core_workers/core_workers.o \
	$(BUILD_DIR)/core_scene/core_scene.o \
	$(BUILD_DIR)/core_authored_texture/core_authored_texture.o \
	$(BUILD_DIR)/core_scene_compile/core_scene_compile.o \
	$(BUILD_DIR)/core_mesh_asset/core_mesh_asset.o \
	$(BUILD_DIR)/core_mesh_asset/core_mesh_asset_authoring_document.o \
	$(BUILD_DIR)/core_mesh_asset/core_mesh_asset_runtime_document.o \
	$(BUILD_DIR)/core_mesh_preview/core_mesh_preview.o \
	$(BUILD_DIR)/core_object/core_object.o \
	$(BUILD_DIR)/core_units/core_units.o \
	$(BUILD_DIR)/core_space/core_space.o \
	$(BUILD_DIR)/export/render_metrics_dataset.o \
	$(BUILD_DIR)/core_theme/core_theme.o \
	$(BUILD_DIR)/core_font/core_font.o \
	$(BUILD_DIR)/kit_viz/kit_viz.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_commands.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_config.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_context.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_device.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_pipeline.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_textures.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_memory.o

SRC := $(shell find $(SRC_DIR) -name '*.c' \
	! -path '$(SRC_DIR)/tools/cli/*' \
	! -path '$(SRC_DIR)/render/integrators/camera_path_integrator_old_version.c' \
	! -path '$(SRC_DIR)/render/TimerHUD_legacy_backup/*')
VK_RENDERER_SRCS := $(shell find $(VK_RENDERER_DIR)/src -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
OBJ := $(filter-out $(BUILD_DIR)/render/integrators/camera_path_integrator_old_version.o,$(OBJ))
OBJ := $(filter-out $(BUILD_DIR)/render/adapters/timer_hud_headless_stub.o,$(OBJ))

TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -name '*.c')
TIMER_HUD_EXTERNAL_SRCS := $(TIMER_HUD_DIR)/external/cJSON.c
TIMER_HUD_OBJS := $(patsubst $(TIMER_HUD_DIR)/src/%.c,$(BUILD_DIR)/timer_hud/%.o,$(TIMER_HUD_SRCS))
TIMER_HUD_EXTERNAL_OBJS := $(patsubst $(TIMER_HUD_DIR)/external/%.c,$(BUILD_DIR)/timer_hud_external/%.o,$(TIMER_HUD_EXTERNAL_SRCS))
CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
CORE_DATA_SRCS := $(CORE_DATA_DIR)/src/core_data.c
CORE_PACK_SRCS := $(CORE_PACK_DIR)/src/core_pack.c
CORE_QUEUE_SRCS := $(CORE_QUEUE_DIR)/src/core_queue.c
CORE_TIME_SRCS := $(CORE_TIME_DIR)/src/core_time.c
ifeq ($(UNAME_S),Darwin)
CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_mac.c
else
CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_posix.c
endif
CORE_WORKERS_SRCS := $(CORE_WORKERS_DIR)/src/core_workers.c
CORE_SIM_SRCS := $(CORE_SIM_DIR)/src/core_sim.c
CORE_SCENE_SRCS := $(CORE_SCENE_DIR)/src/core_scene.c
CORE_SCENE_VIEW_SRCS := $(CORE_SCENE_VIEW_DIR)/src/core_scene_view.c
CORE_AUTHORED_TEXTURE_SRCS := $(CORE_AUTHORED_TEXTURE_DIR)/src/core_authored_texture.c
CORE_SCENE_COMPILE_SRCS := $(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c
CORE_MESH_ASSET_SRCS := \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_authoring_document.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c
CORE_MESH_PREVIEW_SRCS := $(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c
CORE_OBJECT_SRCS := $(CORE_OBJECT_DIR)/src/core_object.c
CORE_UNITS_SRCS := $(CORE_UNITS_DIR)/src/core_units.c
CORE_SPACE_SRCS := $(CORE_SPACE_DIR)/src/core_space.c
CORE_PANE_SRCS := $(CORE_PANE_DIR)/src/core_pane.c
CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
CORE_HEADLESS_JOB_SRCS := $(CORE_HEADLESS_JOB_DIR)/src/core_headless_job.c
KIT_RENDER_SRCS := \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_external_text.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_vk.c
KIT_PANE_SRCS := $(KIT_PANE_DIR)/src/kit_pane.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c
KIT_RUNTIME_DIAG_SRCS := $(KIT_RUNTIME_DIAG_DIR)/src/kit_runtime_diag.c
KIT_WORKSPACE_AUTHORING_SRCS := \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/kit_workspace_authoring.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_overlay.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_font_theme.c
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(BUILD_DIR)/core_base/%.o,$(CORE_BASE_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(BUILD_DIR)/core_io/%.o,$(CORE_IO_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(BUILD_DIR)/core_data/%.o,$(CORE_DATA_SRCS))
CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(BUILD_DIR)/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_QUEUE_OBJS := $(patsubst $(CORE_QUEUE_DIR)/src/%.c,$(BUILD_DIR)/core_queue/%.o,$(CORE_QUEUE_SRCS))
CORE_TIME_OBJS := $(patsubst $(CORE_TIME_DIR)/src/%.c,$(BUILD_DIR)/core_time/%.o,$(CORE_TIME_SRCS))
CORE_WORKERS_OBJS := $(patsubst $(CORE_WORKERS_DIR)/src/%.c,$(BUILD_DIR)/core_workers/%.o,$(CORE_WORKERS_SRCS))
CORE_SIM_OBJS := $(patsubst $(CORE_SIM_DIR)/src/%.c,$(BUILD_DIR)/core_sim/%.o,$(CORE_SIM_SRCS))
CORE_SCENE_OBJS := $(patsubst $(CORE_SCENE_DIR)/src/%.c,$(BUILD_DIR)/core_scene/%.o,$(CORE_SCENE_SRCS))
CORE_SCENE_VIEW_OBJS := $(patsubst $(CORE_SCENE_VIEW_DIR)/src/%.c,$(BUILD_DIR)/core_scene_view/%.o,$(CORE_SCENE_VIEW_SRCS))
CORE_AUTHORED_TEXTURE_OBJS := $(patsubst $(CORE_AUTHORED_TEXTURE_DIR)/src/%.c,$(BUILD_DIR)/core_authored_texture/%.o,$(CORE_AUTHORED_TEXTURE_SRCS))
CORE_SCENE_COMPILE_OBJS := $(patsubst $(CORE_SCENE_COMPILE_DIR)/src/%.c,$(BUILD_DIR)/core_scene_compile/%.o,$(CORE_SCENE_COMPILE_SRCS))
CORE_MESH_ASSET_OBJS := $(patsubst $(CORE_MESH_ASSET_DIR)/src/%.c,$(BUILD_DIR)/core_mesh_asset/%.o,$(CORE_MESH_ASSET_SRCS))
CORE_MESH_PREVIEW_OBJS := $(patsubst $(CORE_MESH_PREVIEW_DIR)/src/%.c,$(BUILD_DIR)/core_mesh_preview/%.o,$(CORE_MESH_PREVIEW_SRCS))
CORE_OBJECT_OBJS := $(patsubst $(CORE_OBJECT_DIR)/src/%.c,$(BUILD_DIR)/core_object/%.o,$(CORE_OBJECT_SRCS))
CORE_UNITS_OBJS := $(patsubst $(CORE_UNITS_DIR)/src/%.c,$(BUILD_DIR)/core_units/%.o,$(CORE_UNITS_SRCS))
CORE_SPACE_OBJS := $(patsubst $(CORE_SPACE_DIR)/src/%.c,$(BUILD_DIR)/core_space/%.o,$(CORE_SPACE_SRCS))
CORE_PANE_OBJS := $(patsubst $(CORE_PANE_DIR)/src/%.c,$(BUILD_DIR)/core_pane/%.o,$(CORE_PANE_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(BUILD_DIR)/core_theme/%.o,$(CORE_THEME_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(BUILD_DIR)/core_font/%.o,$(CORE_FONT_SRCS))
CORE_HEADLESS_JOB_OBJS := $(patsubst $(CORE_HEADLESS_JOB_DIR)/src/%.c,$(BUILD_DIR)/core_headless_job/%.o,$(CORE_HEADLESS_JOB_SRCS))
KIT_RENDER_OBJS := $(patsubst $(KIT_RENDER_DIR)/src/%.c,$(BUILD_DIR)/kit_render/%.o,$(KIT_RENDER_SRCS))
KIT_PANE_OBJS := $(patsubst $(KIT_PANE_DIR)/src/%.c,$(BUILD_DIR)/kit_pane/%.o,$(KIT_PANE_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(BUILD_DIR)/kit_viz/%.o,$(KIT_VIZ_SRCS))
KIT_RUNTIME_DIAG_OBJS := $(patsubst $(KIT_RUNTIME_DIAG_DIR)/src/%.c,$(BUILD_DIR)/kit_runtime_diag/%.o,$(KIT_RUNTIME_DIAG_SRCS))
KIT_WORKSPACE_AUTHORING_OBJS := $(patsubst $(KIT_WORKSPACE_AUTHORING_DIR)/src/%.c,$(BUILD_DIR)/kit_workspace_authoring/%.o,$(KIT_WORKSPACE_AUTHORING_SRCS))

TEST_DEPS += $(KIT_RENDER_OBJS) $(CORE_PANE_OBJS) $(KIT_PANE_OBJS) $(KIT_WORKSPACE_AUTHORING_OBJS) \
	$(BUILD_DIR)/editor/scene_editor_pane_host.o \
	$(BUILD_DIR)/ui/menu/workspace_authoring/ray_tracing_workspace_authoring_host.o \
	$(BUILD_DIR)/ui/menu/workspace_authoring/ray_tracing_workspace_authoring_overlay.o

OBJ := $(OBJ) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS) \
	$(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS)) \
	$(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_DATA_OBJS) $(CORE_PACK_OBJS) $(CORE_QUEUE_OBJS) $(CORE_TIME_OBJS) $(CORE_WORKERS_OBJS) $(CORE_SIM_OBJS) $(CORE_SCENE_OBJS) $(CORE_SCENE_VIEW_OBJS) $(CORE_AUTHORED_TEXTURE_OBJS) $(CORE_SCENE_COMPILE_OBJS) $(CORE_MESH_ASSET_OBJS) $(CORE_MESH_PREVIEW_OBJS) $(CORE_OBJECT_OBJS) $(CORE_UNITS_OBJS) $(CORE_SPACE_OBJS) $(CORE_PANE_OBJS) $(CORE_THEME_OBJS) $(CORE_FONT_OBJS) $(CORE_HEADLESS_JOB_OBJS) $(KIT_RENDER_OBJS) $(KIT_PANE_OBJS) $(KIT_VIZ_OBJS) $(KIT_RUNTIME_DIAG_OBJS) $(KIT_WORKSPACE_AUTHORING_OBJS)
TEST_DEPS := $(sort $(TEST_DEPS) $(filter-out $(BUILD_DIR)/app/animation.o $(BUILD_DIR)/app/ray_tracing_job_runner.o,$(OBJ)))
DEP := $(sort \
	$(OBJ:.o=.d) \
	$(TEST_OBJ:.o=.d) \
	$(TEST_DEPS:.o=.d) \
	$(NATIVE3D_AUDIT_OBJ:.o=.d) \
	$(NATIVE3D_AUDIT_DEPS:.o=.d) \
	$(RAY_TRACING_RENDER_HEADLESS_OBJ:.o=.d) \
	$(RAY_TRACING_RENDER_HEADLESS_DEPS:.o=.d) \
	$(RAY_TRACING_JOB_RUNNER_OBJ:.o=.d) \
	$(RAY_TRACING_JOB_RUNNER_DEPS:.o=.d) \
	$(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_OBJ:.o=.d) \
	$(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_DEPS:.o=.d))
