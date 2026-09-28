# Variable declaration and assignment

Use an explicit assignment condition to declare and initialize a variable:

```lisp
(= ?entity_position (call get_entity_position ?entity_id))
(= ?threat_type main_threat)
(= ?end_time (+ ?start_time ?duration))
(= ?saved_position ?entity_position)
```

The destination must be a fresh variable. A parameter, a previously declared
variable, or a variable mentioned earlier in the same condition path cannot be
an assignment destination. This is a domain validation error in the compiler frontend,
not a runtime type check. Self-reference in the initializer is also rejected.
Independent method branches and independent `or`/`alt` alternatives may declare
the same local name; a subsequent assignment cannot redeclare a name used by
any preceding alternative. Names used inside `not` also count as prior uses.

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
