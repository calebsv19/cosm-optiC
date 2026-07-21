# =========================
#  Linux desktop app packaging
# =========================
LINUX_DESKTOP_HOST_ARCH := $(shell uname -m)
ifndef LINUX_DESKTOP_PLATFORM
ifeq ($(UNAME_S),Linux)
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
else
LINUX_DESKTOP_PLATFORM := linux-x86_64
endif
endif
LINUX_DESKTOP_PACKAGE_EPOCH ?= 0
LINUX_DESKTOP_PACKAGE_CLASS := desktop_app_linux
LINUX_DESKTOP_ARTIFACT_ROLE := desktop_app
LINUX_DESKTOP_RUNTIME := linux_gui
LINUX_DESKTOP_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(LINUX_DESKTOP_PLATFORM)-desktop-$(RELEASE_CHANNEL)
LINUX_DESKTOP_DIR := $(RELEASE_DIR)/$(LINUX_DESKTOP_BASENAME)
LINUX_DESKTOP_BIN_DIR := $(LINUX_DESKTOP_DIR)/bin
LINUX_DESKTOP_RESOURCES_DIR := $(LINUX_DESKTOP_DIR)/resources
LINUX_DESKTOP_SHARE_DIR := $(LINUX_DESKTOP_DIR)/share
LINUX_DESKTOP_CONFIG_DIR := $(LINUX_DESKTOP_RESOURCES_DIR)/config
LINUX_DESKTOP_SHARED_DIR := $(LINUX_DESKTOP_RESOURCES_DIR)/shared
LINUX_DESKTOP_RUNTIME_DATA_DIR := $(LINUX_DESKTOP_RESOURCES_DIR)/data/runtime
LINUX_DESKTOP_LAUNCHER_SRC := tools/packaging/linux/raytracing-launcher
LINUX_DESKTOP_ENTRY_SRC := tools/packaging/linux/optic.desktop
LINUX_DESKTOP_ICON_SRC := tools/packaging/linux/icons/optic.svg
LINUX_DESKTOP_INSTALLER_SRC := tools/packaging/linux/install-desktop-entry.sh
LINUX_DESKTOP_ENTRY := $(LINUX_DESKTOP_SHARE_DIR)/applications/optic.desktop
LINUX_DESKTOP_ICON := $(LINUX_DESKTOP_SHARE_DIR)/icons/hicolor/scalable/apps/optic.svg
LINUX_DESKTOP_INSTALLER := $(LINUX_DESKTOP_SHARE_DIR)/install-desktop-entry.sh
LINUX_DESKTOP_MANIFEST_JSON := $(LINUX_DESKTOP_DIR)/manifest.json
LINUX_DESKTOP_PACKAGE_MANIFEST := $(LINUX_DESKTOP_DIR)/package_manifest.json
LINUX_DESKTOP_ARCHIVE := $(RELEASE_DIR)/$(LINUX_DESKTOP_BASENAME).tar.gz
LINUX_DESKTOP_ARCHIVE_TMP := $(LINUX_DESKTOP_ARCHIVE).tmp
LINUX_DESKTOP_SHA256 := $(LINUX_DESKTOP_ARCHIVE).sha256
LINUX_DESKTOP_ARCHIVE_SHA256 := $(LINUX_DESKTOP_SHA256)
LINUX_DESKTOP_TAR ?= tar
LINUX_DESKTOP_GZIP ?= gzip
LINUX_DESKTOP_SELF_TEST_RUNTIME_DIR ?= $(abspath $(RELEASE_DIR)/linux-desktop-self-test/runtime)
LINUX_DESKTOP_SELF_TEST_STATE_DIR ?= $(abspath $(RELEASE_DIR)/linux-desktop-self-test/state)

package-linux-desktop-contract:
	@echo "Linux desktop package contract"
	@echo "  package class: $(LINUX_DESKTOP_PACKAGE_CLASS)"
	@echo "  artifact role: $(LINUX_DESKTOP_ARTIFACT_ROLE)"
	@echo "  runtime:       $(LINUX_DESKTOP_RUNTIME)"
	@echo "  version:       $(RELEASE_VERSION)"
	@echo "  platform:      $(LINUX_DESKTOP_PLATFORM)"
	@echo "  stage dir:     $(LINUX_DESKTOP_DIR)"
	@echo "  archive:       $(LINUX_DESKTOP_ARCHIVE)"
	@echo "  archive sha:   $(LINUX_DESKTOP_SHA256)"
	@echo "  archive epoch: $(LINUX_DESKTOP_PACKAGE_EPOCH)"
	@echo "  launcher:      bin/raytracing-launcher"
	@echo "  binary:        bin/raytracing-bin"
	@echo "  desktop entry: share/applications/optic.desktop"
	@echo "  icon:          share/icons/hicolor/scalable/apps/optic.svg"
	@echo "  installer:     share/install-desktop-entry.sh"

package-linux-desktop-host-check:
	@if [ "$$(uname -s)" != "Linux" ]; then \
		echo "package-linux-desktop must run on a Linux host; use the Linux PC handoff lane for proof"; \
		exit 2; \
	fi
	@if [ "$(LINUX_DESKTOP_PLATFORM)" != "linux-x86_64" ] && [ "$(LINUX_DESKTOP_PLATFORM)" != "linux-aarch64" ]; then \
		echo "unsupported Linux desktop platform: $(LINUX_DESKTOP_PLATFORM)"; \
		exit 2; \
	fi

package-linux-desktop-clean:
	@rm -rf "$(LINUX_DESKTOP_DIR)" "$(LINUX_DESKTOP_ARCHIVE)" "$(LINUX_DESKTOP_ARCHIVE_TMP)" "$(LINUX_DESKTOP_ARCHIVE_TMP).tar" "$(LINUX_DESKTOP_SHA256)" "$(RELEASE_DIR)/linux-desktop-self-test"
	@echo "Removed Linux desktop package artifacts: $(LINUX_DESKTOP_BASENAME)"

package-linux-desktop: package-linux-desktop-host-check
	@echo "Preparing Linux desktop package..."
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" "$(PACKAGE_SOURCE_BIN)"
	@rm -rf "$(LINUX_DESKTOP_DIR)" "$(LINUX_DESKTOP_ARCHIVE)" "$(LINUX_DESKTOP_ARCHIVE_TMP)" "$(LINUX_DESKTOP_SHA256)"
	@mkdir -p "$(LINUX_DESKTOP_BIN_DIR)" "$(LINUX_DESKTOP_CONFIG_DIR)" "$(LINUX_DESKTOP_SHARED_DIR)" "$(LINUX_DESKTOP_RUNTIME_DATA_DIR)/frames" "$(LINUX_DESKTOP_RUNTIME_DATA_DIR)/videos" "$(LINUX_DESKTOP_RESOURCES_DIR)/data/snapshots" "$(dir $(LINUX_DESKTOP_ENTRY))" "$(dir $(LINUX_DESKTOP_ICON))"
	@mkdir -p "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer" "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders"
	@cp "$(PACKAGE_SOURCE_BIN)" "$(LINUX_DESKTOP_BIN_DIR)/raytracing-bin"
	@cp "$(LINUX_DESKTOP_LAUNCHER_SRC)" "$(LINUX_DESKTOP_BIN_DIR)/raytracing-launcher"
	@cp "$(LINUX_DESKTOP_ENTRY_SRC)" "$(LINUX_DESKTOP_ENTRY)"
	@cp "$(LINUX_DESKTOP_ICON_SRC)" "$(LINUX_DESKTOP_ICON)"
	@cp "$(LINUX_DESKTOP_INSTALLER_SRC)" "$(LINUX_DESKTOP_INSTALLER)"
	@chmod +x "$(LINUX_DESKTOP_BIN_DIR)/raytracing-bin" "$(LINUX_DESKTOP_BIN_DIR)/raytracing-launcher" "$(LINUX_DESKTOP_INSTALLER)"
	@cp -R config/. "$(LINUX_DESKTOP_CONFIG_DIR)/"
	@if [ -d "$(SHARED_ASSETS_DIR)/fonts" ]; then \
		mkdir -p "$(LINUX_DESKTOP_SHARED_DIR)/assets/fonts"; \
		cp -R "$(SHARED_ASSETS_DIR)/fonts/." "$(LINUX_DESKTOP_SHARED_DIR)/assets/fonts/"; \
	fi
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders/"
	@printf '{\n' > "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "schema_version": "codework-desktop-package/v1",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "package_class": "%s",\n' "$(LINUX_DESKTOP_PACKAGE_CLASS)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "artifact_role": "%s",\n' "$(LINUX_DESKTOP_ARTIFACT_ROLE)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "runtime": "%s",\n' "$(LINUX_DESKTOP_RUNTIME)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "product": "%s",\n' "$(RELEASE_PRODUCT_NAME)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "platform": "%s",\n' "$(LINUX_DESKTOP_PLATFORM)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "entrypoint": "bin/raytracing-launcher",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "app_binary": "bin/raytracing-bin",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "desktop_entry": "share/applications/optic.desktop",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "icon": "share/icons/hicolor/scalable/apps/optic.svg",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "desktop_installer": "share/install-desktop-entry.sh",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "SDL2", "SDL2_ttf", "json-c", "libpng16", "vulkan-loader", "gpu-vulkan-driver"],\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "proof_requirement": "real_display_unpacked_package_smoke"\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '}\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '{\n' > "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "schema_version": "codework_package_manifest_v1",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "package_class": "%s",\n' "$(LINUX_DESKTOP_PACKAGE_CLASS)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "package_role": "%s",\n' "$(LINUX_DESKTOP_ARTIFACT_ROLE)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "runtime": "%s",\n' "$(LINUX_DESKTOP_RUNTIME)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "product": "%s",\n' "$(RELEASE_PRODUCT_NAME)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "platform": "%s",\n' "$(LINUX_DESKTOP_PLATFORM)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "entrypoints": {\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "desktop_launcher": "bin/raytracing-launcher",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "runtime_binary": "bin/raytracing-bin"\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "desktop_integration": {\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "desktop_entry": "share/applications/optic.desktop",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "icon": "share/icons/hicolor/scalable/apps/optic.svg",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "installer": "share/install-desktop-entry.sh"\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "resources": ["resources/config", "resources/shared", "resources/shaders", "resources/vk_renderer"],\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "SDL2", "SDL2_ttf", "json-c", "libpng16", "vulkan-loader", "gpu-vulkan-driver"],\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "deterministic_archive": {\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "format": "tar.gz",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "tar_sort": "name",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "mtime_epoch": %s,\n' "$(LINUX_DESKTOP_PACKAGE_EPOCH)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "owner": 0,\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "group": 0,\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "numeric_owner": true,\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "gzip_original_name": false\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "self_test": {\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "type": "command",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "argv": ["bin/raytracing-launcher", "--self-test"]\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  }\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '}\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '# optiC Linux desktop package\n\n' > "$(LINUX_DESKTOP_DIR)/README.md"
	@printf 'Private proof package for the RayTracing/optiC windowed Linux GUI.\n\n' >> "$(LINUX_DESKTOP_DIR)/README.md"
	@printf 'Run `bin/raytracing-launcher --self-test` after unpacking. Launching the GUI requires a real desktop session with SDL2 and Vulkan runtime support.\n' >> "$(LINUX_DESKTOP_DIR)/README.md"
	@$(MAKE) package-linux-desktop-archive >/dev/null
	@echo "Linux desktop package ready: $(LINUX_DESKTOP_ARCHIVE)"

package-linux-desktop-archive:
	@test -d "$(LINUX_DESKTOP_DIR)" || (echo "Missing Linux desktop package stage dir"; exit 1)
	@rm -f "$(LINUX_DESKTOP_ARCHIVE)" "$(LINUX_DESKTOP_ARCHIVE_TMP)" "$(LINUX_DESKTOP_ARCHIVE_TMP).tar" "$(LINUX_DESKTOP_SHA256)"
	@cd "$(RELEASE_DIR)" && find "$(LINUX_DESKTOP_BASENAME)" -print0 | LC_ALL=C sort -z | tar --null --no-recursion --files-from - --format=posix --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime --mtime="@$(LINUX_DESKTOP_PACKAGE_EPOCH)" --owner=0 --group=0 --numeric-owner -cf "$(abspath $(LINUX_DESKTOP_ARCHIVE_TMP)).tar"
	@"$(LINUX_DESKTOP_GZIP)" -n -c "$(LINUX_DESKTOP_ARCHIVE_TMP).tar" > "$(LINUX_DESKTOP_ARCHIVE_TMP)"
	@rm -f "$(LINUX_DESKTOP_ARCHIVE_TMP).tar"
	@mv "$(LINUX_DESKTOP_ARCHIVE_TMP)" "$(LINUX_DESKTOP_ARCHIVE)"
	@cd "$(RELEASE_DIR)" && sha256sum "$(notdir $(LINUX_DESKTOP_ARCHIVE))" > "$(notdir $(LINUX_DESKTOP_SHA256))"

package-linux-desktop-self-test: package-linux-desktop
	@$(MAKE) test-ray-tracing-folder-picker
	@$(MAKE) test-ray-tracing-path-opener
	@test -x "$(LINUX_DESKTOP_BIN_DIR)/raytracing-launcher" || (echo "Missing Linux launcher"; exit 1)
	@test -x "$(LINUX_DESKTOP_BIN_DIR)/raytracing-bin" || (echo "Missing app binary"; exit 1)
	@test -f "$(LINUX_DESKTOP_ENTRY)" || (echo "Missing Linux desktop entry"; exit 1)
	@test -f "$(LINUX_DESKTOP_ICON)" || (echo "Missing Linux desktop icon"; exit 1)
	@test -x "$(LINUX_DESKTOP_INSTALLER)" || (echo "Missing Linux desktop installer"; exit 1)
	@test -f "$(LINUX_DESKTOP_MANIFEST_JSON)" || (echo "Missing manifest.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_PACKAGE_MANIFEST)" || (echo "Missing package_manifest.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/animation_config.json" || (echo "Missing config/animation_config.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/scene_config.json" || (echo "Missing config/scene_config.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/default.ttf" || (echo "Missing config/default.ttf"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/materials/transparent.json" || (echo "Missing material fixtures"; exit 1)
	@grep -q '"thin_walled": true' "$(LINUX_DESKTOP_CONFIG_DIR)/materials/transparent.json" || (echo "Missing transparent material thin-wall fixture"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/objects/Hexagon.asset.json" || (echo "Missing bundled shape assets"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/samples/optic_build_week_showcase/scene_runtime.json" || (echo "Missing Build Week showcase scene"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/samples/optic_build_week_showcase/render_request.json" || (echo "Missing Build Week showcase request"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/samples/optic_build_week_showcase/assets/mesh_assets/asset_build_week_reflection_blob.runtime.json" || (echo "Missing Build Week reflection mesh"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/samples/optic_build_week_showcase/assets/mesh_assets/asset_build_week_grooved_orb.runtime.json" || (echo "Missing Build Week grooved orb mesh"; exit 1)
	@test -f "$(LINUX_DESKTOP_CONFIG_DIR)/samples/optic_build_week_showcase/assets/mesh_assets/asset_build_week_lattice_shell.runtime.json" || (echo "Missing Build Week lattice shell mesh"; exit 1)
	@test -d "$(LINUX_DESKTOP_RUNTIME_DATA_DIR)" || (echo "Missing runtime lane"; exit 1)
	@test -d "$(LINUX_DESKTOP_RESOURCES_DIR)/data/snapshots" || (echo "Missing snapshots lane"; exit 1)
	@test -f "$(LINUX_DESKTOP_SHARED_DIR)/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing bundled shared font"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk_renderer shader"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader"; exit 1)
	@test -f "$(LINUX_DESKTOP_ARCHIVE)" || (echo "Missing Linux desktop archive"; exit 1)
	@test -f "$(LINUX_DESKTOP_SHA256)" || (echo "Missing Linux desktop checksum"; exit 1)
	@python3 -m json.tool "$(LINUX_DESKTOP_MANIFEST_JSON)" >/dev/null
	@python3 -m json.tool "$(LINUX_DESKTOP_PACKAGE_MANIFEST)" >/dev/null
	@rm -rf "$(RELEASE_DIR)/linux-desktop-self-test"
	@mkdir -p "$(RELEASE_DIR)/linux-desktop-self-test/unpack" "$(RELEASE_DIR)/linux-desktop-self-test/home" "$(RELEASE_DIR)/linux-desktop-self-test/xdg-data" "$(LINUX_DESKTOP_SELF_TEST_RUNTIME_DIR)" "$(LINUX_DESKTOP_SELF_TEST_STATE_DIR)"
	@cd "$(RELEASE_DIR)" && sha256sum -c "$(notdir $(LINUX_DESKTOP_SHA256))"
	@tar -xzf "$(LINUX_DESKTOP_ARCHIVE)" -C "$(RELEASE_DIR)/linux-desktop-self-test/unpack"
	@HOME="$(abspath $(RELEASE_DIR)/linux-desktop-self-test/home)" XDG_DATA_HOME="$(abspath $(RELEASE_DIR)/linux-desktop-self-test/xdg-data)" "$(RELEASE_DIR)/linux-desktop-self-test/unpack/$(LINUX_DESKTOP_BASENAME)/share/install-desktop-entry.sh" >/dev/null
	@test -f "$(RELEASE_DIR)/linux-desktop-self-test/xdg-data/applications/optic.desktop" || (echo "Installer did not write optic.desktop"; exit 1)
	@test -f "$(RELEASE_DIR)/linux-desktop-self-test/xdg-data/icons/hicolor/scalable/apps/optic.svg" || (echo "Installer did not write optic.svg"; exit 1)
	@HOME="$(abspath $(RELEASE_DIR)/linux-desktop-self-test/home)" XDG_DATA_HOME="$(abspath $(RELEASE_DIR)/linux-desktop-self-test/xdg-data)" XDG_STATE_HOME="$(abspath $(RELEASE_DIR)/linux-desktop-self-test/xdg-state)" "$(RELEASE_DIR)/linux-desktop-self-test/unpack/$(LINUX_DESKTOP_BASENAME)/bin/raytracing-launcher" --print-config | grep -q '^RAY_TRACING_INPUT_ROOT=$(abspath $(RELEASE_DIR)/linux-desktop-self-test/xdg-data)/RayTracing/runtime/config$$' || (echo "Launcher did not derive input root from XDG data"; exit 1)
	@HOME="$(abspath $(RELEASE_DIR)/linux-desktop-self-test/home)" XDG_DATA_HOME="$(abspath $(RELEASE_DIR)/linux-desktop-self-test/xdg-data)" XDG_STATE_HOME="$(abspath $(RELEASE_DIR)/linux-desktop-self-test/xdg-state)" "$(RELEASE_DIR)/linux-desktop-self-test/unpack/$(LINUX_DESKTOP_BASENAME)/bin/raytracing-launcher" --print-config | grep -q '^RAY_TRACING_OUTPUT_ROOT=$(abspath $(RELEASE_DIR)/linux-desktop-self-test/xdg-data)/RayTracing/runtime/data/runtime$$' || (echo "Launcher did not derive output root from XDG data"; exit 1)
	@RAY_TRACING_RUNTIME_DIR="$(LINUX_DESKTOP_SELF_TEST_RUNTIME_DIR)" XDG_STATE_HOME="$(LINUX_DESKTOP_SELF_TEST_STATE_DIR)" "$(RELEASE_DIR)/linux-desktop-self-test/unpack/$(LINUX_DESKTOP_BASENAME)/bin/raytracing-launcher" --self-test
	@echo "package-linux-desktop-self-test passed."

package-linux-desktop-determinism-test: package-linux-desktop-self-test
	@release_dir="$(abspath $(RELEASE_DIR))"; \
		archive_path="$(abspath $(LINUX_DESKTOP_ARCHIVE))"; \
		sha_path="$(abspath $(LINUX_DESKTOP_SHA256))"; \
		first_sha="$$(cut -d ' ' -f 1 "$$sha_path")"; \
		rm -f "$$archive_path" "$$sha_path"; \
		$(MAKE) package-linux-desktop-archive >/dev/null; \
		second_sha="$$(cut -d ' ' -f 1 "$$sha_path")"; \
		if [ "$$first_sha" != "$$second_sha" ]; then \
			echo "package-linux-desktop determinism failed: $$first_sha != $$second_sha"; \
			exit 1; \
		fi; \
		echo "package-linux-desktop-determinism-test passed: $$second_sha"
