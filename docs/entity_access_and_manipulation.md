### Opcode: `contains_label`
#### Parameters
`[id_path entity] string label_name`
#### Description
Evaluates to true if the label represented by `label_name` exists for `entity`.  If `entity` is omitted or null, then it uses the current entity.
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
(seq
	(create_entities
		"Entity"
		{a 1 b 2 c 3}
	)
	[
		(contains_label "Entity" "a")
		(contains_label "Entity" "z")
	]
)
```
Output:
```amalgam
[.true .false]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `assign_to_entities`
#### Parameters
`[id_path entity1] assoc label_value_pairs1 [id_path entity2] [assoc label_value_pairs2] [...]`
#### Description
For each index-value pair of `label_value_pairs`, assigns the value to the label on the contained entity represented by the respective `entity`, itself if `entity` is not specified or is null.  If the label is not found, it will create it.  Returns true if all assignments were successful, false if not.
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
		"Entity"
		{a 1 b 2 c 3}
	)
	(assign_to_entities
		"Entity"
		{a 2 b 3 c 4}
		"Entity"
		{three 12}
	)
	(retrieve_entity_root "Entity")
)
```
Output:
```amalgam
{
	a 2
	b 3
	c 4
	three 12
}
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `accum_to_entities`
#### Parameters
`[id_path entity1] assoc label_value_pairs1 [id_path entity2] [assoc label_value_pairs2] [...]`
#### Description
For each index-value pair of `label_value_pairs`, it accumulates the value to the label on the contained entity represented by the respective `entity`, itself if `entity` is not specified or is null.  If the label is not found, it will create it.  Returns true if all assignments were successful, false if not.  Accumulation is performed differently based on the type: for numeric values it adds, for strings, it concatenates, for lists it appends, and for assocs it appends based on the pair.
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
		"Entity"
		{a 1 b 2 c 3}
	)
	(accum_to_entities
		"Entity"
		{a 2 b 3 c 4}
		"Entity"
		{doesnt_exist 12}
	)
	(retrieve_entity_root "Entity")
)
```
Output:
```amalgam
{a 3 b 5 c 7}
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `remove_from_entities`
#### Parameters
`[id_path entity1] string|list label_names1 [id_path entity2] [list string|label_names2] [...]`
#### Description
Removes all labels in `label_names1` from `entity1` and so on for each respective entity and label list.  Returns true if all removes were successful, false otherwise.
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
		"Entity"
		{
			a 1
			b 2
			c 3
			d 4
		}
	)
	(remove_from_entities
		"Entity"
		"a"
		"Entity"
		["b" "c"]
	)
	(retrieve_entity_root "Entity")
)
```
Output:
```amalgam
{d 4}
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `retrieve_from_entity`
#### Parameters
`[id_path entity] [string|list|assoc label_names]`
#### Description
Retrieves one or more labels from `entity`, using its own entity if `entity` is omitted or null.  If `label_names` is a string, it returns the value at the corresponding label.  If `label_names` is a list, it returns a list of the values of the labels of the corresponding labels.  If `label_names` is an assoc, it an assoc with label names as keys and the label values as the values.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: true
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): conditional
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"Entity"
		{a 12 b 13}
	)
	[
		(retrieve_from_entity "Entity" "a")
		(retrieve_from_entity
			"Entity"
			["a" "b"]
		)
		(retrieve_from_entity
			"Entity"
			(zip
				["a" "b"]
				null
			)
		)
	]
)
```
Output:
```amalgam
[
	12
	[12 13]
	{a 12 b 13}
]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `call_entity`
#### Parameters
`id_path entity [string label_name] [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [number max_contained_entities] [number max_contained_entity_depth] [number max_entity_id_length] [bool return_warnings]`
#### Description
Calls the contained `entity` and returns the result of the call.  If `label_name` is specified, then it will call the label specified by string, otherwise it will call the null label.  If `arguments` is specified, then it will pass those as the arguments on the scope stack.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed.  If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations.  If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If `max_node_allocations` is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called.  The parameters `max_contained_entities`, `max_contained_entity_depth`, and `max_entity_id_length` constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true, the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: true
 - Creates new scope: true
 - Creates new target scope: false
 - Value newness (whether references existing node): conditional
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"Entity"
		(lambda
			{
				!private_method "should not access"
				copy_entity (while
						.true
						(clone_entities (null) (null))
					)
				hello (declare
						{message ""}
						(concat "hello " message)
					)
				load (while .true)
			}
		)
	)
	[
		(call_entity
			"Entity"
			"hello"
			{message "world"}
		)
		(call_entity "Entity" "!private_method")
		(call_entity "Entity" "load" (null) 100)
		(call_entity
			"Entity"
			"copy_entity"
			(null)
			1000
			1000
			100
			10
			3
			20
		)
	]
)
```
Output:
```amalgam
[
	"hello world"
	(null)
	[(null) {} "Execution step limit exceeded"]
	[(null) {} "Execution step limit exceeded"]
]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `call_entity_get_changes`
#### Parameters
`id_path entity [string label_name] [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [number max_contained_entities] [number max_contained_entity_depth] [number max_entity_id_length] [bool return_warnings]`
#### Description
Calls the contained `entity` and returns the result of the call.  However, it also returns a list of opcodes that hold an executable log of all of the changes that have elapsed to the entity and its contained entities.  The log may be evaluated to apply or re-apply the changes to any entity passed in to the executable log as the parameter "_".  If `label_name` is specified, then it will call the label specified by string, otherwise it will call the null label.  If `arguments` is specified, then it will pass those as the arguments on the scope stack.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed.  If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations.  If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If `max_node_allocations` is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called.  The parameters `max_contained_entities`, `max_contained_entity_depth`, and `max_entity_id_length` constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true, the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: true
 - Creates new scope: true
 - Creates new target scope: false
 - Value newness (whether references existing node): conditional
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"Entity"
		(lambda
			{
				a_assign (seq
						(create_entities
							"Contained"
							{a 4 b 6}
						)
						(assign_to_entities
							"Contained"
							{a 6 b 10}
						)
						(set_entity_rand_seed "Contained" "bbbb")
						(accum_to_entities
							"Contained"
							{b 12}
						)
						(destroy_entities "Contained")
					)
			}
		)
	)
	(set_entity_permissions "Entity" .true)
	(call_entity_get_changes "Entity" "a_assign")
)
```
Output:
```amalgam
[
	.true
	(seq
		(create_entities
			["Entity" "Contained"]
			(lambda
				{a 4 b 6}
			)
		)
		(assign_to_entities
			["Entity" "Contained"]
			{a 6 b 10}
		)
		(set_entity_rand_seed
			["Entity" "Contained"]
			"bbbb"
			.false
		)
		(accum_to_entities
			["Entity" "Contained"]
			{b 12}
		)
		(destroy_entities
			["Entity" "Contained"]
		)
	)
]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `call_on_entity`
#### Parameters
`id_path entity * code [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [number max_contained_entities] [number max_contained_entity_depth] [number max_entity_id_length] [bool return_warnings]`
#### Description
Calls `code` to be run on the contained `entity` and returns the result of the call.  If `arguments` is specified, then it will pass those as the arguments on the scope stack.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed.  If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations.  If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If `max_node_allocations` is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called.  The parameters `max_contained_entities`, `max_contained_entity_depth`, and `max_entity_id_length` constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true, the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: true
 - Creates new scope: true
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"Entity"
		{a 1 b 2}
	)
	(set_entity_permissions "Entity" .true)
	(call_on_entity
		"Entity"
		(lambda
			[a b c]
		)
		{c 3}
	)
)
```
Output:
```amalgam
[1 2 3]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `call_container`
#### Parameters
`string parent_label_name [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [bool return_warnings]`
#### Description
Attempts to call the container associated with `label_name` that must begin with a caret; the caret indicates that the label is allowed to be accessed by contained entities.  It will evaluate to the return value of the call.  If `arguments` is specified, then it will pass those as the arguments on the scope stack.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed.  If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations.  If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If `max_node_allocations` is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called.  The parameters `max_contained_entities`, `max_contained_entity_depth`, and `max_entity_id_length` constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true, the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: true
 - Creates new scope: true
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"OuterEntity"
		(lambda
			{
				^available_method 3
				compute_value (call_entity
						"InnerEntity"
						"inner_call"
						{x x}
					)
			}
		)
	)
	(create_entities
		["OuterEntity" "InnerEntity"]
		(lambda
			{
				inner_call (+
						x
						(call_container "^available_method")
					)
			}
		)
	)
	[
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			30
			30
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			1
			1
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			1
			1
			1
			1
			1
			.false
		)
	]
)
```
Output:
```amalgam
[
	8
	[
		[8 {} (null)]
		{}
		(null)
	]
	[(null) {} "Execution step limit exceeded"]
	[(null) {} "Execution step limit exceeded"]
]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

