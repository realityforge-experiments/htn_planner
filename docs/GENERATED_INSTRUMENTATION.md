# Generated C instrumentation and size accounting

`HTNTranslator` can report how much source it emits for planner logic/support,
the debugger and profiling. It can also omit optional instrumentation during
translation:

```powershell
HTNTranslator Domains/Wanderer.domain CreateWandererHTN Generated --code-stats
HTNTranslator Domains/Wanderer.domain CreateWandererHTN Generated --instrumentation=none --code-stats
```

## Modes

- `--instrumentation=full` is the default. It preserves the existing output,
  including optional macros and metadata controlled by the C build configuration.
- `--instrumentation=none` omits debugger events, debugger metadata/string/source
  tables, domain-expression comments, profiling spans, structural counters,
  preparation timers, profiling storage/lifecycle and their optional includes.
  It does so even if the generated C is compiled against an instrumented SDK.

The emitter categorizes fragments as it writes them. It does not strip macros
from completed C text or rewrite strings from the domain. Compiler/runtime
options that affect backtracking and capacity retain their existing meaning.

`none` preserves argument/type checks, ownership, backtracking, callterm
requirements and registration validation, callterm error policies and original
source locations, storage/capacity checks and execution diagnostics. It preserves
callterms written by the domain author, including application-specific debug
callterms. Removing those calls is a separate language/semantics decision.

The generated descriptor still matches the selected SDK ABI. A few conditional
descriptor initializers remain: an instrumented ABI receives null debugger
metadata, and an ABI requiring a profiling accessor receives an accessor returning
null. Those are compatibility fields, not active instrumentation. The existing
profiling host helpers accept the null state. No debugger tree or internal
profiling samples can be obtained from a domain translated with `none`.
Reusing a host debugger after another domain does not clear that previous domain's
events; clients should reset their debugger when switching definitions.

This option controls generated domain code. It does not reconfigure instrumentation
inside separately compiled runtime libraries, client bindings or host code.

## What the statistics mean

`--code-stats` prints bytes and newline counts for:

- **Logic/support:** executable planning logic, required runtime/ABI support,
  prepared values, storage/lifecycle, validation and diagnostics.
- **Debugger:** optional events, metadata, debug-only includes and source annotations.
- **Profiling:** optional spans, counters, timing and profiling support.
- **Total emitted:** the sum of those three categories, exactly matching the file.
- **Omitted debugger/profiling:** fragments deliberately not emitted in `none`.

Newline counts are additive even when fragments share a line. They are counts of
newline characters, not counts of overlapping "lines containing a macro". This
is source accounting before C preprocessing, not instruction counts or runtime
time attribution. Includes count as directives; their expanded header contents
are not part of the emitted-file total.

The null ABI replacements differ slightly from full-mode descriptor references,
so omitted bytes are not a promise that `full - omitted` equals `none` exactly.
The statistics do not infer that all remaining logic is minimal: specialized
conditions, continuations and snapshots can still expand a short domain into
substantial C. Further reductions in those areas require separate correctness
and performance analysis.

### Tooling API

Set `HTNTranslationRequest::Instrumentation` to
`HTNGeneratedInstrumentation::Full` or `None`. The shared translation API returns
`HTNTranslationResult::CodeStatistics`. Direct generator callers can set
`HTNCCodeGeneratorOptions::Instrumentation` and pass the optional fourth
`HTNGeneratedCodeStatistics*` argument to `Generate`.

An invalid mode is rejected before loading or writing generated output. The
default remains `Full`; existing source calls to `Generate` remain valid.
Tools using these C++ APIs should be rebuilt. No C runtime ABI revision, descriptor
layout, enum value or atom representation changes. The comparison-operator enum
now lives in `HTNGeneratedPlanner.h`, still included by `HTNGeneratedDebug.h`,
because comparisons are executable planner logic.

Regenerate and recompile a domain to select `none`. Existing modules remain
compatible with matching SDK variants. SDK versioning and publication are separate
from this change.

## Measured source sizes (Windows x64, 2026-10-01)

Normal Debug translator, default backtracking/frame capacities, same entry-point
name in both modes. Sizes below are bytes, not compressed archive sizes.

| Domain | Logic/support | Debugger | Profiling | Full total | None total | Reduction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Wanderer | 316607 | 95379 | 191389 | 603375 | 316885 | 47.5% |
| AAACombatNPC | 2024732 | 578699 | 1093071 | 3696502 | 2025018 | 45.2% |
| numeric_expressions | 1049979 | 321025 | 453487 | 1824491 | 1050285 | 42.4% |
| runtime_lists | 978482 | 217273 | 340775 | 1536530 | 978768 | 36.3% |

Evidence: `build/logs/instrumentation-source-measurements.json` and
`build/logs/instrumentation-measure-<domain>-<mode>.log`.

Even this four-line domain includes the fixed storage, lifecycle, dispatch and
continuation support in each generated module:

```lisp
(:domain Minimal top_level_domain
    (:method (run) top_level_method
        (branch () ((!done))))
)
```

It produces 43648 bytes / 875 newline characters in `full`, and 28139 bytes /
627 newline characters in `none` (35.5% fewer bytes). The full output consists
of 27863 logic/support bytes, 6508 debugger bytes and 9277 profiling bytes.
This illustrates why removing instrumentation alone cannot make the C as short
as the domain. Logs: `build/logs/instrumentation-minimal-full.log` and
`build/logs/instrumentation-minimal-none.log`.

### Source size is not binary size

MSVC 19.39, C11, `/O2 /MD`, compiling the same files as C without decomposition
debugging or profiling: **full and none produced equal object sizes and equal
machine-code section sizes in all four examples**. Optional macros were already
removed or optimized away in these plain builds.

| Domain | Plain object bytes (both modes) | Debugger-enabled full object | Debugger-enabled none object |
| --- | ---: | ---: | ---: |
| Wanderer | 80574 | 123713 | 80586 |
| AAACombatNPC | 389932 | 586233 | 389944 |
| numeric_expressions | 241004 | 391789 | 241016 |
| runtime_lists | 304234 | 419831 | 304246 |

With `HTN_DEBUG_DECOMPOSITION` enabled, `none` removes active debugger calls and
metadata. Its machine-code section sizes remain equal to the plain objects;
a small object-size difference remains with the different ABI configuration.
These are `.obj` measurements, not final linked SDK/executable
sizes. Preprocessed C also shrinks, but includes header declarations, whitespace
and no-op statements; it is not a measure of executed instructions.

Evidence: `build/logs/instrumentation-compiled-measurements.json`,
`build/logs/instrumentation-compiled-measurements.log` and per-file compile logs
under `build/logs/instrumentation-measure/`. Recorded single-run compilation times
were collected alongside other builds and are not a controlled performance
benchmark. No planner-speedup claim is made from these size measurements.

## Shared implementation reduction

Phase 2 of the source-size reduction plan is implemented on
`generated_code_reduction`: qualified/effective aliases of the same linked
declaration share a lowered body. Overrides and overloads remain distinct.
See [the IR and emission design](COMPILER_IR.md#shared-qualified-implementations).

Comparison uses the engine integration's `Wanderer.domain` (179 physical lines)
and `Includes/movement2.domain` (119 lines), not the repository demo Wanderer.
The 298 input lines include comments and whitespace. Source hashes match the
2026-10-01 baseline at `77c09b8`; both files link into one module with entry
`CreateEngineHTN`, runtime backtracking enabled, fixed-with-overflow capacity 32
and call-frame capacity 8192. Only implementation sharing changed between runs.

| Mode | Before lines | After lines | Before bytes | After bytes |
| --- | ---: | ---: | ---: | ---: |
| `full` | 22,055 | 11,654 | 1,528,444 | 793,481 |
| `none` | 13,963 | 7,299 | 834,320 | 434,338 |

This saves 47.16% of full-mode lines and 47.73% of none-mode lines; byte savings
are 48.09% and 47.94%, respectively. The IR keeps all 20 callable method
identities while the module emits 10 implementation bodies. Full mode preserves debugger
identity through small wrappers; none mode calls the shared bodies directly.
Unused declarations, including the async movement path, are still generated.
Reachability pruning was implemented subsequently; its additional savings are
recorded below. Further snapshot/cleanup reduction remains separate work.

Callterm registration requirements match the baseline, including source locations.
Both resulting C files compile with MSVC C11 `/O2 /MD /DHTN_DEBUG_DECOMPOSITION`.
This is object compilation, not execution inside the engine or native Linux
validation, and these source savings do not establish a planner speedup.

Local evidence: `build/logs/shared-implementations-engine/measurements.json`,
`measurement.log`, `full-translation.log`, `none-translation.log` and `compile.log`.
Generated files are under the `full/` and `none/` subdirectories. The unchanged
baseline is in `build/logs/engine-wanderer-size-2026-10-01/`; those local artifacts
are not versioned, so the measurements are retained here.

`HTNSharedImplementationCompilerTest` verifies shared IR/body emission and distinct
overrides/overloads. `HTNSharedImplementationTest` executes full and none versions
of `Domains/Test/shared_implementations.domain` with a base include, checking
qualified calls, axiom output propagation, deferred expansion and visibility,
1000-level non-tail recursion, backtracking/rollback, owned inputs, repeated use,
exact missing-callterm source reports and debugger names/parameters/source ranges.

### Shared implementation validation

| MSVC x64 configuration | Passing tests |
| --- | ---: |
| DebugInstrumented | 403 |
| ReleaseInstrumented | 403 |
| DebugPlain | 394 |
| ReleasePlain | 394 |

Logs are `build/logs/shared-implementations-<configuration>-build.log` and
`build/logs/shared-implementations-<configuration>-tests.log`; the summary is
`build/logs/shared-implementations-matrix-results.json`. As in the baseline, the
slow interpreter parity case `InterpreterAndGeneratedProduceSameResult/Recursive100Entities`
was excluded. Existing generated 100/1000-entity stress cases remained enabled.

ProfileDetailed additionally passed all 59 focused shared-implementation,
instrumentation and runtime-list checks with `--atom-diagnostics`,
`--generated-execution-profiling` and `--runtime-backtracking-support=enabled`.
Final atom diagnostics report zero live heap strings, heap-string bytes and list
nodes. Evidence: `build/logs/shared-implementations-ProfileDetailed-tests.log` and
matching `-build.log`. Standard Visual Studio project settings were restored
after validation (`build/logs/shared-implementations-restore-projects.log`).

Representative additional source comparisons, using `CreateMeasure` and runtime
backtracking support disabled in both old/new translators:

| Domain | Full lines before / after | None lines before / after |
| --- | ---: | ---: |
| Four-line Minimal | 875 / 743 | 627 / 538 |
| recursion_dispatch | 6,157 / 3,532 | 3,776 / 2,151 |
| nested_axiom_choices | 45,378 / 23,665 | 28,935 / 14,836 |

Logs and exact byte counts: `build/logs/shared-implementation-samples/measurements.json`
and the per-domain/mode/version `translation.log` files in that directory. Minimal
still carries general execution support; further reductions belong to the pending
support/snapshot work after reachability pruning, not this implementation-sharing phase.

## Reachability reduction

Implemented on `generated_code_reduction` after sharing qualified implementations.
The analysis traverses resolved IR, with every `top_level_method` and every existing
external deferred target as a root. Public entries with no callers, all public
overloads, referenced qualified/base implementations and recursive cycles survive.
Even a deferred target mentioned only in an unused method retains its existing
external dispatch behavior. A cycle with no path from any root is omitted.

Only executable emission is filtered: unused method bodies/wrappers, task functions,
branch continuations and exclusive fact/axiom helpers disappear. Arithmetic and
comparison helpers remain only when reachable expressions need them. When one
callable identity survives for a shared body, the debugger uses that identity
directly, without an alias wrapper. Multiple used identities keep separate wrappers.

IR indices, debug/source tables, fact names, prepared data and callterm requirements
are preserved. In particular, initialization validation still reports missing
callterms in unused declarations. No public C API/ABI, descriptor layout or new
runtime component is introduced. Regenerate/recompile domains for the reduction;
previously generated modules remain usable with their matching runtime ABI.

### Engine Wanderer measurement (2026-10-01)

Same unchanged engine inputs/settings as above, with runtime backtracking enabled:

| Mode | Lines after sharing | Lines after pruning | Bytes after sharing | Bytes after pruning |
| --- | ---: | ---: | ---: | ---: |
| Full | 11,654 | 9,859 | 793,481 | 663,825 |
| None | 7,299 | 6,239 | 434,338 | 369,487 |

This step removes 1,795 full lines (15.40%) and 1,060 none lines (14.52%). Seven
method bodies remain; the unused `move_to_async`, `construct_path_segments_async`
and `do_wait_for_pathfinding_query` bodies are omitted. Public and deferred entries
and their dependencies remain. Input hashes, exact external lookup, full debugger
metadata and source-aware callterm requirements were compared with the preceding
outputs and are unchanged. Both C files compile with MSVC C11 `/O2 /MD
/DHTN_DEBUG_DECOMPOSITION`. The engine itself was not run for this measurement.

Evidence: `build/logs/reachability-engine/measurements.json`, `measurement.log`,
`full-translation.log`, `none-translation.log` and `compile.log`. Full/none C and
object files are in their corresponding subdirectories. These local outputs are
ignored; the numbers above retain the results in version control.

Additional comparisons, `CreateMeasure`, runtime backtracking disabled:

| Domain | Full lines before / after | None lines before / after |
| --- | ---: | ---: |
| Four-line Minimal | 743 / 730 | 538 / 538 |
| recursion_dispatch | 3,532 / 3,441 | 2,151 / 2,151 |
| nested_axiom_choices | 23,665 / 23,288 | 14,836 / 14,836 |

All implementations in those smaller fixtures are reachable. Their full reduction
comes from removing unused alias wrappers. None mode already omitted those wrappers,
so line counts stay unchanged. Exact bytes and per-mode translation logs are under
`build/logs/reachability-samples/`; summary `measurements.json`.

### Reachability regression coverage

`HTNReachabilityTest` checks roots, exact name/arity edges, reachable base/overrides,
unused include declarations, unused overloads/cycles, a shared body whose canonical
identity is unused, helper omission, no public roots, and preserved registration
requirements. `HTNSharedImplementationTest` executes both full and none generated C,
including client-only public overloads, an empty public entry, deferred-only targets,
mutual/non-tail recursion, single-alias debugger identity, backtracking, owned inputs,
source diagnostics and initialization validation without executing unused callterms.

| MSVC x64 configuration | Passing tests |
| --- | ---: |
| DebugInstrumented | 415 |
| ReleaseInstrumented | 415 |
| DebugPlain | 406 |
| ReleasePlain | 406 |

Logs: `build/logs/reachability-<configuration>-build.log` and matching `-tests.log`
and `-tests.xml`; summary `build/logs/reachability-matrix-results.json`. The existing
slow interpreter parity case `InterpreterAndGeneratedProduceSameResult/Recursive100Entities`
was excluded, as in prior measurements. Generated 100/1000-entity regressions remain
included. All 26 focused reachability/sharing checks also pass separately in
`build/logs/reachability-focused-tests.log`.

ProfileDetailed passes all 71 focused reachability/sharing, instrumentation and
runtime-list checks with atom diagnostics, execution profiling and configurable
runtime backtracking enabled. Final diagnostics show zero live heap strings,
heap-string bytes and list nodes. Logs: `build/logs/reachability-ProfileDetailed-tests.log`
and matching `-build.log`. The first diagnostics run caught an output-ownership
mistake in the new deferred test, fixed by releasing the previous plan before
reusing the output; its initial log is retained as
`build/logs/reachability-ProfileDetailed-before-test-cleanup.log`.

The generated-only C11 consumer built with Windows clang-cl passes all four CTest
cases in Debug and Release, including full and none execution. Logs:
`build/logs/reachability-clang-Debug-tests.log` and `reachability-clang-Release-tests.log`,
with corresponding build logs in the same directory. Native Linux was not tested.
Default Visual Studio project settings were restored afterward:
`build/logs/reachability-restore-projects.log`.

Prepared constants, metadata compaction, repeated snapshots/cleanup and remaining
general support are the next optimization phase. Source reductions do not establish
an executable-size or runtime-performance improvement.

## Validation (2026-10-01)

| Configuration | Passing tests | Log |
| --- | ---: | --- |
| DebugInstrumented | 389 | `build/logs/instrumentation-DebugInstrumented-tests.log` |
| ReleaseInstrumented | 389 | `build/logs/instrumentation-ReleaseInstrumented-tests.log` |
| DebugPlain | 380 | `build/logs/instrumentation-DebugPlain-tests.log` |
| ReleasePlain | 380 | `build/logs/instrumentation-ReleasePlain-tests.log` |

These runs include 45 instrumentation/runtime-list checks: exact source accounting,
both storage policies and backtracking-generation modes, literals containing macro
names, unchanged default output, rejected options, owned runtime lists, deferred
execution, missing/invalid/non-boolean callterm errors, fact and axiom backtracking,
recursion at depth 1000, capacity failure and reuse. They also include the existing
generated 100/1000-entity recursion regressions. The existing slow interpreter
parity case `InterpreterAndGeneratedProduceSameResult/Recursive100Entities` was
excluded; it is not reported as passed. Summary:
`build/logs/instrumentation-matrix-results.json`.

With `--atom-diagnostics --generated-execution-profiling`, ProfileDetailed passes
all 45 focused checks. Full domains provide a profiling state; none domains retain
the required accessor and return null. The final atom diagnostics report zero live
heap strings, zero live heap-string bytes and zero live list nodes. Evidence:
`build/logs/instrumentation-ProfileDetailed-tests.log` and matching `-build.log`.

The C11 consumer passes all four CTest cases in Debug and Release using Windows
clang-cl, including the same execution assertions against both generated modes:
`build/logs/instrumentation-clang-Debug-tests.log` and
`build/logs/instrumentation-clang-Release-tests.log`. Native Linux remains unverified
on this host. C generated with `none` by the normal translator also passes an MSVC
syntax check with debugger, detailed profiling and execution profiling all enabled,
including fixed-capacity generation with runtime backtracking enabled:
`build/logs/instrumentation-none-cross-configuration.log`.

Default full C output matches the previous translator byte for byte for
runtime_lists, recursion_dispatch and AAACombatNPC:
`build/logs/instrumentation-default-compatibility.json`. Invalid CLI mode rejection
is recorded in `build/logs/instrumentation-cli-invalid-mode.log`. RuntimeBridge and
SDK source checks pass in `build/logs/instrumentation-runtime-bridge-audit.log` and
`build/logs/instrumentation-sdk-source-boundary.log`.

Build/test/measurement artifacts remain under `build/logs/`. No distributed SDK
archive was replaced or published during this work.
