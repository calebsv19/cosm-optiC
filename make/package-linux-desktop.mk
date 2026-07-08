# =========================
#  Linux desktop GUI packaging
# =========================
LINUX_DESKTOP_HOST_ARCH := $(shell uname -m)
ifndef LINUX_DESKTOP_PLATFORM
ifeq ($(LINUX_DESKTOP_HOST_ARCH),x86_64)
LINUX_DESKTOP_PLATFORM := linux-x86_64
else ifeq ($(LINUX_DESKTOP_HOST_ARCH),amd64)
LINUX_DESKTOP_PLATFORM := linux-x86_64
else ifeq ($(LINUX_DESKTOP_HOST_ARCH),aarch64)
LINUX_DESKTOP_PLATFORM := linux-aarch64
else ifeq ($(LINUX_DESKTOP_HOST_ARCH),arm64)
LINUX_DESKTOP_PLATFORM := linux-aarch64
else
$(error Unsupported Linux desktop package host architecture: $(LINUX_DESKTOP_HOST_ARCH))
endif
endif

LINUX_DESKTOP_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(LINUX_DESKTOP_PLATFORM)-desktop-$(RELEASE_CHANNEL)
LINUX_DESKTOP_DIR := $(RELEASE_DIR)/$(LINUX_DESKTOP_BASENAME)
LINUX_DESKTOP_BIN_DIR := $(LINUX_DESKTOP_DIR)/bin
LINUX_DESKTOP_RESOURCES_DIR := $(LINUX_DESKTOP_DIR)/resources
LINUX_DESKTOP_CONFIG_DIR := $(LINUX_DESKTOP_RESOURCES_DIR)/config
LINUX_DESKTOP_SHARED_DIR := $(LINUX_DESKTOP_RESOURCES_DIR)/shared
LINUX_DESKTOP_RUNTIME_DATA_DIR := $(LINUX_DESKTOP_RESOURCES_DIR)/data/runtime
LINUX_DESKTOP_MANIFEST_JSON := $(LINUX_DESKTOP_DIR)/manifest.json
LINUX_DESKTOP_PACKAGE_MANIFEST := $(LINUX_DESKTOP_DIR)/package_manifest.json
LINUX_DESKTOP_ARCHIVE := $(RELEASE_DIR)/$(LINUX_DESKTOP_BASENAME).tar.gz
LINUX_DESKTOP_LAUNCHER_SRC := tools/packaging/linux/raytracing-launcher

package-linux-desktop-contract:
	@echo "Linux desktop package contract"
	@echo "  package class: desktop_app_linux"
	@echo "  artifact role: desktop_app"
	@echo "  runtime:       linux_gui"
	@echo "  version:       $(RELEASE_VERSION)"
	@echo "  platform:      $(LINUX_DESKTOP_PLATFORM)"
	@echo "  stage dir:     $(LINUX_DESKTOP_DIR)"
	@echo "  archive:       $(LINUX_DESKTOP_ARCHIVE)"
	@echo "  launcher:      $(LINUX_DESKTOP_BIN_DIR)/raytracing-launcher"
	@echo "  binary:        $(LINUX_DESKTOP_BIN_DIR)/raytracing-bin"

package-linux-desktop-clean:
	@rm -rf "$(LINUX_DESKTOP_DIR)" "$(LINUX_DESKTOP_ARCHIVE)"
	@echo "Removed Linux desktop package artifacts: $(LINUX_DESKTOP_BASENAME)"

package-linux-desktop: visual-harness
	@echo "Preparing Linux desktop package..."
	@rm -rf "$(LINUX_DESKTOP_DIR)"
	@mkdir -p "$(LINUX_DESKTOP_BIN_DIR)" "$(LINUX_DESKTOP_CONFIG_DIR)" "$(LINUX_DESKTOP_SHARED_DIR)" "$(LINUX_DESKTOP_RUNTIME_DATA_DIR)/frames" "$(LINUX_DESKTOP_RUNTIME_DATA_DIR)/videos" "$(LINUX_DESKTOP_RESOURCES_DIR)/data/snapshots"
	@mkdir -p "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer" "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders"
	@cp "$(APP_TARGET)" "$(LINUX_DESKTOP_BIN_DIR)/raytracing-bin"
	@cp "$(LINUX_DESKTOP_LAUNCHER_SRC)" "$(LINUX_DESKTOP_BIN_DIR)/raytracing-launcher"
	@chmod +x "$(LINUX_DESKTOP_BIN_DIR)/raytracing-bin" "$(LINUX_DESKTOP_BIN_DIR)/raytracing-launcher"
	@cp -R config/. "$(LINUX_DESKTOP_CONFIG_DIR)/"
	@mkdir -p "$(LINUX_DESKTOP_SHARED_DIR)/assets/fonts"
	@cp -R "$(SHARED_ASSETS_DIR)/fonts/." "$(LINUX_DESKTOP_SHARED_DIR)/assets/fonts/"
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders/"
	@cp README.md "$(LINUX_DESKTOP_DIR)/"
	@printf '{\n' > "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "schema_version": "codework-desktop-app-package/v1",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "package_class": "desktop_app_linux",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "artifact_role": "desktop_app",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "runtime": "linux_gui",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "product": "%s",\n' "$(RELEASE_PRODUCT_NAME)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "platform": "%s",\n' "$(LINUX_DESKTOP_PLATFORM)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "entrypoint": "bin/raytracing-launcher",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "app_binary": "bin/raytracing-bin",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "SDL2", "SDL2_ttf", "json-c", "libpng16", "vulkan-loader", "gpu-vulkan-driver"],\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "proof_requirement": "real_display_unpacked_package_smoke"\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '}\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '{\n' > "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "schema_version": "codework_desktop_app_package_manifest_v1",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "package_role": "desktop_app",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "package_class": "desktop_app_linux",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "runtime": "linux_gui",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "product": "%s",\n' "$(RELEASE_PRODUCT_NAME)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "platform": "%s",\n' "$(LINUX_DESKTOP_PLATFORM)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "entrypoints": {\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "desktop_gui": "bin/raytracing-launcher",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "app_binary": "bin/raytracing-bin"\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "resources": ["resources/config", "resources/shared", "resources/shaders", "resources/vk_renderer"],\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "SDL2", "SDL2_ttf", "json-c", "libpng16", "vulkan-loader", "gpu-vulkan-driver"],\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "self_test": {\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "type": "command",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "argv": ["bin/raytracing-launcher", "--self-test"]\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  }\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '}\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@mkdir -p "$(RELEASE_DIR)"
	@COPYFILE_DISABLE=1 tar -czf "$(LINUX_DESKTOP_ARCHIVE)" -C "$(RELEASE_DIR)" "$(LINUX_DESKTOP_BASENAME)"
	@echo "Linux desktop package ready: $(LINUX_DESKTOP_ARCHIVE)"

package-linux-desktop-self-test: package-linux-desktop
	@test -x "$(LINUX_DESKTOP_BIN_DIR)/raytracing-launcher" || (echo "Missing Linux desktop launcher"; exit 1)
	@test -x "$(LINUX_DESKTOP_BIN_DIR)/raytracing-bin" || (echo "Missing Linux desktop binary"; exit 1)
	@test -f "$(LINUX_DESKTOP_MANIFEST_JSON)" || (echo "Missing manifest.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_PACKAGE_MANIFEST)" || (echo "Missing package_manifest.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/animation_config.json" || (echo "Missing config/animation_config.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/scene_config.json" || (echo "Missing config/scene_config.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/default.ttf" || (echo "Missing config/default.ttf"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/objects/Hexagon.asset.json" || (echo "Missing bundled shape assets"; exit 1)
	@test -f "$(LINUX_DESKTOP_SHARED_DIR)/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing shared packaged font"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk_renderer shader"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader"; exit 1)
	@test -f "$(LINUX_DESKTOP_ARCHIVE)" || (echo "Missing Linux desktop archive"; exit 1)
	@RAY_TRACING_RUNTIME_DIR="$(LINUX_DESKTOP_DIR)/.self_test_runtime" "$(LINUX_DESKTOP_BIN_DIR)/raytracing-launcher" --self-test
	@python3 -m json.tool "$(LINUX_DESKTOP_MANIFEST_JSON)" >/dev/null
	@python3 -m json.tool "$(LINUX_DESKTOP_PACKAGE_MANIFEST)" >/dev/null
	@echo "package-linux-desktop-self-test passed."
