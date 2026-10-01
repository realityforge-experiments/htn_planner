# Compiler pipeline and IR boundary

The generated planner follows this sequence:

```mermaid
flowchart LR
    Source["Domain source text"] --> Lexer["HTNCompilerDomainLexer"]
    Lexer --> Tokens["HTNToken sequence"]
    Tokens --> Parser["HTNCompilerDomainSyntaxParser"]
    Parser --> AST["HTNCompilerAST"]
    AST --> Validator["Validation and linking"]
    Validator --> Builder["HTNCompilerIRBuilder"]
    Builder --> IR["HTNCompilerIR"]
    IR --> Reachability["Reachability analysis"]
    Reachability --> Generator["HTNCCodeGenerator"]
    Generator --> C["Generated C"]
    C --> Native["Native planner definition"]
```

Each stage has one responsibility:

1. `HTNCompilerDomainLexer` converts characters into tokens and records source
   ranges. It does not assign domain meaning to the token sequence.
2. `HTNCompilerDomainSyntaxParser` converts tokens into compiler-owned values,
   conditions, tasks and declarations.
3. `HTNCompilerDomainValidator` and `HTNCompilerDomainLoader` validate declarations,
   traverse includes and select effective and qualified declarations in include order.
4. `HTNCompilerIRBuilder` resolves compile-time references and lowers the linked AST
   into the representation needed by code generation.
5. `HTNCompilerReachability` marks executable declarations from the resolved IR;
   `HTNCCodeGenerator` emits their C and preserves static metadata.
6. The platform C compiler produces the native planner definition consumed by the
   runtime.

The lexer, AST and IR belong to the compiler frontend. They are build-time data and
are not retained by a generated planner while it executes.

`HTNCompilerIRBuilder.cpp` traverses only `HTNCompilerAST`. It lowers the
compiler syntax, resolves compile-time references and validates the IR.
`HTNCompilerDomainLoader` owns the compiler result API for disk and in-memory
sources. `HTNTranslateDomain` consumes that loader, so its path never handles
frontend nodes or declaration pointer maps. Compiler loads use the compiler
parser, include traversal, semantic validator and linker without building
frontend nodes. The compiler parser and validator report their own diagnostics,
including multiple independent declaration errors. The older parser is no
longer called by the generated loader, which lives in
`Translator/HTNCompilerDomainLoader.cpp`.

The generated frontend owns `HTNCompilerDomainLexer` and
`HTNCompilerDomainLexerContext`. Generic token and lexer primitives remain under
`Parser`, where they are also used by the world-state frontend. The SDK build contains
the compiler frontend and omits the legacy domain parser, loader, nodes and semantic
tooling. Building its translator verifies that compiler translation has no dependency
on those removed implementations.

`HTNCCodeGenerator.cpp` emits C from the completed IR. Its public API and options
do not expose frontend nodes. The IR owns
its tables and strings, so it does not retain AST nodes or source-text views.
`HTNCompilerOptions.h` holds options shared by the builder and emitter.

The IR is an internal compiler representation, not a serialized or public SDK
contract. Source locations now come directly from AST ranges. The loader tracks
which input file owns each linked declaration, including qualified declarations
from includes, and the IR carries that file index with line and column ranges.
Generated debug metadata preserves those ranges for debugger nodes. This changes
the layout of debug metadata, so the debug planner ABI versions advance to
`0x48550002` (debug) and `0x48570002` (debug with profiling). Plain and
profiling builds without debug retain their ABI versions.

## Shared qualified implementations

The compiler linker records the original immutable declaration behind a qualified
method or axiom alias. For example, `Wanderer::behave` and the effective `behave`
may identify the same declaration; a base implementation and a derived override
identify different declarations. Equality of names, source text or function
length is not sufficient to share an implementation.

The IR builder lowers a linked declaration once, reusing its parameters, variable
slots, branches, tasks and conditions for its callable aliases. Method records
retain separate names, source ranges, top-level/dispatch visibility and an internal
`ImplementationIndex`. Axiom aliases retain separate identities while sharing
their lowered condition. The builder verifies that the linked alias still shares
the original parameter/body nodes before reusing it. AST pointers are used only
while building the IR; the completed IR owns its data and retains no AST references.

The C emitter writes one method body per implementation. With full instrumentation,
small generated wrappers pass the called method's metadata index to the shared
body, including when a suspended frame resumes. This preserves qualified names,
parameter watches and source locations without duplicating the branch/task code.
With instrumentation omitted, callers address the body directly and those wrappers
are absent. Wrappers add no planner frame, mutable global state or runtime component;
method/task recursion still uses the generated iterative dispatcher.

Implementation sharing alone does not prune unreachable declarations or change name/arity,
override or deferred-call resolution. It preserves callterm registration validation
and source diagnostics. The generated C/runtime ABI, descriptor layout and external
function signatures are unchanged. Rebuild compiler/tooling C++ clients and regenerate
and recompile domains to obtain smaller output; old generated domains remain usable
with their matching runtime ABI.

The engine Wanderer measurement and regression coverage are recorded in
[generated source size accounting](GENERATED_INSTRUMENTATION.md#shared-implementation-reduction).

## Reachability before emission

`HTNAnalyzeCompilerReachability` traverses resolved IR with iterative worklists.
Every `top_level_method` is a root, including public overloads and entries with no
domain callers. All other existing external dispatch entries are also roots:
deferred targets remain callable even when their producer is unused. Qualified
aliases keep their existing visibility; the linker does not make them public
automatically. Name and arity select the exact target for compound/deferred calls.

The traversal marks shared implementations, all their branches/tasks, condition
children and referenced axiom bodies. Visited masks terminate self/mutual cycles.
A reachable alias keeps its canonical body even if the canonical qualified name
itself is unreachable. Instrumented bodies with one surviving identity use that
identity directly; multiple surviving identities retain their thin wrappers.

The emitter omits unreachable method bodies, wrappers, tasks, continuation helpers
and their fact/axiom helpers. Arithmetic and comparison helpers are emitted only
when reachable expressions need them. This is conservative declaration reachability:
it does not predict world-state values or discard branches based on runtime facts.

IR indices, debugger/source tables, prepared data, fact names and the complete
callterm requirement list remain intact. Initialization validation therefore still
reports missing callterms in unused declarations. Exported entry points and
descriptor lifecycle/accessor callbacks are retained. Further prepared-data and
snapshot reduction is a separate phase.

There is no public C API/ABI change or additional runtime component. Regenerate
and recompile domains to benefit; existing generated modules remain compatible.
New opaque execution-storage sizes are obtained through the existing descriptor.

See [reachability measurements and tests](GENERATED_INSTRUMENTATION.md#reachability-reduction).

The same source ranges are emitted as optional generated debug metadata. When
`HTN_DEBUG_DECOMPOSITION` is enabled, generated execution events use that metadata to
recover domain locations and display names. See [generated debugger](GENERATED_DEBUGGER.md).

Validation covers Windows Profile/x64, the SDK variant matrix, compiler architecture
tests, generated domain execution and dynamic module loading.
