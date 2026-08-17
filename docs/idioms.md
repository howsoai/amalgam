# Amalgam Idioms
A short, practical guide to writing idiomatic Amalgam. These patterns follow directly from the language's functional, value-oriented design. For the full behavior of any opcode mentioned here, see the [Amalgam Opcodes Reference](./opcodes.md).

## Prefer value-oriented transformations
Amalgam is primarily a functional language. Where a job can be described as a transformation over values, prefer a sequence of transforms over a manual counter/append loop. The container transform opcodes push a target scope so the body can read `(current_value)`, `(current_index)`, and `(previous_result)`:
```amalgam
(map (lambda (* (current_value) 2)) [1 2 3 4])             ; [2 4 6 8]
(filter (lambda (> (current_value) 2)) [1 2 3 4])          ; [3 4]
(reduce (lambda (+ (previous_result) (current_value))) [1 2 3 4]) ; 10
(range 1 5)                                                ; [1 2 3 4 5]
```
A transform body that is just a constant does not need a `lambda` wrapper; `(map 1 items)` produces a list of `1` the same length as `items`.

`zip` builds an assoc from parallel lists and merges colliding keys with its function, which expresses tasks like counting without an accumulator loop:
```amalgam
(zip (lambda (+ (current_value 1) (current_value))) ["a" "b" "a"] [1 1 1]) ; {a 2 b 1}
```

## Container updates return new values
Amalgam containers are values. `modify` (create/update/deep-copy) and `remove` do not mutate in place; each returns a **new** container that you must rebind if you want to maintain it beyond its current use:
```amalgam
(assign "seen" (modify seen key value))   ; insert/update an entry
(assign "items" (modify items 0 value))   ; replace a list slot by index
(assign "seen" (remove seen key))         ; drop an entry
```
`modify` also accepts a walk path for nested updates, and `assign` accepts a walk path directly:
```amalgam
(assign "grid" [row col] value)           ; update grid at [row][col]
```
A bare `(modify data)` with no replacements is the idiomatic deep copy of `data` and its referenced structures, preserving internal aliases and cycles.

## Assoc order is not insertion order
An assoc has no insertion order: `indices` and `values` only guarantee that, for a given assoc, they return their elements aligned with each other — the key at one position lines up with the value at the same position. That order is **not** insertion order and should not be relied on for human-meaningful output. When output order matters, make it explicit — sort the keys, or build an ordered list of `[key value]` rows and sort with a comparator:
```amalgam
(sort (indices counts))                   ; keys in a defined order
```
A comparator lambda compares `(current_value)` (left) against `(current_value 1)` (right), returning negative, zero, or positive:
```amalgam
(sort (lambda (- (current_value) (current_value 1))) [4 9 3 5 1]) ; [1 3 4 5 9]
```

## Accessing characters in a string
Indexing a string with `get` does not return a character. Use `substr` for a single character, taking the half-open range `[i, i+1)`:
```amalgam
(substr "hello" 1 2)                      ; "e"
```
When traversing a string by character repeatedly, `explode` it once into a list of single-character strings rather than calling `substr` at each index:
```amalgam
(explode "hello")                         ; ["h" "e" "l" "l" "o"]
```

## Sibling bindings are not visible to each other
Within a single `let` or `declare` binding block, or any other operation that uses an `assoc`.  The key-value pairs are evaluated and pushed onto the scope stack as a set: one value's expression cannot see a sibling key being defined in the same block, and there is no guaranteed order of evaluation among siblings. Use `declare` to extend the current scope when a binding depends on an earlier one; a sequence of `declare`s is preferable to nested `let`s:
```amalgam
; Wrong: `need` cannot see the sibling `base`
(let {base (get nums i) need (- target base)}
	; ...
)

; Right: `declare` extends the scope so `base` is visible before `need` is computed
(let {base (get nums i)}
	(declare {need (- target base)})
	; ...
)
```
The same rule applies to grouped `assign` and `accum`: group only independent updates, and split dependent ones into ordered steps.

## Concise assoc literals and calls
`{ ... }` is identical to `(assoc ...)`, and quotes around bareword keys are optional when the key has no whitespace or reserved characters. Prefer the brace form for ordinary literals and for passing named parameters:
```amalgam
{a 1 b 2}                                 ; same as (assoc "a" 1 "b" 2)
(call helper {x 5 y 6})                   ; named parameters to a call
```
Reserve the spelled-out `(assoc key value ...)` form for cases where the explicit layout reads more clearly, such as a large multi-line parameter object.

## Sequencing, `return`, and `.null`
`seq`, `let`, `declare`, and `while` bodies already run their steps in order and evaluate to the last step. Do not wrap such a body in `(seq ...)` just to get sequencing:
```amalgam
(let {x 0}
	(assign "x" (+ x 1))
	(+ x 10)
)
```
Use `seq` only when single expression position must perform several steps — most commonly an `if` branch.

To stop early, choose the right opcode:
 - `return` propagates up through enclosing forms until it reaches a `call` (or `call_entity`, etc.), then evaluates to its value — use it for an early exit out of a whole method or call.
 - `conclude` exits only the nearest consuming `seq`/`let`/`declare`/`while`; an inner scope can swallow it, so it is for breaking out of one local form.

When running untrusted code — such as genetic programming or otherwise generated code, or untrusted user code — use `call`, `call_entity`, or `call_on_entity` with its constraint parameters, to limit what that code can do.

An `if` without an else branch evaluates to `.null` when the condition is false, so omit a redundant explicit `.null` else in side-effect-only branches. Keep an explicit `.null` only when it is a meaningful part of a return contract — and when `.null` is itself a valid value, use a wrapper such as `[found value]` or a distinct sentinel rather than overloading `.null`. Note that `.null` is used as a non-value to indicate that defaults or fall-throughs should be used elsewhere in the language (e.g., in weighted immediate values in the `mutate` opcode`), and can be a key in an `assoc` in the same way that any code can be a key.

## Use the real arithmetic operators
Integer modulo is `(mod a b)`; there is no `%` operator. Division with `/` returns a real number, so wrap it in `floor` or `ceil` when an integer bucket is intended:
```amalgam
(mod a b)
(floor (/ n d))
```
