# HTN Planner 2.2.0

Release preparation: **2.2.0**, prepared on **2026-10-03**.
Final release archives and publication are pending. See the validation status below.

## Highlights

- Native Linux builds and SDK packaging for Ubuntu 24.04 x86_64, validated with
  GCC 14 and Clang 18 using libstdc++.
- Linux support for the generated runtime, integration, translator, visual demos
  and generated debugger, language server, benchmarks and regression tests.
- Linux hot reload example using compiled `.so` domain modules.
- Four Linux SDK variants, CMake integration, external consumer validation and
  `.tar.gz` packaging with a SHA-256 sidecar.
- Windows SDK distribution of `debug/HTN.natvis`, automatic Visual Studio project
  integration through the CMake targets, and package checks for the visualizer.

The runtime C API, generated planner ABI, RuntimeBridge ABI and `HTNAtom` layout
are unchanged from **2.1.0**. Domain language syntax and semantics are unchanged.

## Platforms and tools

| Platform | Toolchain | SDK variants | Archive |
| --- | --- | --- | --- |
| Windows x86_64 | MSVC v143 / Visual Studio 2022 | Eight CRT/configuration/instrumentation combinations | `.zip` |
| Ubuntu 24.04 x86_64, including WSL2 | GCC 14; Clang 18 consumers with libstdc++ | DebugPlain, DebugInstrumented, ReleasePlain, ReleaseInstrumented | `.tar.gz` |

Host code uses C++20 and generated domains use C11. Linux static libraries use
PIC; RuntimeBridge is an optional shared library. Linux packages use system
glibc/libstdc++, `_GLIBCXX_USE_CXX11_ABI=1`, and no `_GLIBCXX_DEBUG`.
The manifest records the compiler, library ABI and variant contract. The SDK is
not a universal binary package for every Linux distribution.

The Linux development build includes HTNDemo, its generated event debugger,
HTNHotReloadDemo, HTNLanguageServer and HTNBenchmark. SDL/ImGui are development
demo dependencies, not SDK dependencies. A graphical Linux session or WSLg is
required for interactive demos.

**HTNEditor remains excluded on Linux.** It is still experimental on Windows,
and its Linux port is not currently a priority. Other architectures,
distributions, libc++, macOS and other Unix platforms remain unverified.
An editor extension bundling Linux language-server binaries is a separate deliverable.

## Visual Studio Natvis

The Windows SDK includes `debug/HTN.natvis` for `HtnSymbol`, `HTNAtom`,
`HTNAtomOwner` and `HTNAtomList` in all eight variants. The CMake targets
`HTN::HTNFramework`, `HTN::HTNIntegration` and `HTN::HTNRuntimeBridge` propagate
the visualizer to consuming projects.

For an engine using its own project generator, include that file in the generated
Visual Studio project. For a manually maintained project, use **Add > Existing
Item**. Use the Natvis file from the same SDK as the headers and libraries.
No HTN instrumentation flag or global Visual Studio installation is required.

The Windows package validator now requires the file and its four type definitions,
verifies its checksum, and checks the generated host/domain projects in every variant.
See [SDK variants and Natvis](SDK_VARIANTS.md#visual-studio-natvis).

## Hot reload integration

The demos demonstrate compiling, validating and adopting candidate domain modules,
including failed-reload recovery and preservation of client state. An integrating
engine owns file watching, compilation scheduling, module loading, synchronization,
active-plan lifetime and unloading. Installing the SDK does not add that engine
orchestration automatically.

## Upgrade from 2.1.0

1. Use the SDK for the target platform and the correct CRT/instrumentation variant.
2. This release introduces no C API/ABI migration and does not require domain
   regeneration solely for an ABI change. Rebuild with the new SDK when adopting
   its portability fixes, and compile generated C for each target platform.
3. Windows binaries cannot be reused on Linux. Build the host, generated domains,
   custom callterm bindings and optional bridge/module integration for Linux.
4. Include `debug/HTN.natvis` when managing Visual Studio projects yourself; CMake
   consumers receive it automatically.
5. Follow the [Linux setup and SDK guide](LINUX.md), including the Linux Premake
   installation and `PREMAKE5` environment variable. Run its shell commands inside Ubuntu.

For earlier SDKs, retain the [2.1.0 migration steps](RELEASE_2_1_0.md) and the
[2.0.4 migration requirements](RELEASE_2_0_4.md).

## Validation status

Completed before final 2.2.0 packaging:

- Public source export: 336 Windows Debug tests, 336 Linux GCC Debug tests and
  327 Linux GCC Release tests passed. Language-server protocol and hot reload
  checks passed; Linux allocation and benchmark checks also passed.
- The Linux export SDK passed all four variants with GCC and Clang external
  consumers: 40 checks total, plus six expected configuration rejections. This
  run used the earlier local identifier `2.1.0-linux-export.1`.
- Natvis integration was checked using an isolated copy of existing Windows SDK
  binaries with the new CMake/validator/visualizer files: 24 consumer executions,
  eight object/export checks, 32 project attachments, and rejection of missing
  Natvis files. This was a packaging regression, not a fresh release SDK build.
- The maintainer reported the new Windows candidate working in an external engine.
  That report does not validate the engine itself on Linux.

Still pending:

- Completion of the current Linux `2.2.0-rc.1` SDK build and consumer validation.
- Generation and validation of the final Windows and Linux **2.2.0** archives
  after release documentation is finalized.
- Verification of final manifests, documentation, Natvis and archive SHA-256 files.
- Tagging and publication.

Public checkout evidence is under `build/logs/linux-export-windows-Debug-tests.log`,
`build/logs/linux-export-linux/linux-full-gcc-{Debug,Release}-tests.log` and
`build/logs/linux-export-linux/sdk/56f397586c1545cea24ee7c1de8b4a17/consumers-{gcc-14,clang-18}.log`.
Natvis regression evidence is in the private development checkout under
`build/logs/natvis-sdk-validation-20261003-102703/RESULTS.md` and its referenced logs.
These local build records are not shipped in the SDK or tracked by Git.

## Planned release assets

Publish the validated final packages and their matching checksum files:

```text
HTNSDK-2.2.0-windows-x86_64.zip
HTNSDK-2.2.0-windows-x86_64.zip.sha256
HTNSDK-2.2.0-linux-x86_64.tar.gz
HTNSDK-2.2.0-linux-x86_64.tar.gz.sha256
```

Generate final packages with version `2.2.0`; do not rename an `rc` archive,
because its manifest and build provenance still identify it as a candidate.
Previously published archives remain unchanged.
