### Opcode: `first`
#### Parameters
`[list|assoc|number|string data]`
#### Description
Evaluates to the first element of `data`.  If `data` is a list, it will be the first element.  If `data` is an assoc, it will evaluate to the first element by assoc storage, but order does not matter.  If `data` is a string, it will be the first character.  If `data` is a number, it will evaluate to 1 if nonzero, 0 if zero.
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
(first
	[4 9.2 "this"]
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(first
	(associate "a" 1 "b" 2)
)
```
Output:
```amalgam
2
```
Example:
```amalgam
(first 3)
```
Output:
```amalgam
1
```
Example:
```amalgam
(first 0)
```
Output:
```amalgam
0
```
Example:
```amalgam
(first "abc")
```
Output:
```amalgam
"a"
```
Example:
```amalgam
(first "")
```
Output:
```amalgam
.null
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `tail`
#### Parameters
`[list|assoc|number|string data] [number retain_count]`
#### Description
Evaluates to everything but the first element.  If `data` is a list, it will be a list of all but the first element.  If `data` is an assoc, it will evaluate to the assoc without the first element by assoc storage order, but order does not matter.  If `data` is a string, it will be all but the first character.  If `data` is a number, it will evaluate to the value minus 1 if nonzero, 0 if zero.  If a `retain_count` is specified, it will be the number of elements to retain.  A positive number means from the end, a negative number means from the beginning.  The default value is -1 (all but the first element).
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
(tail
	[4 9.2 "this"]
)
```
Output:
```amalgam
[9.2 "this"]
```
Example:
```amalgam
(tail
	[1 2 3 4 5 6]
)
```
Output:
```amalgam
[2 3 4 5 6]
```
Example:
```amalgam
(tail
	[1 2 3 4 5 6]
	2
)
```
Output:
```amalgam
[5 6]
```
Example:
```amalgam
(tail
	[1 2 3 4 5 6]
	-2
)
```
Output:
```amalgam
[3 4 5 6]
```
Example:
```amalgam
(tail
	[1 2 3 4 5 6]
	-6
)
```
Output:
```amalgam
[]
```
Example:
```amalgam
(tail
	[1 2 3 4 5 6]
	6
)
```
Output:
```amalgam
[1 2 3 4 5 6]
```
Example:
```amalgam
(tail
	[1 2 3 4 5 6]
	10
)
```
Output:
```amalgam
[1 2 3 4 5 6]
```
Example:
```amalgam
(tail
	[1 2 3 4 5 6]
	-10
)
```
Output:
```amalgam
[]
```
Example:
```amalgam
(tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
)
```
Output:
```amalgam
{
	a 1
	b 2
	c 3
	d 4
	f 6
}
```
Example:
```amalgam
(tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	2
)
```
Output:
```amalgam
{b 2 c 3}
```
Example:
```amalgam
(tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-2
)
```
Output:
```amalgam
{
	a 1
	b 2
	c 3
	d 4
}
```
Example:
```amalgam
(tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	10
)
```
Output:
```amalgam
{
	a 1
	b 2
	c 3
	d 4
	e 5
	f 6
}
```
Example:
```amalgam
(tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-10
)
```
Output:
```amalgam
{}
```
Example:
```amalgam
(tail 3)
```
Output:
```amalgam
2
```
Example:
```amalgam
(tail 0)
```
Output:
```amalgam
0
```
Example:
```amalgam
(tail "abcdef")
```
Output:
```amalgam
"bcdef"
```
Example:
```amalgam
(tail "abcdef" 2)
```
Output:
```amalgam
"ef"
```
Example:
```amalgam
(tail "abcdef" -2)
```
Output:
```amalgam
"cdef"
```
Example:
```amalgam
(tail "abcdef" 6)
```
Output:
```amalgam
"abcdef"
```
Example:
```amalgam
(tail "abcdef" -6)
```
Output:
```amalgam
""
```
Example:
```amalgam
(tail "abcdef" 10)
```
Output:
```amalgam
"abcdef"
```
Example:
```amalgam
(tail "abcdef" -10)
```
Output:
```amalgam
""
```
Example:
```amalgam
(tail "")
```
Output:
```amalgam
.null
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `last`
#### Parameters
`[list|assoc|number|string data]`
#### Description
Evaluates to the last element of `data`.  If `data` is a list, it will be the last element.  If `data` is an assoc, it will evaluate to the first element by assoc storage, because order does not matter.  If `data` is a string, it will be the last character.  If `data` is a number, it will evaluate to 1 if nonzero, 0 if zero.
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
(last
	[4 9.2 "this"]
)
```
Output:
```amalgam
"this"
```
Example:
```amalgam
(last
	(associate "a" 1 "b" 2)
)
```
Output:
```amalgam
2
```
Example:
```amalgam
(last 3)
```
Output:
```amalgam
1
```
Example:
```amalgam
(last 0)
```
Output:
```amalgam
0
```
Example:
```amalgam
(last "abc")
```
Output:
```amalgam
"c"
```
Example:
```amalgam
(last "")
```
Output:
```amalgam
.null
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `trunc`
#### Parameters
`[list|assoc|number|string data] [number retain_count]`
#### Description
Truncates, evaluates to everything in `data` but the last element. If `data` is a list, it will be a list of all but the last element.  If `data` is an assoc, it will evaluate to the assoc without the first element by assoc storage order, because order does not matter.  If `data` is a string, it will be all but the last character.  If `data` is a number, it will evaluate to the value minus 1 if nonzero, 0 if zero. If `truncate_count` is specified, it will be the number of elements to retain.  A positive number means from the beginning, a negative number means from the end.  The default value is -1, indicating all but the last.
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
(trunc
	[4 9.2 "end"]
)
```
Output:
```amalgam
[4 9.2]
```
Example:
```amalgam
(trunc
	[1 2 3 4 5 6]
)
```
Output:
```amalgam
[1 2 3 4 5]
```
Example:
```amalgam
(trunc
	[1 2 3 4 5 6]
	2
)
```
Output:
```amalgam
[1 2]
```
Example:
```amalgam
(trunc
	[1 2 3 4 5 6]
	-2
)
```
Output:
```amalgam
[1 2 3 4]
```
Example:
```amalgam
(trunc
	[1 2 3 4 5 6]
	-6
)
```
Output:
```amalgam
[]
```
Example:
```amalgam
(trunc
	[1 2 3 4 5 6]
	6
)
```
Output:
```amalgam
[1 2 3 4 5 6]
```
Example:
```amalgam
(trunc
	[1 2 3 4 5 6]
	10
)
```
Output:
```amalgam
[1 2 3 4 5 6]
```
Example:
```amalgam
(trunc
	[1 2 3 4 5 6]
	-10
)
```
Output:
```amalgam
[]
```
Example:
```amalgam
(trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
)
```
Output:
```amalgam
{
	a 1
	c 3
	d 4
	e 5
	f 6
}
```
Example:
```amalgam
(trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	2
)
```
Output:
```amalgam
{e 5 f 6}
```
Example:
```amalgam
(trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-2
)
```
Output:
```amalgam
{
	c 3
	d 4
	e 5
	f 6
}
```
Example:
```amalgam
(trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	10
)
```
Output:
```amalgam
{
	a 1
	b 2
	c 3
	d 4
	e 5
	f 6
}
```
Example:
```amalgam
(trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-10
)
```
Output:
```amalgam
{}
```
Example:
```amalgam
(trunc 3)
```
Output:
```amalgam
2
```
Example:
```amalgam
(trunc 0)
```
Output:
```amalgam
0
```
Example:
```amalgam
(trunc "abcdef")
```
Output:
```amalgam
"abcde"
```
Example:
```amalgam
(trunc "abcdef" 2)
```
Output:
```amalgam
"ab"
```
Example:
```amalgam
(trunc "abcdef" -2)
```
Output:
```amalgam
"abcd"
```
Example:
```amalgam
(trunc "abcdef" 6)
```
Output:
```amalgam
"abcdef"
```
Example:
```amalgam
(trunc "abcdef" -6)
```
Output:
```amalgam
""
```
Example:
```amalgam
(trunc "abcdef" 10)
```
Output:
```amalgam
"abcdef"
```
Example:
```amalgam
(trunc "abcdef" -10)
```
Output:
```amalgam
""
```
Example:
```amalgam
(trunc "")
```
Output:
```amalgam
.null
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `append`
#### Parameters
`[list|assoc|* collection1] [list|assoc|* collection2] ... [list|assoc|* collectionN]`
#### Description
Evaluates to a new list or assoc which merges all lists, `collection1` through `collectionN`, based on parameter order. If any assoc is passed in, then returns an assoc (lists will be automatically converted to an assoc with the indices as keys and the list elements as values). If a non-list and non-assoc is specified, then it just adds that one element to the list
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
(append
	[1 2 3]
	[4 5 6]
	[7 8 9]
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
(append
	[1 2 3]
	(associate "a" 4 "b" 5 "c" 6)
	[7 8 9]
	(associate "d" 10 "e" 11)
)
```
Output:
```amalgam
{
	0 1
	1 2
	2 3
	3 7
	4 8
	5 9
	a 4
	b 5
	c 6
	d 10
	e 11
}
```
Example:
```amalgam
(append
	[4 9.2 "this"]
	"end"
)
```
Output:
```amalgam
[4 9.2 "this" "end"]
```
Example:
```amalgam
(append
	(associate 0 4 1 9.2 2 "this")
	"end"
)
```
Output:
```amalgam
{
	0 4
	1 9.2
	2 "this"
	3 "end"
}
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `size`
#### Parameters
`[list|assoc|string collection] collection`
#### Description
Evaluates to the size of the `collection` in number of elements.  If `collection` is a string, returns the length in UTF-8 characters.
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
(size
	[4 9.2 "this"]
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(size
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
4
```
Example:
```amalgam
(size "hello")
```
Output:
```amalgam
5
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `get`
#### Parameters
`* data [number|index|list walk_path_1] [number|string|list walk_path_2] ...`
#### Description
Evaluates to `data` as traversed by the set of values specified by `walk_path_1', which can be any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values.  If multiple walk paths are specified, then `get` returns a list, where each element in the list is the respective element retrieved by the respective walk path.  If the walk path continues past the data structure, it will return a null.
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
(get
	[4 9.2 "this"]
)
```
Output:
```amalgam
[4 9.2 "this"]
```
Example:
```amalgam
(get
	[4 9.2 "this"]
	1
)
```
Output:
```amalgam
9.2
```
Example:
```amalgam
(get
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
	"c"
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(get
	[
		0
		1
		2
		3
		[
			0
			1
			2
			(associate "a" 1)
		]
	]
	[4 3 "a"]
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(get
	[4 9.2 "this"]
	1
	2
)
```
Output:
```amalgam
[9.2 "this"]
```
Example:
```amalgam
(seq
	(declare
		{
			var {
					A (associate "B" 2)
					B 2
				}
		}
	)
	[
		(get
			var
			["A" "B"]
		)
		(get
			var
			["A" "C"]
		)
		(get
			var
			["B" "C"]
		)
	]
)
```
Output:
```amalgam
[2 .null .null]
```
Example:
```amalgam
(get
	{.null 3}
	.null
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(let
	{
		complex_assoc {
				4 "number"
				[4] "list"
				{4 4} "assoc"
				"4" "string"
			}
	}
	[
		(get complex_assoc 4)
		(get complex_assoc "4")
		(get
			complex_assoc
			[
				[4]
			]
		)
		(get
			complex_assoc
			{4 4}
		)
		(sort (indices complex_assoc))
	]
)
```
Output:
```amalgam
[
	"number"
	"string"
	"list"
	"assoc"
	[
		4
		[4]
		{4 4}
		"4"
	]
]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `set`
#### Parameters
`* data [number|string|list walk_path1] [* new_value1] [number|string|list walk_path2] [* new_value2] ... [number|string|list walk_pathN] [* new_valueN]`
#### Description
Performs a deep copy on `data` (a copy of all data structures referenced by it and its references), then looks at the remaining parameters as pairs.  For each pair, the first is any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values as a walk path of indices. `new_value1` to `new_valueN` represent a value that will be used to replace  whatever is in the location the preceding location parameter specifies.  If a particular location does not exist, it will be created assuming the most generic type that will support the index (as a null, list, or assoc); however, it will not change the type of immediate values to an assoc or list. Note that `(target)` will evaluate to the new copy of data, which is the base of the newly constructed data; this is useful for creating circular references.
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
(set
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
	"e"
	5
)
```
Output:
```amalgam
{
	4 "d"
	a 1
	b 2
	c 3
	e 5
}
```
Example:
```amalgam
(set
	[0 1 2 3 4]
	2
	10
)
```
Output:
```amalgam
[0 1 10 3 4]
```
Example:
```amalgam
(set
	(associate "a" 1 "b" 2)
	"a"
	3
)
```
Output:
```amalgam
{a 3 b 2}
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `replace`
#### Parameters
`* data [number|string|list walk_path1] [* function1] [number|string|list walk_path2] [* function2] ... [number|string|list walk_pathN] [* functionN]`
#### Description
Performs a deep copy on `data` (a copy of all data structures referenced by it and its references), then looks at the remaining parameters as pairs.  For each pair, the first is any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values. `function1` to `functionN` represent a function that will be used to replace in place of whatever is in the location of the corresponding walk_path, and will be passed the current node in (current_value).  The function can optionally be just be an immediate value or any code that can be evaluated.  If a particular location does not exist, it will be created assuming the most generic type that will support the index (as a null, list, or assoc). Note that the `(target)` will evaluate to the new copy of data, which is the base of the newly constructed data; this is useful for creating circular references.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: true
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(replace
	[
		(associate "a" 13)
	]
)
```
Output:
```amalgam
[
	{a 13}
]
```
Example:
```amalgam
(replace
	[
		(associate "a" 1)
	]
	[2]
	1
	[0]
	[4 5 6]
)
```
Output:
```amalgam
[
	[4 5 6]
	.null
	1
]
```
Example:
```amalgam
(replace
	[
		(associate "a" 1)
	]
	2
	1
	0
	[4 5 6]
)
```
Output:
```amalgam
[
	[4 5 6]
	.null
	1
]
```
Example:
```amalgam
(replace
	[
		(associate "a" 1)
	]
	[0]
	(lambda
		(set (current_value) "b" 2)
	)
)
```
Output:
```amalgam
[
	{a 1 b 2}
]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `indices`
#### Parameters
`list|assoc collection`
#### Description
Evaluates to the list of strings or numbers that comprise the indices for the list or associative parameter `collection`.  It is guaranteed that the opcodes indices and values will evaluate and return elements in the same order when given the same node.
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
(sort
	(indices
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
)
```
Output:
```amalgam
[4 "a" "b" "c"]
```
Example:
```amalgam
(indices
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
)
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
]
```
Example:
```amalgam
(indices
	(range 0 3)
)
```
Output:
```amalgam
[0 1 2 3]
```
Example:
```amalgam
(sort
	(indices
		(zip
			(range 0 3)
		)
	)
)
```
Output:
```amalgam
[0 1 2 3]
```
Example:
```amalgam
(sort
	(indices
		(zip
			[0 1 2 3]
		)
	)
)
```
Output:
```amalgam
[0 1 2 3]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `values`
#### Parameters
`list|assoc collection [bool only_unique_values]`
#### Description
Evaluates to the list of entities that comprise the values for the list or associative list `collection`.  If `only_unique_values` is true (defaults to false), then it will filter out any duplicate values and only return those that are unique, preserving their order of first appearance.  If `only_unique_values` is not true, then it is guaranteed that the opcodes indices and values will evaluate and return elements in the same order when given the same node.
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
(sort
	(values
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
)
```
Output:
```amalgam
[1 2 3 "d"]
```
Example:
```amalgam
(values
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
)
```
Output:
```amalgam
[
	"a"
	1
	"b"
	2
	"c"
	3
	4
	"d"
]
```
Example:
```amalgam
(values
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
		1
		2
		3
		4
		"a"
		"b"
		"c"
	]
	.true
)
```
Output:
```amalgam
[
	"a"
	1
	"b"
	2
	"c"
	3
	4
	"d"
]
```
Example:
```amalgam
(sort
	(values
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
			"e"
			1
		)
		.true
	)
)
```
Output:
```amalgam
[1 2 3 "d"]
```
Example:
```amalgam
(values
	(append
		(range 1 20)
		(range 1 20)
	)
	.true
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
	13
	14
	15
	16
	17
	18
	19
	20
]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `contains_index`
#### Parameters
`list|assoc collection string|number|list index`
#### Description
Evaluates to true if the index is in the `collection`.  If index is a string, it will attempt to look at `collection` as an assoc, if number, it will look at `collection` as a list.  If index is a list, it will traverse a via the elements in the list as a walk path, with each element .
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
(contains_index
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
	"c"
)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(contains_index
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
	"m"
)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(contains_index
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	2
)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(contains_index
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	100
)
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `contains_value`
#### Parameters
`list|assoc|string collection_or_string string|number value`
#### Description
Evaluates to true if the `value` is contained in `collection_or_string`.  If `collection_or_string` is a string, then it uses `value` as a regular expression and evaluates to true if the regular expression matches.
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
(contains_value
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
	1
)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(contains_value
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
	44
)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(contains_value
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	"d"
)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(contains_value
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	100
)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(contains_value "hello world" ".*world")
```
Output:
```amalgam
.true
```
Example:
```amalgam
(contains_value "abcdefg" "a.*g")
```
Output:
```amalgam
.true
```
Example:
```amalgam
(contains_value "3.141" "[0-9]+\\.[0-9]+")
```
Output:
```amalgam
.true
```
Example:
```amalgam
(contains_value "3.141" "\\d+\\.\\d+")
```
Output:
```amalgam
.true
```
Example:
```amalgam
(contains_value "3.a141" "\\d+\\.\\d+")
```
Output:
```amalgam
.false
```
Example:
```amalgam
(contains_value "abc\r\n123" "(.|\r)*\n.*")
```
Output:
```amalgam
.true
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `remove`
#### Parameters
`list|assoc collection number|string|list index`
#### Description
Removes the index-value pair with `index` being the index in assoc or index of `collection`, returning a new list or assoc with `index` removed.  If `index` is a list of numbers or strings, then it will remove each of the requested indices.  Negative numbered indices will count back from the end of a list.
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
(sort
	(remove
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
		4
	)
)
```
Output:
```amalgam
[1 2 3]
```
Example:
```amalgam
(remove
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	4
)
```
Output:
```amalgam
[
	"a"
	1
	"b"
	2
	3
	4
	"d"
]
```
Example:
```amalgam
(sort
	(remove
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
		[4 "a"]
	)
)
```
Output:
```amalgam
[2 3]
```
Example:
```amalgam
(remove
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	[4]
)
```
Output:
```amalgam
[
	"a"
	1
	"b"
	2
	3
	4
	"d"
]
```
Example:
```amalgam
(remove
	[0 1 2 3 4 5]
	[0 2]
)
```
Output:
```amalgam
[1 3 4 5]
```
Example:
```amalgam
(remove
	[0 1 2 3 4 5]
	-1
)
```
Output:
```amalgam
[0 1 2 3 4]
```
Example:
```amalgam
(remove
	[0 1 2 3 4 5]
	[0 -1]
)
```
Output:
```amalgam
[1 2 3 4]
```
Example:
```amalgam
(remove
	[0 1 2 3 4 5]
	[
		5
		0
		1
		2
		3
		4
		5
		6
	]
)
```
Output:
```amalgam
[]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `keep`
#### Parameters
`list|assoc collection number|string|list index`
#### Description
Keeps only the index-value pair with index being the index in `collection`, returning a new list or assoc with only that index.  If `index` is a list of numbers or strings, then it will only keep those requested indices.  Negative numbered indices will count back from the end of a list.
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
(keep
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
	4
)
```
Output:
```amalgam
{4 "d"}
```
Example:
```amalgam
(keep
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	4
)
```
Output:
```amalgam
["c"]
```
Example:
```amalgam
(sort
	(keep
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
		[4 "a"]
	)
)
```
Output:
```amalgam
[1 "d"]
```
Example:
```amalgam
(keep
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	[4 "a"]
)
```
Output:
```amalgam
["c"]
```
Example:
```amalgam
(keep
	[0 1 2 3 4 5]
	[0 2]
)
```
Output:
```amalgam
[0 2]
```
Example:
```amalgam
(keep
	[0 1 2 3 4 5]
	-1
)
```
Output:
```amalgam
[5]
```
Example:
```amalgam
(keep
	[0 1 2 3 4 5]
	[0 -1]
)
```
Output:
```amalgam
[0 5]
```
Example:
```amalgam
(keep
	[0 1 2 3 4 5]
	[
		5
		0
		1
		2
		3
		4
		5
		6
	]
)
```
Output:
```amalgam
[0 1 2 3 4 5]
```

[Amalgam Opcodes](./opcodes.md)

