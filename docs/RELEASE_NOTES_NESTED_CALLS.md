# Unreleased: nested callterm expressions

The compiler evaluates `(call ...)` operands before using their results in
comparisons (`<`, `<=`, `>`, `>=`, `==`, `!=`) and arithmetic (`+`, `-`, `*`, `/`,
`%`, `++`, `--`). Calls can appear on either side, inside another call, and
inside arithmetic task arguments. They are never interpreted as literal callterm
names. Unsupported unlowered calls produce a compiler diagnostic instead.

```lisp
(= ?old_position (1.0 0.0 0.0))
(= ?new_position (2.0 0.0 0.0))
(< (call get_distance_from_to ?old_position ?new_position) 0.2)
```

Binding first remains supported using the explicit declaration syntax:

```lisp
(= ?distance (call get_distance_from_to ?old_position ?new_position))
(< ?distance 0.2)
```

`=` declares a fresh variable; `==` compares equality. The earlier implicit
`(?distance (call ...))` syntax remains invalid.

Missing nested calls use the existing execution-context policy. `Report` invokes
the callback once per attempted invocation, with `NotRegistered`, `MissingBinding`
or `MissingInstance` as appropriate. `FailSilently` produces no report. `Unset`
retains the SDK assertion/failure contract. The reported source location points
to the opening parenthesis of `(call ...)`, including in explicit assignments,
and remains available in plain builds without debugger instrumentation.

Call arguments and condition operands are evaluated in source order. Failure of
an earlier operand prevents subsequent condition invocations. A failed call cannot
be consumed as a value; its enclosing condition/task fails normally. Backtracking
may legitimately attempt a call again, producing another report. External callterm
side effects are not rolled back.

## Integration

Regenerate and recompile affected domain C sources. Previously generated binaries
retain their previous behavior. C ABI signatures, RuntimeBridge ABI revision and
`HTNAtom` layout are unchanged. No published SDK artifacts are modified.

## Regression validation

`Domains/Test/nested_operator_calls.domain` contains the minimal reproduction and
cases for nested expressions, operand order and missing-call policies. The tests
check generated invocations, runtime results, exact report counts and source
locations, plus the absence of call-name literals used as operands.

Generate the Windows full-suite matrix with:

```powershell
./ThirdParty/premake/bin/premake5.exe --test-matrix vs2022
```

Build `HTN.sln` (so its translator dependency runs before domain generation) for
`DebugPlain`, `DebugInstrumented`, `ReleasePlain`, and `ReleaseInstrumented`, x64.
Run each configuration's `HTNTest.exe` with `HTNTest` as the working directory.
The matrix uses the same instrumentation flags as SDK variants and the existing
GoogleTest package restored by normal solution generation. It does not package or
overwrite any SDK. Run `GenerateProjectFiles.bat` to restore normal configurations.

Validated on Windows x64 with MSVC on 2026-09-28 (full suites, no filters):

| Configuration | Passed | Failed |
| --- | ---: | ---: |
| Debug Plain | 294 | 0 |
| Debug Instrumented | 302 | 0 |
| Release Plain | 294 | 0 |
| Release Instrumented | 302 | 0 |

These results were obtained in the development repository before export.
The public branch contains only the generated backend and its regression tests.
