# HTN Planner 2.3.0

Release: **2.3.0**. Release notes finalized on **2026-10-04**.
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

## Validation

Validated against the public sources and SDK candidates on 2026-10-03/04:

- Source regression suites: **354 Windows MSVC Debug tests** and **345 Ubuntu
  24.04 / GCC 14 Release tests** passed. Clang 18 accepted the generated
  negative-literal fixture as C11 with full and no generated instrumentation.
- SDK source-boundary and RuntimeBridge audits passed.
- Windows **2.3.0** SDK: all eight variants passed validation from a fresh ZIP
  extraction, with **24 external consumer executions** and eight object/export
  checks. Incompatible CRT and unknown variants were rejected. Natvis definitions,
  checksums and Visual Studio project integration passed in every variant.
  The extracted payload and binary provenance were checked against the archive;
  a packaged-translator probe accepted negative literals, including `INT32_MIN`.
- Linux **2.3.0** SDK: all four variants were rebuilt on Ubuntu 24.04 WSL2 using
  GCC 14. External consumers passed with GCC 14 and Clang 18/libstdc++ from a fresh,
  relocated archive extraction: **24 executions**, eight ELF audits and eight
  incompatible-domain ABI rejection checks (**40 checks total**), plus six
  expected configuration rejections.
- Final documentation packaging preserved every other payload byte, including
  binaries, headers, examples, CMake files, manifests and build provenance.
  Archive/payload checksums and Linux executable permissions were checked again
  after updating the documentation.
- The maintainer confirmed successful Windows engine integration, including use
  of negative literals. That external engine itself has not been tested on Linux.

Build IDs: Windows `cf05e2d5f20043468e332d7663904908`;
Linux `fbba18aeaa024703b1f326606668a5a8`.

Local evidence in the public checkout:

- `build/logs/public-negative-literals-export/Debug-tests.log`
- `build/logs/public-negative-literals-export/linux/Release-tests.log`
- `build/logs/public-negative-literals-export/sdk-source-boundary.log`
- `build/logs/public-negative-literals-export/runtime-bridge-audit.log`
- `build/logs/public-negative-literals-export/manifest.json`
- `build/logs/release-2.3.0-windows/audit.json` and its adjacent `external/` logs
- `build/logs/release-2.3.0-windows/negative-literal-translator.log`
- `build/logs/release-2.3.0-linux/build-and-validate.log`
- `build/logs/release-2.3.0-linux/sdk/fbba18aeaa024703b1f326606668a5a8/consumers-{gcc-14,clang-18}.log`
- `build/logs/release-2.3.0-finalization/result.json`

These records are local ignored artifacts, not SDK payloads. The finalization
report records archive hashes, preserved payloads and unchanged published 2.2.0
archives.

## Release assets

Use the package for your target platform and its matching checksum file:

```text
HTNSDK-2.3.0-windows-x86_64.zip
HTNSDK-2.3.0-windows-x86_64.zip.sha256
HTNSDK-2.3.0-linux-x86_64.tar.gz
HTNSDK-2.3.0-linux-x86_64.tar.gz.sha256
```

Both packages identify version **2.3.0** in their manifests and build provenance.
Previously published archives remain unchanged.
