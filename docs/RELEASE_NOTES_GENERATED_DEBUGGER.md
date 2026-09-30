# Generated debugger: source expressions (2.0.4)

## Correction

Nested call expressions are lowered into internal assignments by the compiler.
The debugger previously exposed those assignments, their temporary variables and
a synthetic `and` instead of the condition written in the domain. It also omitted
parentheses when reconstructing titles.

The compiler now records the original expression and source range independently
of the execution IR. Lowering steps and temporary slots are explicitly marked as
internal. The debugger captures one visible node for the original expression,
preserves its outcome and retries, and excludes internal slots from the watch.
Condition and task titles include parentheses; composite titles use `...` for
their separately displayed children. Evaluation order and generated execution
algorithms are unchanged.

## Compatibility

- Plain planner ABI remains `0x48540007`; profiling-only remains `0x48560007`.
- Debug planner ABI becomes `0x48550009`; debug plus profiling becomes `0x48570009`.
- RuntimeBridge ABI revision 8 and `HTNAtom` layout are unchanged.
- Regenerate instrumented domain C sources and rebuild the host and modules together.
- Custom colored renderers should respect `TitleToken.SpaceBefore` to reproduce
  punctuation spacing. The reference ImGui view does so.

The final 2.0.4 SDK includes this correction (build ID
`c9d2878ef0084464964c779ce7cf4cd0`). Replace earlier local candidate packages
and regenerate instrumented domains when updating an integration.

## Regression coverage

`Domains/Test/nested_operator_calls.domain` includes the reported
`continue_move_to_seen_entity` pattern, retries, skipped conditions and nested
assignment initializers. Tests verify source titles and locations, real call
counts, plan results, balanced events, parent/child relationships, preserved
output values and exclusion of temporaries from the watch. Existing cases also
exercise both operands, every arithmetic operator, missing-call policies, list
constants and axiom backtracking with the debugger enabled.

## Public source validation

The complete public solution built successfully on Windows x64 / MSVC on
2026-09-30. Full suites passed: 244 Debug tests and 235 Release tests.

- Debug build: `build/logs/public-port-debugger-Debug-build.log`.
- Debug tests: `build/logs/public-port-debugger-Debug-tests.log`.
- Release build: `build/logs/public-port-debugger-Release-build.log`.
- Release tests: `build/logs/public-port-debugger-Release-tests.log`.

Coverage includes original source titles and punctuation, hidden compiler slots,
nested assignments, skipped conditions, retries and 1000-entity generated execution.

The updated SDK passed extracted-package validation for all eight variants:
24 external consumer executions and eight domain-object/bridge-export checks.
CRT mismatch and unknown-variant rejection, all 184 payload checksums, archive
SHA-256 and instrumented planner ABI `0x48550009` were verified. Final packaging
changed release documentation only; executable payloads remain identical to
those validated. Local evidence: `build/logs/release-2.0.4-sdk-audit.json` and
`build/logs/release-2.0.4-sdk-validation/`.
The engine's copied integration and its UI require a separate integration check.
