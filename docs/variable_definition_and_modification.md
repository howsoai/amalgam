### Opcode: `symbol`
#### Parameters
``
#### Description
A string representing an internal symbol, a variable.
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
(let
	{foo 1}
	foo
)
```
Output:
```amalgam
1
```
Example:
```amalgam
not_defined
```
Output:
```amalgam
(null)
```
Example:
```amalgam
(lambda foo)
```
Output:
```amalgam
foo
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `let`
#### Parameters
`assoc variables [code code1] [code code2] ... [code codeN]`
#### Description
Pushes the key-value pairs of `variables` onto the scope stack so that they become the new variables, then runs each code block sequentially, evaluating to the last code block run, unless it encounters a `conclude` or `return`, in which case it will halt processing and evaluate to the value returned by `conclude` or propagate the `return`.  Note that the last step will not consume a concluded value.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: true
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(let
	{x 4 y 6}
	(+ x y)
)
```
Output:
```amalgam
10
```
Example:
```amalgam
(let
	{x 4 y 6}
	(declare
		{x 5 z 1}
		(+ x y z)
	)
)
```
Output:
```amalgam
11
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `declare`
#### Parameters
`assoc variables [code code1] [code code2] ... [code codeN]`
#### Description
For each key-value pair of `variables`, if not already in the current context in the scope stack, it will define them.  Then it runs each code block sequentially, evaluating to the last code block run, unless it encounters a `conclude` or `return`, in which case it will halt processing and evaluate to the value returned by `conclude` or propagate the `return`.  Note that the last step will not consume a concluded value.
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
	(declare
		{x 7}
		(accum "x" 1)
	)
	(declare
		{x 4}
	)
	x
)
```
Output:
```amalgam
8
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `assign`
#### Parameters
`assoc|string variables [number index1|string index1|list walk_path1|* new_value1] [* new_value1] [number index2|string index2|list walk_path2] [* new_value2] ...`
#### Description
If `variables` is an assoc, then for each key-value pair it assigns the value to the variable represented by the key found by tracing upward on the stack.  If a variable is not found, it will create a variable on the top of the stack with that name.  If `variables` is a string and there are two parameters, it will assign the second parameter to the variable represented by the first.  If `variables` is a string and there are three or more parameters, then it will find the variable by tracing up the stack and then use each pair of walk_path and new_value to assign new_value to that part of the variable's structure.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): null
#### Examples
Example:
```amalgam
(let
	{x 0}
	(assign {x 10} )
	x
)
```
Output:
```amalgam
10
```
Example:
```amalgam
(seq
	(assign "x" 20)
	x
)
```
Output:
```amalgam
20
```
Example:
```amalgam
(seq
	(assign
		"x"
		[
			0
			1
			2
			(associate "a" 1 "b" 2 "c" 3)
		]
	)
	(assign
		"x"
		[1]
		"not 1"
	)
	x
)
```
Output:
```amalgam
[
	0
	"not 1"
	2
	{a 1 b 2 c 3}
]
```
Example:
```amalgam
(seq
	(assign
		"x"
		[
			0
			1
			2
			(associate "a" 1 "b" 2 "c" 3)
		]
	)
	(assign
		"x"
		[3 "c"]
		["c attribute"]
		[3 "a"]
		["a attribute"]
	)
	x
)
```
Output:
```amalgam
[
	0
	1
	2
	{
		a ["a attribute"]
		b 2
		c ["c attribute"]
	}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `accum`
#### Parameters
`assoc|string variables [number index1|string index1|list walk_path1] [* accum_value1] [number index2|string index2|list walk_path2] [* accum_value2] ...`
#### Description
If `variables` is an assoc, then for each key-value pair of data, it assigns the value of the pair accumulated with the current value of the variable represented by the key on the stack, and stores the result in the variable.  It searches for the variable name tracing up the stack to find the variable. If the variable is not found, it will create a variable on the top of the stack.  Accumulation is performed differently based on the type.  For numeric values it adds, for strings it concatenates, for lists and assocs it appends.  If `variables` is a string and there are two parameters, then it will accum the second parameter to the variable represented by the first.  If `variables` is a string and there are three or more parameters, then it will find the variable by tracing up the stack and then use each pair of the corresponding walk path and accum value to that part of the variable's structure.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): null
#### Examples
Example:
```amalgam
(seq
	(assign
		{x 10}
	)
	(accum
		{x 1}
	)
	x
)
```
Output:
```amalgam
11
```
Example:
```amalgam
(declare
	{
		accum_assoc (associate "a" 1 "b" 2)
		accum_list [1 2 3]
		accum_string "abc"
	}
	(accum
		{accum_string "def"}
	)
	(accum
		{
			accum_list [4 5 6]
		}
	)
	(accum
		{
			accum_list (associate "7" 8)
		}
	)
	(accum
		{
			accum_assoc (associate "c" 3 "d" 4)
		}
	)
	(accum
		{
			accum_assoc ["e" 5]
		}
	)
	[accum_string accum_list accum_assoc]
)
```
Output:
```amalgam
[
	"abcdef"
	[
		1
		2
		3
		4
		5
		6
		"7"
		8
	]
	{
		a 1
		b 2
		c 3
		d 4
		e 5
	}
]
```
Example:
```amalgam
(seq
		(assign "x" 1)
		(accum "x" [] 4)
		x
)
```
Output:
```amalgam
5
```
Example:
```amalgam
(seq
	(assign
		"x"
		[
			0
			1
			2
			(associate "a" 1 "b" 2 "c" 3)
		]
	)
	(accum
		"x"
		[1]
		1
	)
	x
)
```
Output:
```amalgam
[
	0
	2
	2
	{a 1 b 2 c 3}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `retrieve`
#### Parameters
`[string|list|assoc variables]`
#### Description
If `variables` is a string, then it gets the value on the stack specified by the string.  If `variables` is a list, it returns a list of the values on the stack specified by each element of the list interpreted as a string.  If `variables` is an assoc, it returns an assoc with the indices of the assoc which was passed in with the values being the appropriate values on the stack for each index.
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
	(assign
		{a 1}
	)
	(retrieve "a")
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(seq
	(assign
		{a 1 b 2}
	)
	[
		(retrieve "a")
		(retrieve
			["a" "b"]
		)
		(retrieve
			(zip
				["a" "b"]
			)
		)
	]
)
```
Output:
```amalgam
[
	1
	[@(target .true 0) 2]
	{a @(target .true 0) b @(target .true [1 1])}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `target`
#### Parameters
`[number|bool stack_distance] [number|string|list walk_path]`
#### Description
Evaluates to the node being created, referenced by the parameters by target.  Useful for serializing graph data structures or looking up data during iteration.  If `stack_distance` is a number, it climbs back up the target stack that many levels.  If `stack_distance` is a boolean, then `.true` indicates the top of the stack and `.false` indicates the bottom.  If `walk_path` is specified, it will walk from the node at `stack_distance` to the corresponding target.  If building an object, specifying `stack_distance` to true is often useful for accessing or traversing the top-level elements.
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
[
	1
	2
	3
	(target 0 1)
	4
]
```
Output:
```amalgam
[1 2 3 @(target .true 1) 4]
```
Example:
```amalgam
[
	0
	1
	2
	3
	(+
		(target 0 1)
	)
	4
]
```
Output:
```amalgam
[0 1 2 3 1 4]
```
Example:
```amalgam
[
	0
	1
	2
	3
	[
		0
		1
		2
		3
		(+
			(target 1 1)
		)
		4
	]
]
```
Output:
```amalgam
[
	0
	1
	2
	3
	[0 1 2 3 1 4]
]
```
Example:
```amalgam
{
	a 0
	b 1
	c 2
	d 3
	e [
			0
			1
			2
			3
			(+
				(target 1 "a")
			)
			4
		]
}
```
Output:
```amalgam
{
	a 0
	b 1
	c 2
	d 3
	e [0 1 2 3 0 4]
}
```
Example:
```amalgam
(call_sandboxed {
	a 0
	b 1
	c 2
	d 3
	e [
			[
				0
				1
				2
				3
				(+
					(target .true "a")
				)
				4
			]
		]
})
```
Output:
```amalgam
{
	a 0
	b 1
	c 2
	d 3
	e [
			[0 1 2 3 0 4]
		]
}
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `stack`
#### Parameters
` `
#### Description
Evaluates to the current execution context, also known as the scope stack, containing all of the variables for each layer of the stack.
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
(stack)
```
Output:
```amalgam
[{}]
```
Example:
```amalgam
(call
	(lambda
		(let
			{a 1}
			(stack)
		)
	)
	{x 1}
)
```
Output:
```amalgam
[
	{}
	{x 1}
	{a 1}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `args`
#### Parameters
`[number stack_distance]`
#### Description
Evaluates to the top context of the stack, the current execution context, or scope stack, known as the arguments.  If `stack_distance` is specified, then it evaluates to the context that many layers up the stack.
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
(call
	(lambda
		(let
			(associate "bbb" 3)
			[
				(args)
				(args 1)
			]
		)
	)
	{x 1}
)
```
Output:
```amalgam
[
	{bbb 3}
	{x 1}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `get_type`
#### Parameters
`* node`
#### Description
Returns a node of the type corresponding to the node.
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
(get_type
	(lambda
		(+ 3 4)
	)
)
```
Output:
```amalgam
(+)
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `get_type_string`
#### Parameters
`* node`
#### Description
Returns a string that represents the type corresponding to the node.
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
(get_type_string
	(lambda
		(+ 3 4)
	)
)
```
Output:
```amalgam
"+"
```
Example:
```amalgam
(get_type_string "hello")
```
Output:
```amalgam
"string"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `set_type`
#### Parameters
`* node [string|* type]`
#### Description
Creates a copy of `node`, setting the type of the node of to `type`.  If `type` is a string, it will look that up as the type, or if `type` is a node that is not a string, it will set the type to match the top node of `type`.  It will convert opcode parameters as necessary.
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
(set_type
	(lambda
		(+ 3 4)
	)
	"-"
)
```
Output:
```amalgam
(- 3 4)
```
Example:
```amalgam
(sort
	(set_type
		(associate "a" 4 "b" 3)
		"list"
	)
)
```
Output:
```amalgam
[3 4 "a" "b"]
```
Example:
```amalgam
(sort
	(set_type
		(associate "a" 4 "b" 3)
		[]
	)
)
```
Output:
```amalgam
[3 4 "a" "b"]
```
Example:
```amalgam
(unparse
	(set_type
		["a" 4 "b" 3]
		"assoc"
	)
)
```
Output:
```amalgam
"{a 4 b 3}"
```
Example:
```amalgam
(call
	(set_type
		[1 0.5 "3.2" 4]
		"+"
	)
)
```
Output:
```amalgam
8.7
```
Example:
```amalgam
(set_type
	[
		(set_annotations
			(lambda
				(+ 3 4)
			)
			"react"
		)
	]
	"unordered_list"
)
```
Output:
```amalgam
(unordered_list
	
	#react
	(+ 3 4)
)
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `format`
#### Parameters
`* data string from_format string to_format [assoc from_params] [assoc to_params]`
#### Description
Converts data from `from_format` into `to_format`.  Supported language types are "number", "string", and "code", where code represents everything beyond number and string.  Beyond the supported language types, additional formats that are stored in a binary string.  The additional formats are "base16", "base64", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float32", "float64", ">int8", ">uint8", ">int16", ">uint16", ">int32", ">uint32", ">int64", ">uint64", ">float32", ">float64", "<int8", "<uint8", "<int16", "<uint16", "<int32", "<uint32", "<int64", "<uint64", "<float32", "<float64", "json", "yaml", "date", and "time" (though date and time are special cases).  Binary types starting with a "<" represent little endian, binary types starting with a ">" represent big endian, and binary types without either will be the endianness of the machine.  Binary types will be handled as strings.  The "date" type requires additional information.  Following "date" or "time" is a colon, followed by a standard strftime date or time format string.  If `from_params` or `to_params` are specified, then it will apply the appropriate from or to as appropriate.  If the format is either "string", "json", or "yaml", then the key "sort_keys" can be used to specify a boolean value, if true, then it will sort the keys, otherwise the default behavior is to emit the keys based on memory layout.  If the format is date or time, then the to or from params can be an assoc with "locale" as an optional key.  If date then "time_zone" is also allowed.  The locale is provided, then it will leverage operating system support to apply appropriate formatting, such as en_US.  Note that UTF-8 is assumed and automatically added to the locale.  If no locale is specified, then the default will be used.  If converting to or from dates, if "time_zone" is specified, it will use the standard time_zone name, if unspecified or empty string, it will assume the current time zone.
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
(map
	(lambda
		(format (current_value) "int8" "number")
	)
	(explode "abcdefg" 1)
)
```
Output:
```amalgam
[
	97
	98
	99
	100
	101
	102
	103
]
```
Example:
```amalgam
(format 65 "number" "int8")
```
Output:
```amalgam
"A"
```
Example:
```amalgam
(format
	(format -100 "number" "float64")
	"float64"
	"number"
)
```
Output:
```amalgam
-100
```
Example:
```amalgam
(format
	(format -100 "number" "float32")
	"float32"
	"number"
)
```
Output:
```amalgam
-100
```
Example:
```amalgam
(format
	(format 100 "number" "uint32")
	"uint32"
	"number"
)
```
Output:
```amalgam
100
```
Example:
```amalgam
(format
	(format 123456789 "number" ">uint32")
	"<uint32"
	"number"
)
```
Output:
```amalgam
365779719
```
Example:
```amalgam
(format
	(format 123456789 "number" ">uint32")
	">uint32"
	"number"
)
```
Output:
```amalgam
123456789
```
Example:
```amalgam
(format
	(format 14294967296 "number" "uint64")
	"uint64"
	"number"
)
```
Output:
```amalgam
14294967296
```
Example:
```amalgam
(format "A" "int8" "number")
```
Output:
```amalgam
65
```
Example:
```amalgam
(format -100 "float32" "number")
```
Output:
```amalgam
6.409830999309918e-10
```
Example:
```amalgam
(format 65 "uint8" "string")
```
Output:
```amalgam
"54"
```
Example:
```amalgam
(format 254 "uint8" "base16")
```
Output:
```amalgam
"32"
```
Example:
```amalgam
(format "AAA" "string" "base16")
```
Output:
```amalgam
"414141"
```
Example:
```amalgam
(format "414141" "base16" "string")
```
Output:
```amalgam
"AAA"
```
Example:
```amalgam
(format "Many hands make light work." "string" "base64")
```
Output:
```amalgam
"TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu"
```
Example:
```amalgam
(format "Many hands make light work.." "string" "base64")
```
Output:
```amalgam
"TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLg=="
```
Example:
```amalgam
(format "Many hands make light work..." "string" "base64")
```
Output:
```amalgam
"TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLi4="
```
Example:
```amalgam
(format "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu" "base64" "string")
```
Output:
```amalgam
"Many hands make light work."
```
Example:
```amalgam
(format "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLg==" "base64" "string")
```
Output:
```amalgam
"Many hands make light work.."
```
Example:
```amalgam
(format "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLi4=" "base64" "string")
```
Output:
```amalgam
"Many hands make light work..."
```
Example:
```amalgam
(format "[{\"a\" : 3, \"b\" : 4}, {\"c\" : \"c\"}]" "json" "code")
```
Output:
```amalgam
[
	{a 3 b 4}
	{c "c"}
]
```
Example:
```amalgam
(format
	[
		{a 3 b 4}
		{c "c" d (null)}
	]
	"code"
	"json"
	(null)
	{sort_keys .true}
)
```
Output:
```amalgam
"[{\"a\":3,\"b\":4},{\"c\":\"c\",\"d\":null}]"
```
Example:
```amalgam
(format
	{
		a 1
		b 2
		c 3
		d 4
		e ["a" "b" (null) .infinity]
	}
	"code"
	"yaml"
	(null)
	{sort_keys .true}
)
```
Output:
```amalgam
"a: 1\nb: 2\nc: 3\nd: 4\ne:\n  - a\n  - b\n  - \n  - .inf\n"
```
Example:
```amalgam
(format "a: 1" "yaml" "code")
```
Output:
```amalgam
{a 1}
```
Example:
```amalgam
(format 1591503779.1 "number" "date:%Y-%m-%d-%H.%M.%S")
```
Output:
```amalgam
"2020-06-07-00.22.59.1000000"
```
Example:
```amalgam
(format 1591503779 "number" "date:%F %T")
```
Output:
```amalgam
"2020-06-07 00:22:59"
```
Example:
```amalgam
(format "Feb 2014" "date:%b %Y" "number")
```
Output:
```amalgam
1391230800
```
Example:
```amalgam
(format "2014-Feb" "date:%Y-%h" "number")
```
Output:
```amalgam
1391230800
```
Example:
```amalgam
(format "02/2014" "date:%m/%Y" "number")
```
Output:
```amalgam
1391230800
```
Example:
```amalgam
(format 1591505665002 "number" "date:%F %T")
```
Output:
```amalgam
"-6053-05-28 00:24:29"
```
Example:
```amalgam
(format 1591330905 "number" "date:%F %T")
```
Output:
```amalgam
"2020-06-05 00:21:45"
```
Example:
```amalgam
(format 1591330905 "number" "date:%c %Z")
```
Output:
```amalgam
"06/05/20 00:21:45 EDT"
```
Example:
```amalgam
(format 1591330905 "number" "date:%S")
```
Output:
```amalgam
"45"
```
Example:
```amalgam
(format 1591330905 "number" "date:%Oe")
```
Output:
```amalgam
" 5"
```
Example:
```amalgam
(format 1591330905 "number" "date:%s")
```
Output:
```amalgam
" s"
```
Example:
```amalgam
(format 1591330905 "number" "date:%s%")
```
Output:
```amalgam
" s"
```
Example:
```amalgam
(format 1591330905 "number" "date:%a%b%c%d%e%f")
```
Output:
```amalgam
"FriJun06/05/20 00:21:4505 5 f"
```
Example:
```amalgam
(format
	"abcd"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "en_US"}
	{locale "es_ES"}
)
```
Output:
```amalgam
"jueves, ene. 01, 1970"
```
Example:
```amalgam
(format
	"2020-06-07"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "en_US"}
	{locale "etete123"}
)
```
Output:
```amalgam
"Sunday, Jun 07, 2020"
```
Example:
```amalgam
(format
	"2020-06-07"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "notalocale"}
	{locale "es_ES"}
)
```
Output:
```amalgam
"domingo, jun. 07, 2020"
```
Example:
```amalgam
(format "2020-06-07" "date:%Y-%m-%d" "number")
```
Output:
```amalgam
1591502400
```
Example:
```amalgam
(format "2020-06-07" "date:%Y-%m-%d" "date:%b %d, %Y")
```
Output:
```amalgam
"Jun 07, 2020"
```
Example:
```amalgam
(format
	"2020-06-07"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "en_US"}
	{locale "es_ES"}
)
```
Output:
```amalgam
"domingo, jun. 07, 2020"
```
Example:
```amalgam
(format "1970-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number")
```
Output:
```amalgam
664428
```
Example:
```amalgam
(format "1960-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number")
```
Output:
```amalgam
-314954772
```
Example:
```amalgam
(format
	(format "1960-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number")
	"number"
	"date:%Y-%m-%d %H.%M.%S"
)
```
Output:
```amalgam
"1960-01-08 11.33.48"
```
Example:
```amalgam
(format
	(+
		0.01
		(format "1960-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number")
	)
	"number"
	"date:%Y-%m-%d %H.%M.%S"
)
```
Output:
```amalgam
"1960-01-08 11.33.48.0100000"
```
Example:
```amalgam
(format "13:22:44" "time:%H:%M:%S" "number")
```
Output:
```amalgam
48164
```
Example:
```amalgam
(format
	"13:22:44"
	"time:%H:%M:%S"
	"number"
	{locale "en_US"}
)
```
Output:
```amalgam
48164
```
Example:
```amalgam
(format "10:22:44" "time:%H:%M:%S" "number")
```
Output:
```amalgam
37364
```
Example:
```amalgam
(format "10:22:44am" "time:%I:%M:%S%p" "number")
```
Output:
```amalgam
37364
```
Example:
```amalgam
(format "10:22:44.33am" "time:%I:%M:%S%p" "number")
```
Output:
```amalgam
37364.33
```
Example:
```amalgam
(format "10:22:44" "time:%I:%M:%S" "number")
```
Output:
```amalgam
0
```
Example:
```amalgam
(format "10:22:44" "time:%qqq:%qqq:%qqq" "number")
```
Output:
```amalgam
0
```
Example:
```amalgam
(format
	"13:22:44"
	"time:%H:%M:%S"
	"number"
	{locale "notalocale"}
)
```
Output:
```amalgam
48164
```
Example:
```amalgam
(format 48164 "number" "time:%H:%M:%S")
```
Output:
```amalgam
"13:22:44"
```
Example:
```amalgam
(format
	48164
	"number"
	"time:%I:%M:%S%p"
	(null)
	{locale "es_ES"}
)
```
Output:
```amalgam
"01:22:44PM"
```
Example:
```amalgam
(format 37364.33 "number" "time:%I:%M:%S%p")
```
Output:
```amalgam
"10:22:44.3300000AM"
```
Example:
```amalgam
(format 0 "number" "time:%I:%M:%S%p")
```
Output:
```amalgam
"12:00:00AM"
```
Example:
```amalgam
(format (null) "number" "time:%I:%M:%S%p")
```
Output:
```amalgam
"12:00:00AM"
```
Example:
```amalgam
(format .infinity "number" "time:%I:%M:%S%p")
```
Output:
```amalgam
"12:00:00AM"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

