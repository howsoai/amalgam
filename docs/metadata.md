### Opcode: `get_annotations`
#### Parameters
`* node`
#### Description
Returns a string comprising all of the annotation lines for `node`.
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
(get_annotations
	(lambda
		
		#annotation line 1
		#annotation line 2
		.true
	)
)
```
Output:
```amalgam
"annotation line 1\r\nannotation line 2"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `set_annotations`
#### Parameters
`* node [string new_annotation]`
#### Description
Evaluates to a new copy of `node` with the annotation specified by `new_annotation`, where each newline is a separate line of annotation.  If `new_annotation` is null or missing, it will clear annotations for `node`.
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
(unparse
	(set_annotations
		(lambda
			
			#labelC
			.true
		)
		["labelD" "labelE"]
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"#[\"labelD\" \"labelE\"]\r\n.true\r\n"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `get_comments`
#### Parameters
`* node`
#### Description
Returns a strings comprising all of the comment lines for `node`.
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
(get_comments
	(lambda
		
		;comment line 1
		;comment line 2
		.true
	)
)
```
Output:
```amalgam
"comment line 1\r\ncomment line 2"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `set_comments`
#### Parameters
`* node [string new_comment]`
#### Description
Evaluates to a new copy of `node` with the comment specified by `new_comment`, where each newline is a separate line of comment.  If `new_comment` is null or missing, it will clear comments for `node`.
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
(unparse
	(set_annotations
		(lambda
			
			#labelC
			.true
		)
		["labelD" "labelE"]
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"#[\"labelD\" \"labelE\"]\r\n.true\r\n"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `get_concurrency`
#### Parameters
`* node`
#### Description
Returns true if `node` has a preference to be processed in a manner where its operations are run concurrentl, false if it is not.  Note that concurrency is potentially subject to race conditions or inconsistent results if tasks write to the same locations without synchronization.
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
(get_concurrency
	(lambda
		(print "hello")
	)
)
```
Output:
```amalgam
.false
```
Example:
```amalgam
(get_concurrency
	(lambda
		||(print "hello")
	)
)
```
Output:
```amalgam
.true
```
Example:
```amalgam
(get_concurrency
	(set_concurrency
		(lambda
			(print "hello")
		)
		.true
	)
)
```
Output:
```amalgam
.true
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `set_concurrency`
#### Parameters
`* node bool concurrent`
#### Description
Evaluates to a new copy of `node` with the preference for concurrency set by `concurrent`.  Note that concurrency is potentially subject to race conditions or inconsistent results if tasks write to the same locations without synchronization.
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
(unparse
	(set_concurrency
		(lambda
			(print "hello")
		)
		.true
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
"||(print \"hello\")\r\n"
```
Example:
```amalgam
(unparse
	(set_concurrency
		(lambda
			
			;complex test
			
			#some annotation
			{a "hello" b 4}
		)
		.true
	)
	.true
	.true
	.true
)
```
Output:
```amalgam
";complex test\r\n#some annotation\r\n||{a \"hello\" b 4}\r\n"
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `get_value`
#### Parameters
`* node`
#### Description
Evaluates to a new copy of `node` without annotations, comments, or concurrency.
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
(get_value
	
	;first comment
	(lambda
		
		;second comment
		
		#annotation part 1
		#annotation part 2
		.true
	)
)
```
Output:
```amalgam
.true
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `set_value`
#### Parameters
`* target * val`
#### Description
Evaluates to a new copy of `node` with the value set to `val`, keeping existing annotations, comments, and concurrency).
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
(set_value
	
	;first comment
	(lambda
		
		;second comment
		.true
	)
	3
)
```
Output:
```amalgam
;second comment
3
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `get_entity_annotations`
#### Parameters
`[id_path entity] [string label] [bool deep_annotations]`
#### Description
Evaluates to the corresponding annotations for `entity`.  If `entity` is null then it will use the current entity.  If `label` is null or empty string, it will retrieve annotations for the entity root, otherwise if it is a valid `label` it will attempt to retrieve the annotations for that label, null if the label doesn't exist.  If `deep_annotations` is specified and the label is a declare, then it will return a list of two elements.  The first element of this list is an assoc with the keys being the parameters and the values being lists of the descriptions followed by the default value.  The second element of this list is the annotation of the assoc itself, which is intended to be used to describe what is returned.  If label is empty string or null and deep_annotations is true, then it will return an assoc of label to annotation for each label in the entity.
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
		"descriptive_entity"
		(lambda
			
			;this is a fully described entity
			
			#entity annotations
			{
				!privatevar 
					;some private variable
					
					#privatevar annotation
					2
				^containervar 
					;a variable accessible to contained entities
					
					#containervar annotation
					3
				foo 
					;the function foo
					
					#foo annotation
					(declare
						
						;a number representing the sum
						
						#return annotation
						{
							x 
								;the value of x
								;the default value of x
								
								#x annotation
								#x value annotation
								1
							y 
								;the value of y
								
								#y value annotation
								2
						}
						(+ x y)
					)
				get_api 
					;returns the api details
					
					#get_api annotation
					(seq
						{
							description (get_entity_comments)
							labels (map
									(lambda
										{
											description (current_value 1)
											parameters (get_entity_comments
													.null
													(current_index 1)
													.true
												)
										}
									)
									(get_entity_comments .null .null .true)
								)
						}
					)
				publicvar 
					;some public variable
					
					#publicvar annotation
					1
			}
		)
	)
	[
		(get_entity_annotations "descriptive_entity")
		(get_entity_annotations "descriptive_entity" .null .true)
		(get_entity_annotations "descriptive_entity" "foo" .true)
	]
)
```
Output:
```amalgam
[
	"entity annotations"
	{
		^containervar "containervar annotation"
		foo "foo annotation"
		get_api "get_api annotation"
		publicvar "publicvar annotation"
	}
	[
		{
			x ["x annotation\r\nx value annotation" 1]
			y ["y value annotation" 2]
		}
		"return annotation"
	]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `get_entity_comments`
#### Parameters
`[id_path entity] [string label] [bool deep_comments]`
#### Description
Evaluates to the corresponding comments for `entity`.  If `entity` is null then it will use the current entity.  If `label` is null or empty string, it will retrieve comments for the entity root, otherwise if it is a valid `label` it will attempt to retrieve the comments for that label, null if the label doesn't exist.  If `deep_comments` is specified and the label is a declare, then it will return a list of two elements.  The first element of this list is an assoc with the keys being the parameters and the values being lists of the descriptions followed by the default value.  The second element of this list is the comment of the assoc itself, which is intended to be used to describe what is returned.  If label is empty string or null and deep_comments is true, then it will return an assoc of label to comment for each label in the entity.
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
		"descriptive_entity"
		(lambda
			
			;this is a fully described entity
			
			#entity annotations
			{
				!privatevar 
					;some private variable
					
					#privatevar annotation
					2
				^containervar 
					;a variable accessible to contained entities
					
					#containervar annotation
					3
				foo 
					;the function foo
					
					#foo annotation
					(declare
						
						;a number representing the sum
						
						#return annotation
						{
							x 
								;the value of x
								;the default value of x
								
								#x annotation
								#x value annotation
								1
							y 
								;the value of y
								
								#y value annotation
								2
						}
						(+ x y)
					)
				get_api 
					;returns the api details
					
					#get_api annotation
					(seq
						{
							description (get_entity_comments)
							labels (map
									(lambda
										{
											description (current_value 1)
											parameters (get_entity_comments
													.null
													(current_index 1)
													.true
												)
										}
									)
									(get_entity_comments .null .null .true)
								)
						}
					)
				publicvar 
					;some public variable
					
					#publicvar annotation
					1
			}
		)
	)
	[
		(get_entity_comments "descriptive_entity")
		(get_entity_comments "descriptive_entity" .null .true)
		(get_entity_comments "descriptive_entity" "foo" .true)
	]
)
```
Output:
```amalgam
[
	"this is a fully described entity"
	{
		^containervar "a variable accessible to contained entities"
		foo "the function foo"
		get_api "returns the api details"
		publicvar "some public variable"
	}
	[
		{
			x ["the value of x\r\nthe default value of x" 1]
			y ["the value of y" 2]
		}
		"a number representing the sum"
	]
]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

