# Generated recursion and call-frame capacity

## Original regression (MSVC x64, before the fix)

`ComplexScenario/HTNGeneratedRecursionStressTest` runs the existing
`Domains/Test/complex_scenario.domain` and its includes. It loads
`complex_scenario_recursive_100.worldstate`, preserves those
100 mixed-behavior entities, and adds 900 distinct idle entities in memory for
the larger case. The domain source and generated definition are unchanged.

| Entities | Debug | Release |
| --- | --- | --- |
| 100 | Pass | Pass |
| 1000 | Native stack overflow | Native stack overflow |

The failing child exits with Windows status `0xC00000FD` (`-1073741571`).
The parent test process survives and reports the failure. This is not an
assertion about the planner's explicit backtracking capacity. No stack-size
override or artificial stack reduction is used.

Run from `HTNTest`, substituting Release for Debug to test the other build:

```powershell
../bin/Debug-windows-x86_64/HTNTest/HTNTest.exe --gtest_filter="*HTNGeneratedRecursionStressTest*" --gtest_catch_exceptions=0
```

Disabling Google Test's SEH interception exposes the native exit status; normal
Google Test settings may turn it into child exit code 1. The regression expects
exit code **zero**, not a crash. It checks the complete plan against the original
100-entity plan plus the new idle actions, counts every recursion increment and
repeats successful runs using the same world and registry. The child-process
isolation keeps a future stack overflow from terminating the full test suite.

## Cause

The previous `HTNCCodeGenerator.cpp` emitted a direct method call for a compound task, then
finishes task debugger/profiling events and returns its result. A method invokes
its branch continuation, which can invoke another task and another method.
`iterate_over_entities` therefore builds native C stack frames on every step.
Existing explicit pending-continuation and backtracking storage does not remove
this method/task call chain. Compiler optimization is not a stack-safety contract.

## Backend implementation

1. Generate a dispatcher loop with domain-specific method/task states. Each step
   schedules the next state or reports success/failure and returns to the loop.
   It must not recursively enter another method or continuation.
2. Store suspended calls explicitly in generated execution storage: return state,
   variable-frame identity, live bindings/owned temporaries, plan checkpoint and
   branch/choice retry state. Reuse existing snapshot/continuation machinery
   where its lifetime and ownership rules match.
3. On child failure, restore bindings and the partial plan, resume alternatives,
   and propagate exhaustion through the explicit frames. Preserve callterm side
   effects and invocation counts; never replay a call just to reconstruct state.
4. Preserve balanced debugger/profiling events when pushing, resuming and popping
   frames. A deferred task must retain its existing execution-time semantics.
5. Use a fixed array owned by generated execution storage, sized by
   `--call-frame-capacity`. Exhaustion is reported and unwound without a native
   stack overflow. Call-frame capacity is separate from backtracking capacity.

The emitted operations remain domain-specific native C, with no interpreter
dependency. The exported decomposition signature is preserved; the descriptor
ABI changes to expose execution diagnostics. Domain regeneration is required.

## Acceptance gates

- Both new cases pass Debug and Release without increasing native stack size.
- Cover non-tail and mutual method recursion; tail-call optimization alone is
  insufficient for the general solution.
- Deep child failure exercises alternate methods, fact/axiom alternatives and
  restoration of owned values and partial plans.
- Repeated decomposition, deferred expansion and debugger lifecycles remain
  correct. Verify the existing equivalence suite and allocation/capacity tests.
- Check native stack usage stays bounded as domain depth grows, and measure
  execution time and allocations before/after the backend change.

## Implementation details and compatibility

Methods and compound tasks return an internal suspension result (`2`) instead
of directly calling their child. The generated `_RUN` loop invokes one function
at a time, pushing or popping explicit call frames. A resumed method jumps to
its branch continuation and uses the original child success/failure path.
Conditions, pending-task snapshots, plan rollback and callterm evaluation are
not replayed. Task and method debugger/profiling scopes close on actual return.

Each frame retains its function, resume state, child result and branch retry
snapshot. All frames belong to a fixed array in generated execution storage.
`--call-frame-capacity=N` selects its size at translation time (default 8192).
The equivalent C++ translation/generator option is `CallFrameCapacity`.
No new runtime component or bridge function is needed. There is no allocation
or resizing of call frames while decomposing. The array belongs to each planner
execution storage instance, not to a global static shared across planners.

Exhausting the array returns `HTN_DECOMPOSITION_CALL_FRAME_CAPACITY_EXCEEDED`
after unwinding active scopes and destroying snapshots. This status is appended
to the enum, preserving existing numeric values. Call-frame capacity is separate
from pending-continuation/backtracking capacity. Each suspended method/task uses
one frame, so the capacity is not a domain recursion-depth limit in methods.

The planner definition now exposes `get_execution_info(storage)`. Its generated
accessor returns a borrowed `HTNGeneratedExecutionInfo` containing the configured
capacity, peak simultaneous frames, bytes per frame and the last error. Peak and
error reset for each decomposition; a null storage pointer returns null.

The descriptor ABI revision has increased in all instrumentation variants to
guard the new accessor. Rebuild hosts and regenerate/recompile domains together.
This recursion change adds no external runtime/bridge functions and preserves
the atom layout. The accompanying unified callterm error policy changes the
bridge ABI independently; see [callterm migration](MISSING_CALLTERMS.md).
The storage size comes from each generated definition as before.
Initial fixed storage can be substantial: every frame has space for the domain's
largest method retry snapshot. Tune capacity for expected depth and memory budget.

For example, a capacity failure provides this generated diagnostic:

```text
Domains/Test/recursion_dispatch.domain: Domain 'recursion_dispatch' exceeded its call-frame capacity (8192). Regenerate the domain with HTNTranslator --call-frame-capacity=<larger value> and recompile the generated code.
```

Core consumers can read `definition->get_execution_info(execution_storage)->last_error`
after the failure. HTNIntegration reports it through `HTN_LOG_ERROR` when logging
is enabled, and the debugger labels this status with the regeneration instruction.
The message is immutable generated text, available even without logging/debugger;
it borrows the module lifetime and should be copied if retained across unloading.

## Measured fixed-array results (MSVC x64)

| Entities | Peak simultaneous frames | Capacity | Debug | Release |
| --- | --- | --- | --- | --- |
| 100 | 243 | 8192 | Pass | Pass |
| 1000 | 3843 | 8192 | Pass | Pass |

Both attempts in each stress case reported identical peaks. The 1000-entity
case uses 46.9% of the configured frames, leaving 4349 available. These values
apply to this domain/world state (the original 100 mixed-behavior entities plus
900 idle entities); entity count is not a general call-depth bound.

Every frame is 288 bytes for ComplexScenario. Its fixed frame array is 2,359,296
bytes (2.25 MiB), and total execution storage is 2,366,984 bytes (about 2.26 MiB)
per instance, in both measured configurations. The default 8192 is sufficient
for this test but remains provisional for product memory budgeting.

The tests print `peak_frames`, `capacity`, `frame_bytes` and `execution_bytes`.
Additional coverage verifies 1000 levels of non-tail and mutual recursion,
deep failure with exact callterm counts, and bounded native stack addresses.
Capacity tests deliberately exceed 8192 frames and verify the failure status,
an empty partial plan, descriptive diagnostic, a peak equal to the limit, and
successful reuse of the same storage with peak/error reset.

The public source validation runs the complete Debug and Release suites,
including the generated debugger, both recursion stress cases, capacity
recovery, axiom/IO backtracking, nested callterm failures and module loading.
See [2.0.4 release notes](RELEASE_2_0_4.md) for release validation and migration.

Comparative performance/allocation profiling remains pending; these regressions
establish correctness and bounded native stack use, not a speedup guarantee.
