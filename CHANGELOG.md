# Changelog

All notable changes to HTN Planner are documented in this file. Compatibility
exceptions are called out explicitly in each release.

## 2.0.3 - 2026-09-29

**Breaking compatibility despite the patch version:** migrate assignments and
regenerate/rebuild all domain modules and the host.

### Callterm evaluation and initialization validation

- Evaluate nested callterms in comparisons and arithmetic instead of treating
  their names as literal values. Preserve missing-call policies and exact source
  locations. See [nested-call regression notes](docs/RELEASE_NOTES_NESTED_CALLS.md).
- Add `HTNCallTermRegistry::ValidateGeneratedCallTerms` for explicit initialization
  checks of missing registrations, bindings and instances without invoking calls.
- Generated definitions expose call-site requirements in plain and instrumented
  builds. Each original call site is reported once, including linked sources.
- **Compatibility:** the planner descriptor ABI changes. Rebuild the host and
  regenerate/recompile domain modules with matching headers; atom layout and
  RuntimeBridge function signatures are unchanged. See
  [initialization validation](docs/MISSING_CALLTERMS.md#initialization-validation-unreleased).

### Explicit variable declarations (breaking domain syntax change)

- Declare and initialize fresh local variables with `(= ?value expression)`.
- Reject implicit call-result binding and destinations already declared or used.
- Support literal, variable, arithmetic and callterm initializers with backtracking.
- Update generated debugger assignment display and compiler regression tests.
- Migrate domains and regenerate their C source; C function signatures and atom
  layout are unchanged. See [migration instructions](docs/ASSIGNMENT.md).

## 2.0.2 - 2026-09-25

- Fix missing `HTNAtom_SetInt` and `HTNAtom_SetFloat` exports in RuntimeBridge.
- **Rebuild required:** RuntimeBridge ABI revision is now 7. Rebuild the host,
  bridge and generated domain modules together. Planner and atom layouts are unchanged.
- Add generated DLL arithmetic/backtracking coverage and object/export checks
  across all eight SDK variants; verify build provenance and complete package checksums.
- Validate hot reload definitions before callbacks and test malformed fact-name rollback.
- Extend missing-callterm policy checks to Release consumers and enforce SDK source isolation.
- See [release notes and migration](docs/RELEASE_2_0_2.md).

## 2.0.1 - 2026-09-24

### World-state fact conversion

- `WriteFact` now converts arguments through `HTNTryToAtom`, supporting custom
  types registered with `HTNTypeTraits` and `HTNTypeConverter`.
- Added `WriteFactWithContext(clientContext, fact, arguments...)`; the context
  is borrowed for conversion and is not stored in the world state.
- Failed conversions or unbound results return `false` without modifying fact
  storage or inserting partial rows.
- Native atoms and lists are copied, even when passed with `std::move`.
- **Compatibility:** rvalues are no longer consumed and unbound arguments are
  rejected. The C ABI and atom layout are unchanged. Rebuild C++ consumers and
  review the [ownership migration notes](docs/RELEASE_NOTES_WRITE_FACT.md).

## 2.0.0 - 2026-09-23

- Method and axiom overloads by arity.
- Arithmetic arguments across task, callterm and axiom calls.
- Nested axiom backtracking and bound-output unification fixes.
- Deferred calls use `&`; `#` is reserved for axioms.
- Execution-owned client context, context-aware converters and missing-callterm policies.
- Located frontend diagnostics and public integration use cases.
- **Migration required:** domain syntax, custom converters and runtime ABI changed.
  See [release notes](docs/RELEASE_2_0_0.md).

## 1.1.0 - 2026-09-20

### Added

- Builtin integer and floating-point arithmetic expressions using `+`, `-`, `*`,
  `/` and `%`.
- Unary `++` and `--` arithmetic expressions.
- Numeric expressions can be nested and used as operands in builtin comparisons.
- Numeric expressions demo domain and generated planner tests.

### Validated

- Verified nested and mixed arithmetic through the generated execution path.
- Verified runtime failure for division by zero and invalid operand types.

## 1.0.0 - 2026-09-19

### Added

- Ahead-of-time domain translation through a compiler-owned lexer, AST and IR.
- Native generated planner runtime with configurable backtracking.
- Generated execution debugger with source ranges and bound values.
- Optional C++ integration layer and runtime bridge for dynamic domain modules.
- Visual SDL and ImGui demo, editor, language server and VS Code extension.
- Eight Windows x64 SDK variants covering static and dynamic CRT linkage and
  optional execution instrumentation.
- External SDK consumer validation and hot reload examples.

### Changed

- Public runtime and SDK now use the generated execution architecture throughout.
- SDK documentation and release validation describe the complete variant matrix.

### Validated

- Integrated the packaged SDK into an external game engine using engine-owned
  planner hooks, planning units, world-state updates and callterm daemons.
- Validated dynamic domain compilation, runtime bridge loading and repeated hot
  reload while the engine was running under Visual Studio.
- Validated all eight SDK variants through isolated external consumers.
