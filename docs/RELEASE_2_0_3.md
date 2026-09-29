# HTN Planner 2.0.3 — pending engine validation

Prepared locally for integration testing; not tagged or published.
Windows x64/MSVC remains the validated distribution target.

**Compatibility warning:** despite the patch version, this update changes domain
syntax and the generated planner descriptor ABI. It is not a drop-in replacement
for 2.0.2 and does not follow the usual SemVer patch compatibility guarantee.

## Changes

- Explicit variable declarations: `(= ?distance (call get_distance ?from ?to))`.
  The destination must be fresh. Implicit `(?distance (call ...))` is rejected;
  `==` remains equality comparison.
- Nested callterms are evaluated in comparisons, arithmetic and arithmetic task
  arguments. Missing-call reports point to the actual call expression.
- `HTNCallTermRegistry::ValidateGeneratedCallTerms` checks registrations, bindings
  and instances without invoking calls. Every original call site is checked,
  including linked sources; repeated IR copies are deduplicated.

## Engine migration

1. Keep the previous SDK for rollback; install 2.0.3 separately.
2. Replace headers, libraries, translator and matching RuntimeBridge from the same
   package and CRT/instrumentation variant. Rebuild copied integration code too.
3. Migrate implicit assignment syntax to explicit `=` declarations of fresh
   variables. Keep `==` comparisons unchanged.
4. Regenerate all domain C sources and rebuild their objects/DLLs and the host.
5. Configure registry bindings and instances, then validate the definition:

```cpp
const bool Ready = Registry.ValidateGeneratedCallTerms(
    *Definition, Bindings, ReportMissingCallTerm, ClientContext);
```

The callback uses `HTNMissingCallTermCallback`. Handle a false result in the engine's
initialization code. Keep runtime missing-call policy configured and revalidate
following hot reload or binding changes.

Planner ABI values: plain `0x48540005`, instrumented `0x48550006`, profiling
`0x48560005`, instrumented/profiling `0x48570006`. Old modules are incompatible.
`HTNAtom` layout and RuntimeBridge function signatures/revision are unchanged from
2.0.2; nevertheless use the matching rebuilt package throughout.

## Engine acceptance before publication

- Compile with the intended SDK variant.
- Validate complete bindings successfully and exercise all three missing reasons.
- Verify nested comparisons/arithmetic, planning and backtracking.
- Check active-plan execution, debugger integration and engine-managed hot reload.

After integration passes, finalize the release notes, commit and tag the tested
public checkout, then publish the validated SDK ZIP and its SHA-256.
