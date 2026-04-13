### Opcode: `rand`
#### Parameters
`[list|assoc|number range] [number number_to_generate] [bool unique]`
#### Description
Generates random values based on its parameters.  The random values are drawn from a random stream specific to each execution flow for each entity.  With no range, evaluates to a random number between 0.0 and 1.0.  If range is a list, it will uniformly randomly choose and evaluate to one element of the list.  If range is a number, it will evaluate to a value greater than or equal to zero and less than the number specified.  If range is an assoc, then it will randomly evaluate to one of the keys using the values as the weights for the probabilities.  If  number_to_generate is specified, it will generate a list of multiple values (even if number_to_generate is 1).  If unique is true (it defaults to false), then it will only return unique values, the same as selecting from the list or assoc without replacement.  Note that if unique only applies to list and assoc ranges.  If unique is true and there are not enough values in a list or assoc, it will only generate the number of elements in range.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): conditional
#### Examples
Example:
```amalgam
(rand)
```
Output:
```amalgam
0.4153759082605256
```
Example:
```amalgam
(rand 50)
```
Output:
```amalgam
20.768795413026282
```
Example:
```amalgam
(rand
	[1 2 4 5 7]
)
```
Output:
```amalgam
1
```
Example:
```amalgam
(rand
	(range 0 10)
)
```
Output:
```amalgam
4
```
Example:
```amalgam
(rand
	(range 0 10)
	0
)
```
Output:
```amalgam
[]
```
Example:
```amalgam
(rand
	(range 0 10)
	1
)
```
Output:
```amalgam
[4]
```
Example:
```amalgam
(rand
	(range 0 10)
	10
	.true
)
```
Output:
```amalgam
[
	4
	0
	5
	9
	10
	1
	2
	7
	6
	8
]
```
Example:
```amalgam
(rand 50 4)
```
Output:
```amalgam
[20.768795413026282 23.51742714184096 6.034392211178502 29.777315548569128]
```
Example:
```amalgam
(rand
	(associate "a" 0.25 "b" 0.75)
)
```
Output:
```amalgam
"b"
```
Example:
```amalgam
(rand
	(associate "a" 0.25 "b" 0.75)
	16
)
```
Output:
```amalgam

```
Example:
```amalgam
(rand
	(associate
		"a"
		0.25
		"b"
		0.75
		"c"
		.infinity
		"d"
		.infinity
	)
	4
)
```
Output:
```amalgam
["c" "c" "c" "d"]
```
Example:
```amalgam
;should come out somewhere near the correct proportion
(zip
	(lambda
		(+
			(current_value 1)
			(current_value)
		)
	)
	(rand
		(associate "a" 0.25 "b" 0.5 "c" 0.25)
		100
	)
	1
)
```
Output:
```amalgam
{a 30 b 50 c 20}
```
Example:
```amalgam
;these should be weighted toward smaller numbers
(rand
	(zip
		(range 1 10)
		(map
			(lambda
				(/
					(/ 1 (current_value))
					2
				)
			)
			(range 1 10)
		)
	)
	3
	.true
)
```
Output:
```amalgam
[2 6 1]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `get_rand_seed`
#### Parameters
``
#### Description
Evaluates to a string representing the current state of the random number generator.  Note that the string will be a string of bytes that may not be valid as UTF-8.
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
(format (get_rand_seed) "string" "base64")
```
Output:
```amalgam
"X6f8e5JTT5kuHHGZUu7r6/8="
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `set_rand_seed`
#### Parameters
`string seed`
#### Description
Initializes the random number stream for the given `seed` without affecting any entity.  If the seed is already a string in the proper format output by `get_entity_rand_seed` or `get_rand_seed`, then it will set the random generator to that current state, picking up where the previous state left off.  If it is anything else, it uses the value as a random seed to start the generator.
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
(seq
	(declare
		{cur_seed (get_rand_seed)}
	)
	(declare
		{
			first_pair [(rand) (rand)]
		}
	)
	(set_rand_seed cur_seed)
	(declare
		{
			second_pair [(rand) (rand)]
		}
	)
	(append first_pair second_pair)
)
```
Output:
```amalgam
[0.4153759082605256 0.47034854283681926 0.4153759082605256 0.47034854283681926]
```
Example:
```amalgam
(seq
	(set_rand_seed "12345")
	(rand)
)
```
Output:
```amalgam
0.5507987428849511
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `get_entity_rand_seed`
#### Parameters
`[id_path entity]`
#### Description
Evaluates to a string representing the current state of the random number generator for `entity` used for seeding the random streams of any calls to the entity.
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
		"Rand"
		(lambda
			{a (rand)}
		)
	)
	(call_entity "Rand" "a")
	(format
		(get_entity_rand_seed "Rand")
		"string"
		"base64"
	)
)
```
Output:
```amalgam
"nHKVcHddHVaqvcDt3AYbD/8="
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `set_entity_rand_seed`
#### Parameters
`[id_path entity] * node [bool deep]`
#### Description
Sets the random number seed and state for the random number generator of `entity`, or the current entity if null or not specified, to the state specified by `node`.  If `node` is already a string in the proper format output by `(get_entity_rand_seed)`, then it will set the random generator to that current state, picking up where the previous state left off.  If `node` is anything else, it uses the value as a random seed to start the generator.  Note that this will not affect the state of the current random number stream, only future random streams created by `entity` for new calls.  The parameter `deep` defaults to false, but if it is true, all contained entities are recursively set with random seeds based on the specified random seed and a hash of their relative id path to the entity being set.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: true
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(seq
	(create_entities
		"Rand"
		(lambda
			{a (rand)}
		)
	)
	(create_entities
		["Rand" "DeepRand"]
		(lambda
			{a (rand)}
		)
	)
	(declare
		{
			seed (get_entity_rand_seed "Rand")
		}
	)
	(declare
		{
			first_rand_numbers [
					(call_entity "Rand" "a")
					(call_entity
						["Rand" "DeepRand"]
						"a"
					)
				]
		}
	)
	(set_entity_rand_seed "Rand" seed .true)
	(declare
		{
			second_rand_numbers [
					(call_entity "Rand" "a")
					(call_entity
						["Rand" "DeepRand"]
						"a"
					)
				]
		}
	)
	[first_rand_numbers second_rand_numbers]
)
```
Output:
```amalgam
[
	[0.9512993766655248 0.3733350484591008]
	[0.9512993766655248 0.3733350484591008]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

