# Negative numeric literals (2.3.0)

Domains now accept signed decimal literals directly, including `-1`, `-1.0`,
`-0.5`, `-123.456` and the minimum signed 32-bit integer `-2147483648`.

```lisp
(!remember -1.0 is_moving)
(= ?offset -0.5)
(== (+ -2 3) 1)
(== (- -2 3) -5)
(call set_target (position -1.0 -0.5 -123.456))
```

The compiler frontend and the world-state reader use the same sign handling.
The AST/IR preserve numeric types, signed values and source ranges. Debug labels
retain the numeric spelling, including all decimals and numbers in static lists,
instead of rounding through the general atom formatter. Generated C
initializes negative numeric atoms directly; a negative literal does not add a
runtime arithmetic operation. The emitter uses the C constant `INT32_MIN` for
`-2147483648`, avoiding MSVC's unsigned-literal warning in prepared atoms, list
elements and arithmetic operands. Literals also work in runtime lists, axiom/method
arguments, backtracking and deferred calls.

`(- 5 2)`, `(- 1.0)`, `(-- 1.5)` and `(- ?value 1)` retain their arithmetic meaning.
A sign must touch its literal's first digit. Invalid values such as `-`, `-.`,
`-abc`, `--1` and `- 1` produce located diagnostics in the compiler frontend.
Use `(-- 1)` for decrement; `--1` is not an alternate spelling of a number.
Scientific/hexadecimal notation and leading-dot decimals remain unsupported.

The regression domain is `Domains/Test/negative_literals.domain`. Its root uses
`top_level_domain`, as required by the loader. `base` is an included domain and
cannot declare `top_level_method`; that existing rule is unchanged.

## Compatibility

No runtime C ABI, generated descriptor layout or `HTNAtom` format changes.
Existing separated arithmetic expressions continue to work. Rebuild the translator
to parse the new syntax and regenerate/recompile domains that adopt it. Existing
generated modules do not require regeneration for compatibility.

The compiler AST owns the literal's debug text. Tools that link to the C++ compiler
frontend must rebuild against the updated headers; runtime consumers are unaffected.

Introduced in HTN Planner 2.3.0. See the [release preparation notes](RELEASE_2_3_0.md).
