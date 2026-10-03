# HTN Planner 2.3.0

Release preparation: **2.3.0**. Not yet published.
Distribution targets: Windows x86_64 and Ubuntu 24.04 x86_64.

## Highlights

- Write negative integer and float literals directly: `-1`, `-1.0`, `-0.5`,
  `-123.456`, including the minimum signed 32-bit integer `-2147483648`.
- Use them in task and callterm arguments, fact queries, assignments, comparisons,
  arithmetic, static/runtime lists, axiom calls and deferred arguments.
- Preserve existing subtraction, unary negation and decrement expressions:
  `(- 5 2)`, `(- 1.0)`, `(-- 1.5)` and `(- ?value 1)`.
- Generate numeric atoms directly, without introducing runtime negation for literals.
- Preserve numeric spelling and source locations in debugger labels, including
  full decimal precision and trailing zeros in static lists.
- Report located diagnostics for malformed signed values and numeric overflow.
- Accept negative values in world-state files using the same lexer rules.

```lisp
(!remember -1.0 is_moving)
(= ?offset -0.5)
(== (+ -2 3) 1)
(== (- -2 3) -5)
```

The sign must touch the first digit. `- 1` is invalid in a value position;
write `-1` or `(- 1)`. `--1` is invalid; `(-- 1)` remains decrement.
`(-2 3)` is a numeric list, whereas `(- 2 3)` is subtraction.
Scientific/hexadecimal notation and leading-dot decimal notation remain unsupported.

## Compatibility and upgrade

The C runtime API, generated planner ABI, RuntimeBridge ABI and `HTNAtom` layout
are unchanged from **2.2.0**. Existing valid separated arithmetic expressions keep
their meaning. There is no runtime API migration.

1. Rebuild tools using the C++ compiler frontend: the owned AST now retains literal
   spelling. Its C++ layout changes, so do not mix old frontend binaries and new headers.
2. Translate domains that adopt negative literals with the new translator and
   compile the emitted C for the target platform. Existing generated modules do not
   require regeneration solely for ABI compatibility.
3. Regenerate instrumented domains to obtain the more precise debugger labels.
4. Keep the platform, CRT and instrumentation variant aligned as described in
   [SDK variants](SDK_VARIANTS.md) and [Linux setup](LINUX.md).

The generated integer emitter uses `INT32_MIN` for `-2147483648`, avoiding MSVC's
unsigned unary-minus warning in scalar, list and arithmetic-operand initialization.

Windows Natvis distribution and Linux platform support introduced in
[2.2.0](RELEASE_2_2_0.md) are retained. HTNEditor remains Windows-only and experimental.
Supported Linux scope remains Ubuntu 24.04 x86_64, GCC 14 and Clang 18 with libstdc++.

See the [language guide](DOMAIN_LANGUAGE.md) and
[detailed syntax notes](RELEASE_NOTES_NEGATIVE_LITERALS.md).

## Validation status

Before export, the feature passed 354 generated-only DebugInstrumented tests and
345 generated-only ReleasePlain tests on Windows. The private development branch
also passed complete Linux suites with GCC 14 and generated C11 checks with Clang 18.
These are source validation results, not validation of a published 2.3.0 SDK.

Validated against the exported public sources on 2026-10-03:

- Windows MSVC Debug: **354 tests passed** in the complete suite.
- Ubuntu 24.04 / GCC 14 Release: **345 tests passed** in the complete suite.
- Clang 18 accepted the generated negative-literal fixture as C11, with full and
  no generated instrumentation.
- SDK source-boundary and RuntimeBridge audits passed; both packaging scripts
  include the new language and release documentation.

Local evidence in the public checkout:

- `build/logs/public-negative-literals-export/Debug-tests.log`
- `build/logs/public-negative-literals-export/linux/Release-tests.log`
- `build/logs/public-negative-literals-export/sdk-source-boundary.log`
- `build/logs/public-negative-literals-export/runtime-bridge-audit.log`
- `build/logs/public-negative-literals-export/manifest.json`

These logs are local ignored files, not SDK payloads. Final **2.3.0 SDK archives,
external package consumers and engine integration have not yet been validated**.
Build the new packages with:

```powershell
.\BuildAndValidateSDK.bat
```

Inside Ubuntu, with the dependencies and `PREMAKE5` from [Linux setup](LINUX.md):

```sh
bash BuildAndValidateSDK.sh
```

Both commands take **2.3.0** from `VERSION`. They rebuild the platform variants,
package the SDK, write SHA-256 sidecars and validate consumers from an extraction.
Do not replace or rename previously published 2.2.0 archives.

## Planned release assets

```text
HTNSDK-2.3.0-windows-x86_64.zip
HTNSDK-2.3.0-windows-x86_64.zip.sha256
HTNSDK-2.3.0-linux-x86_64.tar.gz
HTNSDK-2.3.0-linux-x86_64.tar.gz.sha256
```

Complete SDK and engine validation and finalize these notes before creating
the release tag and publishing these assets.
