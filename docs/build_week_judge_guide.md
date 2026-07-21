# OpenAI Build Week Judge Guide

This is the shortest installation and testing path for the optiC 0.8.0 OpenAI
Build Week submission. The primary judge path uses the packaged desktop app and
does not require the source repository, compiler toolchain, PhysicsSim, or
LineDrawing.

## Supported Platform

The submission release is tested and packaged for:

- macOS on Apple Silicon (`arm64`)
- a Metal-capable Mac supported by the bundled MoltenVK runtime
- the standard macOS desktop environment

The source tree and package tooling also contain Linux desktop support, but a
new 0.8.0 Linux package is not claimed as judge-supported until its separate
real-display proof is complete. Windows is not supported.

## Install Without Rebuilding

1. Open the [optiC program page](https://ecosystem.calebsv.tech/suite/program/?repo=ray_tracing).
2. Download `optiC-0.8.0-macOS-arm64-stable.zip` and its SHA-256 checksum.
3. Verify the archive, unzip it, and move `optiC.app` to Applications or another
   writable folder.
4. Open `optiC.app` normally.

Terminal example:

```sh
shasum -a 256 optiC-0.8.0-macOS-arm64-stable.zip
unzip optiC-0.8.0-macOS-arm64-stable.zip
open optiC.app
```

The published archive will be signed and notarized. It includes the renderer,
MoltenVK/Vulkan runtime libraries, shaders, fonts, material presets, and the
self-contained Build Week scene.

## Five-Minute Test

1. Launch optiC with a fresh runtime profile. The app opens directly in the 3D
   **Object** editor with an editable copy of the showcase loaded.
2. Select the reflection blob, grooved orb, or lattice shell.
3. Change the selected object's material preset, color, roughness, or
   reflectivity, then apply the edit.
4. Move or zoom the camera to inspect the imported geometry.
5. Enable **Deep Render**, choose a shipped 3D integrator, and press **Start**.

Existing runtime profiles keep their saved scene selection. To inspect the
showcase from one of those profiles, choose **Load Scene** and open
`data/runtime/scenes/optic_studio_starter_v1/scene_runtime.json`.

The scene should show three distinct original procedural meshes in a small
studio: a mirror-like organic blob, a glossy grooved orb, and a copper-colored
lattice shell. The demo contains 44,672 imported mesh triangles and does not
enable photon mapping.

## Package Self-Test

To inspect the unpacked app without launching the GUI:

```sh
optiC.app/Contents/MacOS/raytracing-launcher --self-test
```

Expected output starts with `self-test: ok`. This verifies the executable,
runtime libraries, shaders, fonts, material presets, showcase scene, and all
three showcase mesh assets after they are seeded together into the editable
runtime scene root.

## Optional Source Test

The prebuilt app above is the supported judge path. For repository testing on
an Apple Silicon Mac, install `make`, SDL2, SDL2_ttf, json-c, libpng, Vulkan
headers/loader, and FFmpeg through Homebrew, then run:

```sh
git clone https://github.com/calebsv19/cosm-optiC.git ray_tracing
cd ray_tracing
make clean
make ray-tracing-render-headless
make test-optic-build-week-showcase
make package-desktop-self-test
```

The focused showcase test writes:

```text
build/agent_runs/ray_tracing/optic_build_week_showcase/frames/frame_0000.bmp
```

It also verifies the native 3D route, TLAS/BLAS acceleration, object visibility
and triangle counts, photon-free request state, and a valid rendered frame.

## What The Submission Demonstrates

- interactive scene loading and native 3D viewport navigation
- original procedural STL assets compiled into runtime mesh sidecars
- high-triangle imported-mesh rendering through TLAS/BLAS acceleration
- per-object material preset, color, roughness, and reflectivity authoring
- shipped direct-light, bounce, material, transparency/emission, and Disney
  rendering tiers
- asynchronous and synchronous deep-render/export paths
- a reproducible package containing the scene, recipes, STL sources, runtime
  meshes, hashes, and conversion summaries

Production photon mapping remains experimental, default-off, and outside this
submission candidate. It will be added to the Devpost description only if it
passes its separate visual acceptance gate before submission freeze.

## Codex Evidence

optiC is a pre-existing project meaningfully extended during the July 13-21,
2026 submission period. The frozen pre-period baseline is:

```text
bd8ee85dae5e62707810065d413d94c24475535b
```

The final eligible end commit, release tag, public artifact checksum, and
principal `/feedback` Session ID must be inserted here after the 0.8.0 release
candidate is committed and frozen. The repository README describes the
maintainer/Codex collaboration and the Build Week feature delta.
