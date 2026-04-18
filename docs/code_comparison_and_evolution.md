### Opcode: `total_size`
#### Parameters
`* node`
#### Description
Evaluates to the total count of all of the nodes referenced directly or indirectly by `node`.
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
(total_size
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		[5 6]
	]
)
```
Output:
```amalgam
10
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `mutate`
#### Parameters
`* node [number mutation_rate] [assoc mutation_weights] [assoc operation_type] [preserve_type_depth]`
#### Description
Evaluates to a mutated version of `node`.  The `mutation_rate` can range from 0.0 to 1.0 and defaulting to 0.00001, and indicates the probability that any node will experience a mutation.  The parameter `mutation_weights` is an assoc where the keys are the allowed opcode names and the values are the probabilities that each opcode would be chosen; if null or unspecified, it defaults to all opcodes each with their own default probability.  The parameter `operation_type` is an assoc where the keys are mutation operations and the values are the probabilities that the operations will be performed.  The operations can consist of the strings "change_type", "delete", "insert", "swap_elements", "deep_copy_elements", and "delete_elements".  If `preserve_type_depth` is specified, it will retain the types of node down to and including whatever depth is specified, and defaults to 0 indicating that none of the structure needs to be preserved.
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
(mutate
	(lambda
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
			13
			14
			(associate "a" 1 "b" 2)
		]
	)
	0.4
)
```
Output:
```amalgam
[
	1
	(and)
	3
	{}
	5
	6
	(tail)
	(get)
	(acos)
	(floor)
	(let)
	12
	zbiqZH
	14
	(associate .null)
]
```
Example:
```amalgam
(mutate
	(lambda
		[
			1
			2
			3
			4
			(associate "alpha" 5 "beta" 6)
			(associate
				"nest"
				(associate
					"count"
					[7 8 9]
				)
				"end"
				[10 11 12]
			)
		]
	)
	0.2
	(associate "+" 0.5 "-" 0.3 "*" 0.2)
	(associate "change_type" 0.08 "delete" 0.02 "insert" 0.9)
)
```
Output:
```amalgam
[
	1
	(-)
	3
	(-)
	(associate "alpha" 5 (+) 6)
	(associate
		"nest"
		(associate
			"count"
			[(*) 8 9]
		)
		"end"
		[(*) 11 12]
	)
]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `get_mutation_defaults`
#### Parameters
`string value_type`
#### Description
Retrieves the default values of `value_type` for mutation, either "mutation_opcodes" or "mutation_types"
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
(get_mutation_defaults "mutation_types")
```
Output:
```amalgam
{
	change_type 0.29
	deep_copy_elements 0.07
	delete 0.1
	delete_elements 0.05
	insert 0.25
	swap_elements 0.24
}
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `commonality`
#### Parameters
`* node1 * node2 [assoc params]`
#### Description
Evaluates to the total count of all of the nodes referenced within `node1` and `node2` that are equivalent.  The assoc `params` can contain the keys "string_edit_distance", "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "use_string_edit_distance" is true (default is false), it will assume `node1` and `node2` as string literals and compute via string edit distance.  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.
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
(commonality
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(commonality
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
)
```
Output:
```amalgam
15
```
Example:
```amalgam
(commonality .infinity 3)
```
Output:
```amalgam
0.125
```
Example:
```amalgam
(commonality
	.null
	3
	{types_must_match .false}
)
```
Output:
```amalgam
0.125
```
Example:
```amalgam
(commonality .infinity .infinity)
```
Output:
```amalgam
1
```
Example:
```amalgam
(commonality .infinity -.infinity)
```
Output:
```amalgam
0.125
```
Example:
```amalgam
(commonality "hello" "hello")
```
Output:
```amalgam
1
```
Example:
```amalgam
(commonality
	"hello"
	"hello"
	{string_edit_distance .true}
)
```
Output:
```amalgam
5
```
Example:
```amalgam
(commonality
	"hello"
	"el"
	{nominal_strings .false}
)
```
Output:
```amalgam
0.49099467997549845
```
Example:
```amalgam
(commonality
	"hello"
	"el"
	{string_edit_distance .true}
)
```
Output:
```amalgam
2
```
Example:
```amalgam
(commonality
	"el"
	"hello"
	{string_edit_distance .true}
)
```
Output:
```amalgam
2
```
Example:
```amalgam
(commonality
	(lambda
		{a 1 b 2 c 3}
	)
	(lambda
		(if
			x
			{a 1 b 2 c 3}
			.false
		)
	)
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(commonality
	[1 2 3]
	[
		[1 2 3]
	]
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(commonality
	[1 2 3]
	(unordered_list 1 2 3)
	{types_must_match .false}
)
```
Output:
```amalgam
3.5
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `edit_distance`
#### Parameters
`* node1 * node2 [assoc params]`
#### Description
Evaluates to the number of nodes that are different between `node1` and `node2`. The assoc `params` can contain the keys "string_edit_distance", "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "use_string_edit_distance" is true (default is false), it will assume `node1` and `node2` as string literals and compute via string edit distance.  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.
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
(edit_distance
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(edit_distance
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
)
```
Output:
```amalgam
2
```
Example:
```amalgam
(edit_distance "hello" "hello")
```
Output:
```amalgam
0
```
Example:
```amalgam
(edit_distance
	"hello"
	"hello"
	{string_edit_distance .true}
)
```
Output:
```amalgam
0
```
Example:
```amalgam
(edit_distance
	"hello"
	"el"
	{nominal_strings .false}
)
```
Output:
```amalgam
1.018010640049003
```
Example:
```amalgam
(edit_distance
	"hello"
	"el"
	{string_edit_distance .true}
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(edit_distance
	"el"
	"hello"
	{string_edit_distance .true}
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(edit_distance
	[1 2 3]
	(lambda
		(unordered_list
			[1 2 3]
		)
	)
)
```
Output:
```amalgam
1
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `intersect`
#### Parameters
`* node1 * node2 [assoc params]`
#### Description
Evaluates to whatever is common between `node1` and `node2` exclusive.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true, the default, then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.
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
(intersect
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "a" 3 "b" 4)
	]
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "c" 3 "b" 4)
	]
)
```
Output:
```amalgam
[
	1
	(- 4 2)
	{b 4}
]
```
Example:
```amalgam
(intersect
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
)
```
Output:
```amalgam
(seq 2 1)
```
Example:
```amalgam
(intersect
	(lambda
		(unordered_list (get_entity_comments) 1 2)
	)
	(lambda
		(unordered_list (get_entity_comments) 1 2 4)
	)
)
```
Output:
```amalgam
(unordered_list (get_entity_comments) 1 2)
```
Example:
```amalgam
(intersect
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
)
```
Output:
```amalgam
[
	1
	2
	3
	{b 4}
	(if
		true
		1
		(unordered_list (get_entity_comments) 1)
	)
	[5 6]
]
```
Example:
```amalgam
(intersect
	(lambda
		[
			1
			(associate "a" 3 "b" 4)
		]
	)
	(lambda
		[
			1
			(associate "c" 3 "b" 4)
		]
	)
)
```
Output:
```amalgam
[
	1
	(associate .null 3 "b" 4)
]
```
Example:
```amalgam
(intersect
	(lambda
		(replace 4 2 6 1 7)
	)
	(lambda
		(replace 4 1 7 2 6)
	)
)
```
Output:
```amalgam
(replace 4 2 6 1 7)
```
Example:
```amalgam
(unparse
	(intersect
		(lambda
			[
				
				;comment 1
				;comment 2
				;comment 3
				1
				3
				5
				7
				9
				11
				13
			]
		)
		(lambda
			[
				
				;comment 2
				;comment 3
				;comment 4
				1
				4
				6
				8
				10
				12
				14
			]
		)
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"[\r\n\t\r\n\t;comment 2\r\n\t;comment 3\r\n\t1\r\n]\r\n"
```
Example:
```amalgam
(intersect
	[1 2 3]
	[
		[1 2 3]
	]
)
```
Output:
```amalgam
[1 2 3]
```
Example:
```amalgam
(intersect
	[1 2 3]
	[
		[1 2 3]
	]
	{recursive_matching .false}
)
```
Output:
```amalgam
[]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `union`
#### Parameters
`* node1 * node2 [assoc params]`
#### Description
Evaluates to whatever is inclusive when merging `node1` and `node2`.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true, the default, then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.
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
(union
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
)
```
Output:
```amalgam
(seq 2 (get_entity_comments) 1 4 (get_entity_comments))
```
Example:
```amalgam
(union
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "a" 3 "b" 4)
	]
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "c" 3 "b" 4)
	]
)
```
Output:
```amalgam
[
	1
	(- 4 2)
	{a 3 b 4 c 3}
]
```
Example:
```amalgam
(union
	(lambda
		(unordered_list (get_entity_comments) 1 2)
	)
	(lambda
		(unordered_list (get_entity_comments) 1 2 4)
	)
)
```
Output:
```amalgam
(unordered_list (get_entity_comments) 1 2 4)
```
Example:
```amalgam
(union
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
)
```
Output:
```amalgam
[
	1
	2
	3
	{a 3 b 4 c 3}
	(if
		true
		1
		(unordered_list (get_entity_comments) 1)
	)
	[5 6]
]
```
Example:
```amalgam
(union
	(lambda
		[
			1
			(associate "a" 3 "b" 4)
		]
	)
	(lambda
		[
			1
			(associate "c" 3 "b" 4)
		]
	)
)
```
Output:
```amalgam
[
	1
	(associate .null 3 "b" 4)
]
```
Example:
```amalgam
(union
	[3 2]
	[3 4]
)
```
Output:
```amalgam
[3 4 2]
```
Example:
```amalgam
(union
	[2 3]
	[3 2 4]
)
```
Output:
```amalgam
[3 2 4 3]
```
Example:
```amalgam
(unparse
	(union
		(lambda
			[
				
				;comment 1
				;comment 2
				;comment 3
				1
				2
				3
				5
				7
				9
				11
				13
			]
		)
		(lambda
			[
				
				;comment 2
				;comment 3
				;comment 4
				1
				
				;comment x
				2
				4
				6
				8
				10
				12
				14
			]
		)
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"[\r\n\t\r\n\t;comment 1\r\n\t;comment 2\r\n\t;comment 3\r\n\t;comment 4\r\n\t1\r\n\t\r\n\t;comment x\r\n\t2\r\n\t4\r\n\t3\r\n\t6\r\n\t5\r\n\t8\r\n\t7\r\n\t10\r\n\t9\r\n\t12\r\n\t11\r\n\t14\r\n\t13\r\n]\r\n"
```
Example:
```amalgam
(union
	[1 2 3]
	[
		[1 2 3]
	]
)
```
Output:
```amalgam
[
	[1 2 3]
]
```
Example:
```amalgam
(union
	[
		[1 2 3]
	]
	[1 2 3]
)
```
Output:
```amalgam
[
	[1 2 3]
]
```
Example:
```amalgam
(union
	[1 2 3]
	(lambda
		[
			[1 2 3]
		]
	)
)
```
Output:
```amalgam
[
	[1 2 3]
]
```
Example:
```amalgam
(union
	[1 2 3]
	(lambda
		[
			[1 2 3]
		]
	)
	{recursive_matching .false}
)
```
Output:
```amalgam
[
	1
	2
	3
	[1 2 3]
]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `difference`
#### Parameters
`* node1 * node2`
#### Description
Finds the difference between `node1` and `node2`, and generates code that, if evaluated passing `node1` as its parameter "_", would turn it into `node2`.  Useful for finding a small difference of what needs to be changed to apply it to new (and possibly slightly different) data or code.
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
(difference
	(lambda
		{
			a 1
			b 2
			c 4
			d 7
			e 10
			f 12
			g 13
		}
	)
	(lambda
		[
			a
			2
			c
			4
			d
			6
			q
			8
			e
			10
			f
			12
			g
			14
		]
	)
)
```
Output:
```amalgam
(declare
	{_ .null}
	(replace
		_
		[]
		(lambda
			[
				a
				2
				c
				4
				d
				6
				q
				8
				e
				10
				f
				12
				g
				14
			]
		)
	)
)
```
Example:
```amalgam
(difference
	{
		a 1
		b 2
		c 4
		d 7
		e 10
		f 12
		g 13
	}
	{
		a 2
		c 4
		d 6
		e 10
		f 12
		g 14
		q 8
	}
)
```
Output:
```amalgam
(declare
	{_ .null}
	(replace
		_
		[]
		(lambda
			{
				a 2
				c (get
						(current_value 1)
						"c"
					)
				d 6
				e (get
						(current_value 1)
						"e"
					)
				f (get
						(current_value 1)
						"f"
					)
				g 14
				q 8
			}
		)
	)
)
```
Example:
```amalgam
(difference
	(lambda
		[
			1
			2
			4
			7
			10
			12
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
)
```
Output:
```amalgam
(declare
	{_ .null}
	(replace
		_
		[]
		(lambda
			[
				(get
					(current_value 1)
					1
				)
				(get
					(current_value 1)
					2
				)
				6
				8
				(get
					(current_value 1)
					4
				)
				(get
					(current_value 1)
					5
				)
				14
			]
		)
	)
)
```
Example:
```amalgam
(unparse
	(difference
		(lambda
			{
				a 1
				b 2
				c 4
				d 7
				e 10
				f 12
				g 13
			}
		)
		(lambda
			{
				a 2
				c 4
				d 6
				e 10
				f 12
				g 14
				q 8
			}
		)
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"(declare\r\n\t{_ .null}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{\r\n\t\t\t\ta 2\r\n\t\t\t\tc (get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t\"c\"\r\n\t\t\t\t\t)\r\n\t\t\t\td 6\r\n\t\t\t\te (get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t\"e\"\r\n\t\t\t\t\t)\r\n\t\t\t\tf (get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t\"f\"\r\n\t\t\t\t\t)\r\n\t\t\t\tg 14\r\n\t\t\t\tq 8\r\n\t\t\t}\r\n\t\t)\r\n\t)\r\n)\r\n"
```
Example:
```amalgam
(unparse
	(difference
		(lambda
			(associate
				a
				1
				g
				[1 2]
			)
		)
		(lambda
			(associate
				a
				2
				g
				[1 4]
			)
		)
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"(declare\r\n\t{_ .null}\r\n\t(replace\r\n\t\t_\r\n\t\t[3]\r\n\t\t(lambda\r\n\t\t\t[\r\n\t\t\t\t(get\r\n\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t0\r\n\t\t\t\t)\r\n\t\t\t\t4\r\n\t\t\t]\r\n\t\t)\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t(set_type\r\n\t\t\t\t[\r\n\t\t\t\t\ta\r\n\t\t\t\t\t2\r\n\t\t\t\t\tg\r\n\t\t\t\t\t(get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t3\r\n\t\t\t\t\t)\r\n\t\t\t\t]\r\n\t\t\t\t\"associate\"\r\n\t\t\t)\r\n\t\t)\r\n\t)\r\n)\r\n"
```
Example:
```amalgam
(unparse
	(difference
		(zip
			[1 2 3 4 5]
		)
		(append
			(zip
				[2 6 5]
			)
			{a 1}
		)
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"(declare\r\n\t{_ .null}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{\r\n\t\t\t\t2 .null\r\n\t\t\t\t5 .null\r\n\t\t\t\t6 .null\r\n\t\t\t\ta 1\r\n\t\t\t}\r\n\t\t)\r\n\t)\r\n)\r\n"
```
Example:
```amalgam
(unparse
	(difference
		(zip
			[1 2 3 4 5]
		)
		(zip
			[2 6 5]
		)
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"(declare\r\n\t{_ .null}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{2 .null 5 .null 6 .null}\r\n\t\t)\r\n\t)\r\n)\r\n"
```
Example:
```amalgam
(unparse
	(difference
		(zip
			[1 2 5]
		)
		(zip
			[2 6 5]
		)
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"(declare\r\n\t{_ .null}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{2 .null 5 .null 6 .null}\r\n\t\t)\r\n\t)\r\n)\r\n"
```
Example:
```amalgam
(let
	{
		x (lambda
				[
					6
					[1 2]
				]
			)
		y (lambda
				[
					7
					[1 4]
				]
			)
	}
	(call
		(difference x y)
		{_ x}
	)
)
```
Output:
```amalgam
[
	7
	[1 4]
]
```
Example:
```amalgam
(let
	{
		x (lambda
				[
					(+ 0 1)
					[1 2]
				]
			)
		y (lambda
				[
					(+ 7 8)
					[1 4]
				]
			)
	}
	(call
		(difference x y)
		{_ x}
	)
)
```
Output:
```amalgam
[
	(+ 7 8)
	[1 4]
]
```
Example:
```amalgam
(let
	{
		x (lambda
				[
					6
					[
						["a" "b"]
						1
						2
					]
				]
			)
		y (lambda
				[
					7
					[
						["a" "x"]
						1
						4
					]
				]
			)
	}
	(call
		(difference x y)
		{_ x}
	)
)
```
Output:
```amalgam
[
	7
	[
		["a" "x"]
		1
		4
	]
]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `mix`
#### Parameters
`* node1 * node2 [number keep_chance_node1] [number keep_chance_node2] [assoc params]`
#### Description
Performs a union operation on `node1` and `node2`, but randomly ignores nodes from one or the other if the nodes are not equal.  If only `keep_chance_node1` is specified, `keep_chance_node2` defaults to 1 - `keep_chance_node1`. `keep_chance_node1` specifies the probability that a node from `node1` will be kept, and `keep_chance_node2` the probability that a node from `node2` will be kept.  `keep_chance_node1` + `keep_chance_node2` should be between 1 and 2, as there are two objects being merged, otherwise the values will be normalized.  `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", "recursive_matching", and "similar_mix_chance".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  "similar_mix_chance" is the additional probability that two nodes will mix if they have some commonality, which will include interpolating number and string values based on `keep_chance_node1` and `keep_chance_node2`, and defaults to 0.0.  If "similar_mix_chance" is negative, then 1 minus the value will be anded with the commonality probability, so -1 means that it will never mix and 0 means it will only mix when sufficiently common.
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
(mix
	(lambda
		[
			1
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	0
)
```
Output:
```amalgam
[1 3 4 9 11 14]
```
Example:
```amalgam
(mix
	(lambda
		[
	
			;comment 1
			;comment 2
			;comment 3
			1
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			
			;comment 2
			;comment 3
			;comment 4
			1
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	{similar_mix_chance 0}
)
```
Output:
```amalgam
[
	
	;comment 1
	;comment 2
	;comment 3
	;comment 4
	1
	4
	3
	5
	9
	11
	14
]
```
Example:
```amalgam
(mix
	(lambda
		[
			1
			2
			(associate "a" 3 "b" 4)
			(lambda
				(if
					true
					1
					(unordered_list (get_entity_comments) 1)
				)
			)
			[5 6]
		]
	)
	(lambda
		[
			1
			5
			3
			(associate "a" 3 "b" 4)
			(lambda
				(if
					false
					1
					(unordered_list
						(get_entity_comments)
						(lambda
							[2 9]
						)
					)
				)
			)
		]
	)
	0.8
	0.8
	{similar_mix_chance 0.5}
)
```
Output:
```amalgam
[
	1
	5
	3
	(associate "a" 3 "b" 4)
	(lambda
		(if
			true
			1
			(unordered_list
				(get_entity_comments)
				(lambda
					[2 9]
				)
			)
		)
	)
	[5]
]
```
Example:
```amalgam
(mix
	(lambda
		[
			1
			2
			(associate "a" 3 "b" 4)
			(lambda
				(if
					true
					1
					(unordered_list (get_entity_comments) 1)
				)
			)
			[5 6]
		]
	)
	(lambda
		[
			1
			5
			3
			{a 3 b 4}
			(lambda
				(if
					false
					1
					(seq
						(get_entity_comments)
						(lambda
							[2 9]
						)
					)
				)
			)
		]
	)
	0.8
	0.8
	{similar_mix_chance 1}
)
```
Output:
```amalgam
[
	1
	2.5
	{a 3 b 4}
	(associate "a" 3 "b" 4)
	(lambda
		(if
			true
			1
			(seq
				(get_entity_comments)
				(lambda
					[2 9]
				)
			)
		)
	)
	[5]
]
```
Example:
```amalgam
(mix
	(lambda
		[
			.true
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	{similar_mix_chance 1}
)
```
Output:
```amalgam
[
	.true
	3
	5
	7.5
	9.5
	11.5
	13.5
]
```
Example:
```amalgam
(mix
	(lambda
		[
			.true
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	{similar_mix_chance -1}
)
```
Output:
```amalgam
[3 5 2 4 12 11]
```
Example:
```amalgam
(mix
	1
	4
	0.5
	0.5
	{similar_mix_chance -1}
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(mix
	1
	4
	0.5
	0.5
	{similar_mix_chance -0.8}
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(mix
	1
	4
	0.5
	0.5
	{similar_mix_chance 0.5}
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(mix
	1
	4
	0.5
	0.5
	{similar_mix_chance 1}
)
```
Output:
```amalgam
2.5
```
Example:
```amalgam
(mix
	"abcdexyz"
	"abcomxyz"
	0.5
	0.5
	{nominal_strings .false similar_mix_chance 0.5}
)
```
Output:
```amalgam
"abcdexyz"
```
Example:
```amalgam
(mix
	"abcdexyz"
	"abcomxyz"
	0.5
	0.5
	{nominal_strings .false similar_mix_chance 0.5}
)
```
Output:
```amalgam
"abcdexyz"
```
Example:
```amalgam
(mix
	"abcdexyz"
	"abcomxyz"
	0.5
	0.5
	{nominal_strings .false similar_mix_chance 0.5}
)
```
Output:
```amalgam
"abcdexyz"
```
Example:
```amalgam
(mix
	{
		a [0 1]
		b [1 2]
		c [2 3]
	}
	{
		a [0 1]
		b [1 2]
		w [2 3]
		x [3 4]
		y [4 5]
		z [5 6]
	}
	0.5
	0.5
	{recursive_matching .false}
)
```
Output:
```amalgam
{
	a [0 1]
	b [1 2]
	w [2]
	z [5]
}
```

[Amalgam Opcodes](./opcodes.md)

