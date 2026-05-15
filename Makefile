MAKE_DIR := make

include $(MAKE_DIR)/config.mk
include $(MAKE_DIR)/shared.mk
include $(MAKE_DIR)/target.mk
include $(MAKE_DIR)/flags.mk
include $(MAKE_DIR)/paths.mk
include $(MAKE_DIR)/sources.mk
include $(MAKE_DIR)/objects.mk

.PHONY: all clean run run-ide-theme run-daw-theme run-headless-smoke visual-harness package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh debug format video release relrun test test-stable test-legacy test-shared-theme-font-adapter test-ray-tracing-workspace-authoring-host test-ray-tracing-core-sim-runtime-frame-contract test-scene-editor-pane-host-contract test-manifest-to-trace-export test-fluid-pack-contract-parity test-trio-scene-contract-diff native3d-render-audit

include $(MAKE_DIR)/rules-build.mk
include $(MAKE_DIR)/rules-tools.mk
include $(MAKE_DIR)/rules-test.mk
include $(MAKE_DIR)/package-macos.mk
include $(MAKE_DIR)/release.mk

-include $(DEP)
