### Opcode: `create_entities`
#### Parameters
`[id_path entity1] * node1 [id_path entity2] [* node2] [...]`
#### Description
Creates a new entity for id path `entity1` with code specified by `node1`, repeating this for all entity-node pairs, returning a list of the id paths for each of the entities created.  If the execution does not have permission to create the entities, it will evaluate to null.  If the `entity` is omitted, then it will create an unnamed new entity in the calling entity.  If `entity1` specifies an existing entity, then it will create the new entity within that existing entity.  If the last id path in the string is not an existing entity, then it will attempt to create that entity (returning null if it cannot).  If the node is of any other type than assoc, it will create an assoc as the top node and place the node under the null key.  Unlike the rest of the entity creation commands, create_entities specifies the optional id path first to make it easy to read entity definitions.  If more than 2 parameters are specified, create_entities will iterate through all of the pairs of parameters, treating them like the first two as it creates new entities.
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
(create_entities
	"Entity"
	(lambda
		{
			a (+ 3 4)
		}
	)
)
```
Output:
```amalgam
["Entity"]
```
Example:
```amalgam
(seq
	(create_entities
		"EntityWithContainedEntities"
		{a 3 b 4}
	)
	(create_entities
		["EntityWithContainedEntities" "NamedEntity1"]
		{x 3 y 4}
	)
	(create_entities
		["EntityWithContainedEntities" "NamedEntity2"]
		{p 3 q 4}
	)
	(create_entities
		["EntityWithContainedEntities"]
		{m 3 n 4}
	)
	(contained_entities "EntityWithContainedEntities")
)
```
Output:
```amalgam
["NamedEntity1" "NamedEntity2" "_hIcoPxJ8LiS"]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `clone_entities`
#### Parameters
`id_path source_entity1 [id_path destination_entity1] [id_path source_entity2] [id_path destination_entity2] [...]`
#### Description
Creates a clone of `source_entity1`.  If `destination_entity1` is not specified, then it clones the entity into an unnamed entity in the current entity.  If `destination_entity1` is specified, then it clones it into the location specified by `destination_entity1`; if `destination_entity1` is an existing entity, then it will create it as a contained entity within `destination_entity1`, if not, it will attempt to create it with the given id path of `destination_entity1`.  Evaluates to the id path of the new entity.  Can only be performed by an entity that contains both `source_entity1` and the specified path of `destination_entity1`. If multiple entities are specified, it will move each from the source to the destination.  Evaluates to a list of the new entity ids.
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
		{a 3 b 4}
	)
	(create_entities
		["Entity1" "NamedEntity1"]
		{x 3 y 4}
	)
	(create_entities
		["Entity1" "NamedEntity2"]
		{p 3 q 4}
	)
	(create_entities
		["Entity1"]
		{m 3 n 4}
	)
	(clone_entities "Entity1" "Entity2")
	(contained_entities "Entity2")
)
```
Output:
```amalgam
["NamedEntity1" "NamedEntity2" "_539JylCpbqn"]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `move_entities`
#### Parameters
`id_path source_entity1 [id_path destination_entity1] [id_path source_entity2] [id_path destination_entity2] [...]`
#### Description
Moves the entity from location specified by `source_entity1` to destination `destination_entity1`.  If `destination_entity1` exists, it will move `source_entity1` using `source_entity1`'s current id path into `destination_entity1`.  If `destination_entity1` does not exist, then it will move `source_entity1` and rename it to the end of the id path specified by `destination_entity1`. Can only be performed by a containing entity relative to both ids.  If multiple entities are specified, it will move each from the source to the destination.  Evaluates to a list of the new entity ids.
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
		{a 3 b 4}
	)
	(create_entities
		["Entity1" "NamedEntity1"]
		{x 3 y 4}
	)
	(create_entities
		["Entity1" "NamedEntity2"]
		{p 3 q 4}
	)
	(create_entities
		["Entity1"]
		{m 3 n 4}
	)
	(move_entities "Entity1" "Entity2")
	(contained_entities "Entity2")
)
```
Output:
```amalgam
["NamedEntity1" "NamedEntity2" "_539JylCpbqn"]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `destroy_entities`
#### Parameters
`[id_path entity1] [id_path entity2] [...]`
#### Description
Destroys the entities specified by the ids `entity1`, `entity2`, etc. Can only be performed by containing entity.  Returns true if all entities were successfully destroyed, false if not.  Generally entities can be destroyed unless they do not exist or if there is code currently being run in it.
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
		{a 3 b 4}
	)
	(destroy_entities "Entity")
	(contained_entities)
)
```
Output:
```amalgam
[]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `load`
#### Parameters
`string resource_path [string resource_type] [assoc params]`
#### Description
Loads the data specified by `resource_path`, parses it into the appropriate code and data, and returns it. If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.
#### Details
 - Permissions required:  load
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(store
		"file.amlg"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.amlg")
)
```
Output:
```amalgam
(seq
	(print "hello")
)
```
Example:
```amalgam
(seq
	(store
		"file.json"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.json")
)
```
Output:
```amalgam
[
	1
	2
	3
	{a 1 b 2 c (null)}
]
```
Example:
```amalgam
(seq
	(store
		"file.yaml"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.yaml")
)
```
Output:
```amalgam
[
	1
	2
	3
	{a 1 b 2 c (null)}
]
```
Example:
```amalgam
(seq
	(store "file.txt" "This is text.")
	(load "test.txt")
)
```
Output:
```amalgam
(null)
```
Example:
```amalgam
(seq
	(store
		"file.caml"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.caml")
)
```
Output:
```amalgam
(seq
	(print "hello")
)
```
Example:
```amalgam
(seq
	(declare
		{
			csv_data [
					[6.4 2.8 5.6 2.2 "virginica"]
					[4.9 2.5 4.5 1.7 "virg\"inica"]
					[]
					["" "" "" (null)]
					[4.9 3.1 1.5 0.1 "set\nosa" 3]
					[4.4 3.2 1.3 0.2 "setosa"]
				]
		}
	)
	(store "file.csv" csv_data)
	(load "file.csv")
)
```
Output:
```amalgam
[
	[6.4 2.8 5.6 2.2 "virginica"]
	[4.9 2.5 4.5 1.7 "virg\"inica"]
	[(null)]
	[(null) (null) (null) (null)]
	[4.9 3.1 1.5 0.1 "set\nosa" 3]
	[4.4 3.2 1.3 0.2 "setosa"]
]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `load_entity`
#### Parameters
`string resource_path [id_path entity] [string resource_type] [bool persistent] [assoc params]`
#### Description
Loads the data specified by `resource_path` and parse it into the appropriate code and data, and stores it in `entity`.  It follows the same id path creation rules as `(create_entities)`, except that if no id path is specified, it may default to a name based on the resource if available.  If `persistent` is true, default is false, then any modifications to the entity or any entity contained within it will be written out to the resource, so that the memory and persistent storage are synchronized.  If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.
#### Details
 - Permissions required:  load
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(seq
		(create_entities
			"Entity"
			{a 1 b 2}
		)
		(create_entities
			["Entity" "Contained1"]
			{c 3}
		)
		(create_entities
			["Entity" "Contained1" "Contained2_1"]
			{d 4}
		)
		(create_entities
			["Entity" "Contained1" "Contained2_2"]
			{e 5}
		)
		(store_entity "entity.amlg" "Entity")
		(load_entity "entity.amlg" "EntityCopy")
		(difference_entities "Entity" "EntityCopy")
	)
)
```
Output:
```amalgam
(declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
)
```
Example:
```amalgam
(seq
	(create_entities
		"Entity"
		[1 2 3 4]
	)
	(create_entities
		["Entity" "Contained1"]
		[5 6 7]
	)
	(create_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 8 nine 9}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_3"]
		[12 13]
	)
	(store_entity
		"entity.caml"
		"Entity"
		(null)
		.true
		{flatten .true transactional .true}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_2"]
		[10 11]
	)
	(destroy_entities
		["Entity" "Contained1" "Contained1_3"]
	)
	(assign_to_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 88}
	)
	(load_entity
		"entity.caml"
		"EntityCopy"
		(null)
		.false
		{execute_on_load .true require_version_compatibility .true transactional .true}
	)
	(declare
		{
			diff (difference_entities "EntityCopy" "Entity")
		}
	)
	(destroy_entities "EntityCopy" "Entity")
	diff
)
```
Output:
```amalgam
(declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
)
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `store`
#### Parameters
`string resource_path * node [string resource_type] [assoc params]`
#### Description
Stores `node` into `resource_path`.  Returns true if successful, false if not.  If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.
#### Details
 - Permissions required:  store
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(store
		"file.amlg"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.amlg")
)
```
Output:
```amalgam
(seq
	(print "hello")
)
```
Example:
```amalgam
(seq
	(store
		"file.json"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.json")
)
```
Output:
```amalgam
[
	1
	2
	3
	{a 1 b 2 c (null)}
]
```
Example:
```amalgam
(seq
	(store
		"file.yaml"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.yaml")
)
```
Output:
```amalgam
[
	1
	2
	3
	{a 1 b 2 c (null)}
]
```
Example:
```amalgam
(seq
	(store "file.txt" "This is text.")
	(load "test.txt")
)
```
Output:
```amalgam
(null)
```
Example:
```amalgam
(seq
	(store
		"file.caml"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.caml")
)
```
Output:
```amalgam
(seq
	(print "hello")
)
```
Example:
```amalgam
(seq
	(declare
		{
			csv_data [
					[6.4 2.8 5.6 2.2 "virginica"]
					[4.9 2.5 4.5 1.7 "virg\"inica"]
					[]
					["" "" "" (null)]
					[4.9 3.1 1.5 0.1 "set\nosa" 3]
					[4.4 3.2 1.3 0.2 "setosa"]
				]
		}
	)
	(store "file.csv" csv_data)
	(load "file.csv")
)
```
Output:
```amalgam
[
	[6.4 2.8 5.6 2.2 "virginica"]
	[4.9 2.5 4.5 1.7 "virg\"inica"]
	[(null)]
	[(null) (null) (null) (null)]
	[4.9 3.1 1.5 0.1 "set\nosa" 3]
	[4.4 3.2 1.3 0.2 "setosa"]
]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `store_entity`
#### Parameters
`string resource_path id_path entity [string resource_type] [bool persistent] [assoc params]`
#### Description
Stores `entity` into `resource_path`.  Returns true if successful, false if not.  If `persistent` is true, default is false, then any modifications to the entity or any entity contained within it will be written out to the resource, so that the memory and persistent storage are synchronized.  If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.
#### Details
 - Permissions required:  store
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(seq
	(seq
		(create_entities
			"Entity"
			{a 1 b 2}
		)
		(create_entities
			["Entity" "Contained1"]
			{c 3}
		)
		(create_entities
			["Entity" "Contained1" "Contained2_1"]
			{d 4}
		)
		(create_entities
			["Entity" "Contained1" "Contained2_2"]
			{e 5}
		)
		(store_entity "entity.amlg" "Entity")
		(load_entity "entity.amlg" "EntityCopy")
		(difference_entities "Entity" "EntityCopy")
	)
)
```
Output:
```amalgam
(declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
)
```
Example:
```amalgam
(seq
	(create_entities
		"Entity"
		[1 2 3 4]
	)
	(create_entities
		["Entity" "Contained1"]
		[5 6 7]
	)
	(create_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 8 nine 9}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_3"]
		[12 13]
	)
	(store_entity
		"entity.caml"
		"Entity"
		(null)
		.true
		{flatten .true transactional .true}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_2"]
		[10 11]
	)
	(destroy_entities
		["Entity" "Contained1" "Contained1_3"]
	)
	(assign_to_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 88}
	)
	(load_entity
		"entity.caml"
		"EntityCopy"
		(null)
		.false
		{execute_on_load .true require_version_compatibility .true transactional .true}
	)
	(declare
		{
			diff (difference_entities "EntityCopy" "Entity")
		}
	)
	(destroy_entities "EntityCopy" "Entity")
	diff
)
```
Output:
```amalgam
(declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
)
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `contains_entity`
#### Parameters
`id_path entity`
#### Description
Returns true if `entity` exists, false if not.
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
		{a 1 b 2}
	)
	(create_entities
		["Entity" "Contained1"]
		{c 3}
	)
	[
		(contains_entity "Entity")
		(contains_entity
			["Entity" "NotAnEntity"]
		)
	]
)
```
Output:
```amalgam
[.true .false]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `flatten_entity`
#### Parameters
`id_path entity [bool include_rand_seeds] [bool parallel_create] [bool include_version]`
#### Description
Evaluates to code that, if called, would completely reproduce the `entity`, as well as all contained entities.  If `include_rand_seeds` is true, by default, it will include all entities' random seeds.  If `parallel_create` is true, then the creates will be performed with parallel markers as appropriate for each group of contained entities.  If `include_version` is true, it will include a comment on the top node that is the current version of the Amalgam interpreter, which can be used for validating interoperability when loading code.  The code returned accepts two parameters, `create_new_entity`, which defaults to true, and `new_entity`, which defaults to null.  If `create_new_entity` is true, then it will create a new entity using the id path specified by `new_entity`, where null will create an unnamed entity.  If `create_new_entity` is false, then it will overwrite the current entity's code and create all contained entities.
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
		"FlattenEntity"
		(lambda {})
	)
	(call_entity "FlattenEntity" "a")
	(create_entities
		["FlattenEntity" "DeepRand"]
		(lambda
			{a (rand)}
		)
	)
	(declare
		{
			flattened_code (flatten_entity "FlattenEntity" .true .true)
		}
	)
	(declare
		{first_rand (null) second_rand (null)}
	)
	(assign
		{
			first_rand (call_entity
					["FlattenEntity" "DeepRand"]
					"a"
				)
		}
	)
	(let
		{
			new_entity (call flattened_code)
		}
		(assign
			{
				second_rand (call_entity
						[new_entity "DeepRand"]
						"a"
					)
			}
		)
	)
	[first_rand second_rand]
)
```
Output:
```amalgam
[0.611779739433564 0.611779739433564]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `retrieve_entity_root`
#### Parameters
`[id_path entity]`
#### Description
Evaluates to the code contained by `entity`.
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
		(lambda
			{1 2 three 3}
		)
	)
	(retrieve_entity_root "Entity")
)
```
Output:
```amalgam
{1 2 three 3}
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `assign_entity_roots`
#### Parameters
`[id_path entity1] * root1 [id_path entity2] [* root2] [...]`
#### Description
Sets the code of the `entity1 to `root1`, as well as all subsequent entity-code pairs of parameters.  If `entity1` is not specified or null, then uses the current entity.  On assigning the code to the new entity, any root that is not of a type assoc will be put into an assoc under the null key.  If all assignments were successful, then returns true, otherwise returns false.
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
		(lambda
			{1 2 three 3}
		)
	)
	(assign_entity_roots
		"Entity"
		{a 4 b 5 c 6}
	)
	(retrieve_entity_root "Entity")
)
```
Output:
```amalgam
{a 4 b 5 c 6}
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `get_entity_permissions`
#### Parameters
`[id_path entity]`
#### Description
Returns an assoc of the permissions of `entity`, the current entity if `entity` is not specified or null, where each key is the permission and each value is either true or false.  Permission keys consist of: "std_out_and_std_err", which allows output; "std_in", which allows input; "load", which allows reading files; "store", which allows writing files; "environment", which allows reading information about the environment; "alter_performance", which allows adjusting performance characteristics; and "system", which allows running system commands.
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
		(lambda
			(print (system_time))
		)
	)
	(get_entity_permissions "Entity")
)
```
Output:
```amalgam
{
	alter_performance .false
	environment .false
	load .false
	std_in .false
	std_out_and_std_err .false
	store .false
	system .false
}
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `set_entity_permissions`
#### Parameters
`id_path entity bool|assoc permissions [bool deep]`
#### Description
Sets the permissions on the `entity`.  If permissions is true, then it grants all permissions, if it is false, then it removes all.  If permissions is an assoc, it alters the permissions of the assoc keys to the boolean values of the assoc's values.  Permission keys consist of: "std_out_and_std_err", which allows output; "std_in", which allows input; "load", which allows reading files; "store", which allows writing files; "environment", which allows reading information about the environment; "alter_performance", which allows adjusting performance characteristics; and "system", which allows running system commands.  The parameter `deep` defaults to false, but if it is true, all contained entities have their permissions updated.  Returns the id path of `entity`.
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
		(lambda
			(print (system_time))
		)
	)
	(set_entity_permissions "Entity" .true)
	(get_entity_permissions "Entity")
)
```
Output:
```amalgam
{
	alter_performance .true
	environment .true
	load .true
	std_in .true
	std_out_and_std_err .true
	store .true
	system .true
}
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

