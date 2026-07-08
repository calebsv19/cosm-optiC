# Ray Tracing Desktop Packaging

Last updated: 2026-07-08

## Bundle Contract

- output app: `dist/optiC.app`
- Desktop copy target: `/Users/<user>/Desktop/optiC.app`
- launcher: `Contents/MacOS/raytracing-launcher`
- runtime binary: `Contents/MacOS/raytracing-bin`
- app bundle metadata:
  - bundle id: `com.cosm.optic`
  - product name: `optiC`
  - version: generated from `RELEASE_VERSION` during package creation
- bundled frameworks live under `Contents/Frameworks/`

## Public Package Discovery

The current public desktop package release is `optiC 0.5.0`.

Fresh agents should discover public release downloads through the Ecosystem
agent manifest:

```text
https://ecosystem.calebsv.tech/agents/programs/optic.json
```

The manifest links both macOS desktop architectures:

- `optiC-0.5.0-macOS-arm64-stable.zip`
- `optiC-0.5.0-macOS-x86_64-stable.zip`

It also provides immutable version URLs, SHA-256 values, and size metadata. A
public package smoke should verify those values without rebuilding, replacing,
or republishing the release artifacts.

Known readback caveat for the already-published `0.5.0` package: the public
metadata and filenames identify `0.5.0`, while the app bundle plist still
reports `CFBundleShortVersionString=0.1.0`. Treat that as package identity
hygiene for a future approved package build, not as approval to replace the
already-published artifacts in place.

Future release-artifact hygiene:

- `release-bundle-audit` now rejects packaged app bundles whose
  `CFBundleShortVersionString` or `CFBundleVersion` does not equal
  `RELEASE_VERSION`.
- `release-artifact` stages the already-notarized app plus a ZIP-root
  `.manifest.txt` file before creating the release ZIP. The manifest is outside
  `optiC.app`, so adding it does not mutate the signed/notarized app bundle.
- the embedded ZIP-root manifest is self-describing but cannot contain the
  final ZIP digest without creating a circular hash dependency. The adjacent
  external `.manifest.txt` and `.zip.sha256` sidecars remain authoritative for
  the final archive digest.
- checksum sidecars name the ZIP basename, not a local build path, so fresh
  public agents can validate sidecars without knowing the maintainer's
  filesystem layout.

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
- private Linux desktop package proof:
  - `make -C ray_tracing package-linux-desktop-contract`
  - `make -C ray_tracing package-linux-desktop`
  - `make -C ray_tracing package-linux-desktop-self-test`

Linux desktop package target:

- status:
  private proof target only; not a public release target yet
- package class:
  `desktop_app_linux`
- artifact role:
  `desktop_app`
- runtime:
  `linux_gui`
- expected private artifact name:
  `optiC-<version>-linux-x86_64-desktop-stable.tar.gz`
- package root layout:

```text
optiC-<version>-linux-x86_64-desktop-stable/
  bin/raytracing-launcher
  bin/raytracing-bin
  resources/config/
  resources/shared/
  resources/shaders/
  resources/vk_renderer/
  resources/data/runtime/
  manifest.json
  package_manifest.json
  README.md
```

The Linux launcher copies bundled resources into a writable runtime directory,
then launches `bin/raytracing-bin` from that runtime root. By default it uses
`${XDG_DATA_HOME:-$HOME/.local/share}/RayTracing/runtime`, but proof helpers can
override `RAY_TRACING_RUNTIME_DIR` and `XDG_STATE_HOME` so package smoke runs
stay thread-local.

Before any public Linux desktop release, the package must pass a real-display
unpacked-package proof on the Linux PC. The minimum proof is:

- build from a named source branch/commit;
- run `package-linux-desktop-self-test`;
- extract the tarball to a clean directory;
- run `bin/raytracing-launcher --self-test` from the unpacked package;
- launch `bin/raytracing-launcher` in the real KDE/X11 desktop session;
- capture menu, post-action, and late screenshots;
- confirm `[Menu] Start pressed` through the package launcher log;
- keep the artifact role explicit so it cannot be confused with the Linux
  headless worker tarball.

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
  - historical 2026-06-08 notarized artifact set:
    - `build/release/optiC-0.3.0-macOS-arm64-stable.zip`
    - `build/release/optiC-0.3.0-macOS-arm64-stable.zip.sha256`
    - `build/release/optiC-0.3.0-macOS-arm64-stable.manifest.txt`
    - `build/release/optiC-0.3.0-macOS-x86_64-stable.zip`
    - `build/release/optiC-0.3.0-macOS-x86_64-stable.zip.sha256`
    - `build/release/optiC-0.3.0-macOS-x86_64-stable.manifest.txt`
    - `build/release/notary_submit.json`

Release and worker artifact hygiene:
- `make -C ray_tracing package-linux-worker-self-test` validates the Linux
  worker tarball structure, rejects private/generated review lanes, suppresses
  macOS AppleDouble archive sidecars, and limits executable file bits to the
  intended worker binaries and wrapper.
- `release-contract` reports whether signing/notary/team inputs are configured
  without printing identity, profile, or team values.
- Signing, notarization, upload, and distribution targets remain explicit
  operator actions; the contract and self-test targets do not sign, notarize,
  upload, or publish.
- Future release ZIPs produced by `release-artifact` include a ZIP-root
  `.manifest.txt` entry and adjacent `.manifest.txt` / `.zip.sha256` sidecars.
  Already-published `0.5.0` ZIP contents are unchanged.

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
  - bundled material preset JSON has been refreshed into the writable runtime cache
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
- `Resources/config/materials/*.json` as shipped preset sources refreshed into
  the writable runtime cache on launcher startup
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

## Visual Proof Boundary

- `make -C ray_tracing visual-artifact` is the current source-run first-frame
  visual proof. It renders and validates
  `ray_tracing/visual_artifacts/source_first_frame/frames/frame_0000.bmp`
  without launching the packaged app.
- There is no `package-visual-artifact` target. Packaged-app visual capture is
  intentionally release-gated and should be added only when package/runtime
  mismatch is the active question.
- Routine packaged-app confidence remains covered by `package-desktop-self-test`,
  `release-bundle-audit`, launcher `--print-config`, and manual/local launch
  checks.

## Linux Desktop GUI Proof Boundary

- The Linux headless worker package and the Linux desktop/windowed GUI package
  are separate release surfaces. A clean headless render does not prove the SDL
  window, desktop session, or Vulkan presentation path.
- The first Linux desktop GUI proof should build the windowed `visual-harness`
  target from a named Git branch or commit, launch the resulting `Ray_anim`
  binary inside a real logged-in Linux desktop session, detect the RayTracing
  window title, and capture a nonblank first-frame screenshot.
- The current intended Linux PC proof command shape is:

```sh
python3 bin/linux_pc_ray_tracing_gui_proof_request.py \
  --source-archive /path/to/ray-tracing-worker-vps-source-bundle.tar.gz \
  --source-state-note "clean Git branch or commit source snapshot" \
  --jobs 4 \
  --launch-seconds 8
```

- Runtime assumptions for that proof:
  - a real Linux desktop session is already logged in
  - `DISPLAY` and `XAUTHORITY` are discovered from the desktop user's session
  - SDL uses the X11 video driver for the first proof
  - Vulkan and OpenGL must resolve to the installed desktop GPU driver
  - screenshot capture is a proof artifact, not a packaged-app acceptance test
- The expected pass classification is `gui_first_frame_captured`.
- Package work should start only after a clean-source GUI proof passes. The next
  package boundary is a Linux desktop app layout and unpacked smoke proof, not
  publication.

## Current Limits

- This doc describes the current local packaged-app and release-readiness workflow only.
- The 2026-06-08 pass produced a fresh notarized artifact set for the current `0.3.0` macOS arm64 and x86_64 worktrees.
- Current repo-state presence of `dist/`, Desktop, or `build/release/` artifacts should be checked separately from this doc before making artifact-presence claims.
