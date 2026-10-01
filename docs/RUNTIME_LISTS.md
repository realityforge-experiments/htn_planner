# Runtime list values

The compiler frontend and generated backend accept lists containing variables,
arithmetic expressions, callterms and other lists wherever a value is accepted:

```lisp
(call ppr_start (find_closest_location_to_entity ?inp_entity_id))
(= ?segment (segment (++ ?index) ?position))
(= ?target (target ?entity_id (call get_entity_position ?entity_id)))
(= ?action (action
    (target ?entity_id)
    (movement ?from ?to)
    (metadata current_time (call get_time))))
```

## Evaluation and ownership

Elements are evaluated from left to right using the current bindings. Arithmetic
and callterm results become values inside the list. Evaluation stops at the first
failure; later callterms and any enclosing callterm are not invoked. A boolean
`false` returned as a list element is a valid value, not a failed condition.

Every variable read must be bound. A list argument is a constructed input value;
it does not introduce variables or destructure a fact into output bindings.
Unknown variable names are rejected by semantic analysis. A variable that is
known but unbound on the current execution path fails at runtime.

The completed atom owns its elements, including strings and nested lists. Input
atoms are copied, never moved out of caller variable slots. Generated temporaries
are destroyed on success and failure; a failed construction never exposes a
partial list. Existing assignment rules apply: in particular, an already-bound
`?io_` destination rejects the assignment before evaluating its initializer.

Lists work in callterm arguments, assignments, comparisons, fact and axiom
arguments, primitive/compound task parameters, plan markers, deferred calls and
other lists. Fully literal lists, such as `(query_extent_y (1.0 1.0 1.0))`, keep
their prepared/static representation. Domain constants remain literal-only.
Empty-list syntax retains its existing parser contract.

## Backtracking and deferred execution

A list is rebuilt when its expression is evaluated again for another alternative,
using that alternative's bindings. A continuation that already owns the evaluated
value can retain it without replaying its callterms. Variable/plan rollback does
not undo client side effects or permitted world-state insertions from callterms.

Arguments of `(&later (snapshot ?id (call get_time)))` are evaluated when the
deferred plan step is created and captured as owned values. Expressions in the
body of `later` are evaluated when that deferred call is subsequently decomposed.

## Diagnostics

On a failed decomposition, inspect the existing
`definition->get_execution_info(context.execution_storage)->last_error`.
List evaluation records a message with the element/expression, domain, file,
line and column, for example:

```text
Runtime list element '?entity_id' is unbound. Source: Domains/Wanderer.domain:8:60 (domain 'Wanderer').
Callterm 'get_entity_position' failed while constructing a runtime list. Source: Domains/Wanderer.domain:8:60 (domain 'Wanderer').
```

The message borrows immutable text from the generated module. It describes the
most recent list-evaluation failure in that decomposition and is cleared on a
successful decomposition or the next entry. It is not a history of rejected
alternatives. Generated code does not print it; a host may display it.

Registry invocation errors continue to use `HTNCallTermErrorPolicy` and its
callback with the original `(call ...)` source location. The list builder does
not issue a second callback for the same error. A registered callterm returning
an unbound result aborts construction and supplies the list diagnostic even if
there was no registry error. `FailSilently` suppresses the callback; the borrowed
execution diagnostic remains queryable. Nested calls remain listed in the
definition's initialization-time callterm requirements.

## Compatibility and implementation

The internal AST/IR now distinguish runtime list expressions from literal atoms.
Their children retain evaluable values and source locations. Conditions lower a
list into an owned temporary when a fact, axiom or operator needs an atom reference;
task arguments use the existing generated temporary slots and snapshots. Calls
inside a list are emitted in evaluation order, with the standard invocation bridge
and fact-storage refresh after world-state mutations.

There is no new runtime component, exported bridge function, public C structure
layout, atom representation or generated ABI revision. Regenerate and recompile
domains that use runtime lists with the updated translator and matching headers.
Previously generated modules remain compatible. Runtime lists are evaluated by
the generated code using the existing runtime interfaces.
This feature is included in the 2.1.0 SDK candidate. Previously published archives remain unchanged.

## Regression coverage

`Domains/Test/runtime_lists.domain` and `HTNTest/src/HTNRuntimeListTest.cpp` cover
static representation, dynamic bindings, every arithmetic operator, nested calls
and lists, all argument positions, output propagation, backtracking, deferred
capture, failed construction, exact call source, error policies, missing bindings
and instances, assignment guards, debugger titles and owned-resource cleanup.

The portable `cmake/LinuxSmoke` consumer also checks runtime lists containing a
variable, arithmetic and a callterm. It can be built as C11 with native GCC/Clang
on Linux using the commands in [LINUX_SMOKE.md](LINUX_SMOKE.md). Windows clang-cl
validation is separate from native Linux validation.

## Development-branch validation (2026-10-01)

The original translator rejected the minimal `ppr_start` example with `Expected
compiler literal`: `build/logs/runtime-list-before-fix.log`. The updated compiler
translates it and the end-to-end regression checks the list seen by the binding.

Windows x64/MSVC test results:

| Configuration | Passed | Test log |
| --- | ---: | --- |
| DebugInstrumented | 363 | `build/logs/runtime-lists-DebugInstrumented-tests.log` |
| ReleaseInstrumented | 363 | `build/logs/runtime-lists-ReleaseInstrumented-tests.log` |
| DebugPlain | 354 | `build/logs/runtime-lists-DebugPlain-tests.log` |
| ReleasePlain | 354 | `build/logs/runtime-lists-ReleasePlain-tests.log` |

All four runs include the 19 runtime-list regressions and generated recursion at
100 and 1000 entities. They exclude the previously identified slow interpreter
parity case `InterpreterAndGeneratedProduceSameResult/Recursive100Entities`;
that exclusion is not a claim that the case passed. Summary:
`build/logs/runtime-lists-matrix-results.json`. Matching build logs use the same
configuration names and the `-build.log` suffix.

The C11 smoke consumer passed all three CTest checks in Debug and Release with
Windows clang-cl: `build/logs/runtime-lists-clang-Debug-tests.log` and
`build/logs/runtime-lists-clang-Release-tests.log`. Its old bound `?out_` example
was adapted to receive the output in a variable before comparison, preserving
its axiom-backtracking assertion. The RuntimeBridge audit also passed:
`build/logs/runtime-lists-runtime-bridge-audit.log`.

With `--atom-diagnostics` enabled, ProfileDetailed passed all 19 runtime-list
tests. The ownership regression verifies that allocations actually occurred and
that live strings/list nodes return to their baseline after success, partial
construction failure and repeated use. The final suite reports zero live heap
strings, zero live heap-string bytes and zero live list nodes. Evidence:
`build/logs/runtime-lists-memory-tests.log` and
`build/logs/runtime-lists-memory-build.log`.

Native Linux execution remains unverified: WSL is not installed on this host.
Evidence: `build/logs/runtime-lists-linux-availability.log`. No SDK package was
published or replaced during this validation.

## Generated public branch merge (2026-10-01)

After merging into `generated_public_release`, the complete solution built in
Debug and Release. The full generated-only suites passed without exclusions:
283 tests in Debug and 274 in Release, including all 19 runtime-list regressions
and the generated recursion tests at 100 and 1000 entities.

- Build logs: `build/logs/merge-runtime-lists-Debug-build.log` and
  `build/logs/merge-runtime-lists-Release-build.log`.
- Test logs: `build/logs/merge-runtime-lists-Debug-tests.log` and
  `build/logs/merge-runtime-lists-Release-tests.log`.
- Results: `build/logs/merge-runtime-lists-results.json`.
- Source checks: `build/logs/merge-runtime-lists-source-boundary.log` and
  `build/logs/merge-runtime-lists-sdk-source-boundary.log`.
- RuntimeBridge audit: `build/logs/merge-runtime-lists-runtime-bridge-audit.log`.

The merge preserves the generated-only source boundary and adds the runtime-list
documentation to SDK packaging. It does not publish a package or change the SDK
version or ABI.
