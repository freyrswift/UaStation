# Cross-Platform Builds

The workflow in `.github/workflows/build.yml` builds and tests four native targets. Defining a target does not mean a binary has already been produced or validated: check the matching GitHub Actions job before distributing its artifact.

| Target | Runner | Release Artifact |
| --- | --- | --- |
| Windows x64 | `windows-latest` | `Ua-windows-x64.zip` |
| macOS Apple Silicon | `macos-15` | `Ua-macos-arm64.dmg` |
| macOS Intel | `macos-15-intel` | `Ua-macos-x64.dmg` |
| Linux x64 | `ubuntu-24.04` | `Ua-linux-x64.tar.gz` |

## Common Requirements

- CMake 3.21 or newer, Ninja, and a C++20 compiler.
- Qt 6.8.3 for the target platform, with Core, Gui, Qml, Quick, QuickControls2, Svg, Network, Widgets, and Concurrent.
- Pass the Qt installation prefix as `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/kit` if Qt is not already discoverable.
- Build on the target OS. These instructions do not cross-compile macOS from Windows.

## Windows x64

Run from an x64 Visual Studio developer shell with the MSVC Qt kit available:

```powershell
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl
cmake --build build-win --parallel 4
ctest --test-dir build-win --output-on-failure
cmake --install build-win --prefix dist-win
Compress-Archive -Path dist-win\* -DestinationPath Ua-windows-x64.zip -Force
```

Run `dist-win/ua.exe`. The Qt runtime and required MSVC DLLs are included by the deployment rules.

## macOS

Install Xcode Command Line Tools, Ninja, CMake, and Qt's `macos/clang_64` kit. Build natively on Apple Silicon with `arm64`, or on Intel with `x86_64`:

```bash
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build-mac --parallel 4
ctest --test-dir build-mac --output-on-failure
cmake --install build-mac --prefix dist-mac
cpack --config build-mac/CPackConfig.cmake -G DragNDrop -B packages
```

For Intel, replace `arm64` with `x86_64`. The app bundle is `dist-mac/ua.app`, with the executable at `Contents/MacOS/ua`. `macdeployqt` bundles Qt frameworks and QML imports. The two architectures have distinct disk-image names and update targets.

The workflow does not configure Developer ID signing or notarization. Those release steps require the maintainer's signing credentials and are separate from compilation. macOS 12 is the configured deployment minimum; native job success does not replace testing on that minimum OS version.

## Linux x64

The CI build baseline is Ubuntu 24.04 x64. Install the dependencies listed in the workflow, including `patchelf`, then use the Qt `gcc_64` kit:

```bash
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build-linux --parallel 4
ctest --test-dir build-linux --output-on-failure
cmake --install build-linux --prefix dist-linux
cpack --config build-linux/CPackConfig.cmake -G TGZ -B packages
```

Run `dist-linux/bin/ua`, or extract the archive and run its `bin/ua`. The Qt deployment API installs the required Qt libraries, QML imports, plugins, and `qt.conf`. The executable uses a relative runtime library path. System graphics, font, C/C++ runtime, and other OS dependencies still need to be compatible with the build baseline; the archive is not a static executable.

The Linux CPack configuration also exposes `-G DEB` for native package development. The four-target workflow currently publishes only the portable tar archive, not a `.deb` or AppImage.

## Running the Workflow

Pushes and pull requests targeting `main` run the matrix. Maintainers can also use **Actions > Build > Run workflow**. Each job configures, builds, runs the regression tests, installs, verifies its bundle/runtime layout, and uploads the matching artifact. Failed jobs upload available diagnostic logs.

Pushing a `v*` tag runs the same matrix and creates a GitHub Release only after all four builds succeed. Preparing these workflow files locally does not dispatch a remote build or publish a release.

## Verification Boundaries

The installer UI is shared across platforms, but individual editor adapters can depend on platform-specific paths and on the editor itself being available. In particular, the current deep install-directory discovery for WorkBuddy and ZCode is Windows-oriented. A successful desktop build is not evidence that every editor adapter has been exercised on every OS.

Windows ARM64, Linux ARM64, mobile platforms, and a web frontend are not included in the four release targets. The updater does not substitute an x64 download for an unsupported architecture.
