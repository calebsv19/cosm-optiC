# Ray Tracing Desktop Packaging

Last updated: 2026-06-06

## Bundle Contract

- output app: `dist/optiC.app`
- Desktop copy target: `/Users/<user>/Desktop/optiC.app`
- launcher: `Contents/MacOS/raytracing-launcher`
- runtime binary: `Contents/MacOS/raytracing-bin`
- app bundle metadata:
  - bundle id: `com.cosm.optic`
  - product name: `optiC`
  - version: `0.1.0`
- bundled frameworks live under `Contents/Frameworks/`

## Make Targets

- local packaging:
  - `make -C ray_tracing package-desktop`
  - `make -C ray_tracing package-desktop-smoke`
  - `make -C ray_tracing package-desktop-self-test`
  - `make -C ray_tracing package-desktop-copy-desktop`
  - `make -C ray_tracing package-desktop-sync`
  - `make -C ray_tracing package-desktop-open`
  - `make -C ray_tracing package-desktop-remove`
  - `make -C ray_tracing package-desktop-refresh`

Optional icon inputs:

- default local icon store:
  - `ray_tracing/tools/packaging/macos/local_app_icon/AppIcon.icns`
  - `ray_tracing/tools/packaging/macos/local_app_icon/AppIcon.iconset`
- plain `make -C ray_tracing package-desktop-refresh` now consumes that local store by default when present
- the local icon store is intentionally gitignored so icon refreshes do not pollute repo state
- canonical local icon source:
  - `/Users/<user>/Desktop/icns/optic.icns`
  - sync into the packaging lane with:
    - `bin/sync_desktop_icns.sh optic`
    - `bin/sync_desktop_icns.sh --refresh optic`

```sh
make -C ray_tracing package-desktop-refresh \
  PACKAGE_APP_ICONSET_SRC="/absolute/path/AppIcon.iconset"
```

or

```sh
make -C ray_tracing package-desktop-refresh \
  PACKAGE_APP_ICON_SRC="/absolute/path/AppIcon.icns"
```

FFmpeg packaging inputs:

- on macOS, object/test build outputs are now isolated by target architecture under `build/<arch>/` and `build_release/<arch>/` so Intel/Rosetta packaging does not reuse stale Apple Silicon objects
- package flow now prefers a target-matching `ffmpeg` in this order:
  - explicit `PACKAGE_FFMPEG_SRC=/absolute/path/to/ffmpeg`
  - `$(TARGET_HOMEBREW_PREFIX)/bin/ffmpeg`
  - `$(TARGET_ALT_HOMEBREW_PREFIX)/bin/ffmpeg`
  - `/usr/local/bin/ffmpeg`
  - `/opt/homebrew/bin/ffmpeg`
  - `/usr/bin/ffmpeg`
- for release or Intel handoff builds, require the bundle to carry `ffmpeg`:

```sh
HOME=/private/tmp/codex-raytracing-x86-home \
make -C ray_tracing release-artifact \
  TARGET_ARCH=x86_64 \
  BUILD_TOOLCHAIN=clang \
  PACKAGE_TOOLCHAIN=clang
```

- if the host does not already expose an `x86_64` `ffmpeg` in the search order above, pass an explicit override:

```sh
HOME=/private/tmp/codex-raytracing-x86-home \
make -C ray_tracing release-artifact \
  TARGET_ARCH=x86_64 \
  BUILD_TOOLCHAIN=clang \
  PACKAGE_TOOLCHAIN=clang \
  PACKAGE_FFMPEG_SRC="/absolute/path/to/x86_64/ffmpeg"
```

If the local store or either override variable is present, packaging will bundle `Contents/Resources/AppIcon.icns` and the app plist will advertise `CFBundleIconFile=AppIcon`.

Local asset note:
- because `tools/packaging/macos/local_app_icon/` is gitignored, a fresh clone does not automatically carry your chosen app icon until you copy one into that directory
- release/readiness:
  - `make -C ray_tracing release-contract`
  - `make -C ray_tracing release-bundle-audit`
  - `make -C ray_tracing release-sign APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"`
  - `make -C ray_tracing release-notarize APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="<profile>"`
  - `make -C ray_tracing release-staple`
  - `make -C ray_tracing release-verify-notarized`
  - `make -C ray_tracing release-artifact`
  - `make -C ray_tracing release-distribute APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="<profile>"`
  - `make -C ray_tracing release-desktop-refresh`
  - current 2026-06-06 notarized artifact set:
    - `build/release/optiC-0.2.0-macOS-arm64-stable.zip`
    - `build/release/optiC-0.2.0-macOS-arm64-stable.zip.sha256`
    - `build/release/optiC-0.2.0-macOS-arm64-stable.manifest.txt`
    - `build/release/notary_submit.json`

## Launcher Runtime Contract

- `--print-config` prints:
  - `RAY_TRACING_RUNTIME_DIR`
  - `VK_RENDERER_SHADER_ROOT`
  - `SHAPE_ASSET_DIR`
  - `RAY_TRACING_FONT_PRESET`
  - `RAY_TRACING_RENDER_METRICS_DATASET_PATH`
  - `VK_ICD_FILENAMES`
  - `VK_DRIVER_FILES`
  - `MOLTENVK_DYLIB`
- `--self-test` verifies:
  - packaged runtime binary is executable
  - config/default asset presence is intact
  - runtime directories are writable
  - Vulkan portability files and shaders are present
  - when `PACKAGE_REQUIRE_FFMPEG=1`, bundled `ffmpeg` is present and exported through `RAY_TRACING_FFMPEG_BIN`
- launcher runtime root:
  - default: `~/Library/Application Support/RayTracing/runtime`
  - tmp fallback: `${TMPDIR:-/tmp}/RayTracing/runtime`
- launcher font baseline:
  - default packaged `RAY_TRACING_FONT_PRESET=ide`
  - override remains available through the environment when bounded comparison/testing is needed
- launcher log lane:
  - `~/Library/Logs/RayTracing/launcher.log`
  - tmp fallback: `${TMPDIR:-/tmp}/raytracing-launcher.log`

## Packaged Resource And Framework Contract

Bundled resource lanes include:
- `Resources/config/`
- `Resources/data/runtime/`
- `Resources/data/runtime/frames/`
- `Resources/data/runtime/videos/`
- `Resources/data/snapshots/`
- `Resources/vk_renderer/shaders/`
- `Resources/shaders/`
- `Resources/shared/`

Bundled framework/runtime rules include:
- `Contents/Frameworks/libMoltenVK.dylib`
- `Contents/Frameworks/libvulkan.1.dylib`
- launcher-generated ICD JSON under writable runtime root
- package step rewrites local dylib install names and ad-hoc signs bundled dylibs, binaries, and the app bundle for local launch safety

## Recommended Local Validation

1. `make -C ray_tracing clean && make -C ray_tracing`
2. `make -C ray_tracing test-stable`
3. `make -C ray_tracing package-desktop-self-test`
4. `make -C ray_tracing release-bundle-audit`
5. `make -C ray_tracing package-desktop-refresh`
6. `/Users/<user>/Desktop/optiC.app/Contents/MacOS/raytracing-launcher --print-config`
7. `open /Users/<user>/Desktop/optiC.app`
8. `tail -n 120 ~/Library/Logs/RayTracing/launcher.log`

## Current Limits

- This doc describes the current local packaged-app and release-readiness workflow only.
- The 2026-06-06 pass did produce a fresh notarized artifact set for the current `0.2.0` macOS arm64 worktree.
- Current repo-state presence of `dist/`, Desktop, or `build/release/` artifacts should be checked separately from this doc before making artifact-presence claims.
