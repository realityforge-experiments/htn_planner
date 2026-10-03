# Linux validation

For installation, compilation, demos and SDK consumption, follow the
[Linux guide](LINUX.md). This page records the generated public branch's validation;
local logs and build artifacts are ignored by Git.

## Merge validation (2026-10-02)

Validated on Ubuntu 24.04 x86_64 under WSL2 with GCC 14.2 and Clang 18.1, both using
libstdc++. Host sources use C++20 and generated domains use C11. Each compiler used
a fresh, independent source/build tree containing only this branch's tracked files.
The source and generated-project audit excludes the interpreter and legacy debugger.

| Toolchain | Debug | Release | Profile | ProfileDetailed |
| --- | ---: | ---: | ---: | ---: |
| GCC 14 / Linux | 336/336 | 327/327 | 327/327 | 327/327 |
| Clang 18 / Linux | 336/336 | 327/327 | Not repeated | Not repeated |
| MSVC v143 / Windows | 336/336 | 327/327 | Not repeated | Not repeated |

Total: **1,980 Linux and 663 Windows passing regression test executions**.

Every selected Linux configuration builds all supported projects and runs the
regression suite, language-server stdio smoke, allocation self-test, general and
lifecycle benchmarks, and the hot reload pipeline. Profiling configurations also
produce an Optick capture; the regression checks its signature and nonempty output.
Windows Debug and Release rebuild the full solution and pass the regression suite
and hot reload pipeline. HTNEditor builds on Windows and remains excluded on Linux.

The hot reload checks compile and load actual domain modules, change behavior,
reject invalid/corrupt candidates, preserve state through eight moving reloads
with deferred calls, and restore the original module.

### SDK validation

Both SDKs were rebuilt, packaged, extracted outside the source repository and
validated using their packaged translator, headers and libraries:

| Package / consumer compiler | Variants | CTest checks | Negative checks |
| --- | ---: | ---: | ---: |
| Linux SDK / GCC 14 | 4 | 20/20 | 3/3 |
| Linux SDK / Clang 18 | 4 | 20/20 | 3/3 |
| Windows SDK / MSVC v143 | 8 | 32/32 | 2/2 |

The Linux runs include 24 Core/Integration/Bridge consumer executions, eight ELF
export audits and eight incompatible-domain ABI rejection checks. Negative
configurations reject unknown variants and incompatible libstdc++ ABIs. The Windows
run includes 24 consumer executions and eight object/export audits, plus rejection
of incompatible CRT and unknown variants. All checks passed in plain and
instrumented Debug/Release variants. Package checksums, build provenance and
the generated-only source/header boundary are verified by the SDK scripts.

No published SDK was replaced. The local candidate identifier is
`2.0.4-linux-merge.1`; it is not a release version change. The repository's `VERSION`
is unchanged. Linux SDK build ID: `f36658e186be46bb948f56c84ad80cd0`.

### Visual scope

The public Linux HTNDemo and HTNHotReloadDemo each render at least three frames and
exit cleanly through an injected SDL_QUIT event with the SDL dummy/software driver.
This is an automated startup/render check, not an interactive debugger or hardware
GPU certification. Debugger capture/source mapping also have regression coverage.

### Evidence

Paths below are relative to the repository root:

- Linux: `build/logs/merge-linux-public/{gcc,clang}/linux-full-<compiler>-<configuration>-<step>.log`,
  where steps include `build`, `tests`, `lsp`, `allocations`, `benchmark`, `lifecycle`
  and `hot-reload`.
- Windows: `build/logs/merge-linux-public-windows-{Debug,Release}-{build,tests,hot-reload}.log`.
- Linux SDK build and external consumer logs: `build/logs/merge-linux-public/gcc/sdk/`.
- Windows SDK and external consumer summary: `build/logs/merge-linux-public-windows-sdk.log`.
- Detailed Windows consumer and negative-check logs: `build/logs/merge-linux-public/windows-sdk/`.
- Visual startup: `build/logs/merge-linux-public-gui.log`.
- Source/project isolation: `build/logs/merge-linux-public-isolation.log`.

These are local runs; no GitHub Actions execution is implied. SDK API and runtime
ABI versions are unchanged by this merge. Other distributions, architectures,
libc++, macOS and other Unix systems require separate validation. Linux language
server/VSIX binary distribution remains a later delivery step; its source-built
server is included and tested here.
