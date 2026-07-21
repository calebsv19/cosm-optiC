package-desktop:
	@echo "Preparing desktop package..."
	@$(MAKE) BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" "$(PACKAGE_SOURCE_BIN)"
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)" "$(PACKAGE_TOOLS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Set :CFBundleShortVersionString $(RELEASE_VERSION)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c 'Set :CFBundleVersion $(RELEASE_VERSION)' "$(PACKAGE_CONTENTS_DIR)/Info.plist"
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
	@for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$dylib"; \
	done
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-bin"
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-launcher"
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
	@test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/animation_config.json" || (echo "Missing config/animation_config.json"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/scene_config.json" || (echo "Missing config/scene_config.json"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/default.ttf" || (echo "Missing config/default.ttf"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing shared packaged font"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/objects/Hexagon.asset.json" || (echo "Missing bundled shape assets"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/samples/optic_build_week_showcase/scene_runtime.json" || (echo "Missing Build Week showcase scene"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/starter_scene_profile.json" || (echo "Missing starter scene profile"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/samples/optic_build_week_showcase/render_request.json" || (echo "Missing Build Week showcase request"; exit 1)
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
