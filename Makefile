MAKE_DIR := make

include $(MAKE_DIR)/config.mk
include $(MAKE_DIR)/shared.mk
include $(MAKE_DIR)/target.mk
include $(MAKE_DIR)/flags.mk
include $(MAKE_DIR)/paths.mk
include $(MAKE_DIR)/sources.mk
include $(MAKE_DIR)/objects.mk

.PHONY: all clean run run-ide-theme run-daw-theme run-headless-smoke visual-harness visual-artifact package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh package-linux-worker-contract package-linux-worker-clean package-linux-worker package-linux-worker-self-test release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh debug format video release relrun test test-stable test-legacy test-shared-theme-font-adapter test-ray-tracing-workspace-authoring-host test-ray-tracing-core-sim-runtime-frame-contract test-scene-editor-pane-host-contract test-water-surface-import-contract test-manifest-to-trace-export test-fluid-pack-contract-parity test-trio-scene-contract-diff native3d-render-audit ray-tracing-render-headless ray-tracing-job-runner ray-tracing-material-preview-headless test-ray-tracing-render-headless-preflight test-ray-tracing-render-headless-image-export test-ray-tracing-render-headless-volume-handoff test-ray-tracing-render-headless-water-surface-handoff test-ray-tracing-render-headless-water-optics-review test-ray-tracing-render-headless-water-basin-surface-review test-ray-tracing-render-headless-water-moving-light-review test-ray-tracing-render-headless-water-long-motion-review test-ray-tracing-render-headless-water-object-coupling-review test-ray-tracing-render-headless-water-object-coupling-long-review test-ray-tracing-render-headless-tlas-blas-repeated-instance-stress test-ray-tracing-tile-adaptive-t5-matrix test-ray-tracing-job-runner-smoke test-ray-tracing-job-runner-bundle-smoke test-ray-tracing-linux-worker-package-validator test-ray-tracing-release-contract-redaction test-ray-tracing-material-preview-headless test-ray-tracing-material-family-preview-grid test-ray-tracing-spatial-caustic-phase4-matrix test-ray-tracing-spatial-caustic-phase6-surface-matrix test-ray-tracing-spatial-caustic-phase7-calibration-matrix test-ray-tracing-spatial-caustic-phase8-receiver-policy-matrix test-ray-tracing-spatial-caustic-phase9-transmitted-receiver-matrix test-ray-tracing-spatial-caustic-phase10-tangent-receiver-matrix test-ray-tracing-spatial-caustic-visual-sphere-mist-matrix test-ray-tracing-spatial-caustic-authored-validation-matrix test-line-drawing-imported-mesh-runtime memory-check-build memory-check-run memory-check-audit clang-build fisics-build toolchain-contract dump-sema-runtime-scene-bridge

include $(MAKE_DIR)/rules-build.mk
include $(MAKE_DIR)/rules-tools.mk
include $(MAKE_DIR)/rules-test.mk
include $(MAKE_DIR)/rules-memory-check.mk
include $(MAKE_DIR)/package-macos.mk
include $(MAKE_DIR)/package-linux-worker.mk
include $(MAKE_DIR)/release.mk

-include $(DEP)
