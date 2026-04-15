### Opcode: `total_entity_size`
#### Parameters
`id_path entity`
#### Description
Evaluates to the total count of all of the nodes of `entity` and all of its contained entities.  Each entity itself counts as multiple nodes, corresponding to flattening an entity via the `flatten_entity` opcode.
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
		"Entity1"
		{a 3 b 4}
	)
	(create_entities
		["Entity1" "EntityChild1"]
		{x 3 y 4}
	)
	(create_entities
		["Entity1" "EntityChild2"]
		{p 3 q 4}
	)
	(create_entities
		["Entity1"]
		{E 3 F 4}
	)
	(create_entities
		["Entity1"]
		{
			e 3
			f 4
			g 5
			h 6
		}
	)
	(total_entity_size "Entity1")
)
```
Output:
```amalgam
67
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `mutate_entity`
#### Parameters
`id_path source_entity [number mutation_rate] [id_path dest_entity] [assoc mutation_weights] [assoc operation_type] [preserve_type_depth]`
#### Description
Creates a mutated version of the entity specified by `source_entity` like mutate. Returns the id path of a new entity created contained by the entity that ran it.  The value specified by `mutation_rate`, from 0.0 to 1.0 and defaulting to 0.00001, indicates the probability that any node will experience a mutation.  Uses `dest_entity` as the optional destination.  The parameter `mutation_weights` is an assoc where the keys are the allowed opcode names and the values are the probabilities that each opcode would be chosen; if null or unspecified, it defaults to all opcodes each with their own default probability.  The `operation_type` is an assoc where the keys are mutation operations and the values are the probabilities that the operations will be performed.  The operations can consist of the strings "change_type", "delete", "insert", "swap_elements", "deep_copy_elements", and "delete_elements".  If `preserve_type_depth` is specified, it will retain the types of node down to and including whatever depth is specified, and defaults to 1 indicating that the top level of the entities will have a preserved type, namely an assoc.
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
		"MutateEntity"
		(lambda
			{
				a 1
				b 2
				c 3
				d 4
				e 5
				f 6
				g 7
				h 8
				i 9
				j 10
				k 11
				l 12
				m 13
				n 14
				o (associate "a" 1 "b" 2)
			}
		)
	)
	(mutate_entity "MutateEntity" 0.4 "MutatedEntity1")
	(mutate_entity "MutateEntity" 0.5 "MutatedEntity2")
	(mutate_entity
		"MutateEntity"
		0.5
		"MutatedEntity3"
		(associate "+" 0.5 "-" 0.3 "*" 0.2)
		(associate "change_type" 0.08 "delete" 0.02 "insert" 0.9)
	)
	[
		(retrieve_entity_root "MutatedEntity1")
		(retrieve_entity_root "MutatedEntity2")
		(retrieve_entity_root "MutatedEntity3")
	]
)
```
Output:
```amalgam
[
	{
		a 1
		b 2
		c 3
		d (set_type)
		e (if)
		f (>=)
		g (<=)
		h 8
		i 9
		j 10
		k 11
		l 12
		m 13
		n -20.325081516830192
		o "b"
	}
	{
		a 1
		b (map)
		c (min)
		d 4
		e 5
		f (apply)
		g 7
		h (get_type_string)
		i (round)
		j 10
		k (lambda)
		l 12
		m (declare)
		n 14
		o (map)
	}
	{
		a (*)
		b (*)
		c 3
		d 4
		e (+)
		f (*)
		g (-)
		h 8
		i 9
		j 10
		k 11
		l 12
		m (+)
		n 14
		o (associate (-) 1 (*) (+))
	}
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `commonality_entities`
#### Parameters
`id_path entity1 id_path entity2 [assoc params]`
#### Description
Evaluates to the total count of all of the nodes referenced within `entity1` and `entity2` that are equivalent, including all contained entities.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.
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
		"MergeEntity1"
		{a 3 b 4 c "c1"}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild1"]
		{x 3 y 4}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild2"]
		{p 3 q 4}
	)
	(create_entities
		["MergeEntity1"]
		{E 3 F 4}
	)
	(create_entities
		["MergeEntity1"]
		{
			e 3
			f 4
			g 5
			h 6
		}
	)
	(create_entities
		"MergeEntity2"
		{b 4 c "c2"}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild1"]
		{x 3 y 4 z 5}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild2"]
		{
			p 3
			q 4
			u 5
			v 6
			w 7
		}
	)
	(create_entities
		["MergeEntity2"]
		{
			E 3
			F 4
			G 5
			H 6
		}
	)
	(create_entities
		["MergeEntity2"]
		{e 3 f 4}
	)
	[
		(commonality_entities "MergeEntity1" "MergeEntity2")
		(commonality_entities
			"MergeEntity1"
			"MergeEntity2"
			{nominal_strings .false types_must_match .false}
		)
	]
)
```
Output:
```amalgam
[64 64.74178574543642]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `edit_distance_entities`
#### Parameters
`id_path entity1 id_path entity2 [assoc params]`
#### Description
Evaluates to the edit distance of all of the nodes referenced within `entity1` and `entity2` that are equivalent, including all contained entities.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.
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
	(seq
		(create_entities
			"MergeEntity1"
			{a 3 b 4 c "c1"}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild1"]
			{x 3 y 4}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild2"]
			{p 3 q 4}
		)
		(create_entities
			["MergeEntity1"]
			{E 3 F 4}
		)
		(create_entities
			["MergeEntity1"]
			{
				e 3
				f 4
				g 5
				h 6
			}
		)
		(create_entities
			"MergeEntity2"
			{b 4 c "c2"}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild1"]
			{x 3 y 4 z 5}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild2"]
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
		(create_entities
			["MergeEntity2"]
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
		(create_entities
			["MergeEntity2"]
			{e 3 f 4}
		)
		[
			(edit_distance_entities "MergeEntity1" "MergeEntity2")
			(edit_distance_entities
				"MergeEntity1"
				"MergeEntity2"
				{nominal_strings .false types_must_match .false}
			)
		]
	)
)
```
Output:
```amalgam
[11 9.516428509127167]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `intersect_entities`
#### Parameters
`id_path entity1 id_path entity2 [assoc params] [id_path entity3]`
#### Description
Creates an entity of whatever is common between the entities `entity1` and `entity2` exclusive.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true, the default, then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  Uses `entity3` as the optional destination via an internal call create_contained_entity.  Any contained entities will be intersected either based on matching name or maximal similarity for nameless entities.
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
	(seq
		(create_entities
			"MergeEntity1"
			{a 3 b 4 c "c1"}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild1"]
			{x 3 y 4}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild2"]
			{p 3 q 4}
		)
		(create_entities
			["MergeEntity1"]
			{E 3 F 4}
		)
		(create_entities
			["MergeEntity1"]
			{
				e 3
				f 4
				g 5
				h 6
			}
		)
		(create_entities
			"MergeEntity2"
			{b 4 c "c2"}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild1"]
			{x 3 y 4 z 5}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild2"]
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
		(create_entities
			["MergeEntity2"]
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
		(create_entities
			["MergeEntity2"]
			{e 3 f 4}
		)
		(intersect_entities "MergeEntity1" "MergeEntity2" .null "IntersectedEntity")
		[
			(retrieve_entity_root "IntersectedEntity")
			(sort
				(contained_entities "IntersectedEntity")
			)
		]
	)
)
```
Output:
```amalgam
[
	{b 4 c .null}
	["MergeEntityChild1" "MergeEntityChild2" "_2bW5faQkVxs" "_ldZa276M1io"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `union_entities`
#### Parameters
`id_path entity1 id_path entity2 [assoc params] [id_path entity3]`
#### Description
Creates an entity of whatever is inclusive when merging the entities `entity1` and `entity2`.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true, the default, then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  Uses `entity3` as the optional destination via an internal call to create_contained_entity.  Any contained entities will be unioned either based on matching name or maximal similarity for nameless entities.
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
	(seq
		(create_entities
			"MergeEntity1"
			{a 3 b 4 c "c1"}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild1"]
			{x 3 y 4}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild2"]
			{p 3 q 4}
		)
		(create_entities
			["MergeEntity1"]
			{E 3 F 4}
		)
		(create_entities
			["MergeEntity1"]
			{
				e 3
				f 4
				g 5
				h 6
			}
		)
		(create_entities
			"MergeEntity2"
			{b 4 c "c2"}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild1"]
			{x 3 y 4 z 5}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild2"]
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
		(create_entities
			["MergeEntity2"]
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
		(create_entities
			["MergeEntity2"]
			{e 3 f 4}
		)
		(union_entities "MergeEntity1" "MergeEntity2" .null "UnionedEntity")
		[
			(retrieve_entity_root "UnionedEntity")
			(sort
				(contained_entities "UnionedEntity")
			)
		]
	)
)
```
Output:
```amalgam
[
	{a 3 b 4 c .null}
	["MergeEntityChild1" "MergeEntityChild2" "_2bW5faQkVxs" "_ldZa276M1io"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `difference_entities`
#### Parameters
`id_path entity1 id_path entity2`
#### Description
Finds the difference between the entities specified by `entity1` and `entity2` and generates code that, if evaluated passing the entity id_path as its parameter "_", would create a new entity into the id path specified by its parameter "new_entity" (null if unspecified), which would contain the applied difference between the two entities and returns the newly created entity id path.  Useful for finding a small difference of what needs to be changed to apply it to new (and possibly slightly different) entity.
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
		"DiffEntity1"
		(lambda
			{a 3 b 4}
		)
	)
	(create_entities
		["DiffEntity1" "DiffEntityChild1"]
		(lambda
			{x 3 y 4 z 6}
		)
	)
	(create_entities
		["DiffEntity1" "DiffEntityChild1" "DiffEntityChild2"]
		(lambda
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity1" "DiffEntityChild1" "DiffEntityChild2" "DiffEntityChild3"]
		(lambda
			{
				a 5
				e 3
				o 6
				p 4
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity1" "OnlyIn1"]
		(lambda
			{m 4}
		)
	)
	(create_entities
		["DiffEntity1"]
		(lambda
			{e 3 f 4}
		)
	)
	(create_entities
		["DiffEntity1"]
		(lambda
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
	)
	(create_entities
		"DiffEntity2"
		(lambda
			{b 4 c 3}
		)
	)
	(create_entities
		["DiffEntity2" "DiffEntityChild1"]
		(lambda
			{x 3 y 4 z 5}
		)
	)
	(create_entities
		["DiffEntity2" "DiffEntityChild1" "DiffEntityChild2"]
		(lambda
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity2" "DiffEntityChild1" "DiffEntityChild2" "DiffEntityChild3"]
		(lambda
			{
				a 5
				e 3
				o 6
				p 4
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity2" "OnlyIn2"]
		(lambda
			{o 6}
		)
	)
	(create_entities
		["DiffEntity2"]
		(lambda
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
	)
	(create_entities
		["DiffEntity2"]
		(lambda
			{e 3 f 4}
		)
	)
	
	;applying the difference to DiffEntity1 results in an entity identical to DiffEntity2
	(let
		{
			new_entity (call
					(difference_entities "DiffEntity1" "DiffEntity2")
					{_ "DiffEntity1"}
				)
		}
		(difference_entities "DiffEntity2" new_entity)
	)
)
```
Output:
```amalgam
(declare
	{_ .null new_entity .null}
	(clone_entities _ new_entity)
)
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `mix_entities`
#### Parameters
`id_path entity1 id_path entity2 [number keep_chance_entity1] [number keep_chance_entity2] [assoc params] [id_path entity3]`
#### Description
Performs a union operation on the entities represented by `entity1` and `entity2`, but randomly ignores nodes from one or the other tree if not equal.  If only `keep_chance_entity1` is specified, `keep_chance_entity2` defaults to 1 - `keep_chance_entity1`.  `keep_chance_entity1` specifies the probability that a node from the entity represented by `entity1` will be kept, and `keep_chance_entity2` the probability that a node from the entity represented by `entity2` will be kept.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  `similar_mix_chance` is the additional probability that two nodes will mix if they have some commonality, which will include interpolating number and string values based on `keep_chance_node1` and `keep_chance_node2`, and defaults to 0.0.  If `similar_mix_chance` is negative, then 1 minus the value will be anded with the commonality probability, so -1 means that it will never mix and 0 means it will only mix when sufficiently common.  `unnamed_entity_mix_chance` represents the probability that an unnamed entity pair will be mixed versus preserved as independent chunks, where 0.2 would yield 20% of the entities mixed. Returns the id path of a new entity created contained by the entity that ran it.  Uses `entity3` as the optional destination entity.   Any contained entities will be mixed either based on matching name or maximal similarity for nameless entities.
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
		"MergeEntity1"
		{a 3 b 4 c "c1"}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild1"]
		{x 3 y 4}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild2"]
		{p 3 q 4}
	)
	(create_entities
		["MergeEntity1"]
		{E 3 F 4}
	)
	(create_entities
		["MergeEntity1"]
		{
			e 3
			f 4
			g 5
			h 6
		}
	)
	(create_entities
		"MergeEntity2"
		{b 4 c "c2"}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild1"]
		{x 3 y 4 z 5}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild2"]
		{
			p 3
			q 4
			u 5
			v 6
			w 7
		}
	)
	(create_entities
		["MergeEntity2"]
		{
			E 3
			F 4
			G 5
			H 6
		}
	)
	(create_entities
		["MergeEntity2"]
		{e 3 f 4}
	)
	(mix_entities
		"MergeEntity1"
		"MergeEntity2"
		0.5
		0.5
		{similar_mix_chance 0.5 unnamed_entity_mix_chance 0.2}
		"MixedEntities"
	)
	[
		(retrieve_entity_root "MixedEntities")
		(sort
			(contained_entities "MixedEntities")
		)
	]
)
```
Output:
```amalgam
[
	{b 4 c "c1"}
	["MergeEntityChild1" "MergeEntityChild2" "_2bW5faQkVxs" "_ldZa276M1io"]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

