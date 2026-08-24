.PHONY: package-desktop-main-edit package-desktop-main-edit-self-test package-desktop-main-edit-refresh package-desktop-main-edit-open

package-desktop:
	@echo "Preparing desktop package..."
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" "$(PACKAGE_SOURCE_BIN)"
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)" "$(PACKAGE_TOOLS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Set :CFBundleIdentifier $(PACKAGE_BUNDLE_ID)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Set :CFBundleName $(PACKAGE_DISPLAY_NAME)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Add :CFBundleDisplayName string $(PACKAGE_DISPLAY_NAME)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Set :CFBundleShortVersionString $(RELEASE_VERSION)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Set :CFBundleVersion $(RELEASE_VERSION)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Add :OpticPackageProfile string $(PACKAGE_PROFILE)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Add :OpticRuntimeNamespace string $(PACKAGE_RUNTIME_NAMESPACE)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Add :OpticLogNamespace string $(PACKAGE_LOG_NAMESPACE)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Add :OpticBuildLabel string $(PACKAGE_BUILD_LABEL)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(PACKAGE_SOURCE_BIN)" "$(PACKAGE_MACOS_DIR)/raytracing-bin"
	@cp "$(PACKAGE_LAUNCHER_SRC)" "$(PACKAGE_MACOS_DIR)/raytracing-launcher"
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" "$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/raytracing-bin" "$(PACKAGE_FRAMEWORKS_DIR)"
	@chmod +x "$(PACKAGE_MACOS_DIR)/raytracing-bin" "$(PACKAGE_MACOS_DIR)/raytracing-launcher"
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ]; then \
		cp "$(PACKAGE_APP_ICON_SRC)" "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICON_SRC)"; \
	elif [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		/usr/bin/iconutil -c icns -o "$(PACKAGE_BUNDLED_ICON_PATH)" "$(PACKAGE_APP_ICONSET_SRC)" || exit 1; \
		echo "Bundled app icon from $(PACKAGE_APP_ICONSET_SRC)"; \
	else \
		echo "warning: no app icon source found at $(PACKAGE_APP_ICON_SRC) or $(PACKAGE_APP_ICONSET_SRC)"; \
	fi
	@ffmpeg_src=""; \
	ffmpeg_archs=""; \
	if [ -n "$(PACKAGE_FFMPEG_SRC)" ] && [ -x "$(PACKAGE_FFMPEG_SRC)" ]; then \
		ffmpeg_src="$(PACKAGE_FFMPEG_SRC)"; \
	else \
		for candidate in "$(TARGET_HOMEBREW_PREFIX)/bin/ffmpeg" "$(TARGET_ALT_HOMEBREW_PREFIX)/bin/ffmpeg" /usr/local/bin/ffmpeg /opt/homebrew/bin/ffmpeg /usr/bin/ffmpeg; do \
			if [ -x "$$candidate" ]; then \
				ffmpeg_src="$$candidate"; \
				break; \
			fi; \
		done; \
	fi; \
	if [ -n "$$ffmpeg_src" ]; then \
		ffmpeg_archs="$$(/usr/bin/lipo -archs "$$ffmpeg_src" 2>/dev/null || /usr/bin/file -b "$$ffmpeg_src" 2>/dev/null || true)"; \
	fi; \
	case " $$ffmpeg_archs " in \
		*" $(TARGET_ARCH) "*) ffmpeg_arch_match=1 ;; \
		*) ffmpeg_arch_match=0 ;; \
	esac; \
	if [ -n "$$ffmpeg_src" ] && [ "$$ffmpeg_arch_match" = "1" ]; then \
		cp "$$ffmpeg_src" "$(PACKAGE_TOOLS_DIR)/ffmpeg"; \
		chmod +x "$(PACKAGE_TOOLS_DIR)/ffmpeg"; \
		echo "Bundled ffmpeg from $$ffmpeg_src"; \
	elif [ "$(PACKAGE_REQUIRE_FFMPEG)" = "1" ]; then \
		if [ -n "$$ffmpeg_src" ]; then \
			echo "Missing TARGET_ARCH=$(TARGET_ARCH) ffmpeg: $$ffmpeg_src provides '$${ffmpeg_archs:-unknown}'"; \
		else \
			echo "Missing TARGET_ARCH=$(TARGET_ARCH) ffmpeg: searched $(TARGET_HOMEBREW_PREFIX)/bin, $(TARGET_ALT_HOMEBREW_PREFIX)/bin, /usr/local/bin, /opt/homebrew/bin, and /usr/bin"; \
		fi; \
		echo "Set PACKAGE_FFMPEG_SRC=/absolute/path/to/ffmpeg or install an $(TARGET_ARCH) ffmpeg build for packaging."; \
		exit 1; \
	else \
		echo "Skipping bundled ffmpeg for TARGET_ARCH=$(TARGET_ARCH)"; \
	fi
	@cp -R config "$(PACKAGE_RESOURCES_DIR)/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts"
	@cp -R "$(SHARED_ASSETS_DIR)/fonts/." "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/data/runtime" "$(PACKAGE_RESOURCES_DIR)/data/runtime/frames" "$(PACKAGE_RESOURCES_DIR)/data/runtime/videos" "$(PACKAGE_RESOURCES_DIR)/data/snapshots"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/vk_renderer" "$(PACKAGE_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(PACKAGE_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(PACKAGE_RESOURCES_DIR)/shaders/"
	@/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' \
		-exec /usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none {} \;
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-bin"
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-launcher"
	@if [ "$(PACKAGE_EMBED_BUILD_IDENTITY)" = "1" ]; then \
		python3 "$(PACKAGE_BUILD_IDENTITY_TOOL)" \
			--output "$(PACKAGE_RESOURCES_DIR)/build_identity.json" \
			--binary "$(PACKAGE_MACOS_DIR)/raytracing-bin" \
			--profile "$(PACKAGE_PROFILE)" \
			--program "$(RELEASE_PROGRAM_KEY)" \
			--product "$(PACKAGE_DISPLAY_NAME)" \
			--version "$(RELEASE_VERSION)" \
			--architecture "$(TARGET_ARCH)" \
			--toolchain "$(PACKAGE_TOOLCHAIN)" \
			--branch "$(PACKAGE_SOURCE_BRANCH)" \
			--commit "$(PACKAGE_SOURCE_COMMIT)" \
			--dirty "$(PACKAGE_SOURCE_DIRTY)" \
			--source-fingerprint "$(PACKAGE_SOURCE_FINGERPRINT)" \
			--build-label "$(PACKAGE_BUILD_LABEL)"; \
	fi
	@if [ -x "$(PACKAGE_TOOLS_DIR)/ffmpeg" ]; then \
		/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_TOOLS_DIR)/ffmpeg"; \
	fi
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/raytracing-launcher" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/raytracing-bin" || (echo "Missing app binary"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib" || (echo "Missing bundled libvulkan.1.dylib"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled libMoltenVK.dylib"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_BUNDLE_ID)" || (echo "Packaged bundle identifier mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleDisplayName' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_DISPLAY_NAME)" || (echo "Packaged display name mismatch"; exit 1)
	@test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/animation_config.json" || (echo "Missing config/animation_config.json"; exit 1)
	@/usr/bin/grep -Eq '"renderScale3D"[[:space:]]*:[[:space:]]*0' "$(PACKAGE_RESOURCES_DIR)/config/animation_config.json" || (echo "Packaged animation config does not default to automatic HiDPI"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/scene_config.json" || (echo "Missing config/scene_config.json"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/default.ttf" || (echo "Missing config/default.ttf"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing shared packaged font"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/objects/Hexagon.asset.json" || (echo "Missing bundled shape assets"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/samples/optic_build_week_showcase/scene_runtime.json" || (echo "Missing Build Week showcase scene"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/starter_scene_profile.json" || (echo "Missing starter scene profile"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/samples/optic_build_week_showcase/render_request.json" || (echo "Missing Build Week showcase request"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/samples/light_timeline_editor_demo_runtime.json" || (echo "Missing light timeline editor demo scene"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/samples/light_timeline_editor_demo_request.json" || (echo "Missing light timeline editor demo request"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/samples/optic_build_week_showcase/assets/mesh_assets/asset_build_week_reflection_blob.runtime.json" || (echo "Missing Build Week reflection mesh"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/samples/optic_build_week_showcase/assets/mesh_assets/asset_build_week_grooved_orb.runtime.json" || (echo "Missing Build Week grooved orb mesh"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/samples/optic_build_week_showcase/assets/mesh_assets/asset_build_week_lattice_shell.runtime.json" || (echo "Missing Build Week lattice shell mesh"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing runtime dir"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime/frames" || (echo "Missing runtime frames dir"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime/videos" || (echo "Missing runtime videos dir"; exit 1)
	@if [ "$(PACKAGE_REQUIRE_FFMPEG)" = "1" ]; then \
		test -x "$(PACKAGE_TOOLS_DIR)/ffmpeg" || (echo "Missing bundled ffmpeg"; exit 1); \
	fi
	@test -f "$(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk_renderer shader"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader"; exit 1)
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@"$(PACKAGE_MACOS_DIR)/raytracing-launcher" --self-test || (echo "package-desktop self-test failed."; exit 1)
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop
	@if [ ! -d "$(CURDIR)/.git" ] && [ "$${RAY_TRACING_ALLOW_WORKTREE_DESKTOP_REFRESH:-0}" != "1" ]; then \
		echo "Refusing Desktop app overwrite from linked worktree: $(CURDIR)"; \
		echo "Use the canonical ray_tracing checkout, or explicitly set RAY_TRACING_ALLOW_WORKTREE_DESKTOP_REFRESH=1."; \
		exit 1; \
	fi
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop package synchronized: $(DESKTOP_APP_DIR)"

package-desktop-open: package-desktop
	@open "$(PACKAGE_APP_DIR)"

package-desktop-remove:
	@rm -rf "$(PACKAGE_APP_DIR)"
	@echo "Removed desktop package: $(PACKAGE_APP_DIR)"

package-desktop-refresh: package-desktop
	@if [ ! -d "$(CURDIR)/.git" ] && [ "$${RAY_TRACING_ALLOW_WORKTREE_DESKTOP_REFRESH:-0}" != "1" ]; then \
		echo "Refusing Desktop app overwrite from linked worktree: $(CURDIR)"; \
		echo "Use the canonical ray_tracing checkout, or explicitly set RAY_TRACING_ALLOW_WORKTREE_DESKTOP_REFRESH=1."; \
		exit 1; \
	fi
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"

package-desktop-main-edit:
	@set -eu; \
	source_before="$$(python3 "$(PACKAGE_SOURCE_FINGERPRINT_TOOL)" "$(CURDIR)")"; \
	branch="$$(git -C "$(CURDIR)" symbolic-ref --quiet --short HEAD 2>/dev/null || printf '%s' detached)"; \
	commit="$$(git -C "$(CURDIR)" rev-parse HEAD)"; \
	short_commit="$$(git -C "$(CURDIR)" rev-parse --short=12 HEAD)"; \
	dirty=false; \
	if [ -n "$$(git -C "$(CURDIR)" status --porcelain --untracked-files=all)" ]; then dirty=true; fi; \
	build_label="$(MAIN_EDIT_PACKAGE_PROFILE)-$(RELEASE_VERSION)-$$short_commit"; \
	$(MAKE) package-desktop-smoke \
		DIST_DIR="$(MAIN_EDIT_PACKAGE_DIST_DIR)" \
		PACKAGE_APP_NAME="$(MAIN_EDIT_PACKAGE_APP_NAME)" \
		PACKAGE_PROFILE="$(MAIN_EDIT_PACKAGE_PROFILE)" \
		PACKAGE_BUNDLE_ID="$(MAIN_EDIT_PACKAGE_BUNDLE_ID)" \
		PACKAGE_DISPLAY_NAME="$(MAIN_EDIT_PACKAGE_DISPLAY_NAME)" \
		PACKAGE_RUNTIME_NAMESPACE="$(MAIN_EDIT_PACKAGE_RUNTIME_NAMESPACE)" \
		PACKAGE_LOG_NAMESPACE="$(MAIN_EDIT_PACKAGE_LOG_NAMESPACE)" \
		PACKAGE_BUILD_LABEL="$$build_label" \
		PACKAGE_EMBED_BUILD_IDENTITY=1 \
		PACKAGE_SOURCE_BRANCH="$$branch" \
		PACKAGE_SOURCE_COMMIT="$$commit" \
		PACKAGE_SOURCE_DIRTY="$$dirty" \
		PACKAGE_SOURCE_FINGERPRINT="$$source_before"; \
	source_after="$$(python3 "$(PACKAGE_SOURCE_FINGERPRINT_TOOL)" "$(CURDIR)")"; \
	if [ "$$source_before" != "$$source_after" ]; then \
		echo "Source changed during main-edit package build; refusing ambiguous app identity."; \
		rm -rf "$(MAIN_EDIT_PACKAGE_APP_DIR)"; \
		exit 1; \
	fi; \
	echo "Main-edit desktop package ready: $(MAIN_EDIT_PACKAGE_APP_DIR)"; \
	echo "Source fingerprint: $$source_after"

package-desktop-main-edit-self-test: package-desktop-main-edit
	@rm -rf "$(BUILD_DIR_BASE)/package-main-edit-self-test"
	@mkdir -p "$(BUILD_DIR_BASE)/package-main-edit-self-test/home"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(MAIN_EDIT_PACKAGE_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_PACKAGE_BUNDLE_ID)" || (echo "Main-edit bundle identifier mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleDisplayName' "$(MAIN_EDIT_PACKAGE_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_PACKAGE_DISPLAY_NAME)" || (echo "Main-edit display name mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleShortVersionString' "$(MAIN_EDIT_PACKAGE_APP_DIR)/Contents/Info.plist")" = "$(RELEASE_VERSION)" || (echo "Main-edit program version mismatch"; exit 1)
	@test -f "$(MAIN_EDIT_PACKAGE_APP_DIR)/Contents/Resources/build_identity.json" || (echo "Missing main-edit build identity"; exit 1)
	@python3 tools/packaging/macos/verify-build-identity.py \
		--identity "$(MAIN_EDIT_PACKAGE_APP_DIR)/Contents/Resources/build_identity.json" \
		--binary "$(MAIN_EDIT_PACKAGE_APP_DIR)/Contents/MacOS/raytracing-bin" \
		--source-root "$(CURDIR)" \
		--profile "$(MAIN_EDIT_PACKAGE_PROFILE)" \
		--version "$(RELEASE_VERSION)"
	@HOME="$(abspath $(BUILD_DIR_BASE)/package-main-edit-self-test/home)" "$(MAIN_EDIT_PACKAGE_APP_DIR)/Contents/MacOS/raytracing-launcher" --self-test
	@HOME="$(abspath $(BUILD_DIR_BASE)/package-main-edit-self-test/home)" "$(MAIN_EDIT_PACKAGE_APP_DIR)/Contents/MacOS/raytracing-launcher" --print-config > "$(BUILD_DIR_BASE)/package-main-edit-self-test/print-config.txt"
	@/usr/bin/grep -F "RAY_TRACING_PACKAGE_PROFILE=$(MAIN_EDIT_PACKAGE_PROFILE)" "$(BUILD_DIR_BASE)/package-main-edit-self-test/print-config.txt" >/dev/null || (echo "Main-edit launcher profile mismatch"; exit 1)
	@/usr/bin/grep -F "/Library/Application Support/$(MAIN_EDIT_PACKAGE_RUNTIME_NAMESPACE)/runtime" "$(BUILD_DIR_BASE)/package-main-edit-self-test/print-config.txt" >/dev/null || (echo "Main-edit runtime namespace mismatch"; exit 1)
	@/usr/bin/grep -F "/Library/Logs/$(MAIN_EDIT_PACKAGE_LOG_NAMESPACE)/launcher.log" "$(BUILD_DIR_BASE)/package-main-edit-self-test/print-config.txt" >/dev/null || (echo "Main-edit log namespace mismatch"; exit 1)
	@/usr/bin/codesign --verify --deep --strict "$(MAIN_EDIT_PACKAGE_APP_DIR)"
	@echo "package-desktop-main-edit-self-test passed."

package-desktop-main-edit-refresh: package-desktop-main-edit-self-test
	@mkdir -p "$(dir $(MAIN_EDIT_DESKTOP_APP_DIR))"
	@rm -rf "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(MAIN_EDIT_PACKAGE_APP_DIR)" "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@echo "Refreshed isolated main-edit app at $(MAIN_EDIT_DESKTOP_APP_DIR)"

package-desktop-main-edit-open: package-desktop-main-edit-refresh
	@open "$(MAIN_EDIT_DESKTOP_APP_DIR)"
