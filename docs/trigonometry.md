### Opcode: `sin`
#### Parameters
`number theta`
#### Description
Evaluates to the sine of `theta`.
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
(sin 0.5)
```
Output:
```amalgam
0.479425538604203
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `asin`
#### Parameters
`number length`
#### Description
Evaluates to the arc sine (inverse sine) of `length`.
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
(sin 0.5)
```
Output:
```amalgam
0.479425538604203
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `cos`
#### Parameters
`number theta`
#### Description
Evaluates to the cosine of `theta`.
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
(cos 0.5)
```
Output:
```amalgam
0.8775825618903728
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `acos`
#### Parameters
`number length`
#### Description
Evaluates to the arc cosine (inverse cosine) of `length`.
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
(acos 0.5)
```
Output:
```amalgam
1.0471975511965979
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `tan`
#### Parameters
`number theta`
#### Description
Evaluates to the tangent of `theta`.
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
(tan 0.5)
```
Output:
```amalgam
0.5463024898437905
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `atan`
#### Parameters
`number num [number divisor]`
#### Description
Evaluates to the arc tangent (inverse tangent) of `num`.  If two numbers are provided, then it evaluates to the arc tangent of `num` / `divisor`.
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
(atan 0.5)
```
Output:
```amalgam
0.4636476090008061
```
Example:
```amalgam
(atan 0.5 0.5)
```
Output:
```amalgam
0.7853981633974483
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `sinh`
#### Parameters
`number z`
#### Description
Evaluates to the hyperbolic sine of `z`.
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
(sinh 0.5)
```
Output:
```amalgam
0.5210953054937474
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `asinh`
#### Parameters
`number x`
#### Description
Evaluates to the hyperbolic arc sine of `x`.
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
(asinh 0.5)
```
Output:
```amalgam
0.48121182505960347
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `cosh`
#### Parameters
`number z`
#### Description
Evaluates to the hyperbolic cosine of `z`.
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
(cosh 0.5)
```
Output:
```amalgam
1.1276259652063807
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `acosh`
#### Parameters
`number x`
#### Description
Evaluates to the hyperbolic arc cosine of `x`.
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
(acosh 1.5)
```
Output:
```amalgam
0.9624236501192069
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `tanh`
#### Parameters
`number z`
#### Description
Evaluates to the hyperbolic tangent on `z`.
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
(tanh 0.5)
```
Output:
```amalgam
0.46211715726000974
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `atanh`
#### Parameters
`number x`
#### Description
Evaluates to the hyperbolic arc tangent on `x`.
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
(atanh 0.5)
```
Output:
```amalgam
0.5493061443340549
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

