# Standalone callterm conditions

## Contract

In generated domains, `(call some_callterm ...)` used directly as a condition
requires a boolean result. `true` succeeds; `false` fails normally without an
error report. There is no implicit conversion or truthiness.

A successful invocation returning a non-boolean atom reports
`HTNCallTermErrorReason::NonBooleanConditionResult` and fails the condition.
Backtracking may try another alternative. Each actual offending invocation
produces at most one report, including during deferred decomposition. Unbound
results remain ordinary failures; invocation/conversion errors keep their own
reason and are not reported twice.

Use an explicit assignment or a type-compatible comparison for value results:

```lisp
(= ?position (call ppr_finalize_best))
(== (call get_current_target) expected_target)
```

Inside an axiom declaring `?out_position`, the existing output contract also permits:

```lisp
(= ?out_position (call ppr_finalize_best))
```

## Reporting

The existing execution policy applies: `Report` calls the client's callback,
`FailSilently` only fails the condition, and `Unset` asserts when an error occurs.
`Report` without a callback also asserts; with assertions disabled these cases
fail safely without calling a null pointer. The client can assert, log or terminate
in its callback. The SDK does not install an engine-specific assert handler.

The callback receives the callterm name, original domain/file/line/column,
`ExpectedAtomType = HTN_ATOM_TYPE_BOOL`, `ExpectedTypeName = "bool"`, and the actual
atom type. `ArgumentIndex` is `UINT32_MAX`, because this error concerns the result.
Metadata is available in plain and instrumented builds; all report data is borrowed
for the callback's duration. The demo prints the actual type and suggests binding
or comparing the result explicitly.

Custom C++ type names are not retained by the atom representation. For example,
a `Vector3` converter that returns a list is reported as `list`, not `Vector3`.
Optional declared return-type metadata and initialization-time validation are
future work. Runtime checks do not require that metadata.

## Compatibility

This adds an enum reason at the end of the existing reasons, preserving their
numeric values. Update exhaustive client reason switches to handle it.

Policy dispatch is shared through an inline C-compatible helper. No atom, context,
descriptor or callback payload layout changes, and no new RuntimeBridge export
is required. Generated planner and RuntimeBridge ABI versions remain unchanged.

Regenerate and recompile domains with the updated translator and headers to gain
the check. Previously generated domains remain loadable and retain their previous
behavior until regenerated. Published SDK archives are not modified by this fix.

## Validation (2026-09-30)

The initial regression run used the old generator: all six non-boolean cases
failed because no callback ran, while the boolean control passed. Evidence:
`build/logs/boolean-callterms-before-fix.log`.

The fix has 20 dedicated tests covering custom Vector3 conversion, int/float,
symbol/string/list, bool true/false, exact report metadata, single evaluation,
deferred calls, backtracking/fallback, assignments (including axiom outputs),
comparisons, nested arguments, all policies, configuration assertions/Release
fallback, nonduplicated invocation errors, demo diagnostics and owned-list cleanup.

The development test matrix passed on Windows x64/MSVC:

| Configuration | Passed | Test log |
| --- | ---: | --- |
| DebugInstrumented | 344 | `build/logs/boolean-callterms-DebugInstrumented-tests.log` |
| ReleaseInstrumented | 344 | `build/logs/boolean-callterms-ReleaseInstrumented-tests.log` |
| DebugPlain | 335 | `build/logs/boolean-callterms-DebugPlain-tests.log` |
| ReleasePlain | 335 | `build/logs/boolean-callterms-ReleasePlain-tests.log` |

One existing interpreter/generated parity case,
`InterpreterAndGeneratedProduceSameResult/Recursive100Entities`, was excluded
after the initial DebugInstrumented run exceeded six CPU minutes and approximately
4 GB of resident memory. Its partial log is
`build/logs/boolean-callterms-DebugInstrumented-initial-tests.log`. Generated
recursion regressions for both 100 and 1000 entities remained enabled and passed
in all four configurations. This is not a claim that the excluded parity case
was validated. Matrix summary: `build/logs/boolean-callterms-matrix-results.json`.

All eight SDK variants were rebuilt in a private validation package named
`2.0.4-boolean-callterms-validation`. Fresh-package validation passed 24 external
consumer executions and eight object/export checks, including the new condition
checks through direct linkage and a loaded domain DLL. Release consumers also
verify safe failure with Unset or a missing Report callback. Incompatible CRT and
unknown-variant selections were rejected; the ZIP checksum was verified.
Detailed results: `build/logs/boolean-callterms-sdk-validation.log` and
`build/logs/boolean-callterms-sdk-results.json`. The latter records each original
CTest log path. SDK build evidence is in
`build/logs/boolean-callterms-sdk-build-and-initial-validation.log`.

The validation package is a local test artifact, not a published SDK release.

### Generated public branch merge (2026-10-01)

After merging the fix into `generated_public_release`, the complete solution
built successfully in Debug and Release. The full generated-only test suites
passed without exclusions: 264 tests in Debug and 255 in Release, including all
20 new callterm regressions in each configuration.

- Build logs: `build/logs/merge-boolean-callterms-Debug-build.log` and
  `build/logs/merge-boolean-callterms-Release-build.log`.
- Test logs: `build/logs/merge-boolean-callterms-Debug-tests.log` and
  `build/logs/merge-boolean-callterms-Release-tests.log`.
- SDK source boundary: `build/logs/merge-boolean-callterms-sdk-source-boundary.log`.
- RuntimeBridge audit: `build/logs/merge-boolean-callterms-runtime-bridge-audit.log`.

The merge preserves the generated-only sources and existing SDK packaging
documents. This merge does not publish a package or change the SDK version.
