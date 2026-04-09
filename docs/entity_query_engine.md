### Opcode: `contained_entities`
#### Parameters
`[id_path containing_entity | query|list condition1] [query|list condition2] ...[ query|list conditionN]`
#### Description
Returns a list of strings of ids of entities contained in `containing_entity` or the current entity if containing_entity is omitted or null.  The parameters of `condition1` through `conditionN` are query conditions, and they may be any of the query opcodes (beginning with `query_`) or may be a list of query opcodes, where each condition will be executed in order as a conjunction.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: true
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"Entity1"
		{a 1 b 2}
		"Entity2"
		{c 3}
	)
	(create_entities
		["Entity2" "A"]
		{d 4}
		["Entity2"]
		{e 5}
	)
	[
		(contained_entities)
		(contained_entities "Entity2")
		(contained_entities
			(query_exists "a")
		)
	]
)
```
Output:
```amalgam
[
	["Entity1" "Entity2"]
	["A" "_3SaCTguSSie"]
	["Entity1"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `compute_on_contained_entities`
#### Parameters
`[id_path containing_entity | query|list condition1] [query|list condition2] ...[ query|list conditionN]`
#### Description
Performs queries like `(contained_entities)` on `containing_entity` or the current entity if containing_entity is omitted or null, but returns a value or set of values appropriate for the last query in conditions.  The parameters of `condition1` through `conditionN` are query conditions, and they may be any of the query opcodes (beginning with `query_`) or may be a list of query opcodes, where each condition will be executed in order as a conjunction.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: true
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"Entity1"
		{a 1 b 2}
		"Entity2"
		{a 3}
	)
	[
		(compute_on_contained_entities
			(query_exists "a")
		)
		(compute_on_contained_entities
			(query_exists "a")
			(query_sum "a")
		)
	]
)
```
Output:
```amalgam
[
	{
		Entity1 {a 1}
		Entity2 {a 3}
	}
	4
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_select`
#### Parameters
`number num_to_select [number start_offset] [number random_seed]`
#### Description
When used as a query argument, selects `num_to_select` entities sorted by entity id.  If `start_offset` is specified, then it will return `num_to_select` entities starting that far in, and subsequent calls can be used to get all entities in batches.  If `random_seed` is specified, then it will select `num_to_select` entities randomly from the list based on the random seed.  If `random_seed` is specified and `start_offset` is null, then it will not guarantee a position in the order for subsequent calls that specify `start_offset`, and will execute more quickly.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5 q 5}
	)
	[
		(contained_entities
			(query_select 3)
		)
		(contained_entities
			(query_select 3 1)
		)
		(contained_entities
			(query_select 100 2)
		)
		(contained_entities
			(query_select 2 0 1)
		)
		(contained_entities
			(query_select 2 2 1)
		)
		(contained_entities
			(query_select 2 4 1)
		)
		(contained_entities
			(query_select 4 (null) (rand))
		)
		(contained_entities
			(query_select 4 (null) (rand))
		)
		(contained_entities
			(query_not_exists "q")
			(query_select 2 3)
		)
	]
)
```
Output:
```amalgam
[
	["E1" "E2" "E3"]
	["E2" "E3" "E4"]
	["E3" "E4" "E5"]
	["E2" "E3"]
	["E4" "E5"]
	["E1"]
	["E1" "E2" "E3" "E4"]
	["E1" "E2" "E3" "E4"]
	["E4"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_sample`
#### Parameters
`number num_to_select [string weight_label_name] [number random_seed]`
#### Description
When used as a query argument, selects a random sample of `num_to_select` entities sorted by entity id, sampled with replacement.  If `weight_label_name` is specified and not null, it will use `weight_label_name` as the feature containing the weights for the sampling, which will be normalized prior to sampling.  Non-numbers and negative infinite values for weights will be ignored, and if there are any infinite values, those will be selected from uniformly.  If `random_seed` is specified, then it will select `num_to_select` entities randomly from the list based on the random seed.  If `random_seed` is not specified then the subsequent calls will return the same sample of entities.
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
(seq
	(create_entities
		"E1"
		{a 1 weight 0.4}
		"E2"
		{a 2 weight 0.5}
		"E3"
		{a 3 weight 0.01}
		"E4"
		{a 4 weight 0.01}
		"E5"
		{a 5 q 5 weight 3.5}
	)
	[
		(contained_entities (query_sample))
		(contained_entities
			(query_sample 2)
		)
		(contained_entities
			(query_sample 1 (null) (rand))
		)
		(contained_entities
			(query_sample 1 (null) (null))
		)
		(contained_entities
			(query_sample 1 "weight")
		)
		(contained_entities
			(query_sample 1 "weight")
		)
		(contained_entities
			(query_sample 5 "weight" (rand))
		)
		(contained_entities
			(query_sample 5 "weight" (null))
		)
		(contained_entities
			(query_not_in_entity_list
				["E1" "E2" "E5"]
			)
			(query_sample 5 "weight" (rand))
		)
		(contained_entities
			(query_sample 10 "weight" (rand))
			(query_not_in_entity_list
				["E5"]
			)
		)
	]
)
```
Output:
```amalgam
[
	["E1"]
	["E2" "E3"]
	["E4"]
	["E3"]
	["E5"]
	["E5"]
	["E2" "E5" "E2" "E5" "E5"]
	["E5" "E2" "E2" "E5" "E1"]
	["E3" "E4" "E3" "E3" "E4"]
	["E1" "E2"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_in_entity_list`
#### Parameters
`list entity_ids`
#### Description
When used as a query argument, selects only the entities in `entity_ids`.  It can be used to filter results before doing subsequent queries, especially to reduce computation required for complex queries.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
	)
	(contained_entities
		(query_in_entity_list
			["E1" "E2"]
		)
	)
)
```
Output:
```amalgam
["E1" "E2"]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_not_in_entity_list`
#### Parameters
`list entity_ids`
#### Description
When used as a query argument, filters out the entities in `entity_ids`.  It can be used to filter results before doing subsequent queries, especially to reduce computation required for complex queries.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
	)
	(contained_entities
		(query_not_in_entity_list
			["E1" "E2"]
		)
	)
)
```
Output:
```amalgam
["E3" "E4" "E5"]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_exists`
#### Parameters
`string label_name`
#### Description
When used as a query argument, selects entities which have the label `label_name`.  If called last with compute_on_contained_entities, then it returns an assoc of entity ids, where each value is an assoc of corresponding label names and values.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{!e 3 a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q 3}
	)
	[
		(contained_entities
			(query_exists "a")
		)
		
		;can't find private labels
		(contained_entities
			(query_exists "!e")
		)
		(contained_entities
			(query_equals "a" 5)
			(query_exists "q")
		)
	]
)
```
Output:
```amalgam
[
	["E1" "E2" "E3" "E4" "E5" "E5q"]
	[]
	["E5q"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_not_exists`
#### Parameters
`string label_name`
#### Description
When used as a query argument, selects entities which do not have the the label `label_name`.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q 3}
		"Eq"
		{q 0}
		"Er"
		{r 0}
	)
	[
		(contained_entities
			(query_not_exists "q")
		)
		(contained_entities
			(query_not_exists "q")
			(query_exists "a")
		)
	]
)
```
Output:
```amalgam
[
	["E1" "E2" "E3" "E4" "E5" "Er"]
	["E1" "E2" "E3" "E4" "E5"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_equals`
#### Parameters
`string label_name * value`
#### Description
When used as a query argument, selects entities for which the value at label `label_name` is equal to `value`.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q 3}
		"Eq"
		{q 0}
		"Er"
		{r 0}
	)
	[
		(contained_entities
			(query_equals "a" 5)
		)
		(contained_entities
			(query_exists "q")
			(query_equals "a" 5)
		)
	]
)
```
Output:
```amalgam
[
	["E5" "E5q"]
	["E5q"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_not_equals`
#### Parameters
`string label_name * value`
#### Description
When used as a query argument, selects entities for which the value at label `label_name` is not equal to `value`.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q 3}
		"Eq"
		{q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_not_equals "a" 5)
		)
		(contained_entities
			(query_not_equals "q" "q")
			(query_equals "q" 3)
		)
	]
)
```
Output:
```amalgam
[
	["E1" "E2" "E3" "E4"]
	["E5q"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_between`
#### Parameters
`string label_name * lower_bound * upper_bound`
#### Description
When used as a query argument, selects entities for which the value at label `label_name` is at least `lower_bound` and at most `upper_bound`, inclusive for both values.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_between "a" 2 4)
		)
		(contained_entities
			(query_between "a" 3 100)
			(query_between "q" "m" "z")
		)
	]
)
```
Output:
```amalgam
[
	["E2" "E3" "E4"]
	["E6"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_not_between`
#### Parameters
`string label_name * lower_bound * upper_bound`
#### Description
When used as a query argument, selects entities for which the value at label `label_name` is less than `lower_bound` or greater than `upper_bound`.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_not_between "a" 2 4)
		)
		(contained_entities
			(query_exists "a")
			(query_not_between "q" "m" "z")
		)
	]
)
```
Output:
```amalgam
[
	["E1" "E5" "E5q" "E6"]
	["E5q"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_among`
#### Parameters
`string label_name list values`
#### Description
When used as a query argument, selects entities for which the value at label `label_name` is one of the values specified in `values`.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_among
				"a"
				[1 5]
			)
		)
		(contained_entities
			(query_exists "a")
			(query_among
				"q"
				["a"]
			)
		)
	]
)
```
Output:
```amalgam
[
	["E1" "E5" "E5q"]
	["E5q"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_not_among`
#### Parameters
`string label_name list values`
#### Description
When used as a query argument, selects entities for which the value at label `label_name` is not one of the values specified in `values`.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_not_among
				"a"
				[1 5]
			)
		)
		(contained_entities
			(query_exists "a")
			(query_not_among
				"q"
				["a"]
			)
		)
	]
)
```
Output:
```amalgam
[
	["E2" "E3" "E4" "E6"]
	["E6"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_max`
#### Parameters
`string label_name [number num_entities] [bool numeric]`
#### Description
When used as a query argument, selects a number of entities with the highest values for the label `label_name`.  If `num_entities` is specified, it will return that many entities, otherwise will return 1.  If `numeric` is true, its default value, then it only considers numeric values; if false, will consider all types.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_max "a")
		)
		(contained_entities
			(query_max "a" 2)
		)
		(compute_on_contained_entities
			(query_exists "a")
			(query_max "q" 1 .false)
			(query_exists "q")
		)
	]
)
```
Output:
```amalgam
[
	["E6"]
	["E5" "E6"]
	{
		E6 {q "q"}
	}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_min`
#### Parameters
`string label_name [number entities_returned] [bool numeric]`
#### Description
When used as a query argument, selects a number of entities with the lowest values for the label `label_name`.  If `num_entities` is specified, it will return that many entities, otherwise will return 1.  If `numeric` is true, its default value, then it only considers numeric values; if false, will consider all types.
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
(seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_min "a")
		)
		(contained_entities
			(query_min "a" 2)
		)
		(compute_on_contained_entities
			(query_exists "a")
			(query_min "q" 1 .false)
			(query_exists "q")
		)
	]
)
```
Output:
```amalgam
[
	["E1"]
	["E1" "E2"]
	{
		E5q {q "a"}
	}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_sum`
#### Parameters
`string label_name [string weight_label_name]`
#### Description
When used as a query argument, returns the sum of all entities over the value at `label_name`.  If `weight_label_name` is specified, it will find the weighted sum, which is the same as a dot product.
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
(seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
	)
	[
		(compute_on_contained_entities
			(query_sum "a")
		)
		(compute_on_contained_entities
			(query_sum "a" "weight")
		)
	]
)
```
Output:
```amalgam
[15 35]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_mode`
#### Parameters
`string label_name [string weight_label_name]`
#### Description
When used as a query argument, finds the statistical mode of `label_name` across all entities using numerical values.  If `weight_label_name` is specified, it will find the weighted mode.
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
(seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
		"E5_2"
		{a 5 weight 1}
	)
	[
		(compute_on_contained_entities
			(query_mode "a")
		)
		(compute_on_contained_entities
			(query_mode "a" "weight")
		)
	]
)
```
Output:
```amalgam
[5 1]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_quantile`
#### Parameters
`string label_name [number q] [string weight_label_name]`
#### Description
When used as a query argument, finds the statistical quantile of `label_name` for numerical data, using `q` as the parameter to the quantile, the default being 0.5 which is the median.  If `weight_label_name` is specified, it will find the weighted quantile, otherwise the weight of every entity is 1.
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
(seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
	)
	[
		(compute_on_contained_entities
			(query_quantile "a" 0.5)
		)
		(compute_on_contained_entities
			(query_quantile "a" 0.5 "weight")
		)
		(compute_on_contained_entities
			(query_quantile "a" 0.25)
		)
		(compute_on_contained_entities
			(query_quantile "a" 0.25 "weight")
		)
	]
)
```
Output:
```amalgam
[3 2.142857142857143 2 1.2777777777777777]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_generalized_mean`
#### Parameters
`string label_name [number p] [string weight_label_name] [number center] [bool calculate_moment] [bool absolute_value]`
#### Description
When used as a query argument, computes the generalized mean over the label `label_name` for numerical data.  If `p` is specified (which defaults to 1), it is the parameter that can control the type of mean from minimum (negative infinity), to harmonic mean (-1), to geometric mean (0), to arithmetic mean (1), to maximum (infinity).  If `weight_label_name` is specified, it will normalize the weights and compute a weighted mean.  If `center` is specified, calculations will use that value as the central point, and the default is 0.0.  If `calculate_moment` is true, the results will not be raised to 1 / `p`.  If `absolute_value` is true, the differences will take the absolute value.  Various parameterizations of `(generalized_mean)` can be used to compute moments about the mean, especially by setting the `calculate_moment` parameter to true and using the mean as the center.
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
(seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
	)
	(declare
		{
			mean (compute_on_contained_entities
					(query_generalized_mean "a" 1)
				)
		}
	)
	[
		mean
		(compute_on_contained_entities
			(query_generalized_mean "weight" 0)
		)
		(compute_on_contained_entities
			(query_generalized_mean "weight" -1)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 2)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 1 "weight")
		)
		(compute_on_contained_entities
			(query_generalized_mean "weight" 0 "weight")
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 1 (null) mean .true .true)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 2 (null) mean .true)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 3 (null) mean .false)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 4 (null) mean .true)
		)
	]
)
```
Output:
```amalgam
[
	3
	2.6051710846973517
	2.18978102189781
	3.3166247903554
	2.3333333333333335
	86400000.00000006
	1.2
	2
	0
	6.8
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_min_difference`
#### Parameters
`string label_name [number cyclic_range] [bool include_zero_difference]`
#### Description
When used as a query argument, finds the smallest difference between any two values for the label `label_name`. If `cyclic_range` is null, the default value, then it will assume the values are not cyclic.  If `cyclic_range` is a number, then it will assume the range is from 0 to `cyclic_range`.  If `include_zero_difference` is true then it will return 0 if the smallest gap between any two numbers is 0.  If `include_zero_difference` is false, its default value, it will return the smallest nonzero value.
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
(seq
	(create_entities
		"E0.1"
		{a 0.1}
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5.5"
		{a 5.5}
		"E5.5_2"
		{a 5.5}
	)
	[
		(compute_on_contained_entities
			(query_min_difference "a")
		)
		(compute_on_contained_entities
			(query_min_difference "a" (null) .true)
		)
		(compute_on_contained_entities
			(query_min_difference "a" 5.5 .false)
		)
	]
)
```
Output:
```amalgam
[0.5 0 0.1]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_max_difference`
#### Parameters
`string label_name [number cyclic_range]`
#### Description
When used as a query argument, finds the largest difference between any two values for the label `label_name`. If `cyclic_range` is null, the default value, then it will assume the values are not cyclic.  If `cyclic_range` is a number, then it will assume the range is from 0 to `cyclic_range`.
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
(seq
	(create_entities
		"E0.1"
		{a 0.1}
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5.5"
		{a 5.5}
		"E5.5_2"
		{a 5.5}
	)
	[
		(compute_on_contained_entities
			(query_max_difference "a")
		)
		(compute_on_contained_entities
			(query_max_difference "a" 7.5)
		)
	]
)
```
Output:
```amalgam
[1 2.1]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_value_masses`
#### Parameters
`string label_name [string weight_label_name]`
#### Description
When used as a query argument, computes the counts for each value of the label `label_name` and returns an assoc with the keys being the label values and the values being the counts or weights of the values.  If `weight_label_name` is specified, then it will accumulate that weight for each value, otherwise it will use a weight of 1 for each yielding a count.
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
(seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
		"E5_2"
		{a 5 weight 1}
	)
	[
		(compute_on_contained_entities
			(query_value_masses "a")
		)
		(compute_on_contained_entities
			(query_value_masses "a" "weight")
		)
	]
)
```
Output:
```amalgam
[
	{
		1 1
		2 1
		3 1
		4 1
		5 2
	}
	{
		1 5
		2 4
		3 3
		4 2
		5 2
	}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_greater_or_equal_to`
#### Parameters
`string label_name * min_value`
#### Description
When used as a query argument, selects entities with a value in label `label_name` greater than or equal to `min_value`.
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
(seq
	(create_entities
		"E1"
		{a 1 b "a"}
		"E2"
		{a 2 b "b"}
		"E3"
		{a 3 b "c"}
		"E4"
		{a 4 b "d"}
		"E5"
		{a 5 b "e"}
	)
	[
		(contained_entities
			(query_greater_or_equal_to "a" 3)
		)
		(contained_entities
			(query_greater_or_equal_to "b" "c")
		)
	]
)
```
Output:
```amalgam
[
	["E3" "E4" "E5"]
	["E3" "E4" "E5"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_less_or_equal_to`
#### Parameters
`string label_name * max_value`
#### Description
When used as a query argument, selects entities with a value in label `label_name` less than or equal to `max_value`.
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
(seq
	(create_entities
		"E1"
		{a 1 b "a"}
		"E2"
		{a 2 b "b"}
		"E3"
		{a 3 b "c"}
		"E4"
		{a 4 b "d"}
		"E5"
		{a 5 b "e"}
	)
	[
		(contained_entities
			(query_less_or_equal_to "a" 3)
		)
		(contained_entities
			(query_less_or_equal_to "b" "c")
		)
	]
)
```
Output:
```amalgam
[
	["E1" "E2" "E3"]
	["E1" "E2" "E3"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_within_generalized_distance`
#### Parameters
`number max_distance list feature_labels list|string axis_values_or_entity_id [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list]`
#### Description
When used as a query argument, selects the entities with labels that are at least as close as `max_distance` to the given point.  The parameter `axis_values_or_entity_id` specifies the corresponding values for the point to test from, or if `axis_values_or_entity_id` is a string the entity to collect the labels from.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.
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
(seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	(compute_on_contained_entities
		(query_within_generalized_distance
			1.5
			["x" "y"]
			[1 2]
			2
			(null)
			(null)
			(null)
			(null)
			(null)
			1
			(null)
			"random seed 1234"
		)
	)
)
```
Output:
```amalgam
{vert2 1 vert3 1.4142135623730951 vert5 1.4142135623730951}
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_nearest_generalized_distance`
#### Parameters
`list|number selection_bandwidth list feature_labels list|string axis_values_or_entity_id [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list]`
#### Description
When used as a query argument, selects the closest entities to the given point.  The parameter `axis_values_or_entity_id` specifies the corresponding values for the point to test from, or if `axis_values_or_entity_id` is a string the entity to collect the labels from.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.
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
(seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	[
		(compute_on_contained_entities
			(query_nearest_generalized_distance
				3
				["x" "y"]
				[1 2]
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_nearest_generalized_distance
				[0.2 1]
				["x" "y"]
				[1 2]
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				1
				(null)
				"random seed 1234"
			)
		)
	]
)
```
Output:
```amalgam
[
	{vert2 1 vert3 1.4142135623730951 vert5 1.4142135623730951}
	{
		vert2 1
		vert3 1.4142135623730951
		vert4 1.5811388300841898
		vert5 1.4142135623730951
	}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_distance_contributions`
#### Parameters
`list|number selection_bandwidth list feature_labels list axis_values_or_entity_id [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list]`
#### Description
When used as a query argument, computes the distance or surprisal contribution for every entity.  The parameter `axis_values_or_entity_id` specifies the corresponding values for the point to test from, or if `axis_values_or_entity_id` is a string the entity to collect the labels from.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	(compute_on_contained_entities
		(query_distance_contributions
			2
			["x" "y"]
			[
				[1 2]
			]
			2
			(null)
			(null)
			(null)
			(null)
			(null)
			-1
			(null)
			"random seed 1234"
		)
	)
)
```
Output:
```amalgam
[1.17157287525381]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_entity_convictions`
#### Parameters
`list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [bool conviction_of_removal] [* output_sorted_list]`
#### Description
When used as a query argument, computes the case conviction for every case given in `entity_ids_to_compute` with respect to *all* cases in the contained entities set input during a query.  If `entity_ids_to_compute` is null or an empty list, case conviction is computed for all cases.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	[
		(compute_on_contained_entities
			(query_entity_convictions
				2
				["x" "y"]
				(null)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_entity_convictions
				2
				["x" "y"]
				["vert0" "vert1" "vert2"]
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
			)
		)
		(compute_on_contained_entities
			(query_exists "object")
			(query_entity_convictions
				2
				["x" "y"]
				(null)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_exists "object")
			(query_entity_convictions
				2
				["x" "y"]
				(contained_entities
					(query_exists "object")
				)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
	]
)
```
Output:
```amalgam
[
	{
		vert0 20.64507068846579
		vert1 10.936029135274522
		vert2 0.7220080663101216
		vert3 20.64507068846579
		vert4 0.6024424202611007
		vert5 0.361435163373361
	}
	{vert0 10.493921488434916 vert1 5.558800590831786 vert2 0.3669978212333347}
	{
		vert0 20.64507068846579
		vert1 10.936029135274522
		vert2 0.7220080663101216
		vert3 20.64507068846579
		vert4 0.6024424202611007
		vert5 0.361435163373361
	}
	{
		vert0 20.64507068846579
		vert1 10.936029135274522
		vert2 0.7220080663101216
		vert3 20.64507068846579
		vert4 0.6024424202611007
		vert5 0.361435163373361
	}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_entity_group_kl_divergence`
#### Parameters
`list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [bool conviction_of_removal]`
#### Description
When used as a query argument, computes the case kl divergence for every case given in `entity_ids_to_compute` as a group with respect to *all* cases in the contained entities set input during a query.  If `entity_ids_to_compute` is null or an empty list, case conviction is computed for all cases.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	(compute_on_contained_entities
		(query_exists "object")
		(query_entity_group_kl_divergence
			2
			["x" "y"]
			(contained_entities
				(query_equals "object" 2)
			)
			2
			(null)
			(null)
			(null)
			(null)
			(null)
			-1
			(null)
			"random seed 1234"
		)
	)
)
```
Output:
```amalgam
0.01228960638554566
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_entity_distance_contributions`
#### Parameters
`list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list]`
#### Description
When used as a query argument, computes the case conviction for every case given in `entity_ids_to_compute` with respect to *all* cases in the contained entities set input during a query.  If `entity_ids_to_compute` is null or an empty list, case conviction is computed for all cases.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	[
		(compute_on_contained_entities
			(query_entity_distance_contributions
				2
				["x" "y"]
				(null)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_entity_distance_contributions
				2
				["x" "y"]
				["vert0" "vert1" "vert2"]
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
			)
		)
		(compute_on_contained_entities
			(query_exists "object")
			(query_entity_distance_contributions
				2
				["x" "y"]
				(null)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_exists "object")
			(query_entity_distance_contributions
				2
				["x" "y"]
				(contained_entities
					(query_exists "object")
				)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
	]
)
```
Output:
```amalgam
[
	{
		vert0 0.8284271247461902
		vert1 0.8284271247461902
		vert2 0.8284271247461902
		vert3 0.8284271247461902
		vert4 0.7071067811865476
		vert5 1.17157287525381
	}
	{vert0 0.8284271247461902 vert1 0.8284271247461902 vert2 0.8284271247461902}
	{
		vert0 0.8284271247461902
		vert1 0.8284271247461902
		vert2 0.8284271247461902
		vert3 0.8284271247461902
		vert4 0.7071067811865476
		vert5 1.17157287525381
	}
	{
		vert0 0.8284271247461902
		vert1 0.8284271247461902
		vert2 0.8284271247461902
		vert3 0.8284271247461902
		vert4 0.7071067811865476
		vert5 1.17157287525381
	}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_entity_kl_divergences`
#### Parameters
`list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [bool conviction_of_removal] [* output_sorted_list]`
#### Description
When used as a query argument, computes the case conviction for every case given in `entity_ids_to_compute` with respect to *all* cases in the contained entities set input during a query.  If `entity_ids_to_compute` is null or an empty list, case conviction is computed for all cases.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	[
		(compute_on_contained_entities
			[
				(query_exists "object")
				(query_entity_kl_divergences
					2
					["x" "y"]
					(null)
					2
					(null)
					(null)
					(null)
					(null)
					(null)
					-1
					(null)
					"random seed 1234"
				)
			]
		)
		(compute_on_contained_entities
			[
				(query_exists "object")
				(query_entity_kl_divergences
					2
					["x" "y"]
					(contained_entities
						(query_exists "object")
					)
					2
					(null)
					(null)
					(null)
					(null)
					(null)
					-1
					(null)
					"random seed 1234"
				)
			]
		)
	]
)
```
Output:
```amalgam
[
	{
		vert0 0.00018681393615961172
		vert1 0.0003526679446349979
		vert2 0.005341750456218763
		vert3 0.00018681393615961172
		vert4 0.006401917906003654
		vert5 0.010670757326457676
	}
	{
		vert0 0.00018681393615961172
		vert1 0.0003526679446349979
		vert2 0.005341750456218763
		vert3 0.00018681393615961172
		vert4 0.006401917906003654
		vert5 0.010670757326457676
	}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `query_entity_cumulative_nearest_entity_weights`
#### Parameters
`list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list]`
#### Description
When used as a query argument, computes the nearest neighbors to every entity given by `entity_ids_to_compute`, normalizes their influence weights, and accumulates the entity's total influence weights relative to every other case.  It returns a list of all cases whose cumulative neighbor values are greater than zero.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.
#### Details
 - Permissions required:  none
 - Allows concurrency: true
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): partial
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"entity1"
		{alpha 3 b 0.17 c 1}
	)
	(create_entities
		"entity2"
		{alpha 4 b 0.12 c 0}
	)
	(create_entities
		"entity3"
		{
			alpha 5
			b 0.1
			c 0
			x 16
		}
	)
	(create_entities
		"entity4"
		{
			alpha 1
			b 0.14
			c 1
			x 8
		}
	)
	(create_entities
		"entity5"
		{
			alpha 9
			b 0.11
			c 1
			x 32
		}
	)
	(compute_on_contained_entities
		[
			(query_entity_cumulative_nearest_entity_weights
				2
				["alpha" "b" "c"]
				(null)
				0.5
				(null)
				[0 0 1]
			)
		]
	)
)
```
Output:
```amalgam
{
	entity1 2.3019715434701133
	entity2 1.5822400592793713
	entity3 0.7781961968246989
	entity4 0.33759220042581645
}
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

