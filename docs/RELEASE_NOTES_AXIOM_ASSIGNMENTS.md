# HTN 2.0.4: axiom output assignments

Axioms can initialize their own unused `?out_` parameters with explicit `=`
assignments, including literals, arithmetic expressions and callterm results.
The assigned slot is exported to the caller on success and restored on failure
or backtracking. Independent alternatives can produce different output values.

The compiler validates declaration and use before lowering to IR. Inputs, self-reference,
previously used locals and repeated assignments remain validation errors with
file, line and column. An unused `?io_` parameter may be assigned only when it is
unbound at runtime; a bound destination fails before its initializer executes,
including calls lowered from nested expressions.

## Contract and migration

Pure `?out_` arguments now require an unbound caller variable. Previously a
bound value was accepted and compared against the produced output. Calls with
bound variables, literals or arithmetic output arguments now fail before the
axiom body executes in all build configurations. Use `?io_`
for input/output matching, or receive a fresh output and compare it afterwards.

See [ASSIGNMENT.md](ASSIGNMENT.md) for syntax and examples.

## Compatibility

- Regenerate and recompile domain C sources to get the output entry checks and
  support assignments to output parameters. Existing generated binaries retain
  the old contract until regenerated.
- The assignment correction itself does not alter the C ABI or atom layout.
  The accompanying unified callterm error policy expands the callback payload
  and increments planner/RuntimeBridge ABI revisions. Rebuild the host, SDK and
  generated modules together; see [callterm migration](MISSING_CALLTERMS.md).
- The compiler IR has an internal guard for lowered assignment initializers;
  this is not a runtime descriptor field. Rebuild compiler consumers that use
  compiler C++ structures directly.
- No published SDK package or engine integration is modified by this change.

## Regression coverage

Axiom bodies now also receive the variable-provenance checks used for methods.
Unknown variables in callterm arguments, nested expressions, comparisons and
assignment initializers are rejected with source locations by the compiler.
For example, declaring `?inp_entity` does not declare `?inp_entity_id`.
Declared IO parameters remain valid inputs, subject to runtime binding checks.
This is a scope check, not a guarantee that a declared variable is bound on every
control-flow path; a separate definite-binding analysis is still pending.

`HTNAxiomAssignmentTest` checks compiler frontend and generated IR validation.
`Domains/Test/axiom_assignments.domain` contains the exact `select_position`
regression and generated runtime cases for output propagation,
subsequent reads, arithmetic, multiple solutions, rollback, nested forwarding,
bound outputs and bound/unbound IO assignments. Debug builds also verify that
generated debugger nodes complete. Existing axiom matching tests now expect
bound pure outputs to fail, while IO matching/backtracking coverage is retained.

The IO coverage extension adds ten runtime scenarios, each executed twice:
failed-body rollback, assignment-driven solutions, failed and successful `alt`
retries, exhausted solutions, preservation of a different bound value, and
literal/arithmetic assignment guards. Call counts verify that rejected callterm
initializers do not run. Five negative IO syntax cases check diagnostics, plus
valid `or`/`alt` initialization.

See [2.0.4 release notes](RELEASE_2_0_4.md) for the combined release and migration requirements.

The generated-only release branch was validated on Windows x64 with MSVC by
building the complete `HTN.sln` in both configurations:

| Configuration | Full test suite | Result |
| --- | --- | --- |
| Debug (generated debugger/instrumentation enabled) | 232 tests | Passed |
| Release (plain generated) | 224 tests | Passed |
