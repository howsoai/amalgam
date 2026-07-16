### Opcode: `contains_label`
#### Parameters
`[entity_id entity] entity_label label`
#### Returns
`bool`
#### Description
Evaluates to true if the the `label` exists for `entity`.  If `entity` is omitted or null, then it uses the current entity.
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

[Amalgam Opcodes](./opcodes.md)

### Opcode: `assign_to_entities`
#### Parameters
`[entity_id entity1] assoc label_value_pairs1 [entity_id entity2] [assoc label_value_pairs2] ...`
#### Returns
`bool`
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

[Amalgam Opcodes](./opcodes.md)

### Opcode: `accum_to_entities`
#### Parameters
`[entity_id entity1] assoc label_value_pairs1 [entity_id entity2] [assoc label_value_pairs2] ...`
#### Returns
`bool`
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

[Amalgam Opcodes](./opcodes.md)

### Opcode: `remove_from_entities`
#### Parameters
`[entity_id entity1] entity_label|list_of_entity_labels label_names1 [entity_id entity2] [entity_label|list_of_entity_labels label_names12] ...`
#### Returns
`bool`
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

[Amalgam Opcodes](./opcodes.md)

### Opcode: `retrieve_from_entity`
#### Parameters
`[entity_id entity] assoc|entity_label|list_of_entity_labels label_names`
#### Returns
`any`
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

[Amalgam Opcodes](./opcodes.md)

### Opcode: `call_entity`
#### Parameters
`entity_id entity entity_label label assoc params bool|assoc constraints bool return_warnings bool get_changes`
#### Returns
`any`
#### Description
Calls the contained `entity` and returns the result of the call.  If `label` is specified, then it will call the label specified by string, otherwise it will call the null label.  If `params` is specified, then it will pass those as the parameters on the scope stack.  If `constraints` is specified and not false or null, it will constrain execution.  If `constraints` is true or an assoc, it will default all constraints to be on at reasonable values for small execution without access to any data beyond `params`.  They optional key-value combinations for `constraints` are as follows.  If "max_node_operations" is specified, it represents the number of operations that are allowed to be performed. If "max_node_operations" is 0, then an infinite of operations will be allotted, up to the limits of the current calling context.   If "max_node_allocations" is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If "max_node_allocations" is 0 and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if "max_node_allocations" is specified while in a multithreaded environment, if the collective memory from all the executing threads exceeds the average memory specified by "max_node_allocations", that may trigger a memory limit for the call.  If "max_operation_depth" is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise "max_operation_depth" limits how deep nested opcodes will be called. If `return_warnings` is true (default is false), the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or .null if none)).  The keys "read_access" and "write_access" are boolean and control whether the execution can read from or write to entities and access their relevant permissions (e.g., to load files, make system calls).  The keys "max_contained_entities", "max_contained_entity_depth", and "max_entity_id_length" constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true (default is false), the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.  If `get_changes` is true (the default is false), the value will be a tuple in the form of `[value change_log]`, where the change log is a list of opcodes that hold an executable log of all of the changes that have elapsed to the entity and its contained entities.  The log may be evaluated to apply or re-apply the changes to any entity passed in to the executable log as the parameter "_".  If both `return_warnings` and `get_changes` are true, then the tuple will be in the form of `[value warnings performance_constraint_violation change_log]`.
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
						(clone_entities .null .null)
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
		(call_entity "Entity" "load" .null {max_node_operations 100})
		(call_entity
			"Entity"
			"copy_entity"
			.null
			{
				max_node_operations 1000
				max_node_allocations 1000
				max_operation_depth 10
				max_contained_entities 10
				max_contained_entity_depth 3
				max_entity_id_length 20
			}
			.true
			.true
		)
	]
)

```
Output:
```amalgam
[
	"hello world"
	.null
	.null
	[.null {} "Execution step limit exceeded" (seq)]
]
```
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
	(call_entity "Entity" "a_assign" .null .null .false .true)
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

[Amalgam Opcodes](./opcodes.md)

### Opcode: `call_on_entity`
#### Parameters
`entity_id entity any function assoc params bool|assoc constraints bool return_warnings bool get_changes`
#### Returns
`any`
#### Description
Calls `function` to be run on the contained `entity` and returns the result of the call.  If `params` is specified, then it will pass those as the parameters on the scope stack.  If `constraints` is specified and not false or null, it will constrain execution.  If `constraints` is true or an assoc, it will default all constraints to be on at reasonable values for small execution without access to any data beyond `params`.  They optional key-value combinations for `constraints` are as follows.  If "max_node_operations" is specified, it represents the number of operations that are allowed to be performed. If "max_node_operations" is 0, then an infinite of operations will be allotted, up to the limits of the current calling context.   If "max_node_allocations" is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If "max_node_allocations" is 0 and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if "max_node_allocations" is specified while in a multithreaded environment, if the collective memory from all the executing threads exceeds the average memory specified by "max_node_allocations", that may trigger a memory limit for the call.  If "max_operation_depth" is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise "max_operation_depth" limits how deep nested opcodes will be called. If `return_warnings` is true (default is false), the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or .null if none)).  The keys "read_access" and "write_access" are boolean and control whether the execution can read from or write to entities and access their relevant permissions (e.g., to load files, make system calls).  The keys "max_contained_entities", "max_contained_entity_depth", and "max_entity_id_length" constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true (default is false), the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.  If `get_changes` is true (the default is false), the value will be a tuple in the form of `[value change_log]`, where the change log is a list of opcodes that hold an executable log of all of the changes that have elapsed to the entity and its contained entities.  The log may be evaluated to apply or re-apply the changes to any entity passed in to the executable log as the parameter "_".  If both `return_warnings` and `get_changes` are true, then the tuple will be in the form of `[value warnings performance_constraint_violation change_log]`.
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

[Amalgam Opcodes](./opcodes.md)

### Opcode: `call_container`
#### Parameters
`entity_label label assoc params bool|assoc constraints bool return_warnings`
#### Returns
`any`
#### Description
Attempts to call the container associated with `label` that must begin with a caret; the caret indicates that the label is allowed to be accessed by contained entities.  It will evaluate to the return value of the call.  If `params` is specified, then it will pass those as the params on the scope stack.  If `constraints` is true or an assoc, it will default all constraints to be on at reasonable values for small execution without access to any data beyond `params`.  They optional key-value combinations for `constraints` are as follows.  If "max_node_operations" is specified, it represents the number of operations that are allowed to be performed. If "max_node_operations" is 0, then an infinite of operations will be allotted, up to the limits of the current calling context.   If "max_node_allocations" is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If "max_node_allocations" is 0 and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if "max_node_allocations" is specified while in a multithreaded environment, if the collective memory from all the executing threads exceeds the average memory specified by "max_node_allocations", that may trigger a memory limit for the call.  If "max_operation_depth" is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise "max_operation_depth" limits how deep nested opcodes will be called. If `return_warnings` is true (default is false), the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or .null if none)).  The keys "read_access" and "write_access" are boolean and control whether the execution can read from or write to entities and access their relevant permissions (e.g., to load files, make system calls).  The keys "max_contained_entities", "max_contained_entity_depth", and "max_entity_id_length" constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true (default is false), the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.
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
				^available_method 5
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
			{
				max_node_operations 30
				max_node_allocations 50
			}
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			{
				max_node_operations 1
				max_node_allocations 1
			}
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			{
				max_node_operations 1
				max_node_allocations 100
				max_operation_depth 1
				max_contained_entities 1
				max_contained_entity_depth 1
				max_entity_id_length 1
			}
			.true
		)
	]
)
```
Output:
```amalgam
[
	10
	10
	.null
	[.null {} "Execution step limit exceeded"]
]
```

[Amalgam Opcodes](./opcodes.md)

