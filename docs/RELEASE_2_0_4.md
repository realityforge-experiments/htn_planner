# HTN Planner 2.0.4

Release candidate. Windows x64/MSVC v143 is the validated distribution target.

**Compatibility warning:** this version changes the public callterm error API,
generated planner descriptor and RuntimeBridge ABI. Despite the patch number,
it does not provide the usual SemVer patch compatibility guarantee. Replace
any earlier local 2.0.4 candidate and rebuild everything with matching headers.

## Changes

- Generated methods and compound tasks run through an iterative dispatcher.
  Suspended calls occupy a fixed array in each execution storage instance;
  the recursion machinery adds no external runtime component or bridge function.
- `HTNTranslator --call-frame-capacity=N` controls that array (default 8192).
  Exhaustion returns `HTN_DECOMPOSITION_CALL_FRAME_CAPACITY_EXCEEDED`, unwinds
  the attempt and leaves storage reusable. Its diagnostic includes the domain,
  configured capacity and regeneration option.
- `Definition->get_execution_info(storage)` exposes capacity, peak frames,
  bytes per frame and last error. The result is borrowed; peak/error reset on
  each decomposition. No change to existing decomposition status numeric values.
- `HTNCallTermErrorPolicy` extends missing-call reports to argument count/type
  mismatches and argument/return conversion failures. Nested calls and task
  arguments use the same callback, once per failed invocation, with source data.
- Axioms can assign their unused output/IO parameters and propagate results
  through the caller and backtracking. Pure output arguments must arrive unbound;
  assignment to a bound IO slot fails before evaluating its initializer.
- Unknown variables in axiom expressions are rejected with located diagnostics.

## Engine migration

1. Install this SDK separately from the previous published version. Use matching
   CRT and instrumentation variants for all headers, libraries and modules.
2. Replace `Core/HTNMissingCallTerm.h` with `Core/HTNCallTermError.h`.
   Rename `HTNMissingCallTermPolicy/Reason/Info/Callback` to
   `HTNCallTermErrorPolicy/Reason/Info/Callback`, and update custom error switches.
3. Configure `CallTermErrorPolicy` / `CallTermErrorCallback` directly on
   `HTNPlannerExecutionContext`. Core generated hosts use `callterm_error_policy`
   / `callterm_error_callback`. Keep `ClientContext` / `client_context` borrowed
   for the duration of execution. Initialization validation uses the new callback
   type; runtime argument checks require actual invocation values.
4. Rebuild copied integration code. Regenerate every domain using the new
   translator; rebuild the host, RuntimeBridge and generated objects/DLLs together.
5. Review axiom calls: use a fresh unbound variable for `?out_`; use `?io_`
   for bound-value matching. Assignment to an already bound IO slot fails.
6. Test deep planning and inspect peak frames. Increase `--call-frame-capacity`
   if needed, or reduce it to suit the client's memory budget, then regenerate.
   The capacity counts suspended generated method/task frames, not entities.

```cpp
auto& execution = planningUnit.GetExecutionContext();
execution.ClientContext = &engineContext;
execution.CallTermErrorPolicy = HTNCallTermErrorPolicy::Report;
execution.CallTermErrorCallback = ReportCallTermError;
// void ReportCallTermError(void*, const HTNCallTermErrorInfo*);
```

The client chooses how to handle `Report` (log, assert or terminate).
`FailSilently` suppresses reports. `Unset` retains its SDK assertion behavior
when an invocation error occurs. An ordinary callable result of false/unbound
does not itself constitute an invocation error.

| ABI | Plain | Decomposition debug | Profiling | Debug + profiling |
| --- | --- | --- | --- | --- |
| Planner | `0x48540007` | `0x48550008` | `0x48560007` | `0x48570008` |
| RuntimeBridge | `0x48580008` | `0x48590008` | `0x485A0008` | `0x485B0008` |

Old descriptors/tables are rejected. `HTNAtom` layout remains unchanged.
Packaged variants include plain and decomposition debug; advanced profiling
is not enabled in the standard eight-variant distribution.

## Capacity and memory

The 1000-entity ComplexScenario regression reaches 3843 of 8192 frames. Its
execution storage is 2,366,984 bytes per instance in the measured builds.
This is one workload: 100 original mixed-behavior entities plus 900 idle entities.
It is neither a general 1000-entity limit nor a guarantee for other domains.
Capacity determines fixed per-instance memory; tune it for the target game.

## Validation gates

- Build the complete public source solution in Debug and Release and run all tests.
- Exercise non-tail/mutual recursion, deep rollback, capacity exhaustion/reuse,
  debugger completion, nested callterm reports and axiom/IO assignments.
- Build all eight SDK variants, package, extract and validate 24 external
  consumer executions and eight domain-object/bridge-export checks.
- Reject incompatible CRT selections and unknown variants; verify checksums,
  build provenance, ABI metadata and source isolation.

Comparative performance profiling and native Linux validation remain pending.
Hot reload support still requires client-owned loading, lifetime management and
synchronization. Engine integration of this new candidate remains a separate test.

See [recursion](GENERATED_RECURSION.md), [callterm errors](MISSING_CALLTERMS.md)
and [axiom assignments](RELEASE_NOTES_AXIOM_ASSIGNMENTS.md) for full details.
