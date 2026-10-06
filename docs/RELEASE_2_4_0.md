# HTN Planner 2.4.0

Status: **Release preparation; not published.**
Distribution targets: Windows x86_64 and Ubuntu 24.04 x86_64.

## Unregistered world-state facts in instrumented builds

When `HTN_DEBUG_DECOMPOSITION` is defined, `WriteFact` and
`WriteFactWithContext` retain valid writes whose symbols are absent from the
world state's fact registry. This helps clients inspect daemon output that a
compiled domain does not reference, such as `(health 60 60)` or `(hit_reaction)`.

```cpp
#ifdef HTN_DEBUG_DECOMPOSITION
// Both calls return false when these symbols are not registered.
worldState.WriteFact(HtnSymbol::sGetSymbol("health"), 60, 60);
worldState.WriteFact(HtnSymbol::sGetSymbol("hit_reaction"));
const HTNFacts& unusedFacts = worldState.GetUnregisteredFacts();
// Inspect each symbol's tables, indexed by argument count, just like GetFacts().
#endif
```

- These writes still return `false`. `GetFacts()`, fact queries and generated
  planner cursors never expose the inspection rows. Registering a symbol later
  does not promote previously captured rows into planner facts.
- Arguments use the existing `HTNTryToAtom(clientContext, value, atom)` path.
  All conversions must succeed and produce bound atoms before a row is stored.
  Failed conversions leave both stores unchanged. Values are owned copies;
  the client context is borrowed only for conversion.
- Unregistered writes now execute custom converters in instrumented builds.
  Any converter side effects on client services remain the client's responsibility.
- Zero-argument facts and duplicate rows are retained. A null fact symbol is
  rejected without storage. Without a registry, all non-null fact symbols are
  unregistered.
- `ClearFact(symbol, arity)` clears matching inspection rows too, retaining its
  existing return contract (`false` for an unregistered symbol). `RemoveAllFacts()`
  clears all rows in both stores. Empty table containers are retained for reuse.
- Clients can use the new getter in their debugger UI. This change provides the
  inspection data; it does not add a panel to the existing visual debugger.

## Boolean and binary integer compatibility

**Domain language change:** bare `true` and `false` are now ordinary SYMBOL
values in domains and `.worldstate` files. They are not reserved
words or boolean literals. `(== 0 false)` is valid but fails as a condition.
Replace old boolean literals with `0`/`1`, or declare
`(:constants (true 1) (false 0))` and use `@true`/`@false`. The constants are
ordinary user declarations, not predefined aliases. Regenerate existing domains
after migrating their source; previously generated C retains the old literal
types until rebuilt.

- Domain values `0`/`1` can match BOOL `false`/`true` in facts, equality,
  axiom outputs and lists. Matching is symmetric and never changes stored types.
  This behavior is shared by plain and instrumented builds.
- C++ `bool` callterm parameters and `HTNTryParseType` accept BOOL or INT `0`/`1`.
  Other integers and all floats remain invalid boolean arguments. Raw bindings
  receive original atoms and should use the converter to read boolean values.
- Standalone callterm conditions still require a BOOL return value. An INT
  result, including `0` or `1`, continues to report `NonBooleanConditionResult`.
  A callterm's BOOL false result fails a standalone condition but is a valid
  assigned value: `(= ?result (call is_entity_alive ?entity))` binds and continues.
  Missing, failed or unbound results still fail assignment.
- Arithmetic and numeric ordering retain their existing INT/FLOAT behavior.
  Atom text formatting, the planner debugger and Visual Studio natvis display
  BOOL as `0`/`1`, also inside lists, without changing the stored type. Symbols
  named `true`/`false` keep their names.
- The atom layout, generated format and C runtime ABI are unchanged. Rebuild
  runtime libraries and C++ consumers together to use the new equality and
  conversion rules. Regenerate and rebuild domains for the new literal semantics
  and constant comparison rules. Older modules remain ABI-compatible but cannot
  acquire these compile-time changes solely by updating the runtime library.
- This expands matching: facts previously distinguished only by BOOL versus
  INT `0`/`1` may now both match a query. Duplicate rows remain independent
  alternatives during backtracking. Code that needs the actual type can use
  `HTNAtom_GetType`/`HTNAtomIsType`.

## Unregistered-fact compatibility

- The feature and getter exist only under `HTN_DEBUG_DECOMPOSITION`, including
  Release configurations that select an instrumented SDK variant.
- Plain variants keep their previous world-state layout, early rejection of
  unregistered writes and planner behavior. No build variants are added.
- The instrumented C++ `HTNWorldState` layout changes. Rebuild consumers and
  use matching 2.4.0 headers and libraries throughout the client. Do not mix an
  older instrumented library with the new headers.
- The generated domain format, C runtime ABI and `HTNAtom` layout are unchanged.
  This feature alone does not require domain regeneration.

The Windows SDK is built and validated with `BuildAndValidateSDK.bat`, which
reads `VERSION` and packages `dist/HTNSDK-2.4.0-windows-x86_64.zip` and its SHA-256
file. Its extracted SDK directory is suitable for an engine's `thirdparty/HTN/2.4.0`.

## Package validation and release preparation

Build the Windows archive with `BuildAndValidateSDK.bat` and the Linux archive
with `bash BuildAndValidateSDK.sh`, following the Linux dependencies guide.
Both commands read `VERSION` (2.4.0), rebuild SDK variants and validate external
consumers against the extracted archive. Windows validates eight variants;
Linux validates four variants with GCC 14 and Clang 18 consumers.

The source repository records this candidate's completed checks and local log
paths in `docs/VALIDATION_2_4_0.md`. Those logs are local build artifacts and are
not shipped in the SDK. A previous 2.4.0 candidate from before the boolean changes
must be replaced with this rebuilt package.

Engine integration with the final candidate, review, tagging and publication
remain maintainer release steps. Follow `docs/RELEASE_CHECKLIST.md` in the source
repository. No tag or publication is performed by the SDK build commands.
