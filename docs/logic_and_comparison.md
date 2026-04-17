### Opcode: `and`
#### Parameters
`[bool condition1] [bool condition2] ... [bool conditionN]`
#### Description
If all condition expressions are true, evaluates to `conditionN`.  Otherwise evaluates to false.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): conditional
#### Examples
Example:
```amalgam
(and 1 4.8 "true" .true)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(and 1 0 "true" .true)
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `or`
#### Parameters
`[bool condition1] [bool condition2] ... [bool conditionN]`
#### Description
If all condition expressions are false, evaluates to false.  Otherwise evaluates to the first condition that is true.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): conditional
#### Examples
Example:
```amalgam
(or .true .false)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(or .false .false .false)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(or 1 0 "true")
```
Output:
```amalgam
1
```
Example:
```amalgam
(or 1 4.8 "true")
```
Output:
```amalgam
1
```
Example:
```amalgam
(or 0 0 "")
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `xor`
#### Parameters
`[bool condition1] [bool condition2] ... [bool conditionN]`
#### Description
If an even number of condition expressions are true, evaluates to false.  Otherwise evaluates to true.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(xor .true .true)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(xor .true .false)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(xor 1 4.8 "true")
```
Output:
```amalgam
.true
```
Example:
```amalgam
(xor 1 0 "true")
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `not`
#### Parameters
`bool condition`
#### Description
Evaluates to false if `condition` is true, true if false.
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
(not .true)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(not .false)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(not 1)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(not "")
```
Output:
```amalgam
.true
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `=`
#### Parameters
`[* node1] [* node2] ... [* nodeN]`
#### Description
Evaluates to true if the value of all nodes are equal, false otherwise. Values of null are considered equal, and any complex data structures will be traversed evaluated for deep equality.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(= 4 4 5)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(= 4 4 4)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(=
	(sqrt -1)
	.null
)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(= .null .null)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(= .infinity .infinity)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(= .infinity -.infinity)
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `!=`
#### Parameters
`[* node1] [* node2] ... [* nodeN]`
#### Description
Evaluates to true if no two values are equal, false otherwise.  Values of null are considered equal, and any complex data structures will be traversed evaluated for deep equality.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(!= 4 4)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(!= 4 5)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(!= 4 4 5)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(!= 4 4 4)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(!= 4 4 "hello" 4)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(!= 4 4 4 1 3 "hello")
```
Output:
```amalgam
.false
```
Example:
```amalgam
(!=
	1
	2
	3
	4
	5
	6
	"hello"
)
```
Output:
```amalgam
.true
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `<`
#### Parameters
`[* node1] [* node2] ... [* nodeN]`
#### Description
Evaluates to true if all values are in strict increasing order, false otherwise.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(< 4 5)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(< 4 4)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(< 4 5 6)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(< 4 5 6 5)
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `<=`
#### Parameters
`[* node1] [* node2] ... [* nodeN]`
#### Description
Evaluates to true if all values are in nondecreasing order, false otherwise.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(<= 4 5)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(<= 4 4)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(<= 4 5 6)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(<= 4 5 6 5)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(<= .null 2)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(<= 2 .null)
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `>`
#### Parameters
`[* node1] [* node2] ... [* nodeN]`
#### Description
Evaluates to true if all values are in strict decreasing order, false otherwise.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(> 6 5)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(> 4 4)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(> 6 5 4)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(> 6 5 4 5)
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `>=`
#### Parameters
`[* node1] [* node2] ... [* nodeN]`
#### Description
Evaluates to true if all values are in nonincreasing order, false otherwise.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(>= 6 5)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(>= 4 4)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(>= 6 5 4)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(>= 6 5 4 5)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(>= .null 2)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(>= 2 .null)
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `~`
#### Parameters
`[* node1] [* node2] ... [* nodeN]`
#### Description
Evaluates to true if all values are of the same data type, false otherwise.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(~ 1 4 5)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(~ 1 4 "a")
```
Output:
```amalgam
.false
```

[Amalgam Opcodes](./opcodes.md)

### Opcode: `!~`
#### Parameters
`[* node1] [* node2] ... [* nodeN]`
#### Description
Evaluates to true if no two values are of the same data types, false otherwise.
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
(!~
	"true"
	"false"
	[3 2]
)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(!~
	"true"
	1
	[3 2]
)
```
Output:
```amalgam
.true
```

[Amalgam Opcodes](./opcodes.md)

