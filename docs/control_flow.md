### Opcode: `if`
#### Parameters
`[any condition_or_node1] [any node1] [any condition_or_node2] [any node2] ...`
#### Returns
`any`
#### Description
If `condition_or_node1` is true, then it will evaluate to the node1 argument.  Otherwise `condition_or_node2` will be checked, repeating for every pair.  If there is an odd number of parameters, the last `condition_or_node` acts as the final else, and will be evaluated as that if all conditions are false.  If there is an even number of parameters and none are true, then evaluates to null.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(if 1 "if 1")
```
Output:
```amalgam
"if 1"
```
Example:
```amalgam
(if 0 "not this one" "if 2")
```
Output:
```amalgam
"if 2"
```
Example:
```amalgam
(if
	.null 1
	0 2
	0 3
	4
 )
```
Output:
```amalgam
4
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `seq`
#### Parameters
`[any node1] [any node2] ...`
#### Returns
`any`
#### Description
Runs each code block sequentially.  Evaluates to the result of the last code block run, unless it encounters a conclude or return in an earlier step, in which case it will halt processing and evaluate to the value returned by conclude or propagate the return.  Note that the last step will not consume a concluded value (see conclude opcode).
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(seq 1 2 3)
```
Output:
```amalgam
3
```
Example:
```amalgam
(seq
	(declare {a 1})
	(accum "a" 1)
	a
)
```
Output:
```amalgam
2
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `lambda`
#### Parameters
`any function [bool evaluate_and_wrap]`
#### Returns
`any`
#### Description
Evaluates to the code specified without evaluating it.  Useful for referencing functions or handling data without evaluating it.  The parameter `evaluate_and_wrap` defaults to false, but if it is true, it will evaluate the function, but then return the result wrapped in a lambda opcode.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(lambda (+ 1 2))
```
Output:
```amalgam
(+ 1 2)
```
Example:
```amalgam
(seq
	(declare {foo (lambda (+ y 1))})
	(call foo {y 1})
)
```
Output:
```amalgam
2
```
Example:
```amalgam
(lambda (+ 1 2) .true )
```
Output:
```amalgam
(lambda 3)
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `call`
#### Parameters
`any function [assoc params] [bool|assoc constraints] [bool return_warnings]`
#### Returns
`any`
#### Description
Evaluates `function` after pushing the `params` assoc onto the scope stack.  If `constraints` is specified and not false or null, it will constrain execution.  If `constraints` is true or an assoc, it will default all constraints to be on at reasonable values for small execution without access to any data beyond `params`.  They optional key-value combinations for `constraints` are as follows.  If "max_node_operations" is specified, it represents the number of operations that are allowed to be performed. If "max_node_operations" is 0, then an infinite of operations will be allotted, up to the limits of the current calling context.  If "max_node_allocations" is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If "max_node_allocations" is 0 and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if "max_node_allocations" is specified while in a multithreaded environment, if the collective memory from all the executing threads exceeds the average memory specified by "max_node_allocations", that may trigger a memory limit for the call.  If "max_operation_depth" is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise "max_operation_depth" limits how deep nested opcodes will be called. If `return_warnings` is true (default is false), the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or .null if none)).  The keys "read_access" and "write_access" are boolean and control whether the execution can read from or write to entities and access their relevant permissions (e.g., to load files, make system calls).  If the parameter `return_warnings` is false, just the value will be returned.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: true
 - Creates new target scope: false
 - Value newness (whether references existing node): conditional
#### Examples
Example:
```amalgam
(let
	{
		foo (lambda
				(declare
					{x 6}
					(+ x 2)
				)
			)
	}
	(call
		foo
		{x 3}
	)
)
```
Output:
```amalgam
5
```
Example:
```amalgam
(call
	(lambda
		(+
			(+ y 4)
			4
		)
	)
	{y 3}
	.true
)
```
Output:
```amalgam
11
```
Example:
```amalgam
(call
	(lambda
		(+
			(+ y 4)
			4
		)
	)
	{y 3}
	.true
	.true
)
```
Output:
```amalgam
[11 {} .null]
```
Example:
```amalgam
(call
	(lambda
		(call
			(lambda
				(+
					(+ y 4)
					4
				)
			)
			{y 3}
			{max_operation_depth 2}
			.true
		)
	)
	{y 3}
	{max_operation_depth 50}
	.true
)
```
Output:
```amalgam
[
	[.null {} "Execution depth exceeded"]
	{}
	.null
]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `while`
#### Parameters
`bool condition [any node1] [any node2] ...`
#### Returns
`any`
#### Description
Each time the `condition` evaluates to true, it runs each of code sequentially, looping. Evaluates to the last `codeN` or null if the `condition` was initially false or if it encounters a `conclude` or `return`, it will halt processing and evaluate to the value returned by `conclude` or propagate the `return`.  For each iteration of the loop, it pushes a new target scope onto the target stack, with `(current_index)` being the iteration count, and `(previous_result)` being the last evaluated `codeN` of the previous loop.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: true
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(seq
	(assign
		{i 1}
	)
	(while
		(< i 10)
		(accum
			{i 1}
		)
	)
	i
)
```
Output:
```amalgam
10
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `conclude`
#### Parameters
`[any node]`
#### Returns
`any`
#### Description
Evaluates to `node` wrapped in a `conclude` opcode.  If a step in a `seq`, `let`, `declare`, or `while` evaluates to a `conclude` (excluding variable declarations for `let` and `declare`, the last step in `set`, `let`, and `declare`, or the condition of `while`), then it will conclude the execution and evaluate to `node`.  Note that conclude opcodes may be nested to break out of outer opcodes.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(seq
	"seq1"
	(conclude "success")
	"seq2"
)
```
Output:
```amalgam
"success"
```
Example:
```amalgam
(while
	(< 1 100)
	"while1"
	(conclude "success")
	"while2"
)
```
Output:
```amalgam
"success"
```
Example:
```amalgam
(let
	{a 1}
	"let1"
	(conclude "success")
	"let2"
)
```
Output:
```amalgam
"success"
```
Example:
```amalgam
(declare
	{abcd 1}
	"declare1"
	(conclude "success")
	"declare2"
)
```
Output:
```amalgam
"success"
```
Example:
```amalgam
(seq
	1
	(declare {}
		(while .true
			(if .true (conclude))
		)
		4
	)
	2
)
```
Output:
```amalgam
2
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `return`
#### Parameters
`[any node]`
#### Returns
`any`
#### Description
Evaluates to `node` wrapped in a `return` opcode.  If a step in a `seq`, `let`, `declare`, or `while` evaluates to a return (excluding variable declarations for `let` and `declare`, the last step in `set`, `let`, and `declare`, or the condition of `while`), then it will conclude the execution and evaluate to the `return` opcode with its `node`.  This means it will continue to conclude each level up the stack until it reaches any kind of call opcode, including `call`, `call_entity`, `call_on_entity`, or `call_container`, at which point it will evaluate to `node`.  Note that return opcodes may be nested to break out of multiple calls.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(call
	(seq
		1
		2
		(seq
			(return 3)
			4
		)
		5
	)
)
```
Output:
```amalgam
3
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `apply`
#### Parameters
`any to_apply list|assoc collection`
#### Returns
`any`
#### Description
Creates a new list of the values of the elements of the `collection`, and changes the type to that specified by `to_apply` and then evaluates it.  The parameter `to_apply` can either be a string representing the type or an opcode of the specified type.  If `to_apply` is an opcode and has parameters, i.e., it is a node with one or more elements, these are prepended to the `collection` as the first parameters.  When no extra parameters are passed, it is a more efficient equivalent to `(call (set_type type collection))`.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): conditional
#### Examples
Example:
```amalgam
(apply
	(lambda (+))
	[1 2 3 4]
)
```
Output:
```amalgam
10
```
Example:
```amalgam
(apply
	(lambda
		(+ 5)
	)
	[1 2 3 4]
)
```
Output:
```amalgam
15
```
Example:
```amalgam
(apply
	"+"
	[1 2 3 4]
)
```
Output:
```amalgam
10
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `opcode_stack`
#### Parameters
`[number stack_distance] [bool no_child_nodes]`
#### Returns
`list`
#### Description
Evaluates to the list of opcodes that make up the call stack or a single opcode within the call stack.  If `stack_distance` is specified, then a copy of the node at that specified depth is returned, otherwise the list of all opcodes in opcode stack are returned. Negative values for `stack_distance` specify the depth from the top of the stack and positive values specify the depth from the bottom.  If `no_child_nodes` is true, then only the root node(s) are returned, otherwise the returned node(s) are deep-copied.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(size (opcode_stack))
```
Output:
```amalgam
2
```
Example:
```amalgam
(seq
	(seq
		(opcode_stack 2)
	)
)
```
Output:
```amalgam
(seq
	(seq
		(opcode_stack 2)
	)
)
```
Example:
```amalgam
(seq
	(seq
		(opcode_stack -1 .true)
	)
)
```
Output:
```amalgam
(seq)
```

[Amalgam Opcodes](./opcodes.md)

