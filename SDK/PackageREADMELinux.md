# HTN SDK for Linux x86_64

Version 2.3.0 adds native negative numeric literals and precise numeric debugger
labels. The C runtime API/ABI and atom layout are unchanged from 2.2.0. Rebuild
tools using the C++ compiler AST and translate domains that adopt the new syntax.
See [2.3.0 compatibility and release notes](docs/RELEASE_2_3_0.md).

The exact version, compiler and ABI contract are in `manifest.json`.
Validated on Ubuntu 24.04 with GCC 14 / Clang 18 and libstdc++, C++20 hosts and
C11 generated domains. System glibc/libstdc++ must be compatible with the build.

Start with [the Linux build and consumption guide](docs/LINUX.md), section 5,
and [the domain language reference](docs/DOMAIN_LANGUAGE.md).

Four variants: DebugPlain, DebugInstrumented, ReleasePlain, ReleaseInstrumented.
Framework and optional Integration are static libraries; RuntimeBridge is an
optional shared library. All variants contain debug symbols. The interpreter,
its AST nodes and its integration are excluded. SDL/ImGui and GoogleTest are not
SDK dependencies; graphical demos/tools are available from the source repository.

Use `find_package(HTN CONFIG REQUIRED)` with `HTN_DIR` pointing to `cmake/`, link
`HTN::HTNFramework` or `HTN::HTNIntegration`, and call `htn_configure_target` on your
target. Defaults: Debug -> DebugInstrumented; Release -> ReleasePlain.
The SDK uses libstdc++ ABI 1 without `_GLIBCXX_DEBUG`. Advanced profiling and atom
diagnostic flags are not enabled in these binary variants. Rebuild the SDK and
domains together if you change ABI-affecting flags.

Validate without the source repository:

```sh
bash ValidatePackage.sh --cc gcc-14 --cxx g++-14
bash ValidatePackage.sh --cc clang-18 --cxx clang++-18
```

Requires Python 3.12+, CMake 3.25+, Ninja and binutils, plus the chosen compiler.
The validator verifies every packaged file, then translates domains, compiles their
C, and runs Core, Integration and shared-domain consumers for each variant. It
also checks ELF bridge symbols, incompatible domain ABI rejection and negative
configuration cases. Results/logs go to a fresh temporary directory printed at
the end. An optional `--build-root` must name a directory that does not exist.

`CHECKSUMS.sha256` covers the package contents; the archive has a separate SHA-256
sidecar. These are integrity hashes, not cryptographic publisher signatures.
