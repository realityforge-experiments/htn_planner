# Variable declaration and assignment

Use an explicit assignment condition to declare and initialize a variable:

```lisp
(= ?entity_position (call get_entity_position ?entity_id))
(= ?threat_type main_threat)
(= ?end_time (+ ?start_time ?duration))
(= ?saved_position ?entity_position)
```

The destination must be a fresh variable or an unused axiom output parameter.
An input parameter, a previously declared
variable, or a variable mentioned earlier in the same condition path cannot be
an assignment destination. This is a domain validation error in the compiler frontend,
not a runtime type check. Self-reference in the initializer is also rejected.
Independent method branches and independent `or`/`alt` alternatives may declare
the same local name; a subsequent assignment cannot redeclare a name used by
any preceding alternative. Names used inside `not` also count as prior uses.

The generated frontend also accepts [runtime list initializers](RUNTIME_LISTS.md),
such as `(= ?target (target ?entity_id (call get_entity_position ?entity_id)))`.
List elements must resolve successfully before the destination is bound.

## Axiom parameters

An axiom may initialize its own `?out_` parameter using `=`. The parameter declares
the output slot; it does not count as an earlier use of that slot:

```lisp
(:axiom (select_position ?inp_entity ?out_position)
    (and
        (= ?out_position (call get_entity_position ?inp_entity))
    )
)
// In a method condition, with ?entity already bound:
(#select_position ?entity ?selected_position)
// ?selected_position now contains the returned position.
```

The initializer can also be a literal or arithmetic expression. The output can
be consumed later in the same axiom condition. A second assignment, a prior use,
or self-reference is a validation error with file, line and column.

- `?out_` requires an unbound caller variable. A bound variable, literal, or
  arithmetic argument fails the axiom call before entering its body.
- `?inp_` cannot be an assignment destination.
- An unused `?io_` can be an assignment destination, but the assignment fails if
  its slot is already bound. Its initializer, including nested callterms, is not
  invoked in that case. Use ordinary matching conditions for bound IO values.
- Independent `alt`/`or` branches can initialize the same output; this is not a
  second assignment on the same path.

Successful outputs are copied to the caller. Failed alternatives and retries
restore the previous bindings, including outputs already exported to the caller.
All output parameters must be produced for the axiom to succeed.

This tightens the previous output contract, which also accepted bound `?out_`
arguments and unified them. Migrate those parameters to `?io_` when matching is
intentional, or pass a fresh output variable and compare it after the call.
Regenerate domain C to receive the new entry checks. The runtime C ABI, generated
descriptor layout and `HTNAtom` format are unchanged.

The initializer may be a literal, constant, variable, arithmetic expression or
callterm expression, including nested calls. Runtime types are checked during
evaluation. The expression is evaluated once per attempt; failure or an unbound
result fails the condition without publishing a destination binding. A bound
boolean `false` is a valid assigned value, not a failed assignment.

Bindings participate in normal backtracking and are restored when an alternative
is abandoned. A retry may evaluate the initializer again. External effects of
callterms are not rolled back by variable restoration.

`==` remains a pure comparison and never declares or binds a variable.

## Migration

The previous implicit call-result form is rejected:

```lisp
// Old (invalid)
(?position (call get_entity_position ?entity_id))
// New
(= ?position (call get_entity_position ?entity_id))
```

Migrate source domains and regenerate their C code. Rebuild the matching runtime,
debugger and generated domains so the added assignment debug metadata kind is
understood. This change introduces no C function signatures or `HTNAtom` layout
changes. The compiler lowers assignments through its AST and IR to generated C.
