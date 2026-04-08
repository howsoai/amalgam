### Opcode: `+`
#### Parameters
`[number x1] [number x2] ... [number xN]`
#### Description
Sums all numbers.
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
(+ 1 2 3 4)
```
Output:
```amalgam
10
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `-`
#### Parameters
`[number x1] [number x2] ... [number xN]`
#### Description
Evaluates to `x1` - `x2` - ... - `xN`.  If only one parameter is passed, then it is treated as negative
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
(- 1 2 3 4)
```
Output:
```amalgam
-8
```
Example:
```amalgam
(- 3)
```
Output:
```amalgam
-3
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `*`
#### Parameters
`[number x1] [number x2] ... [number xN]`
#### Description
Evaluates to the product of all numbers.
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
(* 1 2 3 4)
```
Output:
```amalgam
24
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `/`
#### Parameters
`[number x1] [number x2] ... [number xN]`
#### Description
Evaluates to `x1` / `x2` / ... / `xN`.
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
(/ 1.0 2 3 4)
```
Output:
```amalgam
0.041666666666666664
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `mod`
#### Parameters
`[number x1] [number x2] ... [number xN]`
#### Description
Evaluates the modulus of `x1` % `x2` % ... % `xN`.
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
(mod 1 2 3 4)
```
Output:
```amalgam
1
```
Example:
```amalgam
(mod 5 3)
```
Output:
```amalgam
2
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `get_digits`
#### Parameters
`number value [number base] [number start_digit] [number end_digit] [bool relative_to_zero]`
#### Description
Evaluates to a list of the number of each digit of `value` for the given `base`.  If `base` is omitted, 10 is the default.  The parameters `start_digit` and `end_digit` can be used to get a specific set of digits, but can also be infinite or null to catch all the digits on one side of the number.  The interpretation of `start_digit` and `end_digit` are with respect to relative_to_zero, which defaults to true.  If relative_to_zero is true, then the digits are indexed from their distance to zero, such as "5 4 3 2 1 0 . -1 -2".  If relative_to_zero is false, then the digits are indexed from their most significant digit, such as "0 1 2 3 4 5 . 6  7".  The default values of `start_digit` and `end_digit` are the most and least significant digits respectively.
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
(get_digits 1234567.8 10)
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
	0
	0
	0
]
```
Example:
```amalgam
(get_digits 1234567.89 10)
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
	8
	9
	9
]
```
Example:
```amalgam
(get_digits 1234.5678 10 -1 -.infinity)
```
Output:
```amalgam
[
	5
	6
	7
	8
	0
	0
	0
	0
	0
	0
	0
]
```
Example:
```amalgam
(get_digits 7 2 .infinity 0)
```
Output:
```amalgam
[1 1 1]
```
Example:
```amalgam
(get_digits 16 2 .infinity 0)
```
Output:
```amalgam
[1 0 0 0 0]
```
Example:
```amalgam
(get_digits 24 4 .infinity 0)
```
Output:
```amalgam
[1 2 0]
```
Example:
```amalgam
(get_digits 40 3 .infinity 0)
```
Output:
```amalgam
[1 1 1 1]
```
Example:
```amalgam
(get_digits 16 2 .infinity 0)
```
Output:
```amalgam
[1 0 0 0 0]
```
Example:
```amalgam
(get_digits 16 8 .infinity 0)
```
Output:
```amalgam
[2 0]
```
Example:
```amalgam
(get_digits 3 2 5 0)
```
Output:
```amalgam
[0 0 0 0 1 1]
```
Example:
```amalgam
(get_digits 1.5 1.5 .infinity 0)
```
Output:
```amalgam
[1 0]
```
Example:
```amalgam
(get_digits 3.75 1.5 .infinity -7)
```
Output:
```amalgam
[
	1
	0
	0
	0
	0
	0
	1
	0
	0
	0
	1
]
```
Example:
```amalgam
(get_digits 1234567.8 10 0 4 .false)
```
Output:
```amalgam
[1 2 3 4 5]
```
Example:
```amalgam
(get_digits 1234567.8 10 4 8 .false)
```
Output:
```amalgam
[5 6 7 8 0]
```
Example:
```amalgam
(get_digits 1.2345678e+100 10 0 4 .false)
```
Output:
```amalgam
[1 2 3 4 5]
```
Example:
```amalgam
(get_digits 1.2345678e+100 10 4 8 .false)
```
Output:
```amalgam
[5 6 7 8 0]
```
Example:
```amalgam
;should print empty list for these
(get_digits 0 2.714 1 3 .false)
```
Output:
```amalgam
[]
```
Example:
```amalgam
(get_digits 0 2.714 1 3 .true)
```
Output:
```amalgam
[]
```
Example:
```amalgam
(get_digits 0 10 0 10 .false)
```
Output:
```amalgam
[]
```
Example:
```amalgam
;4 followed by zeros
(get_digits 0.4 10 0 10 .false)
```
Output:
```amalgam
[
	4
	0
	0
	0
	0
	0
	0
	0
	0
	0
	0
]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `set_digits`
#### Parameters
`number value [number base] [list|number|null digits] [number start_digit] [number end_digit] [bool relative_to_zero]`
#### Description
Evaluates to `value` having each of the values in the list of `digits` replace each of the relative digits in `value` for the given base.  If a digit is null in `digits`, then that digit is not set.  If `base` is omitted, 10 is the default.  The parameters `start_digit` and `end_digit` can be used to get a specific set of digits, but can also be infinite or null to catch all the digits on one side of the number.  The interpretation of `start_digit` and `end_digit` are with respect to `relative_to_zero`, which defaults to true.  If `relative_to_zero` is true, then the digits are indexed from their distance to zero, such as "5 4 3 2 1 0 . -1 -2".  If `relative_to_zer`o is false, then the digits are indexed from their most significant digit, such as "0 1 2 3 4 5 . 6  7".  The default values of `start_digit` and `end_digit` are the most and least significant digits respectively.
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
(set_digits
	1234567.8
	10
	[5 5 5]
)
```
Output:
```amalgam
5554567.8
```
Example:
```amalgam
(set_digits
	1234567.8
	10
	[5 5 5]
	-1
	-.infinity
)
```
Output:
```amalgam
1234567.555
```
Example:
```amalgam
(set_digits
	7
	2
	[1 0 0]
	.infinity
	0
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(set_digits
	1.5
	1.5
	[1]
	.infinity
	0
)
```
Output:
```amalgam
1.5
```
Example:
```amalgam
(set_digits
	1.5
	1.5
	[2]
	.infinity
	0
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(set_digits
	1.5
	1.5
	[1 0]
	1
	0
)
```
Output:
```amalgam
1.5
```
Example:
```amalgam
(set_digits
	1234567.8
	10
	[5 5 5]
	10
)
```
Output:
```amalgam
55501234567.8
```
Example:
```amalgam
(set_digits
	1.5
	1.5
	[1 0 0]
	2
	0
)
```
Output:
```amalgam
2.25
```
Example:
```amalgam
(set_digits
	1234567.8
	10
	[5 5 5 5 5]
	0
	4
	.false
)
```
Output:
```amalgam
5555567.8
```
Example:
```amalgam
(set_digits
	1234567.8
	10
	[5 5 5 5 5]
	4
	8
	.false
)
```
Output:
```amalgam
1234555.55
```
Example:
```amalgam
(set_digits
	1.2345678e+100
	10
	[5 5 5 5 5]
	0
	4
	.false
)
```
Output:
```amalgam
5.555567800000001e+100
```
Example:
```amalgam
(set_digits
	1.2345678e+100
	10
	[5 5 5 5 5]
	4
	8
	.false
)
```
Output:
```amalgam
1.2345555499999999e+100
```
Example:
```amalgam
(set_digits
	1.2345678e+100
	10
	[5 (null) 5 (null) 5]
	4
	8
	.false
)
```
Output:
```amalgam
1.23456585e+100
```
Example:
```amalgam
;these should all print (list 1 0 1)
(get_digits
	(set_digits
		1234567.8
		10
		[1 0 1 0]
		2
		5
		.false
	)
	10
	2
	5
	.false
)
```
Output:
```amalgam
[1 0 1 0]
```
Example:
```amalgam
(get_digits
	(set_digits
		1234567.8
		2
		[1 0 1 0]
		2
		5
		.false
	)
	2
	2
	5
	.false
)
```
Output:
```amalgam
[1 0 1 0]
```
Example:
```amalgam
(get_digits
	(set_digits
		1234567.8
		3.1
		[1 0 1 0]
		2
		5
		.false
	)
	3.1
	2
	5
	.false
)
```
Output:
```amalgam
[1 0 1 0]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `floor`
#### Parameters
`number x`
#### Description
Evaluates to the mathematical floor of x.
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
(floor 1.5)
```
Output:
```amalgam
1
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `ceil`
#### Parameters
`number x`
#### Description
Evaluates to the mathematical ceiling of x.
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
(ceil 1.5)
```
Output:
```amalgam
2
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `round`
#### Parameters
`number x [number significant_digits] [number significant_digits_after_decimal]`
#### Description
Rounds the value `x` and evaluates to the new value.  If only one parameter is specified, it rounds to the nearest integer.  If `significant_digits` is specified, then it rounds to the specified number of significant digits.  If `significant_digits_after_decimal` is specified, then it ensures that `x` will be rounded at least to the number of decimal points past the integer as specified, and takes priority over `significant_digits`.
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
(round 12.7)
```
Output:
```amalgam
13
```
Example:
```amalgam
(round 12.7 1)
```
Output:
```amalgam
10
```
Example:
```amalgam
(round 123.45678 5)
```
Output:
```amalgam
123.46
```
Example:
```amalgam
(round 123.45678 2)
```
Output:
```amalgam
120
```
Example:
```amalgam
(round 123.45678 2 2)
```
Output:
```amalgam
120
```
Example:
```amalgam
(round 123.45678 6 2)
```
Output:
```amalgam
123.46
```
Example:
```amalgam
(round 123.45678 4 0)
```
Output:
```amalgam
123
```
Example:
```amalgam
(round 123.45678 0 0)
```
Output:
```amalgam
0
```
Example:
```amalgam
(round 1.2345678 2 4)
```
Output:
```amalgam
1.2
```
Example:
```amalgam
(round 1.2345678 0 4)
```
Output:
```amalgam
0
```
Example:
```amalgam
(round 0.012345678 2 4)
```
Output:
```amalgam
0.012
```
Example:
```amalgam
(round 0.012345678 4 2)
```
Output:
```amalgam
0.01
```
Example:
```amalgam
(round 0.012345678 0 0)
```
Output:
```amalgam
0
```
Example:
```amalgam
(round 0.012345678 100 100)
```
Output:
```amalgam
0.012345678
```
Example:
```amalgam
(round 0.6 2)
```
Output:
```amalgam
0.6
```
Example:
```amalgam
(round 0.6 32 2)
```
Output:
```amalgam
0.6
```
Example:
```amalgam
(round
	(/ 1 3)
	32
	1
)
```
Output:
```amalgam
0.3
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `abs`
#### Parameters
`number x`
#### Description
Evaluates to absolute value of `x`
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
(abs -0.5)
```
Output:
```amalgam
0.5
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `max`
#### Parameters
`[number x1] [number x2] ... [number xN]`
#### Description
Evaluates to the maximum of all of parameters.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(max 0.5 1 7 9 -5)
```
Output:
```amalgam
9
```
Example:
```amalgam
(max (null) 4 8)
```
Output:
```amalgam
8
```
Example:
```amalgam
(max (null))
```
Output:
```amalgam
(null)
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `min`
#### Parameters
`[number x1] [number x2] ... [number xN]`
#### Description
Evaluates to the minimum of all of the numbers.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(min 0.5 1 7 9 -5)
```
Output:
```amalgam
-5
```
Example:
```amalgam
(min (null) 4 8)
```
Output:
```amalgam
4
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `index_max`
#### Parameters
`[[number x1] [number x2] [number x3] ... [number xN]] | assoc|list values`
#### Description
If given multiple arguments, returns a list of the indices of the arguments with the maximum value.  If given a single argument that is an assoc, it returns the a list of keys associated with the maximum values; the list will be a single value unless there are ties.  If given a single argument that is a list, it returns a list of list indices with the maximum value.
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
(index_max 0.5 -12 3 5 7)
```
Output:
```amalgam
[4]
```
Example:
```amalgam
(index_max
	[1 1 3 2 1 3]
)
```
Output:
```amalgam
[2 5]
```
Example:
```amalgam
(index_max (null) 34 -66)
```
Output:
```amalgam
[1]
```
Example:
```amalgam
(index_max (null) (null) (null))
```
Output:
```amalgam
(null)
```
Example:
```amalgam
(index_max
	{1 2 3 5 tomato 4444}
)
```
Output:
```amalgam
["tomato"]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `index_min`
#### Parameters
`[[number x1] [number x2] [number x3] ... [number xN]] | assoc values | list values`
#### Description
If given multiple arguments, returns a list of the indices of the arguments with the minimum value.  If given a single argument that is an assoc, it returns the a list of keys associated with the minimum values; the list will be a single value unless there are ties.  If given a single argument that is a list, it returns a list of list indices with the minimum value.
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
(index_min 0.5 -12 3 5 7)
```
Output:
```amalgam
[1]
```
Example:
```amalgam
(index_min
	[1 1 3 2 1 3]
)
```
Output:
```amalgam
[0 1 4]
```
Example:
```amalgam
(index_min (null) 34 -66)
```
Output:
```amalgam
[2]
```
Example:
```amalgam
(index_min
	{1 2 3 5 tomato 4444}
)
```
Output:
```amalgam
[1]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

