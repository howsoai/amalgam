### Opcode: `null`
#### Parameters
``
#### Description
Evaluates to the immediate null value and cannot have any parameters.  Null is returned any time there would be a not-a-number result (NaN), such as dividing zero by zero or adding null to a number, or when intermixed with string operations.
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
.null
```
Output:
```amalgam
.null
```
Example:
```amalgam
(lambda .null )
```
Output:
```amalgam
.null
```
Example:
```amalgam
(lambda
	
	#annotation
	.null
)
```
Output:
```amalgam
#annotation
.null
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `bool`
#### Parameters
``
#### Description
A boolean value that may hold true and false as `.true` and `.false` respectively.
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
.true
```
Output:
```amalgam
.true
```
Example:
```amalgam
.false
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `number`
#### Parameters
``
#### Description
A 64-bit floating point value.  Note that `.infinity` and `-.infinity` are used to denote infinite values and not-a-number is transformed into a null value.
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
1
```
Output:
```amalgam
1
```
Example:
```amalgam
1.5
```
Output:
```amalgam
1.5
```
Example:
```amalgam
6.02214076e+23
```
Output:
```amalgam
6.02214076e+23
```
Example:
```amalgam
.infinity
```
Output:
```amalgam
.infinity
```
Example:
```amalgam
(-
	(* 3 .infinity)
)
```
Output:
```amalgam
-.infinity
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `string`
#### Parameters
``
#### Description
A string.  Many opcodes assume UTF-8 formatted strings, but many, such as `format`, can work with any bytes.  Internally, the string is just a sequence of bytes, but when specifying a string to be parsed, `\` is the escape character and the values that can be escaped are null character as `\0`, `\` as `\\`, `"` as `\"`, tab as `\t`, newline as `\n`, and carriage return as `\r`.
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
"hello"
```
Output:
```amalgam
"hello"
```
Example:
```amalgam
"\tHello\n\"Hello\""
```
Output:
```amalgam
"\tHello\n\"Hello\""
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `list`
#### Parameters
`any node1 any node2 ...`
#### Description
Evaluates to a list with the parameters as elements.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the list itself, the current index, and the current value.  If `[]`'s are used instead of parenthesis, the keyword `list` may be omitted.  `[]` are considered identical to `(list)`.
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
["a" 1 "b"]
```
Output:
```amalgam
["a" 1 "b"]
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `unordered_list`
#### Parameters
`any node1 any node2 ...`
#### Description
Evaluates to the list specified by parameters as elements.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the unordered list itself, the current index, and the current value.  It operates like a list, except any operations that would normally consider a list's order.  For example, union, intersect, and mix, will consider the values unordered.
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
(unordered_list 4 4 5)
```
Output:
```amalgam
(unordered_list 4 4 5)
```
Example:
```amalgam
(unordered_list
	(unordered_list 4 4 5)
	(unordered_list 4 5 6)
)
```
Output:
```amalgam
(unordered_list
	(unordered_list 4 4 5)
	(unordered_list 4 5 6)
)
```
Example:
```amalgam
(=
	(unordered_list 4 4 5)
	(unordered_list 4 4 5)
)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(=
	(unordered_list 4 4 5)
	(unordered_list 4 4 6)
)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(=
	(unordered_list 4 4 5)
	(unordered_list 4 4 5)
)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(=
	(set_type
		(range 0 100)
		"unordered_list"
	)
	(set_type
		(reverse
			(range 0 100)
		)
		"unordered_list"
	)
)
```
Output:
```amalgam
.true
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `assoc`
#### Parameters
`[any index1] [any value1] [any index2] [any value2] ...`
#### Description
Evaluates to an associative list, where each pair of parameters (e.g., `index1` and `value1`) comprises a index-value pair.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the assoc, the current index, and the current value.  If any index does not have reserved characters or whitespace, then quotes are optional; if whitespace or reserved characters are present, then quotes are required.  If `{}`'s are used instead of parenthesis, the keyword assoc may be omitted.  `{}` are considered identical to `(assoc)`
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
(unparse
	{b 2 c 3}
)
```
Output:
```amalgam
"{b 2 c 3}"
```
Example:
```amalgam
(unparse
	{.null 0 (+ 1 2) 3}
)
```
Output:
```amalgam
"{.null 0 (+ 1 2) 3}"
```

[Amalgam Opcodes](./opcodes.md)

