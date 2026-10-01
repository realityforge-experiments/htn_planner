# HTN Planner 2.1.0

Release: **2.1.0**. Release notes finalized on **2026-10-01**.
Windows x64/MSVC v143 is the supported distribution target.

## Changes

### Domain language documentation

The [domain language guide](DOMAIN_LANGUAGE.md) provides a complete walkthrough
and a reference for syntax, facts, methods, axioms, parameter modes, assignment,
operators, callterms, lists, includes and deferred decomposition. It is linked
from the repository and SDK READMEs and included in the SDK archive.

### Runtime list expressions

Lists can contain bound variables, arithmetic expressions, callterms and nested
lists in any supported value position:

```lisp
(call ppr_start (find_closest_location_to_entity ?inp_entity_id))
(= ?target (target ?inp_entity_id (call get_entity_position ?inp_entity_id)))
```

Elements are evaluated left to right and copied into an owned list. Unbound
elements or failed calls abort construction, clean up temporaries and preserve
source diagnostics. Lists are rebuilt as required by backtracking; deferred
arguments capture their evaluated values. Fully literal lists retain static
storage. See [runtime list values](RUNTIME_LISTS.md).

### Standalone callterm diagnostics

An independent `(call ...)` condition requires a boolean result. A valid
non-boolean result now reports `NonBooleanConditionResult` using the existing
error policy, with name, actual/expected type and original source location, then
fails the condition. Boolean false fails normally without an error. Assignments
and type-compatible comparisons can still use non-boolean results.
See [standalone callterm conditions](RELEASE_NOTES_BOOLEAN_CALLTERMS.md).

### Generated code size

- Qualified aliases share their original implementation where semantics allow.
  Overloads, overrides and callable/debugger identities remain distinct.
- Reachability analysis removes unused implementations and associated execution
  helpers. Every `top_level_method` remains callable from client code, even when
  no other domain method calls it. Deferred entries and their dependencies remain.
- `--instrumentation=full` is the default; `--instrumentation=none` omits optional
  debugger/profiling emission. It retains type checks, ownership, backtracking,
  callterm requirements/errors, source locations and capacity diagnostics.
- `--code-stats` reports source bytes/newlines for logic/support, debugger and
  profiling. It measures emitted source, not runtime CPU time or binary size.

```powershell
HTNTranslator Domains/Wanderer.domain CreateWandererHTN Generated --code-stats
HTNTranslator Domains/Wanderer.domain CreateWandererHTN Generated --instrumentation=none --code-stats
```

In the measured external engine Wanderer domain and its include, implementation
sharing and reachability reduced full output from 22,055 to 9,859 lines and none
output from 13,963 to 6,239 lines (about 55% in both modes). These are one workload's
source measurements, not a runtime speedup or general size guarantee.
See [instrumentation and measurements](GENERATED_INSTRUMENTATION.md).

## Compatibility and upgrade

The generated planner and RuntimeBridge ABI revisions, public C layouts and
`HTNAtom` representation are unchanged from 2.0.4. No runtime component or bridge
export is added. Existing 2.0.4 domain modules remain compatible with the matching
CRT/instrumentation variant; they retain their old behavior until regenerated.

1. Use the matching SDK headers, libraries and CRT/instrumentation variant.
2. Rebuild tooling that uses C++ compiler AST/IR, translation or generator APIs;
   those internal/tooling types have been extended and are not binary compatible.
3. Update exhaustive error-reason switches for `NonBooleanConditionResult`.
   Bind or compare non-boolean callterm results instead of using them as conditions.
4. Regenerate and recompile domains to use runtime lists, the new condition
   diagnostic and generated-code reductions.
5. Keep full instrumentation for a generated debugger tree or internal profiling.
   None mode produces neither, even with an instrumented SDK. It does not remove
   author-written debug callterms or change instrumentation in runtime libraries.

For upgrades from before 2.0.4, apply the previous
[2.0.4](RELEASE_2_0_4.md) and [2.0.3](RELEASE_2_0_3.md) migration requirements.

## Validation

Validated in the public repository on 2026-10-01:

- The complete solution built in Debug and Release. All 335 Debug and 326 Release
  tests passed without exclusions, including runtime lists, callterm diagnostics,
  shared implementations, public/deferred entries and generated recursion.
- The final Profile rebuild passed all 326 tests without exclusions. The
  allocation probe and hot reload pipeline self-tests passed, including candidate
  isolation, failed-reload rollback, deferred calls and preservation of NPC state.
- The client reported the candidate working in the external engine integration.
- All eight SDK variants were rebuilt and packaged, then tested from a fresh ZIP
  extraction: 24 external consumer executions and eight object/export checks.
- Incompatible CRT selections and unknown variants were rejected. Source isolation,
  runtime export coverage, build provenance and payload checksums were validated.
- The language guide examples passed 34 translator checks/generation runs: 11
  root examples in full/none modes plus validation, one included base domain,
  and one expected reassignment rejection. Fragments use suitable wrappers.
  These checks do not execute the example host callterms. Evidence is in
  `build/logs/domain-language/translator-validation.log` and `validation.json`.
  Documentation repackaging preserved all validated executable/header payloads.
- The SDK audit was updated to classify `HTNAnalyzeCompilerReachability` as a
  compiler-only function; it is not a generated runtime dependency.

Build ID: `9bb1dc2ac56c4008902fafab5d53322f`. Local evidence (ignored build artifacts):

- `build/logs/release-2.1.0-source-results.json`, with matching Debug/Release
  `-build.log`, `-tests.log` and `-tests.xml` files.
- `build/logs/release-2.1.0-final-gates.json`, `release-2.1.0-Profile-build.log`,
  `release-2.1.0-Profile-tests.log`, `release-2.1.0-allocation-self-test.log` and
  `release-2.1.0-hot-reload-self-test.log` record the final automated checks.
- `build/logs/release-2.1.0-finalization/result.json` verifies the final ZIP,
  checksum and unchanged validated payload after finalizing these notes.
- `build/logs/release-2.1.0-sdk-build-and-validation.log` records the temporary
  consumer build directory and the complete package validation.
- `build/logs/release-2.1.0-sdk-audit.json` records archive/payload checks and
  confirms that final documentation packaging did not change validated binaries,
  headers, examples or build provenance.

Before export, the generated public branch additionally passed the four-way
Debug/Release plain/instrumented matrix (335/326 tests per configuration) and
Windows clang-cl C11 consumers in both full and none modes. These are Windows
results, not native Linux validation.

Native Linux validation and comparative performance profiling remain pending.
Hot reload requires client-owned loading, lifetime management and synchronization,
as in previous releases.
