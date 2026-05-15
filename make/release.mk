release-contract:
	@mkdir -p "$(RELEASE_DIR)"
	@echo "release-contract:"
	@echo "  product: $(RELEASE_PRODUCT_NAME)"
	@echo "  program: $(RELEASE_PROGRAM_KEY)"
	@echo "  version: $(RELEASE_VERSION)"
	@echo "  channel: $(RELEASE_CHANNEL)"
	@echo "  bundle_id: $(RELEASE_BUNDLE_ID)"
	@echo "  app_name: $(PACKAGE_APP_NAME)"
	@echo "  artifact_base: $(RELEASE_ARTIFACT_BASENAME)"
	@echo "  release_zip: $(RELEASE_APP_ZIP)"
	@echo "  signing_identity: $(RELEASE_CODESIGN_IDENTITY)"
	@echo "  notary_profile_set: $$( [ -n \"$(APPLE_NOTARY_PROFILE)\" ] && echo yes || echo no )"
	@echo "  team_id_set: $$( [ -n \"$(APPLE_TEAM_ID)\" ] && echo yes || echo no )"

release-clean:
	@rm -rf "$(RELEASE_DIR)"
	@echo "Removed release dir: $(RELEASE_DIR)"

release-build: all
	@echo "Release build complete: $(TARGET)"

release-bundle-audit: PACKAGE_REQUIRE_FFMPEG=1
release-bundle-audit: package-desktop-self-test
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_id.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_id.txt")" = "$(RELEASE_BUNDLE_ID)" || (echo "bundle id mismatch: expected $(RELEASE_BUNDLE_ID), got $$(cat "$(RELEASE_DIR)/bundle_id.txt")"; exit 1)
	@env -i HOME="$(HOME)" PATH="$(PATH)" "$(PACKAGE_MACOS_DIR)/raytracing-launcher" --print-config > "$(RELEASE_DIR)/print_config.txt"
	@runtime_dir="$$(/usr/bin/grep '^RAY_TRACING_RUNTIME_DIR=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$runtime_dir" ]; then echo "runtime dir missing from print-config"; exit 1; fi; \
	case "$$runtime_dir" in *"/Contents/Resources"*) echo "runtime dir incorrectly points into app bundle: $$runtime_dir"; exit 1;; esac; \
	case "$$runtime_dir" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "runtime dir is not user-writable rooted: $$runtime_dir"; exit 1;; esac
	@dataset_path="$$(/usr/bin/grep '^RAY_TRACING_RENDER_METRICS_DATASET_PATH=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$dataset_path" ]; then echo "render metrics dataset path missing from print-config"; exit 1; fi; \
	case "$$dataset_path" in *"/Contents/Resources"*) echo "render metrics dataset path incorrectly points into app bundle: $$dataset_path"; exit 1;; esac; \
	case "$$dataset_path" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "render metrics dataset path is not user-writable rooted: $$dataset_path"; exit 1;; esac
	@ffmpeg_bin="$$(/usr/bin/grep '^RAY_TRACING_FFMPEG_BIN=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$ffmpeg_bin" ]; then echo "ffmpeg path missing from print-config"; exit 1; fi; \
	test -x "$$ffmpeg_bin" || (echo "ffmpeg path from print-config is not executable: $$ffmpeg_bin"; exit 1)
	@/usr/bin/grep -q '^VK_ICD_FILENAMES=' "$(RELEASE_DIR)/print_config.txt" || (echo "missing VK_ICD_FILENAMES in print-config"; exit 1)
	@/usr/bin/grep -q '^VK_DRIVER_FILES=' "$(RELEASE_DIR)/print_config.txt" || (echo "missing VK_DRIVER_FILES in print-config"; exit 1)
	@otool -L "$(PACKAGE_MACOS_DIR)/raytracing-bin" > "$(RELEASE_DIR)/otool_raytracing_bin.txt"
	@if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_raytracing_bin.txt"; then \
		echo "non-portable dylib dependency detected in $(PACKAGE_MACOS_DIR)/raytracing-bin"; \
		cat "$(RELEASE_DIR)/otool_raytracing_bin.txt"; \
		exit 1; \
	fi
	@for file in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		base="$$(/usr/bin/basename "$$file")"; \
		otool -L "$$file" > "$(RELEASE_DIR)/otool_$$base.txt" || exit 1; \
		if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_$$base.txt"; then \
			echo "non-portable dylib dependency detected in $$file"; \
			cat "$(RELEASE_DIR)/otool_$$base.txt"; \
			exit 1; \
		fi; \
	done
	@echo "release-bundle-audit passed."

release-sign: release-bundle-audit
	@echo "Signing with identity: $(RELEASE_CODESIGN_IDENTITY)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-launcher"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"; \
	else \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/raytracing-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/raytracing-launcher"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_APP_DIR)"; \
	fi
	@echo "release-sign complete."

release-verify: release-sign
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-verify note: ad-hoc identity in use; skipping spctl Gatekeeper assessment"; \
	else \
		spctl_output="$$(spctl --assess --type execute --verbose=2 "$(PACKAGE_APP_DIR)" 2>&1)"; \
		spctl_status=$$?; \
		if [ $$spctl_status -ne 0 ]; then \
			if printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "internal error in Code Signing subsystem"; then \
				echo "release-verify note: spctl internal subsystem error on this host; codesign verification remains authoritative"; \
			elif printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "Unnotarized Developer ID"; then \
				echo "release-verify note: app is Developer ID signed but not notarized yet"; \
			else \
				printf '%s\n' "$$spctl_output"; \
				exit $$spctl_status; \
			fi; \
		else \
			printf '%s\n' "$$spctl_output"; \
		fi; \
	fi
	@echo "release-verify passed."

release-verify-signed: release-sign release-verify
	@echo "release-verify-signed passed."

release-notarize: release-sign
	@if [ -z "$(APPLE_NOTARY_PROFILE)" ]; then \
		echo "APPLE_NOTARY_PROFILE is required for release-notarize"; \
		exit 1; \
	fi
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-notarize requires a real Developer ID signing identity (APPLE_SIGN_IDENTITY)"; \
		exit 1; \
	fi
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@submission_json="$$(xcrun notarytool submit "$(RELEASE_APP_ZIP)" --keychain-profile "$(APPLE_NOTARY_PROFILE)" --wait --output-format json)"; \
	echo "$$submission_json" > "$(RELEASE_DIR)/notary_submit.json"; \
	status="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*\"status\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p' | /usr/bin/tail -n 1)"; \
	if [ "$$status" != "Accepted" ]; then \
		submission_id="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*\"id\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p' | /usr/bin/head -n 1)"; \
		echo "release-notarize failed: status=$$status id=$$submission_id"; \
		if [ -n "$$submission_id" ]; then \
			xcrun notarytool log "$$submission_id" --keychain-profile "$(APPLE_NOTARY_PROFILE)" > "$(RELEASE_DIR)/notary_log_$$submission_id.json" || true; \
			echo "notary log: $(RELEASE_DIR)/notary_log_$$submission_id.json"; \
		fi; \
		exit 1; \
	fi
	@echo "release-notarize passed."

release-staple:
	@attempt=1; \
	while [ $$attempt -le "$(STAPLE_MAX_ATTEMPTS)" ]; do \
		if xcrun stapler staple "$(PACKAGE_APP_DIR)"; then \
			break; \
		fi; \
		if [ $$attempt -eq "$(STAPLE_MAX_ATTEMPTS)" ]; then \
			echo "release-staple failed after $$attempt attempts"; \
			exit 1; \
		fi; \
		echo "release-staple retry $$attempt/$(STAPLE_MAX_ATTEMPTS) in $(STAPLE_RETRY_DELAY_SEC)s"; \
		sleep "$(STAPLE_RETRY_DELAY_SEC)"; \
		attempt=$$((attempt + 1)); \
	done
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-staple passed."

release-verify-notarized: release-verify
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-verify-notarized passed."

release-artifact: release-verify
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@shasum -a 256 "$(RELEASE_APP_ZIP)" > "$(RELEASE_APP_ZIP).sha256"
	@{ \
		echo "product=$(RELEASE_PRODUCT_NAME)"; \
		echo "program=$(RELEASE_PROGRAM_KEY)"; \
		echo "bundle_id=$(RELEASE_BUNDLE_ID)"; \
		echo "version=$(RELEASE_VERSION)"; \
		echo "channel=$(RELEASE_CHANNEL)"; \
		echo "platform=$(RELEASE_PLATFORM)"; \
		echo "arch=$(RELEASE_ARCH)"; \
		echo "artifact=$(RELEASE_APP_ZIP)"; \
		echo "sha256_file=$(RELEASE_APP_ZIP).sha256"; \
	} > "$(RELEASE_MANIFEST)"
	@echo "release-artifact complete: $(RELEASE_APP_ZIP)"

release-distribute: release-notarize release-staple release-verify-notarized release-artifact
	@echo "release-distribute passed."

release-desktop-refresh:
	@if [ ! -d "$(PACKAGE_APP_DIR)" ]; then \
		echo "release-desktop-refresh requires an existing built app at $(PACKAGE_APP_DIR)"; \
		echo "run release-distribute first"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Release app refreshed at $(DESKTOP_APP_DIR)"
