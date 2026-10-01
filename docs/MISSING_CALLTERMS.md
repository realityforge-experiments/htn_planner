# Callterm error policy

Nested `(call ...)` expressions in comparisons and arithmetic use this same
policy. Reports point to the call expression itself, including when its result is
assigned to a local variable. See the [nested-call regression and migration notes](RELEASE_NOTES_NESTED_CALLS.md).

Runtime options are direct fields of the execution descriptor. Configure them
through `HTNPlannerExecutionContext` (reference integration) or
`HTNGeneratedPlannerContext` (core generated execution). Callterm bindings contain
only registry/daemon configuration and do not own policies or callbacks.

```cpp
void ReportCallTermError(void* clientContext, const HTNCallTermErrorInfo* info)
{
    auto& engine = *static_cast<EngineHTNContext*>(clientContext);
    // Use engine.Diagnostics and info->Reason / Name / DaemonID / Source.
}

auto& execution = planningUnit.GetExecutionContext();
execution.ClientContext = &engineContext;
execution.CallTermErrorPolicy = HTNCallTermErrorPolicy::Report;
execution.CallTermErrorCallback = ReportCallTermError;
execution.BacktrackingMode = HTN_BACKTRACKING_ALL;
```

Planning units copy these options for every decomposition, including deferred
calls. They supply their own world state, bindings, call and storage pointers in
the per-invocation copy. Configure options only while idle. If calling the hook
directly, populate an `HTNPlannerExecutionContext` and pass it to `Decompose`.
No separate runtime-options substructure is required.

Core-only generated hosts use the equivalent direct fields:

```cpp
HTNGeneratedPlannerContext execution{};
execution.callterm_binding_context = &bindings;
execution.client_context = &engineContext;
execution.callterm_error_policy = HTNCallTermErrorPolicy::Report;
execution.callterm_error_callback = ReportCallTermError;
// Also provide world_state, backtracking_mode, prepared_storage and execution_storage.
```

C clients use `HTN_CALLTERM_ERROR_UNSET`, `HTN_CALLTERM_ERROR_FAIL_SILENTLY`
and `HTN_CALLTERM_ERROR_REPORT`. Policy/reason enums use 32-bit representations
and the callback takes an info pointer in both C and C++.

## Policy and failure behavior

- `Unset` is zero, the initial value of a zero-initialized descriptor. An invocation error triggers an SDK assert with an explicit configuration message.
- `FailSilently` returns failure without logging or reporting.
- `Report` invokes the callback with the current execution's client pointer,
  then returns failure if the callback returns. The client decides whether to
  log, assert or terminate. A null callback triggers an SDK assert.
- Unknown enum values also assert on an invocation error.

With assertions disabled, invalid/missing configuration fails safely without
calling a null callback. Valid callterms execute normally even with `Unset`;
configuration checks occur when an invocation error must be handled.
Argument type/count mismatches and converter failures use this same policy.
A callable returning false/unbound normally is not an invocation error and does
not trigger the callback. `Report` runs exactly once per failed invocation;
backtracking may legitimately attempt that call again. Failed call/arithmetic
expressions used as task arguments fail that task attempt; generated execution
does not append a primitive task containing that failed result.

## Reasons and provenance

- `ArgumentCountMismatch`: the typed binding's arity does not match the call.
- `ArgumentTypeMismatch`: the supplied atom type does not match the signature.
- `ArgumentConversionFailed`: the converter rejected a representation that
  passed the signature check (for example, a stale entity ID).
- `ReturnConversionFailed`: the function ran but its result could not be converted.
- `NonBooleanConditionResult`: a generated standalone callterm condition returned
  a valid non-boolean atom. `ExpectedAtomType` is bool, `ExpectedTypeName` is
  `"bool"`, and `ActualAtomType` is the returned representation. The callback runs
  once under `Report`, then the condition fails if the callback returns. A bool
  `false` is still an ordinary failure. Use an explicit assignment or comparison
  for non-boolean results; implicit truthiness is not supported.

For the standalone-condition check, source data points to `(call ...)`, including
in deferred decomposition and without debugger instrumentation. Custom types are
reported using their atom representation: a Vector3 stored as a list is `list`.
See [standalone callterm conditions](RELEASE_NOTES_BOOLEAN_CALLTERMS.md) for examples
and compatibility. This new reason preserves existing payload layouts and ABI
versions; regenerate domains to enable the check.

`ArgumentIndex` is zero-based (`UINT32_MAX` when not applicable).
`ExpectedArgumentCount` and `ActualArgumentCount` describe the invocation.
`ExpectedAtomType` and `ActualAtomType` contain `HTNAtomType` numeric values,
with `UINT32_MAX` for unavailable metadata. `ExpectedTypeName` is available for
converter failures. Type/count errors prevent calling the client function.
Raw/untyped bindings cannot validate an unspecified signature automatically.


- `NotRegistered`: the name is absent from the registry.
- `MissingBinding`: the registry entry has no callable.
- `MissingInstance`: a member callable exists but its daemon instance is absent;
  `DaemonID` identifies the required daemon type.

Reports include the name, reason, optional daemon ID and source provenance.
Generated calls supply domain, file, line and column even without debugging.
Calls without provenance use null strings/zero coordinates. Preparing a cached
slot does not report: only actually attempting the missing call does.

All report data is borrowed during the callback; copy strings to retain it.
Callbacks are synchronous, and the client owns its payload/services. Keep them
alive throughout execution and while planning units may expand deferred calls.
Do not reconfigure an active execution or throw across the runtime boundary.

Multiple executions can share bindings while selecting different policies,
callbacks and client pointers. Changes between executions take effect without
rebuilding or invalidating cached callterm slots. Clients synchronize any shared
mutable services. Registry mutation during execution remains unsupported.

## World-state restrictions in callterms

Callterms and report callbacks must not delete facts while decomposition is in
progress. Do not remove rows, clear fact tables or reset/replace the world state
from a callback. Queue removals and apply them after decomposition returns, when
no other decomposition is using that world state. This is a planner assumption,
not a runtime-enforced check, and also applies to deferred method expansion.
See the [world-state decomposition contract](../HTNFramework/src/Translator/HTNGeneratedPlannerInterface.md#world-state-lifetime-during-decomposition).

## Migration and ABI

The unified API replaces `HTNMissingCallTermPolicy/Reason/Info/Callback` with
`HTNCallTermErrorPolicy/Reason/Info/Callback` in `Core/HTNCallTermError.h`.
Rename execution fields to `CallTermErrorPolicy` / `CallTermErrorCallback`
(or `callterm_error_policy` / `callterm_error_callback` for core generated hosts).
Rebuild the host, SDK libraries, and generated modules together: the callback
payload has expanded and both planner and RuntimeBridge ABI revisions changed.
Regenerate domains with the matching translator. `HTNAtom` layout is unchanged.
Initialization validation keeps reporting missing registrations/bindings/instances;
argument conversion requires actual runtime values and is checked at invocation.


Remove legacy `SetMissingCallTermPolicy` calls on bindings. Assign the execution's policy
and callback directly. Change report callbacks from an info reference to a pointer.
Both C invocation exports now take an `HTNGeneratedPlannerContext*` as their first
argument; resolving a slot still takes the binding context. The C++ registry
`Execute` takes `HTNPlannerExecutionContext`.

The initial policy implementation used planner ABI versions plain `0x48540004`,
debug `0x48550005`, profiling `0x48560004`, debug/profiling `0x48570005`.
The final 2.0.4 release, including the debugger metadata correction, uses planner
ABI plain `0x48540007`, debug `0x48550009`, profiling `0x48560007`, debug+profiling
`0x48570009`, and RuntimeBridge revision 8. Version 2.1.0 retains these revisions;
its non-boolean condition diagnostic adds an error reason without changing layouts.
Regenerate domains and rebuild hosts, runtime bridge and modules together.
Old definitions/tables are rejected; the atom and cached-callterm layouts remain
unchanged. See [type conversions](TYPE_CONVERSION.md) for converter migration.

## Production fallback validation

The SDK's external CoreConsumer checks both the C++ registry and the generated
invocation API for unregistered names, absent bindings and missing daemon instances.
It checks FailSilently and Report in all variants. Release variants additionally
check Unset, an invalid enum value and Report without a callback: each returns an
unbound failure without invoking a callback. A registered working call remains
callable under each policy. Checks use return codes, not assertions.

The SDK manifest propagates NDEBUG for Release and _DEBUG for Debug, matching
the compiled library. Development Release does not currently disable assertions;
its death tests do not replace these packaged production checks. No API or ABI
change is required for this coverage.

## Initialization validation (unreleased)

After loading the generated definition and configuring the registry and daemon
instances, validate all required call sites explicitly:

```cpp
HTNCallTermBindingContext Bindings(Registry);
Bindings.SetDaemon("agent", &Agent);
const bool Ready = Registry.ValidateGeneratedCallTerms(
    *Definition, Bindings, ReportCallTermError, ClientContext);
// The host decides whether a false result should prevent initialization.
```

The callback has the existing `HTNCallTermErrorCallback` signature. Validation
shares runtime checks for `NotRegistered`, `MissingBinding` and `MissingInstance`,
in that order. It never executes callterms and does not use the runtime error
policy. An omitted callback still returns the correct success/failure result.
Reports are synchronous, with borrowed name, daemon ID and source data; copy data
that must outlive the callback. Do not mutate the registry or bindings from the
callback or concurrently with validation.

Every emitted call site is checked, including linked domains, nested calls, task
arguments and currently unreachable branches. A missing name used at multiple
locations produces a report for each location. Source metadata is present in
plain and instrumented definitions. No argument type checking is attempted.

A descriptor with an incompatible ABI or malformed metadata, or a binding context
belonging to another registry, returns false without a missing-callterm report.
A domain with no callterms succeeds. Revalidate after hot reload or binding changes;
normal runtime checks remain active because instances can change afterward.

This adds a call-site table to `HTNGeneratedPlannerDefinition`. Regenerate and
recompile domain modules and rebuild their host with matching headers. Planner ABI
versions for the call-site-table introduction were plain `0x48540005`, debug
`0x48550006`, profiling `0x48560005`, and debug/profiling `0x48570006`.
The unified error policy increments those revisions as listed under migration. `HTNAtom` and runtime bridge function signatures
are unchanged. Existing published SDK artifacts are not modified.
