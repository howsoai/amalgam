### Opcode: `range`
#### Parameters
`[* function] number low_endpoint number high_endpoint [number step_size]`
#### Description
Evaluates to a list with the range from `low_endpoint` to `high_endpoint`.  The default `step_size` is 1.  Evaluates to an empty list if the range is not valid.  If four arguments are specified, then `function` will be evaluated for each value in the range.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: true
 - Value newness (whether references existing node): conditional
#### Examples
Example:
```amalgam
(range 0 10)
```
Output:
```amalgam
[
	0
	1
	2
	3
	4
	5
	6
	7
	8
	9
	10
]
```
Example:
```amalgam
(range 10 0)
```
Output:
```amalgam
[
	10
	9
	8
	7
	6
	5
	4
	3
	2
	1
	0
]
```
Example:
```amalgam
(range 0 5 0)
```
Output:
```amalgam
[]
```
Example:
```amalgam
(range 0 5 1)
```
Output:
```amalgam
[0 1 2 3 4 5]
```
Example:
```amalgam
(range 12 0 5 1)
```
Output:
```amalgam
[12 12 12 12 12 12]
```
Example:
```amalgam
(range
	(lambda
		(+ (current_index) 1)
	)
	0
	5
	1
)
```
Output:
```amalgam
[1 2 3 4 5 6]
```
Example:
```amalgam
||(range
	(lambda
		(+ (current_index) 1)
	)
	0
	5
	1
)
```
Output:
```amalgam
[1 2 3 4 5 6]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `rewrite`
#### Parameters
`* function * target`
#### Description
Rewrites `target` by applying the `function` in a bottom-up manner.  For each node in the `target` structure, it pushes a new target scope onto the target stack, with `(current_value)` being the current node and `(current_index)` being to the index to the current node relative to the node passed into rewrite accessed via target, and evaluates `function`.  Returns the resulting structure, after have been rewritten by function.  Note that there is a small performance overhead if `target` is a graph structure rather than a tree structure.
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
(rewrite
	(lambda
		(if
			(~ (current_value) 0)
			(+ (current_value) 1)
			(current_value)
		)
	)
	[
		(associate "a" 13)
	]
)
```
Output:
```amalgam
[
	{a 14}
]
```
Example:
```amalgam
;rewrite all integer additions into multiplies and then fold constants
(rewrite
	(lambda
		
		;find any nodes with a + and where its list is filled to its size with integers
		(if
			(and
				(=
					(get_type (current_value))
					(lambda (+))
				)
				(=
					(size (current_value))
					(size
						(filter
							(lambda
								(~ (current_value) 0)
							)
							(current_value)
						)
					)
				)
			)
			(reduce
				(lambda
					(* (previous_result) (current_value))
				)
				(current_value)
			)
			(current_value)
		)
	)
	
	;original code with additions to be rewritten
	(lambda
		[
			(associate
				"a"
				(+
					3
					(+ 13 4 2)
				)
			)
		]
	)
)
```
Output:
```amalgam
[
	(associate "a" 312)
]
```
Example:
```amalgam
;rewrite numbers as sums of position in the list and the number (all 8s)
(rewrite
	(lambda
		
		;find any nodes with a + and where its list is filled to its size with integers
		(if
			(=
				(get_type_string (current_value))
				"number"
			)
			(+
				(current_value)
				(get_value (current_index))
			)
			(current_value)
		)
	)
	
	;original code with additions to be rewritten
	(lambda
		[
			8
			7
			6
			5
			4
			3
			2
			1
			0
		]
	)
)
```
Output:
```amalgam
[
	8
	8
	8
	8
	8
	8
	8
	8
	8
]
```
Example:
```amalgam
(rewrite
	(lambda
		(if
			(and
				(=
					(get_type (current_value))
					(lambda (+))
				)
				(=
					(size (current_value))
					(size
						(filter
							(lambda
								(~ (current_value) 0)
							)
							(current_value)
						)
					)
				)
			)
			(reduce
				(lambda
					(+ (previous_result) (current_value))
				)
				(current_value)
			)
			(current_value)
		)
	)
	(lambda
		(+
			(+ 13 4)
			a
		)
	)
)
```
Output:
```amalgam
(+ 17 a)
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `map`
#### Parameters
`* function [list|assoc collection1] [list|assoc collection2] ... [list|assoc collectionN]`
#### Description
For each element in the collection, pushes a new target scope onto the stack, so that `(current_value)` accesses the element or elements in the list and `(current_index)` accesses the list or assoc index, with `(target)` representing the outer set of lists or assocs, and evaluates the function.  Returns the list of results, mapping the list via the specified `function`.  If multiple lists or assocs are specified, then it pulls from each list or assoc simultaneously (null if overrun or index does not exist) and `(current_value)` contains an array of the values in parameter order.  Note that concurrency is only available when more than one one collection is specified.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: true
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(map
	(lambda
		(* (current_value) 2)
	)
	[1 2 3 4]
)
```
Output:
```amalgam
[2 4 6 8]
```
Example:
```amalgam
(map
	(lambda
		(+ (current_value) (current_index))
	)
	[
		10
		1
		20
		2
		30
		3
		40
		4
	]
)
```
Output:
```amalgam
[
	10
	2
	22
	5
	34
	8
	46
	11
]
```
Example:
```amalgam
(map
	(lambda
		(+ (current_value) (current_index))
	)
	(associate
		10
		1
		20
		2
		30
		3
		40
		4
	)
)
```
Output:
```amalgam
{
	10 11
	20 22
	30 33
	40 44
}
```
Example:
```amalgam
(map
	(lambda
		(+
			(get (current_value) 0)
			(get (current_value) 1)
		)
	)
	[1 2 3 4 5 6]
	[2 2 2 2 2 2]
)
```
Output:
```amalgam
[3 4 5 6 7 8]
```
Example:
```amalgam
(map
	(lambda
		(+
			(get (current_value) 0)
			(get (current_value) 1)
		)
	)
	[1 2 3 4 5]
	[2 2 2 2 2 2]
)
```
Output:
```amalgam
[3 4 5 6 7 (null)]
```
Example:
```amalgam
(map
	(lambda
		(+
			(get (current_value) 0)
			(get (current_value) 1)
			(get (current_value) 2)
		)
	)
	(associate 0 0 1 1 "a" 3)
	(associate 0 1 "a" 4)
	[2 2 2 2]
)
```
Output:
```amalgam
{
	0 3
	1 (null)
	2 (null)
	3 (null)
	a (null)
}
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `filter`
#### Parameters
`[* function] list|assoc collection [bool match_on_value]`
#### Description
For each element in the `collection`, pushes a new target scope onto the stack, so that `(current_value)` accesses the element in the list and `(current_index)` accesses the list or assoc index, with `(target)` representing the original list or assoc, and evaluates the function.  If `function` evaluates to true, then the element is put in a new list or assoc (matching the input type) that is returned.  If function is omitted, then it will remove any elements in the collection that are null.  The parameter match_on_value defaults to null, which will evaluate the function.  However, if match_on_value is true, it will only retain elements which equal the value in function and if match_on_value is false, it will retain elements which do not equal the value in function.  Using match_on_value and wrapping filter in a size opcode additionally acts as an efficient way to count the number of a specific element in a container.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: true
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(filter
	(lambda
		(> (current_value) 2)
	)
	[1 2 3 4]
)
```
Output:
```amalgam
[3 4]
```
Example:
```amalgam
(filter
	(lambda
		(< (current_index) 3)
	)
	[
		10
		1
		20
		2
		30
		3
		40
		4
	]
)
```
Output:
```amalgam
[10 1 20]
```
Example:
```amalgam
(filter
	(lambda
		(< (current_index) 20)
	)
	(associate
		10
		1
		20
		2
		30
		3
		40
		4
	)
)
```
Output:
```amalgam
{10 1}
```
Example:
```amalgam
(filter
	[
		10
		1
		20
		(null)
		30
		(null)
		(null)
		40
		4
	]
)
```
Output:
```amalgam
[10 1 20 30 40 4]
```
Example:
```amalgam
(filter
	[
		10
		1
		20
		(null)
		30
		""
		40
		4
	]
)
```
Output:
```amalgam
[
	10
	1
	20
	30
	""
	40
	4
]
```
Example:
```amalgam
(filter
	{
		a 10
		b 1
		c 20
		d ""
		e 30
		f 3
		g (null)
		h 4
	}
)
```
Output:
```amalgam
{
	a 10
	b 1
	c 20
	d ""
	e 30
	f 3
	h 4
}
```
Example:
```amalgam
(filter
	{
		a 10
		b 1
		c 20
		d ""
		e 30
		f 3
		g (null)
		h 4
	}
)
```
Output:
```amalgam
{
	a 10
	b 1
	c 20
	d ""
	e 30
	f 3
	h 4
}
```
Example:
```amalgam
(filter (null) [(null) 1 (null) 2 (null) 3] .false)
```
Output:
```amalgam
[1 2 3]
```
Example:
```amalgam
(filter (null) {a (null) b 1 c (null) d 2 e (null) f 3} .true)
```
Output:
```amalgam
{a (null) c (null) e (null)}
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `weave`
#### Parameters
`[* function] list|immediate values1 [list|immediate values2] [list|immediate values3]...`
#### Description
Interleaves the values lists optionally by applying a function.  If only `values1` is passed in, then it evaluates to `values1`. If `values1` and `values2` are passed in, or, if more values are passed in but function is null, it interleaves the lists and extends the result to the length of the longest list, filling in the remainder with null.  If any of the value parameters are immediate, then it will repeat that immediate value when weaving.  If the `function` is specified and not null, it pushes a new target scope onto the stack, so that `(current_value)` accesses a list of elements to be woven together from the list, and `(current_index)` accesses the list or assoc index, with `(target)` representing the resulting list or assoc.  The `function` should evaluate to a list, and weave will evaluate to a concatenated list of all of the lists that the function evaluated to.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: true
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(weave
	[1 2 3]
)
```
Output:
```amalgam
[1 2 3]
```
Example:
```amalgam
(weave
	[1 3 5]
	[2 4 6]
)
```
Output:
```amalgam
[1 2 3 4 5 6]
```
Example:
```amalgam
(weave
	(null)
	[2 4 6]
	(null)
)
```
Output:
```amalgam
[2 (null) 4 (null) 6 (null)]
```
Example:
```amalgam
(weave
	"a"
	[2 4 6]
)
```
Output:
```amalgam
["a" 2 @(target .true 0) 4 @(target .true 0) 6]
```
Example:
```amalgam
(weave
	(null)
	[1 4 7]
	[2 5 8]
	[3 6 9]
)
```
Output:
```amalgam
[
	1
	2
	3
	4
	5
	6
	7
	8
	9
]
```
Example:
```amalgam
(weave
	[1 3 5 7 9 11]
	[2 4 6 8 10 12]
)
```
Output:
```amalgam
[
	1
	2
	3
	4
	5
	6
	7
	8
	9
	10
	11
	12
]
```
Example:
```amalgam
(weave
	(lambda (current_value))
	[1 3 5 7 9 11]
	[2 4 6 8 10 12]
)
```
Output:
```amalgam
[
	1
	2
	3
	4
	5
	6
	7
	8
	9
	10
	11
	12
]
```
Example:
```amalgam
(weave
	(lambda
		(map
			(lambda
				(* 2 (current_value))
			)
			(current_value)
		)
	)
	[1 3 5 7 9 11]
	[2 4 6 8 10 12]
)
```
Output:
```amalgam
[
	2
	4
	6
	8
	10
	12
	14
	16
	18
	20
	22
	24
]
```
Example:
```amalgam
(weave
	(lambda
		[
			(apply
				"min"
				(current_value 1)
			)
		]
	)
	[1 3 4 5 5 6]
	[2 2 3 4 6 7]
)
```
Output:
```amalgam
[1 2 3 4 5 6]
```
Example:
```amalgam
(weave
	(lambda
		(if
			(<=
				(get (current_value) 0)
				4
			)
			[
				(apply
					"min"
					(current_value 1)
				)
			]
			(current_value)
		)
	)
	[1 3 4 5 5 6]
	[2 2 3 4 6 7]
)
```
Output:
```amalgam
[
	1
	2
	3
	5
	4
	5
	6
	6
	7
]
```
Example:
```amalgam
(weave
	(lambda
		(if
			(>=
				(first (current_value))
				3
			)
			[
				(first
					(current_value 1)
				)
			]
			[]
		)
	)
	[1 2 3 4 5]
	(null)
)
```
Output:
```amalgam
[3 4 5]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `reduce`
#### Parameters
`* function list|assoc collection`
#### Description
For each element in the `collection` after the first one, it evaluates `function` with a new scope on the stack where `(current_value)` accesses each of the elements from the `collection`, `(current_index)` accesses the list or assoc index and `(previous_result)` accesses the previously reduced result.  If the `collection` is empty, null is returned.  If the `collection` is of size one, the single element is returned.
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
(reduce
	(lambda
		(* (current_value) (previous_result))
	)
	[1 2 3 4]
)
```
Output:
```amalgam
24
```
Example:
```amalgam
(reduce
	(lambda
		(* (current_value) (previous_result))
	)
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
	)
)
```
Output:
```amalgam
24
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `associate`
#### Parameters
`[* index1] [* value1] [* index2] [* value2] ... [* indexN] [* valueN]`
#### Description
Evaluates to the assoc, where each pair of parameters (e.g., `index1` and `value1`) comprises a index/value pair.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the assoc, the current index, and the current value.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: true
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(unparse
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
)
```
Output:
```amalgam
"{4 \"d\" a 1 b 2 c 3}"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `zip`
#### Parameters
`[* function] list indices [* values]`
#### Description
Evaluates to a new assoc where `indices` are the keys and `values` are the values, with corresponding positions in the list matched.  If the `values` is omitted and only one parameter is specified, then it will use nulls for each of the values.  If `values` is not a list, then all of the values in the assoc returned are set to the same value.  When two parameters are specified, it is the `indices` and `values`.  When three values are specified, it is the `function`, indices, and values.  The parameter `values` defaults to null and `function` defaults to `(lambda (current_value))`.  When there is a collision of indices, `function` is called with a of new target scope pushed onto the stack, so that `(current_value)` accesses a list of elements from the list, `(current_index)` accesses the list or assoc index if it is not already reduced, and `(target)` represents the original list or assoc.  When evaluating `function`, existing indices will be overwritten.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: true
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(unparse
	(zip
		["a" "b" "c" "d"]
		[1 2 3 4]
	)
)
```
Output:
```amalgam
"{a 1 b 2 c 3 d 4}"
```
Example:
```amalgam
(unparse
	(zip
		["a" "b" "c" "d"]
	)
)
```
Output:
```amalgam
"{a (null) b (null) c (null) d (null)}"
```
Example:
```amalgam
(unparse
	(zip
		["a" "b" "c" "d"]
		3
	)
)
```
Output:
```amalgam
"{a 3 b (target .true \"a\") c (target .true \"a\") d (target .true \"a\")}"
```
Example:
```amalgam
(unparse
	(zip
		(lambda (current_value))
		["a" "b" "c" "d" "a"]
		[1 2 3 4 4]
	)
)
```
Output:
```amalgam
"{a 4 b 2 c 3 d 4}"
```
Example:
```amalgam
(unparse
	(zip
		(lambda
			(+
				(current_value 1)
				(current_value)
			)
		)
		["a" "b" "c" "d" "a"]
		[1 2 3 4 4]
	)
)
```
Output:
```amalgam
"{a 5 b 2 c 3 d 4}"
```
Example:
```amalgam
(unparse
	(zip
		(lambda
			(+
				(current_value 1)
				(current_value)
			)
		)
		["a" "b" "c" "d" "a"]
		1
	)
)
```
Output:
```amalgam
"{a 2 b 1 c (target .true \"b\") d (target .true \"b\")}"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `unzip`
#### Parameters
`[list|assoc collection] list indices`
#### Description
Evaluates to a new list, using `indices` to look up each value from the `collection` in the same order as each index is specified in `indices`.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(unzip
	[1 2 3]
	[0 -1 1]
)
```
Output:
```amalgam
[1 3 2]
```
Example:
```amalgam
(unzip
	(associate "a" 1 "b" 2 "c" 3)
	["a" "b"]
)
```
Output:
```amalgam
[1 2]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `reverse`
#### Parameters
`list collection`
#### Description
Returns a new list containing the `collection` with its elements in reversed order.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(reverse
	[1 2 3 4 5]
)
```
Output:
```amalgam
[5 4 3 2 1]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `sort`
#### Parameters
`[* function] list|assoc collection [number k]`
#### Description
Returns a new list containing the elements from `collection` sorted in increasing order, regardless of whether `collection` is an assoc or list.  If `function` is null or true it sorts ascending, if false it sorts descending, and if any other value it pushes a pair of new scope onto the stack with `(current_value)` and `(current_value 1)` accessing a pair of elements from the list, and evaluates `function`.  The function should return a number, positive if `(current_value)` is greater, negative if `(current_value 1)` is greater, or 0 if equal.  If `k` is specified in addition to `function` and not null, then it will only return the `k` smallest values sorted in order, or, if `k` is negative, it will return the highest `k` values using the absolute value of `k`.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: true
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(sort
	[4 9 3 5 1]
)
```
Output:
```amalgam
[1 3 4 5 9]
```
Example:
```amalgam
(sort
	{
		a 4
		b 9
		c 3
		d 5
		e 1
	}
)
```
Output:
```amalgam
[1 3 4 5 9]
```
Example:
```amalgam
(sort
	[
		"n"
		"b"
		"hello"
		"soy"
		4
		1
		3.2
		[1 2 3]
	]
)
```
Output:
```amalgam
[
	1
	3.2
	4
	[1 2 3]
	"b"
	"hello"
	"n"
	"soy"
]
```
Example:
```amalgam
(sort
	[
		1
		"1x"
		"10"
		20
		"z2"
		"z10"
		"z100"
	]
)
```
Output:
```amalgam
[
	1
	20
	"1x"
	"10"
	"z2"
	"z10"
	"z100"
]
```
Example:
```amalgam
(sort
	[
		1
		"001x"
		"010"
		20
		"z002"
		"z010"
		"z100"
	]
)
```
Output:
```amalgam
[
	1
	20
	"001x"
	"010"
	"z002"
	"z010"
	"z100"
]
```
Example:
```amalgam
(sort
	(lambda
		(-
			(current_value)
			(current_value 1)
		)
	)
	[4 9 3 5 1]
)
```
Output:
```amalgam
[1 3 4 5 9]
```
Example:
```amalgam
(sort
	(lambda
		(- (rand) (rand))
	)
	(range 0 10)
)
```
Output:
```amalgam
[
	8
	10
	6
	9
	7
	5
	1
	0
	2
	4
	3
]
```
Example:
```amalgam
(sort
	[
		"2020-06-08 lunes 11.33.36"
		"2020-06-08 lunes 11.32.47"
		"2020-06-08 lunes 11.32.49"
		"2020-06-08 lunes 11.32.37"
		"2020-06-08 lunes 11.33.48"
		"2020-06-08 lunes 11.33.40"
		"2020-06-08 lunes 11.33.45"
		"2020-06-08 lunes 11.33.42"
		"2020-06-08 lunes 11.33.47"
		"2020-06-08 lunes 11.33.43"
		"2020-06-08 lunes 11.33.38"
		"2020-06-08 lunes 11.33.39"
		"2020-06-08 lunes 11.32.36"
		"2020-06-08 lunes 11.32.38"
		"2020-06-08 lunes 11.33.37"
		"2020-06-08 lunes 11.32.58"
		"2020-06-08 lunes 11.33.44"
		"2020-06-08 lunes 11.32.48"
		"2020-06-08 lunes 11.32.46"
		"2020-06-08 lunes 11.32.57"
		"2020-06-08 lunes 11.33.41"
		"2020-06-08 lunes 11.32.39"
		"2020-06-08 lunes 11.32.59"
		"2020-06-08 lunes 11.32.56"
		"2020-06-08 lunes 11.33.46"
	]
)
```
Output:
```amalgam
[
	"2020-06-08 lunes 11.32.36"
	"2020-06-08 lunes 11.32.37"
	"2020-06-08 lunes 11.32.38"
	"2020-06-08 lunes 11.32.39"
	"2020-06-08 lunes 11.32.46"
	"2020-06-08 lunes 11.32.47"
	"2020-06-08 lunes 11.32.48"
	"2020-06-08 lunes 11.32.49"
	"2020-06-08 lunes 11.32.56"
	"2020-06-08 lunes 11.32.57"
	"2020-06-08 lunes 11.32.58"
	"2020-06-08 lunes 11.32.59"
	"2020-06-08 lunes 11.33.36"
	"2020-06-08 lunes 11.33.37"
	"2020-06-08 lunes 11.33.38"
	"2020-06-08 lunes 11.33.39"
	"2020-06-08 lunes 11.33.40"
	"2020-06-08 lunes 11.33.41"
	"2020-06-08 lunes 11.33.42"
	"2020-06-08 lunes 11.33.43"
	"2020-06-08 lunes 11.33.44"
	"2020-06-08 lunes 11.33.45"
	"2020-06-08 lunes 11.33.46"
	"2020-06-08 lunes 11.33.47"
	"2020-06-08 lunes 11.33.48"
]
```
Example:
```amalgam
(sort
	(null)
	[4 9 3 5 1]
	2
)
```
Output:
```amalgam
[1 3]
```
Example:
```amalgam
(sort
	(null)
	[4 9 3 5 1]
	-2
)
```
Output:
```amalgam
[5 9]
```
Example:
```amalgam
(sort
	(lambda
		(-
			(current_value)
			(current_value 1)
		)
	)
	[4 9 3 5 1]
	2
)
```
Output:
```amalgam
[1 3]
```
Example:
```amalgam
(sort
	(lambda
		(-
			(current_value)
			(current_value 1)
		)
	)
	[4 9 3 5 1]
	-2
)
```
Output:
```amalgam
[9 5]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `current_index`
#### Parameters
`[number stack_distance]`
#### Description
Evaluates to the index of the current node being iterated on within the current target.  If `stack_distance` is specified, it climbs back up the target stack that many levels.
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
[0 1 2 3 (current_index) 5]
```
Output:
```amalgam
[0 1 2 3 4 5]
```
Example:
```amalgam
[
	0
	1
	[
		0
		1
		2
		3
		(current_index 1)
		4
	]
]
```
Output:
```amalgam
[
	0
	1
	[0 1 2 3 2 4]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `current_value`
#### Parameters
`[number stack_distance]`
#### Description
Evaluates to the current node being iterated on within the current target.  If `stack_distance` is specified, it climbs back up the target stack that many levels.
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
(map
	(lambda
		(* 2 (current_value))
	)
	(range 0 4)
)
```
Output:
```amalgam
[0 2 4 6 8]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `previous_result`
#### Parameters
`[number stack_distance] [bool copy]`
#### Description
Evaluates to the resulting node of the previous iteration for applicable opcodes. If `stack_distance` is specified, it climbs back up the target stack that many levels.  If `copy` is true, which is false by default, then a copy of the resulting node of the previous iteration is returned, otherwise the result of the previous iteration is returned directly and consumed.
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
(while
	(< (current_index) 3)
	(append (previous_result) (current_index))
)
```
Output:
```amalgam
[(null) 0 1 2]
```
Example:
```amalgam
(while
	(< (current_index) 3)
	(if
		(= (current_index) 0)
		3
		(append
			(previous_result 0 .true)
			(previous_result 0)
			(previous_result 0)
		)
	)
)
```
Output:
```amalgam
[
	3
	3
	(null)
	3
	3
	(null)
	(null)
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

