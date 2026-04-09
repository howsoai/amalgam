### Opcode: `exp`
#### Parameters
`number x`
#### Description
e^x
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
(exp 0.5)
```
Output:
```amalgam
1.6487212707001282
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `log`
#### Parameters
`number x [number base]`
#### Description
Evaluates to the logarithm of `x`.  If `base` is specified, uses that base, otherwise defaults to natural log.
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
(log 0.5)
```
Output:
```amalgam
-0.6931471805599453
```
Example:
```amalgam
(log 0.5 2)
```
Output:
```amalgam
-1
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `erf`
#### Parameters
`number errno`
#### Description
Evaluates to the error function on `errno`.
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
(erf 0.5)
```
Output:
```amalgam
0.5204998778130465
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `tgamma`
#### Parameters
`number z`
#### Description
Evaluates the true (complete) gamma function on `z`.
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
(tgamma 0.5)
```
Output:
```amalgam
1.772453850905516
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `lgamma`
#### Parameters
`number z`
#### Description
Evaluates the log-gamma function function on `z`.
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
(lgamma 0.5)
```
Output:
```amalgam
0.5723649429247001
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `sqrt`
#### Parameters
`number x`
#### Description
Returns the square root of `x`.
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
(sqrt 0.5)
```
Output:
```amalgam
0.7071067811865476
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `pow`
#### Parameters
`number base number exponent`
#### Description
Returns `base` raised to the `exponent` power.
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
(pow 0.5 2)
```
Output:
```amalgam
0.25
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `dot_product`
#### Parameters
`list|assoc x1 list|assoc x2`
#### Description
Evaluates to the sum of all corresponding element-wise products of `x1` and `x2`.
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
(dot_product
	[0.5 0.25 0.25]
	[4 8 8]
)
```
Output:
```amalgam
6
```
Example:
```amalgam
(dot_product
	(associate "a" 0.5 "b" 0.25 "c" 0.25)
	(associate "a" 4 "b" 8 "c" 8)
)
```
Output:
```amalgam
6
```
Example:
```amalgam
(dot_product
	(associate 0 0.5 1 0.25 2 0.25)
	[4 8 8]
)
```
Output:
```amalgam
6
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `normalize`
#### Parameters
`list|assoc values [number p]`
#### Description
Evaluates to a container of the values with the elements normalized, where `p` represents the order of the Lebesgue space to normalize the vector (e.g., 1 is Manhattan or surprisal space, 2 is Euclidean) and defaults to 1.
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
(normalize
	[0.5 0.5 0.5 0.5]
)
```
Output:
```amalgam
[0.25 0.25 0.25 0.25]
```
Example:
```amalgam
(normalize
	[0.5 0.5 0.5 .infinity]
)
```
Output:
```amalgam
[0 0 0 1]
```
Example:
```amalgam
(normalize
	{
		a 1
		b 1
		c 1
		d 1
	}
	2
)
```
Output:
```amalgam
{
	a 0.5
	b 0.5
	c 0.5
	d 0.5
}
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `mode`
#### Parameters
`list|assoc values [list|assoc weights]`
#### Description
Evaluates to mode of the `values`.  If `values` is an assoc, it will return the key.  If `weights` is specified and both `values` and `weights` are lists, then the corresponding elements will be weighted by `weights`.  If weights is specified and is an assoc, then each value will be looked up in the `weights`.
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
(mode
	[1 1 2 3 4 5]
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(mode
	[
		1
		1
		2
		3
		4
		5
		5
		5
	]
)
```
Output:
```amalgam
5
```
Example:
```amalgam
(mode
	[
		1
		1
		[]
		[]
		[]
		{}
		{}
	]
)
```
Output:
```amalgam
[]
```
Example:
```amalgam
(mode
	[
		1
		1
		2
		3
		4
		5
		(null)
	]
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(mode
	[1 1 2 3 4 5]
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(mode
	[1 1 2 3 4 5]
	[0.5 0.1 0.1 0.1 0.1]
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(mode
	{
		a 1
		b 1
		c 3
		d 4
		e 5
	}
	{
		a 0.5
		b 0.1
		c 0.1
		d 0.1
		e 0.1
	}
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(mode
	[1 1 2 3 4 5]
	{0 0.75 4 0.125}
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(mode
	{
		0 1
		1 1
		2 2
		3 3
		4 4
		5 5
	}
	[0.75 0 0 0 0.125]
)
```
Output:
```amalgam
1
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `quantile`
#### Parameters
`list|assoc values number quantile [list|assoc weights]`
#### Description
Evaluates to the quantile of the `values` specified by `quantile` ranging from 0 to 1.  If `weights` is specified and both `values` and `weights` are lists, then the corresponding elements will be weighted by `weights`.  If `weights` is specified and is an assoc, then each value will be looked up in the `weights`.
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
(quantile
	[1 2 3 4 5]
	0.5
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(quantile
	[1 2 3 4 5 (null)]
	0.5
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(quantile
	[1 2 3 4 5]
	0.5
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(quantile
	[1 2 3 4 5]
	0.5
	[0.5 0.1 0.1 0.1 0.1]
)
```
Output:
```amalgam
1.6666666666666667
```
Example:
```amalgam
(quantile
	{
		a 1
		b 2
		c 3
		d 4
		e 5
	}
	0.5
	{
		a 0.5
		b 0.1
		c 0.1
		d 0.1
		e 0.1
	}
)
```
Output:
```amalgam
1.6666666666666667
```
Example:
```amalgam
(quantile
	[1 2 3 4 5]
	0.5
	{0 0.75 4 0.125}
)
```
Output:
```amalgam
1.5714285714285716
```
Example:
```amalgam
(quantile
	{
		0 1
		1 2
		2 3
		3 4
		4 5
		5 (null)
	}
	0.5
	[0.75 0 0 0 0.125]
)
```
Output:
```amalgam
1.1666666666666667
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `generalized_mean`
#### Parameters
`list|assoc values [number p] [list|assoc weights] [number center] [bool calculate_moment] [bool absolute_value]`
#### Description
Evaluates to the generalized mean of the `values`.  If `p` is specified (which defaults to 1), it is the parameter that can control the type of mean from minimum (negative infinity) to harmonic mean (-1) to geometric mean (0) to arithmetic mean (1) to maximum (infinity).  If `weights` are specified, it uses those when calculating the corresponding values for the generalized mean.  If `center` is specified, calculations will use that as central point, and the default center is is 0.0.  If `calculate_moment` is true, which defaults to false, then the results will not be raised to 1/`p` at the end.  If `absolute_value` is true, which defaults to false, the differences will take the absolute value.  Various parameterizations of generalized_mean can be used to compute moments about the mean, especially setting the calculate_moment parameter to true and using the mean as the center.
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
(generalized_mean
	[1 2 3 4 5]
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(generalized_mean
	[1 2 3 4 5 (null)]
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(generalized_mean
	[1 2 3 4 5]
	2
)
```
Output:
```amalgam
3.3166247903554
```
Example:
```amalgam
(generalized_mean
	[1 2 3 4 5]
	1
	[0.5 0.1 0.1 0.1 0.1]
)
```
Output:
```amalgam
2.111111111111111
```
Example:
```amalgam
(generalized_mean
	{
		a 1
		b 2
		c 3
		d 4
		e 5
	}
	1
	{
		a 0.5
		b 0.1
		c 0.1
		d 0.1
		e 0.1
	}
)
```
Output:
```amalgam
2.111111111111111
```
Example:
```amalgam
(generalized_mean
	[1 2 3 4 5]
	1
	{0 0.75 4 0.125}
)
```
Output:
```amalgam
1.5714285714285714
```
Example:
```amalgam
(generalized_mean
	{
		0 1
		1 2
		2 3
		3 4
		4 5
		5 (null)
	}
	1
	[0.75 0 0 0 0.125]
)
```
Output:
```amalgam
1.5714285714285714
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `generalized_distance`
#### Parameters
`list|assoc|* vector1 [list|assoc|* vector2] [number p_value] [list|assoc|assoc of assoc|number weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc|number deviations] [list value_names] [list|string weights_selection_features] [bool surprisal_space]`
#### Description
Computes the generalized norm between `vector1` and `vector2` (or an equivalent zero vector if unspecified) using the numerical distance or edit distance as appropriate.  The parameter `value_names`, if specified as a list of the names of the values, will transform via unzipping any assoc into a list for the respective parameter in the order of the `value_names`, or if a number will use the number repeatedly for every element.  If any vector value is null or any of the differences between `vector1` and `vector2` evaluate to null, then it will compute a corresponding maximum distance value based on the properties of the feature.  If `surprisal_space` is true, which defaults to false, it will perform all computations in surprisal space.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.
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
(generalized_distance
	(map
		10000
		(range 0 200)
	)
	(null)
	0.01
)
```
Output:
```amalgam
2.0874003024080013e+234
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[0 2 3]
	0.01
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	2
)
```
Output:
```amalgam
5
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	-.infinity
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[0 2 3]
	0.01
	[0.3333 0.3333 0.3333]
)
```
Output:
```amalgam
1.9210176984148622e-48
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	2
	[1 1]
)
```
Output:
```amalgam
5
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	2
	[0.5 0.5]
)
```
Output:
```amalgam
3.5355339059327378
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	1
	[0.5 0.5]
)
```
Output:
```amalgam
3.5
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	0.5
	[0.5 0.5]
)
```
Output:
```amalgam
3.482050807568877
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	0.1
	[0.5 0.5]
)
```
Output:
```amalgam
3.467687001077147
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	0.01
	[0.5 0.5]
)
```
Output:
```amalgam
3.4644599990846436
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	0.001
	[0.5 0.5]
)
```
Output:
```amalgam
3.4641374518767565
```
Example:
```amalgam
(generalized_distance
	[3 4]
	(null)
	0
	[0.5 0.5]
)
```
Output:
```amalgam
3.4641016151377544
```
Example:
```amalgam
(generalized_distance
	[(null) 4]
	(null)
	2
	[1 1]
)
```
Output:
```amalgam
.infinity
```
Example:
```amalgam
(generalized_distance
	[(null) 4]
	(null)
	0
	[1 1]
)
```
Output:
```amalgam
.infinity
```
Example:
```amalgam
(generalized_distance
	[(null) 4]
	(null)
	2
	[0.5 0.5]
)
```
Output:
```amalgam
.infinity
```
Example:
```amalgam
(generalized_distance
	[(null) 4]
	(null)
	0
	[0.5 0.5]
)
```
Output:
```amalgam
.infinity
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[10 2 4]
	1
	(null)
	["nominal_number"]
	[1]
)
```
Output:
```amalgam
2
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[10 2 10]
	1
	(null)
	["nominal_number"]
	[1]
)
```
Output:
```amalgam
8
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[10 2 10]
	1
	(null)
	["nominal_number"]
	[1]
)
```
Output:
```amalgam
8
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[10 2 4]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number"]
	[1]
)
```
Output:
```amalgam
0.6666
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number"]
	[1]
)
```
Output:
```amalgam
2.6664
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number"]
	[1]
)
```
Output:
```amalgam
2.6664
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number" "continuous_number_cyclic" "continuous_number_cyclic"]
	[1 360 12]
)
```
Output:
```amalgam
1.9997999999999998
```
Example:
```amalgam
(generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number"]
	[1.1]
	[0.25 180 -12]
)
```
Output:
```amalgam
92.57407500000001
```
Example:
```amalgam
(generalized_distance
	[4 4 (null)]
	[2 (null) (null)]
	2
	[1 0 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
	[0.1 0.1 0.1]
)
```
Output:
```amalgam
2.227195548101088
```
Example:
```amalgam
(generalized_distance
	[4 4 (null)]
	[2 (null) (null)]
	2
	[1 0 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
)
```
Output:
```amalgam
2.23606797749979
```
Example:
```amalgam
(generalized_distance
	[4 4 (null) 4]
	[2 (null) (null) 2]
	2
	[1 0 1 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
	[0.1 0.1 0.1 0.1]
)
```
Output:
```amalgam
2.9933927271513525
```
Example:
```amalgam
(generalized_distance
	[4 4 (null) 4]
	[2 (null) (null) 2]
	2
	[1 0 1 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
)
```
Output:
```amalgam
3
```
Example:
```amalgam
(generalized_distance
	[4 4 4 4 4]
	[2 (null) 2 2 2]
	1
	[1 0 1 1 1]
)
```
Output:
```amalgam
(null)
```
Example:
```amalgam
(generalized_distance
	[4 4 4]
	[2 2 2]
	1
	{x 1 y 1 z 1}
	{x "nominal_number" y "continuous_number" z "continuous_number"}
	{z 5}
	(null)
	(null)
	(null)
	["x" "y" "z"]
)
```
Output:
```amalgam
6
```
Example:
```amalgam
(generalized_distance
	[4 4 (null)]
	[2 2 (null)]
	1
	[1 1 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(generalized_distance
	[4 4 4 4]
	[2 2 2 (null)]
	0
	[1 1 1 1]
	["continuous_number" "nominal_number" "nominal_number" "continuous_number"]
	[(null) 5 5 (null)]
	[
		[0 2]
		(null)
		(null)
		[0 2]
	]
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(generalized_distance
	[4 "s" "s" 4]
	[2 "s" 2 (null)]
	1
	[1 1 1 1]
	["continuous_number" "nominal_string" "nominal_string" "continuous_number"]
	[(null) 5 5 (null)]
	[
		[0 1]
		(null)
		(null)
		[0 1]
	]
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(generalized_distance
	[
		[1 2 3 4 5]
		"s"
	]
	[
		[1 2 3]
		"s"
	]
	1
	[1 1]
	["continuous_code" "nominal_string"]
	[0 5]
)
```
Output:
```amalgam
2
```
Example:
```amalgam
(generalized_distance
	[
		[1.5 2 3 4 5]
		"s"
	]
	[
		[1 2 3]
		"s"
	]
	1
	[1 1]
	["continuous_code" "nominal_string"]
	[0 5]
)
```
Output:
```amalgam
3.3255881193876142
```
Example:
```amalgam
(generalized_distance
	[1 1]
	[1 1]
	1
	[1 1]
	["continuous_number" "continuous_number"]
	(null)
	[0.5 0.5]
	(null)
	(null)
	.true
)
```
Output:
```amalgam
0
```
Example:
```amalgam
(generalized_distance
	[1 1]
	[1 1]
	1
	[1 1]
	["nominal_number" "nominal_number"]
	(null)
	[0.5 0.5]
	(null)
	(null)
	.true
)
```
Output:
```amalgam
0
```
Example:
```amalgam
(generalized_distance
	[1 1]
	[2 2]
	1
	[1 1]
	["continuous_number" "continuous_number"]
	(null)
	[0.5 0.5]
	(null)
	(null)
	.true
)
```
Output:
```amalgam
1.6766764161830636
```
Example:
```amalgam
(generalized_distance
	[1 1]
	[2 2]
	1
	[1 1]
	["nominal_number" "nominal_number"]
	[2 2]
	[0.25 0.25]
	(null)
	(null)
	.true
)
```
Output:
```amalgam
2.197224577336219
```
Example:
```amalgam
(generalized_distance
	["b"]
	
	;vector 1
	["c"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		{
			a {a 0.00744879 b 0.996275605 c 0.996275605}
			b {a 0.501736111 b 0.501736111 c 0.996527778}
			c {a 0.996539792 b 0.996539792 c 0.006920415}
		}
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
)
```
Output:
```amalgam
4.966335099422683
```
Example:
```amalgam
(generalized_distance
	["b"]
	
	;vector 1
	["a"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		{
			a {a 0.00744879 b 0.996275605 c 0.996275605}
			b {a 0.501736111 b 0.501736111 c 0.996527778}
			c {a 0.996539792 b 0.996539792 c 0.006920415}
		}
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
)
```
Output:
```amalgam
0
```
Example:
```amalgam
(generalized_distance
	["b"]
	
	;vector 1
	["q"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		{
			a {a 0.00744879 b 0.996275605 c 0.996275605}
			b [
					{a 0.501736111 b 0.501736111 c 0.996527778}
					0.8
				]
			c {a 0.996539792 b 0.996539792 c 0.006920415}
		}
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
)
```
Output:
```amalgam
0.9128124677208268
```
Example:
```amalgam
(generalized_distance
	["q"]
	
	;vector 1
	["u"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[2 2]
	
	;attributes
	[0.2]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
)
```
Output:
```amalgam
1.3862943611198906
```
Example:
```amalgam
(generalized_distance
	["q"]
	
	;vector 1
	["u"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		[
			{
				a {a 0.00744879 b 0.996275605 c 0.996275605}
				b [
						{a 0.501736111 b 0.501736111 c 0.996527778}
						0.8
					]
				c {a 0.996539792 b 0.996539792 c 0.006920415}
			}
			0.2
		]
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
)
```
Output:
```amalgam
1.3862943611198906
```
Example:
```amalgam
(generalized_distance
	["q"]
	
	;vector 1
	["u"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		[
			[
				{
					a {a 0.00744879 b 0.996275605 c 0.996275605}
					b [
							{a 0.501736111 b 0.501736111 c 0.996527778}
							0.8
						]
					c {a 0.996539792 b 0.996539792 c 0.006920415}
				}
				0.2
			]
			0.2
		]
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
)
```
Output:
```amalgam
1.3862943611198906
```
Example:
```amalgam
(generalized_distance
	{
		A1 1
		A2 1
		A3 1
		B 1
	}
	
	;vector 1
	{
		A1 2
		A2 2
		A3 2
		B 2
	}
	
	;vector 2
	1
	
	;p
	{
		A1 {
				A1 0
				A2 0.372145984
				A3 0.370497589
				B 0.082723928
				sum 0.174632499
			}
		A2 {
				A1 0.371518433
				A2 0
				A3 0.370520996
				B 0.082668725
				sum 0.175291846
			}
		A3 {
				A1 0.370319458
				A2 0.370968492
				A3 0
				B 0.085480882
				sum 0.173231167
			}
		B {
				A1 0.061363751
				A2 0.049512288
				A3 0.05628626
				B 0
				sum 0.832837701
			}
		sum {
				A1 0.114003407
				A2 0.106173002
				A3 0.100958636
				B 0.678864956
				sum 0
			}
	}
	
	;weights
	["continuous_number"]
	
	;types
	(null)
	
	;attributes
	0.5
	
	;deviations
	["A2" "A3" "B"]
	
	;names
	"sum"
	
	;weights_selection_feature
	.true
)
```
Output:
```amalgam
0.8383382080915319
```
Example:
```amalgam
(generalized_distance
	[
		[1.5 2 3 4 5 "s12"]
	]
	[
		[1 2 3 "s21"]
	]
	1
	[1]
	["continuous_code"]
	[{}]
)
```
Output:
```amalgam
5.325588119387614
```
Example:
```amalgam
(generalized_distance
	[
		[1.5 2 3 4 5 "s12"]
	]
	[
		[1 2 3 "s21"]
	]
	1
	[1]
	["continuous_code"]
	[
		{nominal_strings .false types_must_match .false}
	]
)
```
Output:
```amalgam
3.697640774259515
```
Example:
```amalgam
(generalized_distance
	{
		A1 1
		A2 1
		A3 1
		B 1
	}
	
	;vector 1
	{
		A1 2
		A2 2
		A3 2
		B 2
	}
	
	;vector 2
	1
	
	;p
	{
		A1 {
				A1 0
				A2 0.372145984
				A3 0.370497589
				B 0.082723928
				sum 0.174632499
			}
		A2 {
				A1 0.371518433
				A2 0
				A3 0.370520996
				B 0.082668725
				sum 0.175291846
			}
		A3 {
				A1 0.370319458
				A2 0.370968492
				A3 0
				B 0.085480882
				sum 0.173231167
			}
		B {
				A1 0.061363751
				A2 0.049512288
				A3 0.05628626
				B 0
				sum 0.832837701
			}
		sum {
				A1 0.114003407
				A2 0.106173002
				A3 0.100958636
				B 0.678864956
				sum 0
			}
	}
	
	;weights
	["continuous_number"]
	
	;types
	(null)
	
	;attributes
	0.5
	
	;deviations
	["A2" "A3"]
	
	;names
	["sum" "A1" "B"]
	
	;weights_selection_feature
	.true
)
```
Output:
```amalgam
0.8383382080915318
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `entropy`
#### Parameters
`list|assoc|number p [list|assoc|number q] [number p_exponent] [number q_exponent]`
#### Description
Computes a form of entropy on the specified vectors `p` and `q` using nats (natural log, not bits) in the form of -sum p_i ln (p_i^p_exponent * q_i^q_exponent).  For both `p` and `q`, if `p` or `q` is a list of numbers, then it will treat each entry as being the probability of that element.  If it is an associative array, then elements with matching keys will be matched.  If `p` or `q` is a number then it will use that value in place of each element.  If `p` or `q` is null or not specified, it will be calculated as the reciprocal of the size of the other element (p_i would be 1/|q| or q_i would be 1/|p|).  If either `p_exponent` or `q_exponent` is 0, then that exponent will be ignored.  Shannon entropy can be computed by ignoring the q parameters by specifying it as null, setting `p_exponent` to 1 and `q_exponent` to 0. KL-divergence can be computed by providing both `p` and `q` and setting `p_exponent` to -1 and `q_exponent` to 1.  Cross-entropy can be computed by setting `p_exponent` to 0 and `q_exponent` to 1.
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
(entropy
	[0.5 0.5]
)
```
Output:
```amalgam
0.6931471805599453
```
Example:
```amalgam
(entropy
	[0.5 0.5]
	[0.25 0.75]
	-1
	1
)
```
Output:
```amalgam
0.14384103622589045
```
Example:
```amalgam
(entropy
	[0.5 0.5]
	[0.25 0.75]
)
```
Output:
```amalgam
0.14384103622589045
```
Example:
```amalgam
(entropy
	0.5
	[0.25 0.75]
	-1
	1
)
```
Output:
```amalgam
0.14384103622589045
```
Example:
```amalgam
(entropy
	0.5
	[0.25 0.75]
	0
	1
)
```
Output:
```amalgam
1.6739764335716716
```
Example:
```amalgam
(entropy
	{A 0.5 B 0.5}
	{A 0.75 B 0.25}
)
```
Output:
```amalgam
0.14384103622589045
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

