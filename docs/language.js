
var data = [
/*
	{
		"parameter" : "Token params column text goes here",
		"output" : "Output column text goes here",
		"permissions" : "P column text", 	//optional, one of: std_out_and_std_err, std_in, load, store, environment, alter_performance, system
		"new value" : "N column text", 		//optional, one of: new, conditional, partial
		"description" : "Description column text goes here",
		"example" : ""			//insert this everywhere but leave it blank like so
	},
*/
	{
		"parameter" : "system string command",
		"output" : "*",
		"permissions" : "depends on command",
		"new value" : "new",
		"description" : "Executes system command specified by command.  See the system commands table for further information.",
		"example" : "(system \"exit\")"
	},

	{
		"parameter" : "get_defaults string value_type",
		"output" : "*",
		"description" : "Retrieves the default values of the named field, either \"mutation_opcodes\" or \"mutation_types\"",
		"example" : "(get_defaults mutation_opcodes)"
	},
	
	{
		"parameter" : "reclaim_resources [id_path entity] [bool apply_to_all_contained_entities] [bool clear_query_caches] [bool collect_garbage] [bool force_free_memory] ",
		"output" : "*",
		"permissions" : "alter_performance",
		"description" : "Frees resources of the specified types on entity, which is the current entity if null, and will include all contained entities if apply_to_all_contained_entities is true, which defaults to false, though the opcode will be unable to complete if there are concurrent threads running on any of the contained entities.  The parameter clear_query_caches will remove the query caches, which will make it faster to add, remove, or edit contained entities, but the cache will be rebuilt once a query is called.  The parameter collect_garbage will perform garbage collection on the entity, and if force_free_memory is true, it will reallocate memory buffers to their current size, after garbage collection if both are specified.",
		"example" : "(reclaim_resources (null) .true .false .true .false)"
	},

	{
		"parameter" : "parse string str [bool transactional] [bool return_warnings]",
		"output" : "*",
		"new value" : "new",
		"description" : "String is parsed into code, and the result is returned.  If transactional is false, the default, it will attempt to parse the whole string and will return the closest code possible if there are any parse issues.  If transactional is true, it will parse the string transactionally, meaning that any node that has a parse error or is incomplete will be omitted along with all child nodes except for the top node.  If any performance constraints are given or return_warnings is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is an assoc mapping all warnings to their number of occurrences, and perf_constraint violation is a string denoting the constraint exceeded (or (null) if none)), unless return_warnings is false, in which case just the value will be returned.",
		"example" : "(parse \"(list 1 2 3 4 5)\")"
	},

	{
		"parameter" : "unparse code c [bool pretty_print] [bool sort_keys]",
		"output" : "string",
		"new value" : "new",
		"description" : "Code is unparsed and the representative string is returned. If the pretty-print boolean is passed as true, output will be in pretty-print format, otherwise by default it will be inlined.  If sort_keys is true, then in will print assoc structures and anything that could come in different orders in a natural sorted order by key, otherwise it will default to whatever order it is stored in memory.",
		"example" : "(unparse (lambda (+ 4 3)) .true)"
	},

	{
		"parameter" : "if [bool condition1] [code then1] [bool condition2] [code then2] ... [bool conditionN] [code thenN] [code else]",
		"output" : "*",
		"description" : "If the condition1 bool is true, then it will evaluate to the then1 argument.  Otherwise condition2 will be checked, repeating for every pair.  If there is an odd number of parameters, the last is the final 'else', and will be evaluated as that if all conditions are false.  If there is an even number of parameters and none are true, then evaluates to null.",
		"example" : "(if (null) (print \"nothing\") 0 (print \"nothing\") (print \"hello\") )"
	},

	{
		"parameter" : "seq [code c1] [code c2] ... [code cN]",
		"output" : "*",
		"description" : "Runs each code block sequentially. Evaluates to the result of the last code block run, unless it encounters a conclude or return in an earlier step, in which case it will halt processing and evaluate to the value returned by conclude or propagate the return. Note that the last step will not consume a concluded value.",
		"example" : "(seq (print 1) (print 2) (print 3))"
	},

	{
		"parameter" : "parallel [code c1] [code c2] ... [code cN]",
		"output" : "null",
		"concurrency" : true,
		"description" : "Runs each code block, possibly in any order. Evaluates to null",
		"example" : "(parallel (assign (assoc foo 1)) (assign (assoc bar 2)))"
	},

	{
		"parameter" : "lambda * function [bool evaluate_and_wrap]",
		"output" : "*",
		"description" : "Evaluates to the code specified without evaluating it.  Useful for referencing functions or handling data without evaluating it.  The parameter evaluate_and_wrap defaults to false, but if it is true, it will evaluate the function, but then return the result wrapped in a lambda opcode.",
		"example" : "(declare (assoc foo (lambda\n  (declare (assoc x 6)\n  (+ x 2)\n)))"
	},

	{
		"parameter" : "conclude * conclusion",
		"output" : "*",
		"description" : "Evaluates to the conclusion wrapped in a conclude opcode.  If a step in a seq, let, declare, or while evaluates to a conclude (excluding variable declarations for let and declare, the last step in set, let, and declare, or the condition of while), then it will conclude the execution and evaluate to the value conclusion.  Note that conclude opcodes may be nested to break out of outer opcodes.",
		"example" : "(print (seq (print \"seq1 \") (conclude \"success\") (print \"seq2\") ) )"
	},
	
	{
		"parameter" : "return * return_value",
		"output" : "*",
		"description" : "Evaluates to return_value wrapped in a return opcode.  If a step in a seq, let, declare, or while evaluates to a return (excluding variable declarations for let and declare, the last step in set, let, and declare, or the condition of while), then it will conclude the execution and evaluate to the return opcode with its return_value.  This means it will continue to conclude each level up the stack until it reaches any kind of call opcode, including call, call_sandboxed, call_entity, call_entity_get_changes, or call_container, at which point it will evaluate to return_value.  Note that return opcodes may be nested to break out of multiple calls.",
		"example" : " (print (call (seq 1 2 (seq (return 3) 4) 5)) \"\\n\")"
	},

	{
		"parameter" : "call * function assoc arguments",
		"output" : "*",
		"new scope" : true,
		"description" : "Evaluates the code after pushing the arguments assoc onto the scope stack.",
		"example" : "(call foo (assoc x 3))"
	},

	{
		"parameter" : "call_sandboxed * function assoc arguments [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [bool return_warnings]",
		"output" : "*",
		"new scope" : true,
		"description" : "Evaluates the code specified by *, isolating it from everything except for arguments, which is used as a single layer of the scope stack.  This is useful when evaluating code passed by other entities that may or may not be trusted.  Opcodes run from within call_sandboxed that require any form of permissions will not perform any action and will evaluate to null.  If operation_limit is specified, it represents the number of operations that are allowed to be performed. If operation_limit is 0 or infinite, then an infinite of operations will be allotted, up to the limits of the current calling context. If max_node_allocations is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If max_node_allocations is 0 or infinite and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if max_node_allocations is specified while call_sandboxed is being called in a multithreaded environment, if the collective memory from all the related threads exceeds the average memory specified by call_sandboxed, that may trigger a memory limit for the call_sandboxed.  If max_opcode_execution_depth is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise max_opcode_execution_depth limits how deep nested opcodes will be called. If any performance constraints are given or return_warnings is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or (null) if none)), unless return_warnings is false, in which case just the value will be returned.",
		"example" : ";x will be null because it cannot be accessed\n(call_sandboxed (lambda (+ y x 4)) (assoc y 3))"
	},

	{
		"parameter" : "while bool condition [code code1] [code code2] ... [code codeN]",
		"output" : "*",
		"new target scope": true,
		"description" : "Each time the condition evaluates to true, it runs each of the code trees sequentially, looping. Evaluates to the last codeN or null if the condition was initially false or if it encounters a conclude or return, it will halt processing and evaluate to the value returned by conclude or propagate the return.  For iteration of the loop, pushes a new target scope onto the target stack, with current_index being the iteration count, and previous_result being the last evaluated codeN of the previous loop.",
		"example" : "(let (assoc zz 1)\n  (while (< zz 10)\n    (print zz)\n    (assign (assoc zz (+ zz 1)))\n  )\n)"
	},

	{
		"parameter" : "let assoc data [code function1] [code function2] ... [code functionN]",
		"output" : "*",
		"new scope" : true,
		"description" : "Pushes the key-value pairs of data onto the scope stack so that they become the new variables, then runs each code block sequentially, evaluating to the last code block run, unless it encounters a conclude or return, in which case it will halt processing and evaluate to the value returned by conclude or propagate the return.  Note that the last step will not consume a concluded value.",
		"example" : "(let (assoc x 4 y 6) (print (+ x y)))"
	},

	{
		"parameter" : "declare assoc data [code function1] [code function2] ... [code functionN]",
		"output" : "*",
		"description" : "For each key-value pair of data, if not already in the current context in the scope stack, it will define them.  Then runs each code block sequentially, evaluating to the last code block run, unless it encounters a conclude or return, in which case it will halt processing and evaluate to the value returned by conclude or propagate the return.  Note that the last step will not consume a concluded value.",
		"example" : "(let (assoc x 4 y 6)\n  (declare (assoc x 5 z 1)\n    (print (+ x y z)) )\n)"
	},

	{
		"parameter" : "assign assoc data|string variable_name [number index1|string index1|list walk_path1|* new_value1] [* new_value1] [number index2|string index2|list walk_path2] [* new_value2] ...",
		"output" : "null",
		"description" : "If the assoc data is specified, then for each key-value pair of data, assigns the value to the variable represented by the key found by tracing upward on the stack. If none found, it will create a variable on the top of the stack. If the string variable_name is specified, then it will find the variable by tracing up the stack and then use each pair of walk_path and new_value to assign new_value to that part of the variable's structure.  If there are only two parameters, then it will assign the second parameter to the variable represented by the first.",
		"example" : "(print (assign (assoc x 10)))\n(print x)\n(print (assign \"x\" 10)"
	},

	{
		"parameter" : "accum assoc data|string variable_name [number index1|string index1|list walk_path1] [* accum_value1] [number index2|string index2|list walk_path2] [* accum_value2] ...",
		"output" : "null",
		"description" : "If the assoc data is specified, then for each key-value pair of data, assigns the value of the pair accumulated with the current value of the variable represented by the key on the stack, and stores the sum in the variable.  It searches for the variable name tracing up the stack to find the variable. If none found, it will create a variable on the top of the stack. Accumulation is performed differently based on the type: for numeric values it adds, for strings, it concatenates, for lists it appends, and for assocs it appends based on the pair. If the string variable_name is specified, then it will find the variable by tracing up the stack and then use each pair of walk_path and new_value to accum accum_value to that part of the variable's structure.  If there are only two parameters, then it will accum the second parameter to the variable represented by the first.",
		"example" : "(print (assign (assoc x 10)))\n(print x)\n(print (accum (assoc x 1)))\n(print x)"
	},

	{
		"parameter" : "retrieve [string variable_name|list variable_names|assoc index_set]",
		"output" : "*",
		"description" : "If string specified, gets the value on the stack specified by the string. If list specified, returns a list of the values on the stack specified by each element of the list interpreted as a string. If assoc specified, returns an assoc with the indices of the assoc which was passed in with the values being the appropriate values on the stack for each index.",
		"example" : "(retrieve \"my_variable\")\n(assign (assoc rwww 1 raaa 2))\n(print (retrieve \"rwww\"))\n(print (retrieve (list \"rwww\" \"raaa\")))\n(print (retrieve (zip (list \"rwww\" \"raaa\") null)))\n"
	},
	
	{
		"parameter" : "get * data [number index|string index|list walk_path_1] [number index|string index|list walk_path_2] ...",
		"output" : "*",
		"description" : "Evaluates to data as traversed by the set of values specified by the second parameter, which can be any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values.  If multiple walk paths are specified, then get returns a list, where each element in the list is the respective element retrieved by the respective walk path. If the walk path continues past the data structure, it will return a (null).",
		"example" : "(print (get (list 1 2 3)))\n(print (get (list 4 9.2 \"this\") 1))\n(print (get (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\") \"c\"))\n(print (get (list 0 1 2 3 (list 0 1 2 (assoc \"a\" 1))) (list 4 3 \"a\")))\n (print (get (list 4 9.2 \"this\") 1 2) \"\\n\")"
	},

	{
		"parameter" : "set * data [number index1|string index1|list walk_path1] [* new_value1] [number index2|string index2|list walk_path2] [* new_value2] ...",
		"output" : "*",
		"new value" : "new",
		"description" : "Performs a deep copy on data (a copy of all data structures referenced by it and its references), then looks at the remaining parameters as pairs.  For each pair, the first is any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values as a walk path of indices. new_value1 to new_valueN represent a value that will be used to replace  whatever is in the location the preceding location parameter specifies. If a particular location does not exist, it will be created assuming the most generic type that will support the index (as a null, list, or assoc); however, it will not change the type of immediate values to an assoc or list. Note that the target operation will evaluate to the new copy of data, which is the base of the newly constructed data; this is useful for creating circular references.",
		"example" : "(print (set (list 1 2 3 4) 2 7))\n(print (set\n  (list (assoc \"a\" 1))\n    (list 2) 1\n    (list 1) (get (target) 0)))"
	},

	{
		"parameter" : "replace * data [number index1|string index1|list walk_path1] [* function1] [number index2|string index2|list walk_path2] [* function2] ...",
		"output" : "*",
		"new value" : "new",
		"new target scope": true,
		"description" : "Performs a deep copy on data (a copy of all data structures referenced by it and its references), then looks at the remaining parameters as pairs.  For each pair, the first is any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values. function1 to functionN represent a function that will be used to replace in place of whatever is in the location of the corresponding walk_path, and will be passed the current node in (current_value).  The function does not need to be a function and can just be a constant (which it will be evaluated as).  If a particular location does not exist, it will be created assuming the most generic type that will support the index (as a null, list, or assoc). Note that the target operation will evaluate to the new copy of data, which is the base of the newly constructed data; this is useful for creating circular references.",
		"example" : "(print (replace (list (assoc \"a\" 13)) ))\n(print (replace\n    (list (assoc \"a\" 1))\n	   (list 2) 1\n	   (list 0) (list 4 5 6)))\n\n(print (replace\n    (list (assoc \"a\" 1))\n	   (list 0) (lambda (set (current_value) \"b\" 2))\n ))"
	},

	{
		"parameter" : "target [number stack_distance|bool top_of_stack] [number index|string index|list walk_path]",
		"output" : "*",
		"description" : "Evaluates to the current node that is being iterated over, or the base code of a set or replace that is being created.  Useful for serializing graph data structures or looking up data during iteration.  If stack_distance is specified, it climbs back up the target stack that many levels, but if the parameter is a boolean, then true indicates the top of the stack and false indicates the bottom.  If walk_path is specified, it will walk up the index path from the corresponding target stack.  If building an object, specifying top_of_stack to true can be useful for accessing or traversing the top level elements.",
		"example" : ";prints the list of what has been created before its return value is included in the list\n(list 1 2 3 (print (target)) 4)\n (let (assoc moveref (list 0 (list 7 8) (get (target 0) 1) ) )\n  (assign (assoc moveref (set moveref 1 1)))\n  (print moveref)\n)"
	},

	{
		"parameter" : "current_index [number stack_distance]",
		"output" : "*",
		"new value" : "new",
		"description" : "Evaluates to the index of the current node being iterated on within the current target.  If stack_distance is specified, it climbs back up the target stack that many levels.",
		"example" : "(list 1 2 3 (print (current_index)) 4)"
	},

	{
		"parameter" : "current_value [number stack_distance]",
		"output" : "*",
		"description" : "Evaluates to the current node being iterated on within the current target.  If stack_distance is specified, it climbs back up the target stack that many levels.",
		"example" : "(list 1 2 3 (print (current_value)) 4)"
	},
	
	{
		"parameter" : "previous_result [number stack_distance] [bool copy]",
		"output" : "*",
		"description" : "Evaluates to the resulting node of the previous iteration for applicable opcodes. If stack_distance is specified, it climbs back up the target stack that many levels.  If copy is true, then a copy of the resulting node of the previous iteration is returned, otherwise the result of the previous iteration is returned directly and consumed.",
		"example" : "(while (< (target_index) 3) (print (previous_result)) (target_index))"
	},
	
	{
		"parameter" : "opcode_stack [number stack_distance] [bool no_child_nodes]",
		"output" : "list of *",
		"description" : "Evaluates to the list of opcodes that make up the call stack or a single opcode within the call stack. If stack_distance is specified, then a copy of the node at that specified depth is returned, otherwise the list of all opcodes in opcode stack are returned. Negative values for stack_distance specify the depth from the top of the stack and positive values specify the depth from the bottom. If no_child_nodes is true, then only the root node(s) are returned, otherwise the returned node(s) are deep-copied.",
		"example" : "(print (opcode_stack))"
	},

	{
		"parameter" : "stack",
		"output" : "list of assoc",
		"description" : "Evaluates to the current execution context, also known as the scope stack, containing all of the variables for each layer of the stack.",
		"example" : "(print (stack))"
	},

	{
		"parameter" : "args [number stack_distance]",
		"output" : "assoc",
		"description" : "Evaluates to the top context of the stack, the current execution context, or scope stack, known as the arguments. If number is specified, then it evaluates to the context that many layers up the stack.",
		"example" : "(let (assoc \"bbb\" 3)\n  (print (args))\n)"
	},
	
	{
		"parameter" : "rand [list|assoc|number range] [number number_to_generate] [bool unique]",
		"output" : "*",
		"new value" : "conditional",
		"description" : "Generates random values based on its parameters.  The random values are drawn from a random stream specific to each execution flow for each entity.  With no range, evaluates to a random number between 0.0 and 1.0.  If range is a list, it will uniformly randomly choose and evaluate to one element of the list.  If range is a number, it will evaluate to a value greater than or equal to zero and less than the number specified.  If range is an assoc, then it will randomly evaluate to one of the keys using the values as the weights for the probabilities.  If  number_to_generate is specified, it will generate a list of multiple values (even if number_to_generate is 1).  If unique is true (it defaults to false), then it will only return unique values, the same as selecting from the list or assoc without replacement.  Note that if unique only applies to list and assoc ranges.  If unique is true and there are not enough values in a list or assoc, it will only generate the number of elements in range.",
		"example" : "(print (rand))\n(print (rand 50))\n(print (rand (list 1 2 4 5 7)))\n(print (rand (range 0 10) 10 .true) \"\\n\")"
	},

	{
		"parameter" : "get_rand_seed",
		"output" : "string",
		"new value" : "new",
		"description" : "Evaluates to a string representing the current state of the random number generator.",
		"example" : "(print (get_rand_seed) \"\\n\")"
	},

	{
		"parameter" : "set_rand_seed * node",
		"output" : "string",
		"description" : "Sets the random number seed and state for the current random number stream without affecting any entity.  If node is already a string in the proper format output by get_entity_rand_seed, then it will set the random generator to that current state, picking up where the previous state left off.  If it is anything else, it uses the value as a random seed to start the generator.",
		"example" : " (declare (assoc cur_seed (get_rand_seed)))\n (print (rand) \"\\n\")\n (set_rand_seed cur_seed)\n (print (rand) \"\\n\")"
	},

	{
		"parameter" : "system_time",
		"output" : "number",
		"permissions" : "environment",
		"description" : "Evaluates to the current system time since epoch in seconds (including fractions of seconds).",
		"example" : "(print (system_time))"
	},

	{
		"parameter" : "+ [number x1] [number x2] ... [number xN]",
		"output" : "number",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Sums all numbers.",
		"example" : "(print (+ 1 2 3 4))"
	},

	{
		"parameter" : "- [number x1] [number x2] ... [number xN]",
		"output" : "number",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to x1 - x2 - ... - xN. If only one parameter is passed, then it is treated as negative",
		"example" : "(print (- 1 2 3 4))"
	},

	{
		"parameter" : "* [number x1] [number x2] ... [number xN]",
		"output" : "number",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to the product of all numbers.",
		"example" : "(print (* 1 2 3 4))"
	},

	{
		"parameter" : "/ [number x1] [number x2] ... [number xN]",
		"output" : "number",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to x1 / x2 / ... / xN.",
		"example" : "(print (/ 1.0 2 3 4))"
	},

	{
		"parameter" : "mod [number x1] [number x2] ... [number xN]",
		"output" : "number",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates the modulus of x1 % x2 % ... % xN.",
		"example" : "(print (mod 1 2 3 4))"
	},

	{
		"parameter" : "get_digits number value [number base] [number start_digit] [number end_digit] [bool relative_to_zero]",
		"output" : "list of number",
		"new value" : "new",
		"description" : "Evaluates to a list of the number of each digit of value for the given base.  If base is omitted, 10 is the default.  The parameters start_digit and end_digit can be used to get a specific set of digits, but can also be infinite or null to catch all the digits on one side of the number.  The interpretation of start_digit and end_digit are with respect to relative_to_zero, which defaults to true.  If relative_to_zero is true, then the digits are indexed from their distance to zero, such as \"5 4 3 2 1 0 . -1 -2\".  If relative_to_zero is false, then the digits are indexed from their most significant digit, such as \"0 1 2 3 4 5 . 6  7\".  The default values of start_digit and end_digit are the most and least significant digits respectively.",
		"example" : "(print (get_digits 16 8 .infinity 0))\n(print (get_digits 3 2 5 0))\n(print (get_digits 1.5 1.5 .infinity 0))"
	},

	{
		"parameter" : "set_digits number value [number base] [list of number or null digits] [number start_digit] [number end_digit] [bool relative_to_zero]",
		"output" : "number",
		"new value" : "new",
		"description" : "Evaluates to a number having each of the values in the list of digits replace each of the relative digits in value for the given base.  If a digit is null in digits, then that digit is not set.  If base is omitted, 10 is the default.  The parameters start_digit and end_digit can be used to get a specific set of digits, but can also be infinite or null to catch all the digits on one side of the number.  The interpretation of start_digit and end_digit are with respect to relative_to_zero, which defaults to true.  If relative_to_zero is true, then the digits are indexed from their distance to zero, such as \"5 4 3 2 1 0 . -1 -2\".  If relative_to_zero is false, then the digits are indexed from their most significant digit, such as \"0 1 2 3 4 5 . 6  7\".  The default values of start_digit and end_digit are the most and least significant digits respectively.",
		"example" : "(print (set_digits 16 8 (list 1 1)))\n(print (get_digits (set_digits 1234567.8 10 (list 1 0 1 0) 2 5 .false) 10 2 5 .false))"
	},

	{
		"parameter" : "floor number x",
		"output" : "int",
		"new value" : "new",
		"description" : "Evaluates to the mathematical floor of x.",
		"example" : "(print (floor 1.5))"
	},

	{
		"parameter" : "ceil number x",
		"output" : "int",
		"new value" : "new",
		"description" : "Evaluates to the mathematical ceiling of x.",
		"example" : "(print (ceil 1.5))"
	},

	{
		"parameter" : "round number x [number significant_digits] [number significant_digits_after_decimal]",
		"output" : "int",
		"new value" : "new",
		"description" : "Rounds the value x and evaluates to the new value.  If only one parameter is specified, it rounds to the nearest integer.  If significant_digits is specified, then it rounds to the specified number of significant digits.  If significant_digits_after_decimal is specified, then it ensures that x will be rounded at least to the number of decimal points past the integer as specified, and takes priority over the significant_digits.",
		"example" : "(print (round 12.7) \"\\n\")\n(print (round 12.7 1) \"\\n\")\n(print (round 123.45678 5) \"\\n\")\n(print (round 123.45678 2) \"\\n\")\n(print (round 123.45678 2 2) \"\\n\")"
	},

	{
		"parameter" : "exp number x",
		"output" : "number",
		"new value" : "new",
		"description" : "e^x",
		"example" : "(print (exp 0.5))"
	},

	{
		"parameter" : "log number x [number base]",
		"output" : "number",
		"new value" : "new",
		"description" : "Log of x.  If a base is specified, uses that base, otherwise defaults to natural log.",
		"example" : "(print (log 0.5))"
	},

	{
		"parameter" : "sin number theta",
		"output" : "number",
		"new value" : "new",
		"description" : "sine",
		"example" : "(print (sin 0.5))"
	},
	
	{
		"parameter" : "asin number length",
		"output" : "number",
		"new value" : "new",
		"description" : "Arc sine (inverse sine)",
		"example" : "(print (sin 0.5))"
	},

	{
		"parameter" : "cos number theta",
		"output" : "number",
		"new value" : "new",
		"description" : "cosine",
		"example" : "(print (cos 0.5))"
	},

	{
		"parameter" : "acos number length",
		"output" : "number",
		"new value" : "new",
		"description" : "Arc cosine (inverse cosine)",
		"example" : "(print (acos 0.5))"
	},

	{
		"parameter" : "tan number theta",
		"output" : "number",
		"new value" : "new",
		"description" : "tangent",
		"example" : "(print (tan 0.5))"
	},

	{
		"parameter" : "atan number num [number divisor]",
		"output" : "number",
		"new value" : "new",
		"description" : "Arc tangent (inverse tangent).  If two numbers are provided, then it evaluates atan num/divisor.",
		"example" : "(print (atan 0.5))\n(print (atan 0.5 0.5))"
	},

	{
		"parameter" : "sinh number z",
		"output" : "number",
		"new value" : "new",
		"description" : "hyperbolic sine",
		"example" : "(print (sinh 0.5))"
	},

	{
		"parameter" : "asinh number x",
		"output" : "number",
		"new value" : "new",
		"description" : "area hyperbolic sine",
		"example" : "(print (asinh 0.5))"
	},

	{
		"parameter" : "cosh number z",
		"output" : "number",
		"new value" : "new",
		"description" : "hyperbolic cosine",
		"example" : "(print (cosh 0.5))"
	},

	{
		"parameter" : "acosh number x",
		"output" : "number",
		"new value" : "new",
		"description" : "area hyperbolic cosine",
		"example" : "(print (acosh 0.5))"
	},

	{
		"parameter" : "tanh number z",
		"output" : "number",
		"new value" : "new",
		"description" : "hyperbolic tangent",
		"example" : "(print (tanh 0.5))"
	},

	{
		"parameter" : "atanh number x",
		"output" : "number",
		"new value" : "new",
		"description" : "area hyperbolic tanh",
		"example" : "(print (atanh 0.5))"
	},

	{
		"parameter" : "erf number errno",
		"output" : "number",
		"new value" : "new",
		"description" : "error function",
		"example" : "(print (erf 0.5))"
	},

	{
		"parameter" : "tgamma number z",
		"output" : "number",
		"new value" : "new",
		"description" : "true (complete) gamma function",
		"example" : "(print (tgamma 0.5))"
	},

	{
		"parameter" : "lgamma number z",
		"output" : "number",
		"new value" : "new",
		"description" : "log-gamma function",
		"example" : "(print (l-gamma 0.5))"
	},

	{
		"parameter" : "sqrt number x",
		"output" : "number",
		"new value" : "new",
		"description" : "Returns the square root of x.",
		"example" : "(print (sqrt 0.5))"
	},

	{
		"parameter" : "pow number base number exponent",
		"output" : "number",
		"new value" : "new",
		"description" : "Returns the base raised to the exponent",
		"example" : "(print (pow 0.5 2))"
	},

	{
		"parameter" : "abs number x",
		"output" : "number",
		"new value" : "new",
		"description" : "absolute value of x",
		"example" : "(print (abs -0.5))"
	},

	{
		"parameter" : "max [number x1] [number x2] ... [number xN]",
		"output" : "number",
		"new value" : "new",
		"concurrency" : true,
		"description" : "maximum of all of the numbers",
		"example" : "(print (max 0.5 1 7 9 -5))"
	},

	{
		"parameter" : "min [number x1] [number x2] ... [number xN]",
		"output" : "number",
		"new value" : "new",
		"concurrency" : true,
		"description" : "minimum of all of the numbers",
		"example" : "(print (min 0.5 1 7 9 -5))"
	},
	{
		"parameter" : "index_max [[number x1] [number x2] [number x3] ... [number xN]] | assoc values | list values",
		"output" : "[*]",
		"concurrency" : true,
		"description" : "If given multiple arguments, returns a list of the indices of the arguments with the maximum value.  If given a single argument that is an assoc returns the a list of keys associated with the maximum values.  If given a single argument that is a list, returns a list of list indices with the maximum value.",
		"example" : "(print (index_max 0.5 1 7 9 -5))\n(print (index_max (list 0.5 1 7 9 -5) ))\n(print (index_max (assoc 1 0.5 2 1 3 7)))"
	},
	{
		"parameter" : "index_min [[number x1] [number x2] [number x3] ... [number xN]] | assoc values | list values",
		"output" : "[*]",
		"concurrency" : true,
		"description" : "If given multiple arguments, returns a list of the indices of the arguments with the minimum value.  If given a single argument that is an assoc returns the a list of keys associated with the minimum values.  If given a single argument that is a list, returns a list of list indices with the minimum value.",
		"example" : "(print (index_min 0.5 1 7 9 -5))\n(print (index_min (list 0.5 1 7 9 -5) ))\n(print (index_min (assoc 1 0.5 2 1 3 7)))"
	},
	{
		"parameter" : "dot_product list|assoc x1 list|assoc x2",
		"output" : "number",
		"new value" : "new",
		"description" : "Evaluates to the sum of all element-wise products of x1 and x2.",
		"example" : "(print (dot_product (list 0.5 0.25 0.25) (list 4 8 8)))"
	},
	{
		"parameter" : "normalize list|assoc values [number p_value]",
		"output" : "list|assoc",
		"new value" : "new",
		"description" : "Evaluates to the values but with the elements normalized by p_value, representing the order of the Lebesgue space to normalize the vector (e.g., 1 is Manhattan / probability, 2 is Euclidean).",
		"example" : "(print (normalize [ 0.5 0.5 0.5 0.5 ])\n(print (normalize { a 0.5 b 0.5 c 0.5 d 0.5 }))"
	},
	{
		"parameter" : "mode list|assoc values [list|assoc weights]",
		"output" : "*",
		"description" : "Evaluates to mode of the values; if values is an assoc, it will return the key.  If weights is specified and both values and weights are lists, then the corresponding elements will be weighted by weights.  If weights is specified and is an assoc, then each value will be looked up in the weights.",
		"example" : "(print (mode [ 1 2 3 3 4 4 4 4])\n(print (mode { a 0.7 b 0.5 })\n)(print (mode [\"a\" \"b\"] {a 1 b 100}))"
	},
	{
		"parameter" : "quantile list|assoc values number quantile [list|assoc weights]",
		"output" : "number",
		"new value" : "new",
		"description" : "Evaluates to the quantile of the values specified by quantile ranging from 0 to 1.  If weights is specified and both values and weights are lists, then the corresponding elements will be weighted by weights.  If weights is specified and is an assoc, then each value will be looked up in the weights.",
		"example" : "(print (quantile [ 1 2 3 4 5 ] 0.5)\n(print (quantile { a 1 b 2 c 3 d 4 } 0.5)\n(print (quantile [ 1 2 3 4 ] 0.5 [0.5 0.5 1 1]))"
	},
	{
		"parameter" : "generalized_mean list|assoc values [number p] [list|assoc weights] [number center] [bool calculate_moment] [bool absolute_value]",
		"output" : "number",
		"new value" : "new",
		"description" : "Evaluates to the generalized mean of the values.  If p is specified (which defaults to 1), it is the parameter that can control the type of mean from minimum (negative infinity) to harmonic mean (-1) to geometric mean (0) to arithmetic mean (1) to maximum (infinity).  If weights are specified, it normalizes and uses those to weight the corresponding values.  If center is specified, calculations will use that as central point, default is 0.0.  If calculate_moment is true, which defaults to false, results will not be raised to 1/p.  If absolute_value is true, which defaults to false, the differences will take the absolute value.  Various parameterizations of generalized_mean can be used to compute moments about the mean, especially setting the calculate_moment parameter to true and using the mean as the center.",
		"example" : "(print (generalized_mean [ 1 2 3 4 5 ])\n(print (generalized_mean { a 0.5 b 0.5 c 0.5 d 0.5 } -1)\n(print (generalized_mean [ 1 2 3 4 5 ] 1 [0.5 0.5 1 1 1 1))"
	},
	{
		"parameter" : "generalized_distance list|assoc|* vector1 [list|assoc|* vector2] [number p_value] [list|assoc|assoc of assoc|number weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc|number deviations] [list value_names] [string weights_selection_feature] [bool surprisal_space]",
		"output" : "number",
		"new value" : "new",
		"description" : "Computes the generalized norm between vector1 and vector2 (or an equivalent zero vector if unspecified) with parameter specified by the p_value (1 being probability space and Manhattan distance, the default, and e.g., 2 being Euclidean distance), using the numerical distance or edit distance as appropriate.  The parameter value_names, if specified as a list of the names of the values, will transform via unzipping any assoc into a list for the respective parameter in the order of the value_names, or if a number will use the number repeatedly for every element.  The weights parameter specifies how to weight the different dimensions.  If weights is a list, each value maps to its respective element in the vectors.  If weights is null, then it will assume that the weights are 1 and additionally will ignore null values for the vectors instead of treating them as unknown differences.  If weights is an assoc, then the parameter value_names will select the weights from the assoc.  If weights is an assoc of assocs, then the parameter weights_selection_feature will select which set of weights to use.  The parameter distance_types is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are \"nominal_bool\", \"nominal_number\", \"nominal_string\", \"nominal_code\", \"continuous_number\", \"continuous_number_cyclic\", \"continuous_string\", and \"continuous_code\".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around.  \nFor attributes, the particular distance_types specifies what particular attributes are expected.  For a nominal distance_type, a number indicates the nominal count, whereas null will infer from the values given.  Cyclic requires a single value, which is the upper bound of the difference for the cycle range (e.g., if the value is 360, then the supremum difference between two values will be 360, leading 1 and 359 to have a difference of 2).\n  Deviations are used during distance calculation to specify uncertainty per-element, the minimum difference between two values prior to exponentiation.  Specifying null as a deviation is equivalent to setting each deviation to 0.  Each deviation for each feature can be a single value or a list.  If it is a single value, that value is used as the deviation and differences and deviations for null values will automatically computed from the data based on the maximum difference.  If a deviation is provided as a list, then the first value is the deviation, the second value is the difference to use when one of the values being compared is null, and the third value is the difference to use when both of the values are null.  If the third value is omitted, it will use the second value for both.  If both of the null values are omitted, then it will compute the maximum difference and use that for both.  For nominal types, the value for each feature can be a numeric deviation, an assoc, or a list.  If the value is an assoc it specifies deviation information, where each key of the assoc is the nominal value, and each value of the assoc can be a numeric deviation value, a list, or an assoc, with the list specifying either an assoc followed optionally by the default deviation.  This inner assoc, regardless of whether it is in a list, maps the value to each actual value's deviation.   If any vector value is null or any of the differences between vector1 and vector2 evaluate to null, then it will compute a corresponding maximum distance value based on the properties of the feature.  If surprisal space is true, which defaults to false, it will perform all computations in surprisal space.",
		"example" : "(generalized_distance (list 1 2 3) (list 10 2 4)  1 (null) (list \"nominal_number\") (list 1))\n(generalized_distance (list 3 4) (null) 2)\n(generalized_distance (list 1 1) (list 1 1) 1 (list 1 1) (list \"continuous_number\" \"continuous_number\") (null) (list 0.5 0.5) (null) (null) .true )"
	},

	{
		"parameter" : "entropy list|assoc|number p [list|assoc|number q] [number p_exponent] [number q_exponent]",
		"output" : "number",
		"new value" : "new",
		"description" : "Computes a form of entropy on the specified vectors using nats (natural log, not bits) in the form of -sum p_i ln (p_i^p_exponent * q_i^q_exponent).  For both p and q, if p or q is a list of numbers, then it will treat each entry as being the probability of that element.  If it is an associative array, then elements with matching keys will be matched.  If p or q a number then it will use that value in place of each element.  If p or q is null or not specified, it will be calculated as the reciprocal of the size of the other element (p_i would be 1/|q| or q_i would be 1/|p|).  If either p_exponent or q_exponent is 0, then that exponent will be ignored.  Shannon entropy can be computed by ignoring the q parameters, setting p_exponent to 1 and q_exponent to 0. KL-divergence can be computed by providing both p and q and setting p_exponent to -1 and q_exponent to 1.  Cross-entropy can be computed by setting p_exponent to 0 and q_exponent to 1.",
		"example" : "(entropy (list 0.5 0.5))\n(entropy (list 0.5 0.5) (list 0.25 0.75) 1 -1)\n(entropy 0.5 (list 0.25 0.75) 1 -1)\n(entropy 0.5 (list 0.25 0.75) 0 1)"
	},

	{
		"parameter" : "first [list|assoc|number|string data]",
		"output" : "*",
		"description" : "Evaluates to the first element.  If data is a list, it will be the first element.  If data is an assoc, it will evaluate to the first element by assoc storage, but order does not matter. If data is a string, it will be the first character. If data is a number, it will evaluate to 1 if nonzero, 0 if zero.",
		"example" : "(print (first (list 4 9.2 \"this\")))\n(print (first (assoc a 1 b 2)))\n(print (first 3))\n(print (first 0))\n(print (first \"abc\"))\n(print (first \"\"))"
	},

	{
		"parameter" : "tail [list|assoc|number|string data] [number retain_count]",
		"output" : "list",
		"description" : "Evaluates to everything but the first element.  If data is a list, it will be a list of all but the first element.  If data is an assoc, it will evaluate to the assoc without the first element by assoc storage order, but order does not matter. If data is a string, it will be all but the first character. If data is a number, it will evaluate to the value minus 1 if nonzero, 0 if zero. If a retain_count is specified, it will be the number of elements to retain.  A positive number means from the end, a negative number means from the beginning.  The default value is -1 (all but the first).",
		"example" : "(print (tail (list 4 9.2 \"this\")))\n(print (tail (assoc a 1 b 2)))\n(print (tail 3))\n(print (tail 0))\n(print (tail \"abc\"))\n(print (tail \"\"))\n(print (tail (list 1 2 3 4 5 6) 2))"
	},

	{
		"parameter" : "last [list|assoc|number|string data]",
		"output" : "*",
		"description" : "Evaluates to the last element.  If it is a list, it will be the last element.  If assoc, it will evaluate to the first element by assoc storage, because order does not matter. If it is a string, it will be the last character. If it is a number, it will evaluate to 1 if nonzero, 0 if zero.",
		"example" : "(print (last (list 4 9.2 \"this\")))\n(print (last (assoc a 1 b 2)))\n(print (last 3))\n(print (last 0))\n(print (last \"abc\"))\n(print (last \"\"))"
	},

	{
		"parameter" : "trunc [list|assoc|number|string data] [number retain_count]",
		"output" : "list",
		"description" : "Truncates, evaluates to everything but the last element. If data is a list, it will be a list of all but the last element.  If data is an assoc, it will evaluate to the assoc without the first element by assoc storage order, because order does not matter. If data is a string, it will be all but the last character. If data is a number, it will evaluate to the value minus 1 if nonzero, 0 if zero. If truncate_count is specified, it will be the number of elements to retain.  A positive number means from the beginning, a negative number means from the end.  The default value is -1 (all but the last).",
		"example" : "(print (trunc (list 4 9.2 \"this\")))\n(print (trunc (assoc \"a\" 1 \"b\" 2)))\n(print (trunc 3))\n(print (trunc 0))\n(print (trunc \"abc\"))\n(print (trunc \"\"))\n(print (trunc (list 1 2 3 4 5 6) -2))"
	},

	{
		"parameter" : "append [list|assoc|* collection1] [list|assoc|* collection2] ... [list|assoc|* collectionN]",
		"output" : "list|assoc",
		"new value" : "partial",
		"description" : "Evaluates to a new list or assoc which merges all lists (collection1 through collectionN) based on parameter order. If any assoc is passed in, then returns an assoc (lists will be automatically converted to an assoc with the indices as keys and the list elements as values). If a non-list and non-assoc is specified, then it just adds that one element to the list",
		"example" : "(print (append (list 1 2 3) (list 4 5 6) (list 7 8 9)))\n(print (append (list 1 2 3) (assoc \"a\" 4 \"b\" 5 \"c\" 6) (list 7 8 9) (assoc d 10 e 11)))\n(print (append (list 4 9.2 \"this\") \"end\"))\n(print (append (assoc 0 4 1 9.2 2 \"this\") \"end\"))"
	},

	{
		"parameter" : "size [list|assoc|string collection] collection",
		"output" : "number",
		"new value" : "new",
		"description" : "Evaluates to the size of the collection in number of elements.  If collection is a string, returns the length in UTF-8 characters.",
		"example" : "(print (size (list 4 9.2 \"this\")))\n(print (size (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\")))"
	},

	{
		"parameter" : "range [* function] number low_endpoint number high_endpoint [number step_size]",
		"output" : "list",
		"new value" : "conditional",
		"concurrency" : true,
		"new target scope": true,
		"description" : "Evaluates to a list with the range from low_endpoint to high_endpoint.  The default step_size is 1.  Evaluates to an empty list if the range is not valid.  If four arguments are specified, then the function will be evaluated for each value in the range.",
		"example" : "(print (range 0 10))\n(print (range 10 0))\n(print (range 0 5 0.0))"
	},

	{
		"parameter" : "rewrite * function * target",
		"output" : "*",
		"new value" : "new",
		"new target scope": true,
		"description" : "Rewrites target by applying the function in a bottom-up manner.  For each node in the target tree, pushes a new target scope onto the target stack, with current_value being the current node and current_index being to the index to the current node relative to the node passed into rewrite accessed via target, and evaluates function.  Returns the resulting tree, after have been rewritten by function.",
		"example" : "(print (rewrite\n                 (lambda (if (~ (current_value) 0) (+ (current_value) 1) (current_value)) )\n                 (list (assoc \"a\" 13))  ) )\n ;rewrite all integer additions into multiplies and then fold constants\n(print (rewrite\n                 (lambda\n					;find any nodes with a + and where its list is filled to its size with integers\n					(if (and \n					       (= (get_type (current_value)) \"+\")\n						   (= (size (current_value)) (size (filter (lambda (~ (current_value) 0)) (current_value))) )\n						 )\n					   (reduce (lambda (* (previous_result) (current_value)) ) (current_value))\n					   (current_value))\n				 )\n				 ;original code with additions to be rewritten\n             			    (lambda\n					(list (assoc \"a\" (+ 3 (+ 13 4 2)) ))  )\n) )\n(print (rewrite\n                 (lambda\n					(if (and \n					       (= (get_type (current_value)) \"+\")\n						   (= (size (current_value)) (size (filter (lambda (~ (current_value) 0)) (current_value))) )\n						 )\n					   (reduce (lambda (+ (previous_result) (current_value)) ) (current_value))\n					   (current_value))\n				 )\n                 (lambda\n					(+ (+ 13 4) (current_value 1)) )\n) )"
	},

	{
		"parameter" : "map * function [list|assoc collection1] [list|assoc collection2] ... [list|assoc collectionN]",
		"output" : "list",
		"new value" : "partial",
		"concurrency" : true,
		"new target scope": true,
		"description" : "For each element in the collection, pushes a new target scope onto the stack, so that current_value accesses the element or elements in the list and current_index accesses the list or assoc index, with target representing the outer set of lists or assocs, and evaluates the function.  Returns the list of results, mapping the list via the specified function. If multiple lists or assocs are specified, then it pulls from each list or assoc simultaneously (null if overrun or index does not exist) and (current_value) contains an array of the values in parameter order.  Note that concurrency is only available when one collection is specified.",
		"example" : "(print (map (lambda (* (current_value) 2)) (list 1 2 3 4)))\n(print (map (lambda (+ (current_value) (current_index))) (assoc 10 1 20 2 30 3 40 4)))\n(print (map\n  (lambda\n    (+ (get (current_value) 0) (get (current_value) 1) (get (current_value) 2))\n  )\n  (assoc \"0\" 0 \"1\" 1 \"a\" 3)\n  (assoc \"a\" 1 \"b\" 4)\n  (list 2 2 2 2)\n))"
	},

	{
		"parameter" : "filter [* function] list|assoc collection",
		"output" : "list|assoc",
		"new value" : "partial",
		"concurrency" : true,
		"new target scope": true,
		"description" : "For each element in the collection, pushes a new target scope onto the stack, so that current_value accesses the element in the list and current_index accesses the list or assoc index, with target representing the original list or assoc, and evaluates the function.  If function evaluates to true, then the element is put in a new list or assoc (matching the input type) that is returned.  If function is omitted, then it will remove any elements in the collection that are null.",
		"example" : "(print (filter (lambda (> (current_value) 2)) (list 1 2 3 4)))"
	},

	{
		"parameter" : "weave [* function] list|immediate values1 [list|immediate values2] [list|immediate values3]...",
		"output" : "list",
		"new target scope": true,
		"description" : "Interleaves the values lists optionally by applying a function.  If only values1 is passed in, then it evaluates to values1. If values1 and values2 are passed in, or, if more values are passed in but function is null, it interleaves the two lists out to whichever list is longer, filling in the remainder with null, and if any value is an immediate, then it will repeat the immediate value.  If the function is specified and not nulll, it pushes a new target scope onto the stack, so that current_value accesses a list of elements to be woven together from the list, and current_index accesses the list or assoc index, with target representing the resulting list or assoc.  The function should evaluate to a list, and weave will evaluate to a concatenated list of all of the lists that the function evaluated to.",
		"example" : "(print (weave (list 1 3 5) (list 2 4 6)) \"\\n\")\n(print (weave (lambda (list (apply \"min\" (current_value) ) ) (list 1 3 4 5 5 6) (list 2 2 3 4 6 7) )\"\\n\")\n(print (weave (lambda (if (<= (get (current_value) 0) 4) (list (apply \"min\" (current_value 1)) ) (current_value)) ) (list 1 3 4 5 5 6) (list 2 2 3 4 6 7) )\"\\n\")\n(print (weave (null) (list 2 4 6) (null) ) \"\\n\")"
	},

	{
		"parameter" : "reduce * function list|assoc collection",
		"output" : "*",
		"new target scope": true,
		"description" : "For each element in the collection after the first one, it evaluates function with a new scope on the stack where current_value accesses each of the elements from the collection, current_index accesses the list or assoc index and previous_result accesses the previously reduced result. If the collection is empty, null is returned. if the collection is of size one, the single element is returned.",
		"example" : "(print (reduce (lambda (* (previous_result) (current_value))) (list 1 2 3 4)))"
	},

	{
		"parameter" : "apply * to_apply [list|assoc collection]",
		"output" : "*",
		"new value" : "conditional",
		"description" : "Creates a new list of the values of the elements of the collection, applies the type specified by to_apply, which is either the type corresponding to a string or the type of to_apply, and then evaluates it. If to_apply has any parameters, these are prepended to the collection as the first parameters. When no extra parameters are passed, it is roughly equivalent to (call (set_type list \"+\")).",
		"example" : "(print (apply (lambda (+)) (list 1 2 3 4)))\n(print (apply (lambda (+ 5)) (list 1 2 3 4)) \"\\n\")\n(print (apply \"+\" (list 1 2 3 4)))"
	},

	{
		"parameter" : "reverse list l",
		"output" : "list",
		"new value" : "partial",
		"description" : "Returns a new list containing the list with its elements in reversed order.",
		"example" : "(print (reverse (list 1 2 3 4 5)))"
	},

	{
		"parameter" : "sort [* function] list|assoc l [number k]",
		"output" : "list",
		"new value" : "partial",
		"new target scope": true,
		"description" : "Returns a new list containing the list with its values sorted in increasing order, regardless of whether l is an assoc or list.  Numerical values come before strings, and code will be evaluated as the representative strings.  If function is null or true it sorts ascending, if false it sorts descending, and if any other value it pushes a pair of new scope onto the stack with (current_value) and (current_value 1) accessing a pair of elements from the list, and evaluates function.  The function should return a number, positive if \"(current_value)\" is greater, negative if \"(current_value 1)\" is greater, 0 if equal.  If k is specified in addition to function and not null, then it will only return the k smallest values sorted in order, or, if k is negative, it will ignore the negative sign and return the highest k values.",
		"example" : "(print (sort (list 4 9 3 5 1)))\n(print (sort (list \"n\" \"b\" \"hello\" 4 1 3.2 (list 1 2 3))))\n(print (sort (list 1 \"1x\" \"10\" 20 \"z2\" \"z10\" \"z100\")))\n(print (sort (lambda (- (current_value) (current_value 1))) (list 4 9 3 5 1)))"
	},

	{
		"parameter" : "indices list|assoc a",
		"output" : "list of string|number",
		"new value" : "new",
		"description" : "Evaluates to the list of strings or numbers that comprise the indices for the list or associative list.  It is guaranteed that the opcodes indices and values (assuming the parameter only_unique_values is not true) will evaluate and return elements in the same order when given the same node.",
		"example" : "(print (indices (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\")))\n(print (indices (list \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\")))"
	},

	{
		"parameter" : "values list|assoc a [bool only_unique_values]",
		"output" : "list of *",
		"new value" : "partial",
		"description" : "Evaluates to the list of entities that comprise the values for the list or associative list. For a list, it evaluates to itself.  If only_unique_values is true (defaults to false), then it will filter out any duplicate values and only return those that are unique (preserving order of first appearance).  If only_unique_values is not true, then it is guaranteed that the opcodes indices and values will evaluate and return elements in the same order when given the same node.",
		"example" : "(print (values (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\")))\n(print (values (list \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\")))"
	},

	{
		"parameter" : "contains_index list|assoc a string|number|list index",
		"output" : "bool",
		"new value" : "new",
		"description" : "Evaluates to true if the index is in the list or associative list.  If index is a string, it will attempt to look at a as an assoc, if number, it will look at a as a list.  If index is a list, it will traverse a via the elements in the list.",
		"example" : "(print (contains_index (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\") \"c\"))\nprint (contains_index (list \"a\" 1 2 3 4 \"d\") 2))"
	},

	{
		"parameter" : "contains_value list|assoc|string a string|number value",
		"output" : "bool",
		"new value" : "new",
		"description" : "Evaluates to true if the value is a value in the list or associative list.  If a is a string, then it uses value as a regular expression and evaluates to true if the regular expression matches.",
		"example" : "(print (contains_value (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\") 1))\n(print (contains_value (list \"a\" 1 2 3 4 \"d\") 2))"
	},

	{
		"parameter" : "remove list|assoc a number|string|list index",
		"output" : "list|assoc",
		"new value" : "partial",
		"description" : "Removes the index-value pair with index being the index in assoc or index of the list or assoc, returning a new list or assoc with that index removed.  If index is a list of numbers or strings, then it will remove each of the requested indices.  Negative numbered indices will count back from the end of a list.",
		"example" : "(print (remove (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\") 4))\n(print (remove (list \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\") 4))\n (print (remove (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\") (list 4 \"a\") ))"
	},

	{
		"parameter" : "keep list|assoc a number|string|list index",
		"output" : "list|assoc",
		"new value" : "partial",
		"description" : "Keeps only the index-value pair with index being the index in assoc or index of the list or assoc, returning a new list or assoc with that only that index.  If index is a list of numbers or strings, then it will only keep each of the requested indices.  Negative numbered indices will count back from the end of a list.",
		"example" : "(print (keep (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\") 4))\n(print (keep (list \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\") 4))\n (print (keep (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\") (list 4 \"a\") ))"
	},

	{
		"parameter" : "associate [* index1] [* value1] [* index2] [* value2] ... [* indexN] [* valueN]",
		"output" : "assoc",
		"new value" : "partial",
		"concurrency" : true,
		"new target scope": true,
		"description" : "Evaluates to the assoc, where each pair of parameters (e.g., index1 and value1) comprises a index/value pair. Pushes a new target scope such that (target), (current_index), and (current_value) access the assoc, the current index, and the current value.",
		"example" : "(print (assoc \"a\" 1 \"b\" 2 \"c\" 3 4 \"d\"))"
	},

	{
		"parameter" : "zip [* function] list indices [* values]",
		"output" : "assoc",
		"new value" : "partial",
		"new target scope": true,
		"description" : "Evaluates to a new assoc where the indices are the keys and the values are the values, with corresponding positions in the list matched. If the values is omitted, then it will use nulls for each of the values.  If values is not a list, then all of the values in the assoc returned are set to the same value.  When one parameter is specified, it is the list of indices.  When two parameters are specified, it is the indices and values.  When three values are specified, it is the function, indices and values.  Values defaults to (null) and function defaults to (lambda (current_value)).  When there is a collision of indices, the function is called, it pushes a pair of new target scope onto the stack, so that current_value accesses a list of elements from the list, current_index accesses the list or assoc index if it is not already reduced, with target representing the original list or assoc, evaluates function if one exists, and (current_value) is the new value attempted to be inserted over (current_value 1).",
		"example" : "(print (zip (list \"a\" \"b\" \"c\" \"d\") (list 1 2 3 4)))"
	},

	{
		"parameter" : "unzip [list|assoc values] list indices",
		"output" : "list",
		"new value" : "partial",
		"description" : "Evaluates to a new list, using the indices list to look up each value from the values list or assoc, in the same order as each index is specified in indices.",
		"example" : "(print (unzip (assoc \"a\" 1 \"b\" 2 \"c\" 3) (list \"a\" \"b\")))\n(print (unzip (list 1 2 3) (list 0 -1 1)))"
	},

	{
		"parameter" : "and [bool condition1] [bool condition2] ... [bool conditionN]",
		"output" : "*",
		"new value" : "conditional",
		"concurrency" : true,
		"description" : "If all condition expressions are true, evaluates to conditionN. Otherwise evaluates to false.",
		"example" : "(print (and 1 4.8 \"true\"))\n(print (and 1 0.0 \"true\"))"
	},

	{
		"parameter" : "or [bool condition1] [bool condition2] ... [bool conditionN]",
		"output" : "*",
		"new value" : "conditional",
		"concurrency" : true,
		"description" : "If all condition expressions are false, evaluates to false. Otherwise evaluates to the first condition that is true.",
		"example" : "(print (or 1 4.8 \"true\"))\n(print (or 1 0.0 \"true\"))\n(print (or 0 0.0 \"\"))"
	},

	{
		"parameter" : "xor [bool condition1] [bool condition2] ... [bool conditionN]",
		"output" : "*",
		"new value" : "new",
		"concurrency" : true,
		"description" : "If an even number of condition expressions are true, evaluates to false. Otherwise evaluates to true.",
		"example" : "(print (xor 1 4.8 \"true\"))\n(print (xor 1 0.0 \"true\"))"
	},

	{
		"parameter" : "not bool condition",
		"output" : "bool",
		"new value" : "new",
		"description" : "Evaluates to false if condition is true, true if false.",
		"example" : "(print (not 1))\n(print (not \"\"))"
	},

	{
		"parameter" : "= [* node1] [* node2] ... [* nodeN]",
		"output" : "bool",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to true if all values are equal (will recurse into data structures), false otherwise. Values of null are considered equal.",
		"example" : "(print (= 4 4 5))\n(print (= 4 4 4))"
	},

	{
		"parameter" : "!= [* node1] [* node2] ... [* nodeN]",
		"output" : "bool",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to true if no two values are equal (will recurse into data structures), false otherwise.",
		"example" : "(print (!= 4 4))\n(print (!= 4 5))\n(print (!= 4 4 5))\n(print (!= 4 4 4))\n(print (!= 4 4 \"hello\" 4))\n(print (!= 4 4 4 1 3.0 \"hello\"))\n(print (!= 1 2 3 4 5 6 \"hello\"))\n"
	},

	{
		"parameter" : "< [* node1] [* node2] ... [* nodeN]",
		"output" : "bool",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to true if all values are in strict increasing order, false otherwise.",
		"example" : "(print (< 4 5))\n(print (< 4 4))\n(print (< 4 5 6))\n(print (< 4 5 6 5))\n"
	},

		{
		"parameter" : "<= [* node1] [* node2] ... [* nodeN]",
		"output" : "bool",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to true if all values are in nondecreasing order, false otherwise.",
		"example" : "(print (<= 4 5))\n(print (<= 4 4))\n(print (<= 4 5 6))\n(print (<= 4 5 6 5))"
	},

	{
		"parameter" : "> [* node1] [* node2] ... [* nodeN]",
		"output" : "bool",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to true if all values are in strict decreasing order, false otherwise.",
		"example" : "(print (> 6 5))\n(print (> 4 4))\n(print (> 6 5 4))\n(print (> 6 5 4 5))"
	},

	{
		"parameter" : ">= [* node1] [* node2] ... [* nodeN]",
		"output" : "bool",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to true if all values are in nonincreasing order, false otherwise.",
		"example" : "(print (>= 6 5))\n(print (>= 4 4))\n(print (>= 6 5 4))\n(print (>= 6 5 4 5))"
	},

	{
		"parameter" : "~ [* node1] [* node2] ... [* nodeN]",
		"output" : "bool",
		"new value" : "new",
		"concurrency" : true,
		"description" : "Evaluates to true if all values are of the same data type, false otherwise.",
		"example" : "(print (~ 1 4 5))\n(print (~ 1 4 \"a\"))"
	},

	{
		"parameter" : "!~ [* node1] [* node2] ... [* nodeN]",
		"output" : "bool",
		"new value" : "new",
		"description" : "Evaluates to true if no two values are of the same data types, false otherwise.",
		"example" : "(print (!~ \"true\" \"false\" (list 3 2)))\n(print (!~ \"true\" 1 (list 3 2)))"
	},

	{
		"parameter" : "null",
		"output" : "immediate null",
		"description" : "Evaluates to the immediate null value.",
		"example" : "(print (null))\n(print (lambda (null (+ 3 5) 7)) )\n(print (lambda (#nulltest null)))"
	},

	{
		"parameter" : "list [* node1] [* node2] ... [* nodeN]",
		"output" : "list of *",
		"new value" : "partial",
		"concurrency" : true,
		"new target scope": true,
		"description" : "Evaluates to the list specified by the parameters.  Pushes a new target scope such that (target), (current_index), and (current_value) access the list, the current index, and the current value.  If []'s are used instead of parenthesis, the keyword list may be omitted.  [] are considered identical to (list).",
		"example" : "(print (list \"a\" 1 \"b\"))\n(print [1 2 3])"
	},

	{
		"parameter" : "assoc [bstring index1] [* value1] [bstring index1] [* value2] ...",
		"output" : "assoc",
		"new value" : "partial",
		"concurrency" : true,
		"new target scope": true,
		"description" : "Evaluates to the associative list, where each pair of parameters (e.g., index1 and value1) comprises a index/value pair. Pushes a new target scope such that (target), (current_index), and (current_value) access the assoc, the current index, and the current value.  If any of the bstrings do not have reserved characters or spaces, then quotes are optional; if spaces or reserved characters are present, then quotes are required.  If {}'s are used instead of parenthesis, the keyword assoc may be omitted.  {} are considered identical to (assoc)",
		"example" : "(print (assoc b 2 c 3))\n(print (assoc a 1 \"b\\ttab\" 2 c 3 4 \"d\"))\n(print {a 1 b 2})"
	},

	{
		"parameter" : "[number]",
		"output" : "number",
		"new value" : "new",
		"description" : "A 64-bit floating point value",
		"example" : "4\n2.22228"
	},

	{
		"parameter" : "[string]",
		"output" : "number",
		"new value" : "new",
		"description" : "A string.",
		"example" : "\"hello\""
	},

	{
		"parameter" : "[symbol]",
		"output" : "string",
		"description" : "A string representing an internal symbol (a variable).",
		"example" : "my_variable"
	},

	{
		"parameter" : "get_type * node",
		"output" : "*",
		"new value" : "new",
		"description" : "Returns a node of the type corresponding to the node.",
		"example" : "(print (get_type (lambda (+ 3 4))))"
	},

	{
		"parameter" : "get_type_string * node",
		"output" : "string",
		"new value" : "new",
		"description" : "Returns a string that represents the type corresponding to the node.",
		"example" : "(print (get_type_string (lambda (+ 3 4))))"
	},

	{
		"parameter" : "set_type * node1 [string|* type]",
		"output" : "*",
		"new value" : "partial",
		"description" : "Creates a copy of node1, setting the type of the node of to whatever node type is specified by string or to the same type as the top node of type.  It will convert the parameters to or from assoc if necessary.",
		"example" : "(print (set_type (lambda (+ 3 4)) \"-\"))\n(print (set_type (assoc \"a\" 4 \"b\" 3) \"list\"))\n(print (set_type (assoc \"a\" 4 \"b\" 3) (list)))\n(print (set_type (list \"a\" 4 \"b\" 3) \"assoc\"))\n(print (call (set_type (list 1 0.5 \"3.2\" 4) \"+\")))"
	},

	{
		"parameter" : "format * data string from_format string to_format [assoc from_params] [assoc to_params]",
		"output" : "*",
		"new value" : "new",
		"description" : "Converts data from from_format into to_format.  Supported language types are \"number\", \"string\", and \"code\", where code represents everything beyond number and string.  Beyond the supported language types, additional formats that are stored in a binary string.  The additional formats are \"base16\", \"base64\", \"int8\", \"uint8\", \"int16\", \"uint16\", \"int32\", \"uint32\", \"int64\", \"uint64\", \"float32\", \"float64\", \">int8\", \">uint8\", \">int16\", \">uint16\", \">int32\", \">uint32\", \">int64\", \">uint64\", \">float32\", \">float64\", \"<int8\", \"<uint8\", \"<int16\", \"<uint16\", \"<int32\", \"<uint32\", \"<int64\", \"<uint64\", \"<float32\", \"<float64\", \"json\", \"yaml\", \"date\", and \"time\" (though date and time are special cases).  Binary types starting with a < represent little endian, binary types starting with a > represent big endian, and binary types without either will be the endianness of the machine.  Binary types will be handled as strings.  The \"date\" type requires additional information.  Following \"date\" or \"time\" is a colon, followed by a standard strftime date or time format string.  If from_params or to_params are specified, then it will apply the appropriate from or to as appropriate.  If the format is either \"string\", \"json\", or \"yaml\", then the key \"sort_keys\" can be used to specify a boolean value, if true, then it will sort the keys, otherwise the default behavior is to emit the keys based on memory layout.  If the format is date or time, then the to or from params can be an assoc with \"locale\" as an optional key.  If date then \"time_zone\" is also allowed.  The locale is provided, then it will leverage operating system support to apply appropriate formatting, such as en_US.  Note that UTF-8 is assumed and automatically added to the locale.  If no locale is specified, then the default will be used.  If converting to or from dates, if time_zone is specified, it will use the standard time_zone name, if unspecified or empty string, it will assume the current time zone.",
		"example" : "(print (format 65 \"number\" \"int8\") \"\\n\")\n(print (format (format -100 \"number\" \"double\") \"double\" \"number\") \"\\n\")"
	},

	{
		"parameter" : "get_labels * node",
		"output" : "list of string",
		"new value" : "new",
		"description" : "Returns a list of strings comprising all of the labels for the particular node of *.",
		"example" : "(print (get_labels ( #labelA lambda #labelB .true)))"
	},

	{
		"parameter" : "get_all_labels * node",
		"output" : "assoc",
		"new value" : "new",
		"description" : "Returns an associative list of the labels for the node of code and everything underneath it, where the index is the label and the value is the reference to *.",
		"example" : "(print (get_all_labels (lambda (#label21 print \"hello world: \" (* #label-number-22 3 4) #label23 \" and \" (* 1 2) )) ))\n(print (get_all_labels (lambda\n  ( #labelA #labelQ * #labelB\n    (+ 1 #labelA 3) 2))))"
	},

	{
		"parameter" : "set_labels * node (list [string new_label1]...[string new_labelN])",
		"output" : "*",
		"new value" : "partial",
		"description" : "Sets the labels for the node of code. Evaluates to the node represented by the input node.",
		"example" : "(print (set_labels\n  ( lambda\n   (#labelC true)) (list \"labelD\" \"labelE\")))"
	},

	{
		"parameter" : "zip_labels list labels * to_add_labels",
		"output" : "*",
		"new value" : "partial",
		"description" : "For each of the values in to_add_labels, it takes respective value for labels and applies that string as a label to the respective value, and returns a new set of values with the labels.",
		"example" : "(print (zip_labels (list \"l1\" \"l2\" \"l3\") (list 1 2 3)))"
	},

	{
		"parameter" : "get_comments * node",
		"output" : "string",
		"new value" : "new",
		"description" : "Returns a strings comprising all of the comments for the input node.",
		"example" : "(print (get_comments\n  ;this is a comment\n  (lambda ;comment too\n    .true)))"
	},

	{
		"parameter" : "set_comments * node [string new_comment]",
		"output" : "*",
		"new value" : "partial",
		"description" : "Sets the comments for the node of code. Evaluates to the node represented by new_comment.",
		"example" : "(print (set_comments\n  ;this is a comment\n  (lambda ;comment too\n    .true) \"new comment\"))"
	},

	{
		"parameter" : "get_concurrency * node",
		"output" : "bool",
		"new value" : "new",
		"description" : "Returns true if the node has a preference to be processed in a manner where its operations are run concurrently (and potentially subject to race conditions).  False if it is not.",
		"example" : "(print (get_concurrency (lambda ||(map foo array))) \"\n\")"
	},

	{
		"parameter" : "set_concurrency * node bool concurrent",
		"output" : "*",
		"new value" : "partial",
		"description" : "Sets whether the node has a preference to be processed in a manner where its operations are run concurrently (and potentially subject to race conditions). Evaluates to the node represented by the input node.",
		"example" : "(print (set_concurrency (lambda (map foo array)) .true) \"\n\")"
	},

	{
		"parameter" : "get_value * node",
		"output" : "*",
		"new value" : "new",
		"description" : "Returns just the value portion of node (no labels or comments). Will evaluate to a copy of the value if it is not a unique reference, making it useful to ensure that the copy of the data is unique.",
		"example" : "(print (get_value\n  ;this is a comment\n  (lambda ;comment too\n    #withalabel .true)))"
	},

	{
		"parameter" : "set_value * target * val",
		"output" : "*",
		"new value" : "partial",
		"description" : "Sets target's value to the value of val, keeping existing labels, and comments).",
		"example" : "(print (set_value\n  ;this is a comment\n  (lambda ;comment too\n    .true) 3))"
	},

	{
		"parameter" : "explode [string str] [number stride]",
		"output" : "list of string",
		"new value" : "new",
		"description" : "Explodes string str into the pieces that make it up.  If stride is zero or unspecified, then it explodes the string by character per UTF-8 parsing.  If stride is specified, then it breaks it into chunks of that many bytes.  For example, a stride of 1 would break it into bytes, whereas a stride of 4 would break it into 32-bit chunks.",
		"example" : "(print (explode \"test\"))\n(print (explode \"test\" 2))"
	},

	{
		"parameter" : "split [string str] [string split_string] [number max_split_count] [number stride]",
		"output" : "list of string",
		"new value" : "new",
		"description" : "Splits the string str into a list of strings based on the split_string, which is handled as a regular expression.  Any data matching split_string will not be included in any of the resulting strings.  If max_split_count is provided and greater than zero, it will only split up to that many times.  If stride is zero or unspecified, then it explodes the string by character per UTF-8 parsing.  If stride is specified and a value other than zero, then it does not use split_string as a regular expression but rather a string, and it breaks the result into chunks of that many bytes.  For example, a stride of 1 would break it into bytes, whereas a stride of 4 would break it into 32-bit chunks.",
		"example" : "(print (split \"hello world\" \" \"))"
	},

	{
		"parameter" : "substr [string str] [number|string location] [number|string param] [string replacement] [number stride]",
		"output" : "string | list of string | list of list of string",
		"new value" : "new",
		"description" : "Finds a substring of string str.  If location is a number, then evaluates to a new string representing the substring starting at offset, but if location is a string, then it will treat location as a regular expression.  If param is specified, if location is a number it will go until that length beyond the offset, and if location is a regular expression param will represent one of the following: if null or \"first\", then it will return the first match of the regular expression; if param is a number or the string \"all\", then substr will evaluate to a list of up to param matches (which may be infinite yielding the same result as \"all\").  If param is a negative number or the string \"submatches\", then it will return a list of list of strings, for each match up to the count of the negative number or all matches if \"submatches\", each inner list will represent the full regular expression match followed by each submatch as captured by parenthesis in the regular expression, ordered from an outer to inner, left-to-right manner.  If location is a number and offset or length are negative, then it will measure from the end of the string rather than the beginning.  If replacement is specified and not null, it will return the original string rather than the substring, but the substring will be replaced by replacement regardless of what location is; and if replacement is specified, then it will override some of the logic for the param type and always return just a string and not a list.  If stride is zero or unspecified, then it explodes the string by character per UTF-8 parsing.  If stride is specified, then it breaks it into chunks of that many bytes.  For example, a stride of 1 would break it into bytes, whereas a stride of 4 would break it into 32-bit chunks.",
		"example" : "(print (substr \"hello world\" 5))"
	},

	{
		"parameter" : "concat [string str1] [string str2] ... [string strN]",
		"output" : "string",
		"new value" : "new",
		"description" : "Concatenates all strings and evaluates to the single string that is the result.",
		"example" : "(print (concat \"hello\" \" \" \"world\"))"
	},

	{
		"parameter" : "crypto_sign string message string secret_key",
		"output" : "string",
		"new value" : "new",
		"description" : "Signs the message given the secret key and returns the signature using the Ed25519 algorithm.  Note that the message is not included in the signature.  The system opcode using the command sign_key_pair can be used to create a public/secret key pair.",
		"example" : "(print (crypto_sign \"hello world\" secret_key))"
	},

	{
		"parameter" : "crypto_sign_verify string message string public_key string signature",
		"output" : "bool",
		"new value" : "new",
		"description" : "Verifies that the message was signed with the signature via the public key using the Ed25519 algorithm and returns true if the signature is valid, false otherwise.  Note that the message is not included in the signature.  The system opcode using the command sign_key_pair can be used to create a public/secret key pair.",
		"example" : "(print (crypto_sign_verify \"hello world\" public_key signature))"
	},

	{
		"parameter" : "encrypt string plaintext_message string key1 [string nonce] [string key2]",
		"output" : "string",
		"new value" : "new",
		"description" : "If key2 is not provided, then it uses the XSalsa20 algorithm to perform shared secret key encryption on the message, returning the encrypted value.  If key2 is provided, then the Curve25519 algorithm will additionally be used, and key1 will represent the receiver's public key and key2 will represent the sender's secret key.  The nonce is a string of bytes up to 24 bytes long, that will be used to randomize the encryption, and will need to be provided to the decryption in order to work.  Nonces are not technically required, but strongly recommended to prevent replay attacks.  The system opcode using the command encrypt_key_pair can be used to create a public/secret key pair.",
		"example" : "(print (encrypt \"hello world\" shared_secret_key nonce))\n(print (encrypt \"hello world\" sender_secret_key nonce receiver_public_key))"
	},

	{
		"parameter" : "decrypt string cyphertext_message string key1 [string nonce] [string key2]",
		"output" : "string",
		"new value" : "new",
		"description" : "If key2 is not provided, then it uses the XSalsa20 algorithm to perform shared secret key decryption on the message, returning the encrypted value.  If key2 is provided, then the Curve25519 algorithm will additionally be used, and key1 will represent the sender's public key and key2 will represent the receiver's secret key.  The nonce is a string of bytes up to 24 bytes long, that will be used to randomize the encryption, and will need to be provided to the decryption in order to work.  Nonces are not technically required, but strongly recommended to prevent replay attacks.  The system opcode using the command encrypt_key_pair can be used to create a public/secret key pair.",
		"example" : "(print (decrypt \"hello world\" shared_secret_key nonce))\n(print (decrypt \"hello world\" sender_public_key nonce receiver_secret_key))"
	},

	{
		"parameter" : "print [* node1] [* node2] ... [* nodeN]",
		"output" : "null",
		"description" : "Prints each of the parameters in order in a manner interpretable as if they were code. Output is pretty-printed. Printing a node which evaluates to a literal string or number will not be printed (the value will be printed directly) and not have a newline appended.",
		"example" : "(print \"hello\")"
	},

	{
		"parameter" : "total_size * node",
		"output" : "number",
		"new value" : "new",
		"description" : "Evaluates to the total count of all of the nodes referenced within the input node. Each label on a node counts for an additional node.  The volume of data in an individual node (such as in a string) counts as an additional node for each 48 characters.",
		"example" : "(print (total_size (list 1 2 3 (assoc \"a\" 3 \"b\" 4) (list 5 6))))"
	},

	{
		"parameter" : "mutate * node [number mutation_rate] [assoc mutation_weights] [assoc operation_type]",
		"output" : "*",
		"new value" : "new",
		"description" : "Evaluates to a mutated version of the input node.  The value specified in mutation_rate, from 0.0 to 1.0 and defaulting to 0.00001, indicates the probability that any node will experience a mutation. The parameter mutation_weights is an assoc where the keys are the allowed opcode names and the values are the probabilities that each opcode would be chosen; if null or unspecified, it defaults to all opcodes each with their own default probability.  The operation_type is an assoc where the keys are mutation operations and the values are the probabilities that the operations will be performed.  The operations can consist of the strings change_type, delete, insert, swap_elements, deep_copy_elements, delete_elements, and change_label.",
		"example" : "(print (mutate\n  (lambda (list 1 2 3 4 5 6 7 8 9 10 11 12 13 14 (assoc \"a\" 1 \"b\" 2)))\n0.4))\n"
	},

	{
		"parameter" : "commonality * node1 * node2 [bool use_string_edit_distance]",
		"output" : "number",
		"new value" : "new",
		"description" : "Evaluates to the total count of all of the nodes referenced within node1 and node2 that are equivalent, using fractions to represent somewhat similar nodes. If use_string_edit_distance is true and node1 and node2 are both string literals, string edit distance will be used to calculate commonality.",
		"example" : "(print (commonality\n  (lambda (seq 2 (get_entity_comments) 1))\n  (lambda (seq 2 1 4 (get_entity_comments)))\n))\n (print (commonality\n  (list 1 2 3 (assoc \"a\" 3 \"b\" 4) (lambda (if true 1 (parallel (get_entity_comments) 1))) (list 5 6))\n  (list 1 2 3 (assoc \"c\" 3 \"b\" 4) (lambda (if true 1 (parallel 1 (get_entity_comments)))) (list 5 6))\n))"
	},

	{
		"parameter" : "edit_distance * node1 * node2 [bool use_string_edit_distance]",
		"output" : "number",
		"new value" : "new",
		"description" : "Evaluates to the number of nodes that are different between 1 and 2, using fractions to represent somewhat similar nodes. If use_string_edit_distance is true and node1 and node2 are both string literals, string edit distance will be calculated.",
		"example" : "(print (edit_distance\n  (lambda (seq 2 (get_entity_comments) 1))\n  (lambda (seq 2 1 4 (get_entity_comments)))\n))\n (print (edit_distance\n  (list 1 2 3 (assoc \"a\" 3 \"b\" 4) (lambda (if true 1 (parallel (get_entity_comments) 1))) (list 5 6))\n  (list 1 2 3 (assoc \"c\" 3 \"b\" 4) (lambda (if true 1 (parallel 1 (get_entity_comments)))) (list 5 6))\n))"
	},

	{
		"parameter" : "intersect * node1 * node2",
		"output" : "*",
		"new value" : "new",
		"description" : "Evaluates to whatever is common between node1 and node2 exclusive.",
		"example" : "(print (intersect\n  (list 1 (lambda (- 4 2)) (assoc \"a\" 3 \"b\" 4))\n  (list 1 (lambda (- 4 2)) (assoc \"c\" 3 \"b\" 4))\n))\n\n(print (intersect\n  (lambda (seq 2 (get_entity_comments) 1))\n  (lambda (seq 2 1 4 (get_entity_comments)))\n))\n  \n(print (intersect\n  (lambda (parallel 2 (get_entity_comments) 1))\n  (lambda (parallel 2 1 4 (get_entity_comments)))\n))\n\n(print (intersect\n  (list 1 2 3 (assoc \"a\" 3 \"b\" 4) (lambda (if true 1 (parallel (get_entity_comments) #label-not-1 1))) (list 5 6))\n  (list 1 2 3 (assoc \"c\" 3 \"b\" 4) (lambda (if true 1 (parallel #label-not-1 1 (get_entity_comments)))) (list 5 6))\n))\n  \n(print (intersect\n  (lambda (list 1 (assoc \"a\" 3 \"b\" 4)))\n  (lambda (list 1 (assoc \"c\" 3 \"b\" 4)))\n))\n\n(print (intersect\n  (lambda (replace 4 2 6 1 7))\n  (lambda (replace 4 1 7 2 6))\n))"
	},

	{
		"parameter" : "union * node1 * node2",
		"output" : "*",
		"new value" : "new",
		"description" : "Evaluates to whatever is inclusive when merging node1 and node2.",
		"example" : "(print (union\n  (lambda (seq 2 (get_entity_comments) 1))\n  (lambda (seq 2 1 4 (get_entity_comments)))\n))\n\n(print (union\n  (list 1 (lambda (- 4 2)) (assoc \"a\" 3 \"b\" 4))\n  (list 1 (lambda (- 4 2)) (assoc \"c\" 3 \"b\" 4))\n))\n  \n(print (union\n  (lambda (parallel 2 (get_entity_comments) 1))\n  (lambda (parallel 2 1 4 (get_entity_comments)))\n))\n\n(print (union\n  (list 1 2 3 (assoc \"a\" 3 \"b\" 4) (lambda (if true 1 (parallel (get_entity_comments) #label-not-1 1))) (list 5 6))\n  (list 1 2 3 (assoc \"c\" 3 \"b\" 4) (lambda (if true 1 (parallel #label-not-1 1 (get_entity_comments)))) (list 5 6))\n))\n  \n(print (union\n  (lambda (list 1 (assoc \"a\" 3 \"b\" 4)))\n  (lambda (list 1 (assoc \"c\" 3 \"b\" 4)))\n))\n\n"
	},

	{
		"parameter" : "difference * node1 * node2",
		"output" : "*",
		"new value" : "new",
		"description" : "Finds the difference between node1 and node2, and generates code that, if evaluated passing node1 as its parameter \"_\", would turn it into node2.  Useful for finding the smallest set of what needs to be changed to apply it to new (and possibly slightly different) data or code.",
		"example" : "(print (difference\n  (lambda (assoc a 1 b 2 c 4 d 7 e 10 f 12 g 13))\n  (lambda (list a 2 c 4 d 6 q 8 e 10 f 12 g 14))\n))\n(print (difference\n  (assoc a 1 b 2 c 4 d 7 e 10 f 12 g 13)\n  (assoc a 2 c 4 d 6 q 8 e 10 f 12 g 14)\n))\n(print (difference\n  (lambda (list 1 2 4 7 10 12 13))\n  (lambda (list 2 4 6 8 10 12 14))\n))\n(print (difference\n  (lambda (assoc a 1 b 2 c 4 d 7 e 10 f 12 g 13))\n  (lambda (assoc a 2 c 4 d 6 q 8 e 10 f 12 g 14))\n))\n\n(print (difference\n  (lambda (assoc a 1 g (list 1 2)))\n  (lambda (assoc a 2 g (list 1 4)))\n))\n\n(print (difference\n  (lambda (assoc a 1 g (list 1 2)))\n  (lambda (assoc a 2 g (list 1 4)))\n))\n\n(let (assoc\n    x (lambda (list 6 (list 1 2)))\n    y (lambda (list 7 (list 1 4)))\n  )\n  \n  (print (difference x y) )\n  (print (call (difference x y) (assoc _ x)) )\n)\n\n(let (assoc\n    x (lambda (list 6 (list (list \"a\" \"b\") 1 2)))\n    y (lambda (list 7 (list (list \"a\" \"x\") 1 4)))\n  )\n  (print (difference x y) )\n  (print (call (difference x y) (assoc _ x)) )\n)\n"
	},

	{
		"parameter" : "mix * node1 * node2 [number keep_chance_node1] [number keep_chance_node2] [number similar_mix_chance] [number max_mix_depth]",
		"output" : "*",
		"new value" : "new",
		"description" : "Performs a union operation on node1 and node2, but randomly ignores nodes from one or the other if the node is not equal.  If only keep_chance_node1 is specified, keep_chance_node2 defaults to 1-keep_chance_node1. keep_chance_node1 specifies the probability that a node from node1 will be kept, and keep_chance_node2 the probability that a node from node2 will be kept.  keep_chance_node1 + keep_chance_node2 should be between 1 and 2, otherwise it will be normalized.  similar_mix_chance is the additional probability that two nodes will mix if they have some commonality, which will include interpolating number and string values based on keep_chance_node1 and keep_chance_node2, and defaults to 0.0.  If similar_mix_chance is negative, then 1 minus the value will be anded with the commonality probability, so -1 means that it will never mix and 0 means it will only mix when sufficiently common.  If max_mix_depth not null and greater than 0, it will specify how deep recursively to mix the nodes.",
		"example" : "(print (mix\n  (lambda (list 1 3 5 7 9 11 13))\n  (lambda (list 2 4 6 8 10 12 14))\n0.5 0.5))\n\n(print (mix\n  (lambda (list 1 2 (assoc \"a\" 3 \"b\" 4) (lambda (if true 1 (parallel (get_entity_comments) 1))) (list 5 6)) )\n  (lambda (list 1 5 3 (assoc \"a\" 3 \"b\" 4) (lambda (if false 1 (parallel (get_entity_comments) (lambda (print (list 2 9))) ))) ) )\n0.8 0.8))\n\n"
	},

	{
		"parameter" : "total_entity_size id_path entity",
		"output" : "number",
		"new value" : "new",
		"description" : "Evaluates to the total count of all of the nodes of the entity represented by the input id_path and all its contained entities.  Each entity itself counts as multiple nodes, as it requires multiple nodes to create an entity as exhibited by flattening an entity.",
		"example" : "(create_entities \"MergeEntity1\" (lambda (assoc \"a\" 3 \"b\" 4)) )\n(create_entities (list \"MergeEntity1\" \"MergeEntityChild1\") (lambda (assoc \"x\" 3 \"y\" 4)) )\n(create_entities (list \"MergeEntity1\" \"MergeEntityChild2\") (lambda (assoc \"p\" 3 \"q\" 4)) )\n(create_entities (list \"MergeEntity1\") (lambda (assoc \"E\" 3 \"F\" 4)) )\n(create_entities (list \"MergeEntity1\") (lambda (assoc \"e\" 3 \"f\" 4 \"g\" 5 \"h\" 6)) )\n\n(create_entities \"MergeEntity2\" (lambda (assoc \"c\" 3 \"b\" 4)) )\n(create_entities (list \"MergeEntity2\" \"MergeEntityChild1\") (lambda (assoc \"x\" 3 \"y\" 4 \"z\" 5)) )\n(create_entities (list \"MergeEntity2\" \"MergeEntityChild2\") (lambda (assoc \"p\" 3 \"q\" 4 \"u\" 5 \"v\" 6 \"w\" 7)) )\n(create_entities (list \"MergeEntity2\") (lambda (assoc \"E\" 3 \"F\" 4 \"G\" 5 \"H\" 6)) )\n(create_entities (list \"MergeEntity2\") (lambda (assoc \"e\" 3 \"f\" 4)) )\n\n(print (total_entity_size \"MergeEntity1\"))\n(print (total_entity_size \"MergeEntity2\"))"
	},

	{
		"parameter" : "flatten_entity id_path entity [bool include_rand_seeds] [bool parallel_create] [bool include_version]",
		"output" : "*",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Evaluates to code that, if called, would completely reproduce the entity specified by id_path, as well as all contained entities.  If include_rand_seeds is true, its default, it will include all entities' random seeds.  If parallel_create is true, then the creates will be performed with parallel markers as appropriate for each group of contained entities.  If include_version is true, it will include a comment on the top node that is the current version of the Amalgam interpreter, which can be used for validating interoperability when loading code.  The code returned accepts two parameters, create_new_entity, which defaults to true, and new_entity, which defaults to null.  If create_new_entity is true, then it will create a new entity with id_path specified by new_entity, where null will create an unnamed entity.  If create_new_entity is false, then it will overwrite the current entity's code and create all contained entities.",
		"example" : "(create_entities \"FlattenTest\" (lambda\n  (parallel ##a (rand) )\n))\n(let (assoc fe (flatten_entity \"FlattenTest\"))\n  (print fe)\n  (print (flatten_entity (call fe)))\n  (print (difference_entities \"FlattenTest\" (call fe)))\n (call fe (assoc create_new_entity .false new_entity \"new_entity_name\")) \n)"
	},

	{
		"parameter" : "mutate_entity id_path entity1 [number mutaton_rate] [id_path entity2] [assoc mutation_weights] [assoc operation_type]",
		"output" : "id_path",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Creates a mutated version of the entity specified by entity1 like mutate. Returns the id_path of a new entity created contained by the entity that ran it.  The value specified in mutation_rate, from 0.0 to 1.0 and defaulting to 0.00001, indicates the probability that any node will experience a mutation.  Uses entity2 as the optional destination via an internal call to create_contained_entity. The parameter mutation_weights is an assoc where the keys are the allowed opcode names and the values are the probabilities that each opcode would be chosen; if null or unspecified, it defaults to all opcodes each with their own default probability.  The operation_type is an assoc where the keys are mutation operations and the values are the probabilities that the operations will be performed.  The operations can consist of the strings change_type, delete, insert, swap_elements, deep_copy_elements, delete_elements, and change_label.",
		"example" : "(create_entities\n    \"MutateEntity\"\n  (lambda (list 1 2 3 4 5 6 7 8 9 10 11 12 13 14 (assoc \"a\" 1 \"b\" 2)))\n)\n(mutate_entity \"MutateEntity\" 0.4 \"MutatedEntity\")\n(print (retrieve_entity_root \"MutatedEntity\"))"
	},

	{
		"parameter" : "commonality_entities id_path entity1 id_path entity2",
		"output" : "number",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Evaluates to the total count of all of the nodes referenced within entity1 and entity2 that are equivalent, including all contained entities.",
		"example" : "(create_entities \"e1\" (lambda (assoc \"a\" 3 \"b\" 4)) )\n(create_entities \"e2\" (lambda (assoc \"c\" 3 \"b\" 4)) )\n(print (commonality_entities \"e1\" \"e2\"))"
	},

	{
		"parameter" : "edit_distance_entities id_path entity1 id_path entity2",
		"output" : "number",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Evaluates to the edit distance of all of the nodes referenced within entity1 and entity2 that are equivalent, including all contained entities.",
		"example" : "(create_entities \"e1\" (lambda (assoc \"a\" 3 \"b\" 4)) )\n(create_entities \"e2\" (lambda (assoc \"c\" 3 \"b\" 4)) )\n(print (edit_distance_entities \"e1\" \"e2\"))"
	},


	{
		"parameter" : "intersect_entities id_path entity1 id_path entity2 [id_path entity3]",
		"output" : "id_path",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Creates an entity of whatever is common between the Entities represented by entity1 and entity2 exclusive.  Returns the id_path of a new entity created contained by the entity that ran it.  Uses entity3 as the optional destination via an internal call create_contained_entity. Any contained entities will be intersected either based on matching name or maximal similarity for nameless entities.",
		"example" : "(create_entities \"e1\" (lambda (assoc \"a\" 3 \"b\" 4)) )\n(create_entities \"e2\" (lambda (assoc \"c\" 3 \"b\" 4)) )\n(intersect_entities \"e1\" \"e2\" \"e3\"))\n(print (retrieve_entity_root \"e3\")))"
	},

	{
		"parameter" : "union_entities id_path entity1 id_path entity2 [id_path entity3]",
		"output" : "id_path",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Creates an entity of whatever is inclusive when merging the Entities represented by entity1 and entity2.  Returns the id_path of a new entity created contained by the entity that ran it.  Uses entity3 as the optional destination via an internal call to create_contained_entity.  Any contained entities will be unioned either based on matching name or maximal similarity for nameless entities.",
		"example" : "(create_entities \"e1\" (lambda (assoc \"a\" 3 \"b\" 4)) )\n(create_entities \"e2\" (lambda (assoc \"c\" 3 \"b\" 4)) )\n(union_entities \"e1\" \"e2\" \"e3\"))\n(print (retrieve_entity_root \"e3\")))"
	},

	{
		"parameter" : "difference_entities id_path entity1 id_path entity2",
		"output" : "*",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Finds the difference between the entities specified by entity1 and entity2 and generates code that, if evaluated passing the entity id_path as its parameter \"_\", would create a new entity into the id_path specified by its parameter \"new_entity\" (null if unspecified), which would contain the applied difference between the two entities and returns the newly created entity id_path.  Useful for finding the smallest set of what needs to be changed to apply it to a new and different entity.",
		"example" : "(create_entities \"DiffEntity1\" (lambda (assoc \"a\" 3 \"b\" 4)) )\n(create_entities (list \"DiffEntity1\" \"DiffEntityChild1\") (lambda (assoc \"x\" 3 \"y\" 4 \"z\" 6)) )\n(create_entities (list \"DiffEntity1\" \"DiffEntityChild1\" \"DiffEntityChild2\") (lambda (assoc \"p\" 3 \"q\" 4 \"u\" 5 \"v\" 6 \"w\" 7)) )\n(create_entities (list \"DiffEntity1\" \"DiffEntityChild1\" \"DiffEntityChild2\" \"DiffEntityChild3\") (lambda (assoc \"e\" 3 \"p\" 4 \"a\" 5 \"o\" 6 \"w\" 7)) )\n(create_entities (list \"DiffEntity1\" \"OnlyIn1\") (lambda (assoc \"m\" 4)) )\n(create_entities (list \"DiffEntity1\") (lambda (assoc \"E\" 3 \"F\" 4)) )\n(create_entities (list \"DiffEntity1\") (lambda (assoc \"e\" 3 \"f\" 4 \"g\" 5 \"h\" 6)) )\n\n(create_entities \"DiffEntity2\" (lambda (assoc \"c\" 3 \"b\" 4)) )\n(create_entities (list \"DiffEntity2\" \"DiffEntityChild1\") (lambda (assoc \"x\" 3 \"y\" 4 \"z\" 5)) )\n(create_entities (list \"DiffEntity2\" \"DiffEntityChild1\" \"DiffEntityChild2\") (lambda (assoc \"p\" 3 \"q\" 4 \"u\" 5 \"v\" 6 \"w\" 7)) )\n(create_entities (list \"DiffEntity2\" \"DiffEntityChild1\" \"DiffEntityChild2\" \"DiffEntityChild3\") (lambda (assoc \"e\" 3 \"p\" 4 \"a\" 5 \"o\" 6 \"w\" 7)) )\n(create_entities (list \"DiffEntity2\" \"OnlyIn2\") (lambda (assoc \"o\" 6)) )\n(create_entities (list \"DiffEntity2\") (lambda (assoc \"E\" 3 \"F\" 4 \"G\" 5 \"H\" 6)) )\n(create_entities (list \"DiffEntity2\") (lambda (assoc \"e\" 3 \"f\" 4)) )\n\n(print (contained_entities \"DiffEntity2\"))\n\n(print (difference_entities \"DiffEntity1\" \"DiffEntity2\"))\n\n(let (assoc new_entity\n    (call (difference_entities \"DiffEntity1\" \"DiffEntity2\") (assoc _ \"DiffEntity1\")))\n  (print new_entity)\n  (print (retrieve_entity_root new_entity))\n  (print (retrieve_entity_root (list new_entity \"DiffEntityChild1\")))\n  (print (contained_entities new_entity))\n)\n\n(create_entities \"DiffContainer\" null)\n\n(create_entities (list \"DiffContainer\" \"DiffEntity1\") (lambda (assoc \"a\" 3 \"b\" 4)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity1\" \"DiffEntityChild1\") (lambda (assoc \"x\" 3 \"y\" 4 \"z\" 6)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity1\" \"DiffEntityChild1\" \"DiffEntityChild2\") (lambda (assoc \"p\" 3 \"q\" 4 \"u\" 5 \"v\" 6 \"w\" 7)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity1\" \"DiffEntityChild1\" \"DiffEntityChild2\" \"DiffEntityChild3\") (lambda (assoc \"e\" 3 \"p\" 4 \"a\" 5 \"o\" 6 \"w\" 7)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity1\" \"OnlyIn1\") (lambda (assoc \"m\" 4)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity1\") (lambda (assoc \"E\" 3 \"F\" 4)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity1\") (lambda (assoc \"e\" 3 \"f\" 4 \"g\" 5 \"h\" 6)) )\n\n(create_entities (list \"DiffContainer\" \"DiffEntity2\") (lambda (assoc \"c\" 3 \"b\" 4)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity2\" \"DiffEntityChild1\") (lambda (assoc \"x\" 3 \"y\" 4 \"z\" 6)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity2\" \"DiffEntityChild1\" \"DiffEntityChild2\") (lambda (assoc \"p\" 3 \"q\" 4 \"u\" 5 \"v\" 6 \"w\" 7)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity2\" \"DiffEntityChild1\" \"DiffEntityChild2\" \"DiffEntityChild3\") (lambda (assoc \"e\" 3 \"p\" 4 \"a\" 5 \"o\" 6 \"w\" 7)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity2\" \"OnlyIn2\") (lambda (assoc \"o\" 6)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity2\") (lambda (assoc \"E\" 3 \"F\" 4 \"G\" 5 \"H\" 6)) )\n(create_entities (list \"DiffContainer\" \"DiffEntity2\") (lambda (assoc \"e\" 3 \"f\" 4)) )\n\n(print (difference_entities (list \"DiffContainer\" \"DiffEntity1\") (list \"DiffContainer\" \"DiffEntity2\") ))\n\n(let (assoc new_entity\n    (call (difference_entities (list \"DiffContainer\" \"DiffEntity1\") (list \"DiffContainer\" \"DiffEntity2\") )\n      (assoc _ (list \"DiffContainer\" \"DiffEntity1\") )))\n  (print new_entity)\n  (print (get_entity_code new_entity))\n  (print (get_entity_code (list new_entity \"DiffEntityChild1\")))\n  (print (contained_entities new_entity))\n)\n"
	},

	{
		"parameter" : "mix_entities id_path entity1 id_path entity2 [number keep_chance_entity1] [number keep_chance_entity2] [number similar_mix_chance] [number max_mix_depth] [number chance_mix_unnamed_children] [id_path entity3]",
		"output" : "id_path",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Performs a union operation on the entities represented by entity1 and entity2, but randomly ignores nodes from one or the other tree if not equal.  If only keep_chance_entity1 is specified, keep_chance_entity2 defaults to 1-keep_chance_entity1.  keep_chance_entity1 specifies the probability that a node from the entity represented by entity1 will be kept, and keep_chance_entity2 the probability that a node from the entity represented by entity2 will be kept.  similar_mix_chance is the additional probability that two nodes will mix if they have some commonality, which will include interpolating number or string values based on keep_chance_node1 and keep_chance_node2, and defaults to 0.0.  If similar_mix_chance is negative, then 1 minus the value will be anded with the commonality probability, so -1 means that it will never mix and 0 means it will only mix when sufficiently common.  If max_mix_depth not null and greater than 0, it will specify how deep recursively to mix the nodes.  chance_mix_unnamed_children represents the probability that an unnamed entity pair will be mixed versus preserved as independent chunks, where 0.2 would yield 20% of the entities mixed. Returns the id_path of a new entity created contained by the entity that ran it.  Uses entity3 as the optional destination via an internal call to create_contained_entity.   Any contained entities will be mixed either based on matching name or maximal similarity for nameless entities.",
		"example" : "(create_entities \"e1\" (lambda (assoc \"a\" 3 \"b\" 4)) )\n(create_entities \"e2\" (lambda (assoc \"c\" 3 \"b\" 4)) )\n(mix_entities \"e1\" \"e2\" 0.5 0.5 \"e3\")"
	},

	{
		"parameter" : "get_entity_comments [id_path entity] [string label] [bool deep_comments]",
		"output" : "*",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Evaluates to the corresponding comments based on the parameters.  If the id_path is specified or null is specified as the id_path, then it will use the current entity.  If the label is null or empty string, it will retrieve comments for the entity root, otherwise if it is a valid label it will attempt to retrieve the comments for that label, null if the label doesn't exist.  If deep_comments is specified and the label is a declare, then it will return a list of two elements.  The first element of this list is an assoc with the keys being the parameters and the values being lists of the descriptions followed by the default value.  The second element of this list is the comment of the assoc itself, which is intended to be used to describe what is returned.  If label is empty string or null and deep_comments is true, then it will return an assoc of label to comment for each label in the entity.",
		"example" : "(print (get_entity_comments))\n(print (get_entity_comments \"label_name\" .true)"
	},

	{
		"parameter" : "retrieve_entity_root [id_path entity] [bool suppress_label_escapes]",
		"output" : "*",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Evaluates to the entity's code, looking up the entity by the id_path. If suppress_label_escapes is false or omitted, will disable any labels obtained by inserting an extra # at the beginning of each.",
		"example" : "(print (retrieve_entity_root))\n(print (retrieve_entity_root 1))"
	},

	{
		"parameter" : "assign_entity_roots [id_path entity_1] * root_1 [id_path entity_2] [* root_2] [...]",
		"output" : "bool",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Sets the code of the entity specified by id_path to node.  If no id_path specified, then uses the current entity, otherwise accesses a contained entity. On assigning the code to the new entity, it will enable any labels obtained by removing any extra #s from the beginning of each.  If all assignments were successful, then returns true, otherwise returns false.",
		"example" : "(print (assign_entity_roots (list)))"
	},

	{
		"parameter" : "accum_entity_roots [id_path entity_1] * root_1 [id_path entity_2] [* root_2] [...]",
		"output" : "bool",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Accumulates the code of the entity specified by id_path to node. If no id_path specified, then uses the current entity, otherwise accesses a contained entity. On assigning the code to the new entity, it will enable any labels obtained by removing any extra #s from the beginning of each.  If all accumulations were successful, then returns true, otherwise returns false.",
		"example" : "(create_entities \"AER_test\" (lambda (null ##a 1 ##b 2)))\n(accum_entity_roots \"AER_test\" (list ##c 3))\n(print (retrieve_entity_root \"AER_test\" 1))"
	},

	{
		"parameter" : "get_entity_rand_seed [id_path entity]",
		"output" : "string",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Evaluates to a string representing the current state of the random number generator for the entity specified by id_path used for seeding the random streams of any calls to the entity.",
		"example" : "(create_entities \"RandTest\" (lambda\n  (null ##a (rand) )\n  ))\n(print (call_entity \"RandTest\" \"a\"))\n(print (get_entity_rand_seed \"RandTest\"))\n"
	},

	{
		"parameter" : "set_entity_rand_seed [id_path entity] * node [bool deep]",
		"output" : "string",
		"permissions" : "entity",
		"description" : "Sets the random number seed and state for the random number generator of the specified entity, or the current entity if not specified, to the state specified by node.  If node is already a string in the proper format output by get_entity_rand_seed, then it will set the random generator to that current state, picking up where the previous state left off.  If it is anything else, it uses the value as a random seed to start the generator.  Note that this will not affect the state of the current random number stream, only future random streams created by the entity for new calls.  The parameter deep defaults to false, but if it is true, all contained entities are recursively set with random seeds based on the specified random seed and a hash of their relative id_path path to the entity being set.",
		"example" : "(create_entities \"RandTest\" (lambda\n  (null ##a (rand) )\n  ) )\n(create_entities (list \"RandTest\" \"DeepRand\") (lambda\n  (null ##a (rand) )\n  ) )\n(declare (assoc seed (get_entity_rand_seed \"RandTest\")))\n(print (call_entity \"RandTest\" \"a\"))\n(set_entity_rand_seed \"RandTest\" 1234)\n(print (call_entity \"RandTest\" \"a\"))"
	},

	{
		"parameter" : "get_entity_permissions [id_path entity]",
		"output" : "bool",
		"new value" : "new",
		"permissions" : "entity",
		"description" : "Returns an assoc of the permissions of the specified entity, where each key is the permission and each value is either true or false.",
		"example" : " (create_entities \"RootTest\" (lambda (print (system_time)) ))\n(print (get_entity_permissions \"RootTest\"))"
	},

	{
		"parameter" : "set_entity_permissions id_path entity bool|assoc permissions [bool deep]",
		"output" : "id_path",
		"new value" : "new",
		"permissions" : "entity",
		"description" : "Sets the permissions on the entity specified by id_path.  If permissions is true, then it grants all permissions, if it is false, then it removes all.  If permissions is an assoc, it alters the permissions of the assoc keys to the boolean values of the assoc's values.  The parameter deep defaults to false, but if it is true, all contained entities have their permissions updated.  Returns the id_path of the entity.",
		"example" : "(create_entities \"RootTest\" (lambda (print (system_time)) ))\n(set_entity_permissions \"RootTest\" .true)\n(call_entity \"RootTest\")"
	},

	{
		"parameter" : "create_entities [id_path entity_1] * node_1 [id_path entity_2] [* node_2] [...]",
		"output" : "list of id_path",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Creates a new entity with code specified by node, returning the list of id_path paths for each of the entities created.  Uses the optional entity location specified by the id_path, ignored if null or invalid.  Evaluates to a list of all of the new entities ids, null in place of each id_path if it was unable to create the id_path.  If the entity does not have permission to create the entities, it will evaluate to null.  If the id_path is omitted, then it will create the new entity in the calling entity.  If id_path specifies an existing entity, then it will create the new entity within that existing entity.  If the last id_path in the string is not an existing entity, then it will attempt to create that entity (returning null if it cannot).  Can only be performed by an entity that contains to the destination specified by id_path. Will automatically remove a # from the beginning of each label in case the label had been disabled.  Unlike the rest of the entity creation commands, create_entities specifies the optional id_path first to make it easy to read entity definitions.  If more than 2 parameters are specified, create_entities will iterate through all of the pairs of parameters, treating them like the first two as it creates new entities.",
		"example" : "(print (create_entities \"MyLibrary\" (lambda (+ #three 3 4)) ) )\n\n(create_entities \"EntityWithChildren\" (lambda (assoc \"a\" 3 \"b\" 4)) )\n(create_entities (list \"EntityWithChildren\" \"Child1\") (lambda (assoc \"x\" 3 \"y\" 4)) )\n(create_entities (list \"EntityWithChildren\" \"Child2\") (lambda (assoc \"p\" 3 \"q\" 4)) )\n(print (contained_entities \"EntityWithChildren\"))"
	},

	{
		"parameter" : "clone_entities id_path source_entity_1 [id_path destination_entity_1] [id_path source_entity_2] [id_path destination_entity_2] [...]",
		"output" : "list of id_path",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Creates a clone of source_entity_1.  If destination_entity_1 is not specified, then it clones the entity into the current entity.  If destination_entity_1 is specified, then it clones it into the location specified by destination_entity_1; if destination_entity_1 is an existing entity, then it will create it within that entity, if not, it will attempt to create it with the given id_path.  Evaluates to the id_path of the new entity.  Can only be performed by an entity that contains both source_entity_1 and the specified path of destination_entity_1. If multiple entities are specified, it will move each from the source to the destination.  Evaluates to a list of the new entity ids.",
		"example" : "(print (create_entities \"MyLibrary\" (lambda (+ #three 3 4)) ) )\n(print (clone_entities \"MyLibrary\" \"MyNewLibrary\"))"
	},

	{
		"parameter" : "move_entities id_path source_entity_1 [id_path destination_entity_1] [id_path source_entity_2] [id_path destination_entity_2] [...]",
		"output" : "list of id_path",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Moves the entity from location specified by source_entity_1 to destination destination_entity_1.  If destination_entity_1 exists, it will move source_entity_1 using source_entity_1's current id_path into destination_entity_1.  If destination_entity_1 does not exist, then it will move source_entity_1 and rename it to the end of the id_path specified in destination_entity_1. Can only be performed by a containing entity relative to both ids.  If multiple entities are specified, it will move each from the source to the destination.  Evaluates to a list of the new entity ids.",
		"example" : "(print (create_entities \"MyLibrary\" (lambda (+ #three 3 4)) ) )\n(print (move_entities \"MyLibrary\" \"MyLibrary2\"))"
	},

	{
		"parameter" : "destroy_entities [id_path entity_1] [id_path entity_2] [...]",
		"output" : "bool",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Destroys the entities specified by the ids entity_1, entity_2, etc. Can only be performed by containing entity.  Returns true if all entities were successfully destroyed, false if not due to not existing in the first place or due to code being currently run in it.",
		"example" : "(print (create_entities \"MyLibrary\" (lambda (+ #three 3 4)) ) )\n(print (contained_entities))\n(destroy_entities \"MyLibrary\")\n(print (contained_entities))"
	},

	{
		"parameter" : "load string resource_path [string resource_type] [assoc params]",
		"output" : "*",
		"new value" : "new",
		"permissions" : "load",
		"description" : "Loads the data specified by the resource in string.  Attempts to load the file type and parse it into appropriate data and evaluate to the corresponding code. The parameter escape_filename defaults to false, but if it is true, it will aggressively escape filenames using only alphanumeric characters and the underscore, using underscore as an escape character.  If resource_type is specified and not null, it will use the resource_type specified instead of the extension of the resource_path.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  Note that loading from a non-'.amlg' extension will only ever provide lists, assocs, numbers, and strings.",
		"example" : "(print (load \"my_directory/MyModule.amlg\"))"
	},

	{
		"parameter" : "load_entity string resource_path [id_path entity] [string resource_type] [bool persistent] [assoc params]",
		"output" : "id_path",
		"new value" : "new",
		"permissions" : "load",
		"description" : "Loads an entity specified by the resource in string.  Attempts to load the file type and parse it into appropriate data and store it in the entity specified by id_path, following the same id_path creation rules as create_entities, except that if no id_path is specified, it may default to a name based on the resource if available.  If persistent is true, default is false, then any modifications to the entity or any entity contained within it will be written out to the resource, so that the memory and persistent storage are synchronized.  Options for the file I/O are specified as key-value pairs in params.  See File I/O for the file types and related params.",
		"example" : "(load_entity \"my_directory/MyModule.amlg\" \"MyModule\")"
	},

	{
		"parameter" : "store string resource_path * node [string resource_type] [assoc params]",
		"output" : "bool",
		"new value" : "new",
		"permissions" : "store",
		"description" : "Stores the code specified by * to the resource in string. Returns true if successful, false if not. If resource_type is specified and not null, it will use the resource_type specified instead of the extension of the resource_path.    Options for the file I/O are specified as key-value pairs in params.  See File I/O for the file types and related params.",
		"example" : "(store \"my_directory/MyData.amlg\" (list 1 2 3))"
	},

	{
		"parameter" : "store_entity string resource_path id_path entity [string resource_type] [bool persistent] [assoc params]",
		"output" : "bool",
		"new value" : "new",
		"permissions" : "store",
		"description" : "Stores the entity specified by the id_path to the resource in string. Returns true if successful, false if not. If resource_type is specified and not null, it will use the resource_type specified instead of the extension of the resource_path.  If persistent is true, default is false, then any modifications to the entity or any entity contained within it will be written out to the resource, so that the memory and persistent storage are synchronized.  Options for the file I/O are specified as key-value pairs in params.  See File I/O for the file types and related params.",
		"example" : "(store_entity \"my_directory/MyData.amlg\" \"MyData\")"
	},

	{
		"parameter" : "contains_entity id_path entity",
		"output" : "bool",
		"permissions" : "entity",
		"new value" : "new",
		"description" : "Returns true if the referred to entity specified by id_path exists.",
		"example" : "(print (create_entities \"MyLibrary\" (lambda (+ #three 3 4)) ) )\n(print (contains_entity \"MyLibrary\"))\n(print (contains_entity (list \"MyLibrary\")))"
	},

	{
		"parameter" : "contained_entities [id_path containing_entity | query|list condition1] [query|list condition2] ...[ query|list conditionN]",
		"output" : "list of string",
		"permissions" : "entity",
		"new value" : "conditional",
		"description" : "Returns a list of strings of ids of entities contained in the entity specified by id_path or current entity if containing_entity is omitted.  The parameters of condition1 through conditionN are query conditions, and they may be any of the query opcodes or may be a list of query opcodes (beginning with query_), where each condition will be executed in order.",
		"example" : "(create_entities (list \"TestEntity\" \"Child\")\n  (lambda (null ##TargetLabel 3))\n) \n\n (contained_entities \"TestEntity\" (list\n  (query_exists \"TargetLabel\")\n)) \n\n ; For more examples see the individual entries for each query."
	},

	{
		"parameter" : "compute_on_contained_entities [id_path containing_entity | query|list condition1] [query|list condition2] ...[ query|list conditionN]",
		"output" : "*",
		"permissions" : "entity",
		"new value" : "conditional",
		"description" : "Performs queries like contained_entities but returns a value or set of values appropriate for the last query in conditions.  The parameters of condition1 through conditionN are query conditions, and they may be any of the query opcodes or may be a list of query opcodes (beginning with query_), where each condition will be executed in order.  If the last query does not return anything, then it will just return the matching entities.",
		"example" : "(create_entities (list \"TestEntity\" \"Child\")\n  (lambda (null ##TargetLabel 3))\n) \n\n (compute_on_contained_entities \"TestEntity\" (list\n  (query_exists \"TargetLabel\")\n)) \n\n ; For more examples see the individual entries for each query."
	},

	{
		"parameter" : "query_select number num_to_select [number start_offset] [number random_seed]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects num_to_select entities sorted by entity id.  If start_offset is specified, then it will return num_to_select starting that far in, and subsequent calls can be used to get all entities in batches.  If random_seed is specified, then it will select num_to_select entities randomly from the list based on the random seed.  If random_seed is specified and start_offset is null, then it will not guarantee a position in the order for subsequent calls that specify start_offset, and will execute more quickly.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_select 4 (null) (rand))\n))"
	},

	{
		"parameter" : "query_sample number num_to_select [string weight_label_name] [number random_seed]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects a random sample of num_to_select entities sorted by entity_id with replacement. If weight_label_name is specified and not null, it will use weight_label_name as the feature containing the weights for the sampling, which will be normalized prior to sampling.  Non-numbers and negative infinite values for weights will be ignored, and if there are any infinite values, those will be selected from uniformly.  If random_seed is specified, then it will select num_to_select entities randomly from the list based on the random seed. If random_seed is not specified then the subsequent calls will return the same sample of entities.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_sample 4 (rand))\n))\n(contained_entities \"TestEntity\" (list\n  (query_sample 4 \"weight\" (rand))\n))"
	},

	{
		"parameter" : "query_in_entity_list list list_of_entity_ids",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects only the entities in list_of_entity_ids.  It can be used to filter results before doing subsequent queries.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_in_entity_list (list \"Entity1\" \"Entity2\"))\n))"
	},

	{
		"parameter" : "query_not_in_entity_list list list_of_entity_ids",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, filters out the entities in list_of_entity_ids.  It can be used to filter results before doing subsequent queries.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_not_in_entity_list (list \"Entity1\" \"Entity2\"))\n))"
	},

	{
		"parameter" : "query_exists string label_name",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities which have the named label.  If called last with compute_on_contained_entities, then it returns an assoc of entity ids, where each value is an assoc of corresponding label names and values.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_exists \"TargetLabel\")\n))"
	},

	{
		"parameter" : "query_not_exists string label_name",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities which do not have the named label.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_not_exists \"TargetLabel\")\n))"
	},

	{
		"parameter" : "query_equals string label_name * node_value",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities for which the specified label is equal to the specified *.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_equals \"TargetLabel\" 3)\n))"
	},

	{
		"parameter" : "query_not_equals string label_name * node_value",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities for which the specified label is not equal to the specified *.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_not_equals \"TargetLabel\" 3)\n))"
	},

	{
		"parameter" : "query_between string label_name * lower_bound * upper_bound",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities for which the specified label has a value between the specified lower_bound an upper_bound.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_between \"TargetLabel\" 2 5)\n)) \n\n (contained_entities \"TestEntity\" (list\n  (query_between \"x\" -4 5)\n  (query_between \"y\" -4 0)\n))"
	},

	{
		"parameter" : "query_not_between string label_name * lower_bound * upper_bound",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities for which the specified label has a value outside the specified lower_bound an upper_bound.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_not_between \"TargetLabel\" 2 5)\n)) \n\n (contained_entities \"TestEntity\" (list\n  (query_not_between \"x\" -4 5)\n  (query_not_between \"y\" -4 0)\n))"
	},

	{
		"parameter" : "query_among string label_name list values",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities for which the specified label has one of the values specified in values.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_among \"TargetLabel\" (2 5))\n)) \n\n (contained_entities \"TestEntity\" (list\n  (query_among \"x\" (list -4 5))\n  (query_among \"y\" (list -4 0))\n))"
	},

	{
		"parameter" : "query_not_among string label_name list values",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities for which the specified label does not have one of the values specified in values.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_not_among \"TargetLabel\" (2 5))\n)) \n\n (contained_entities \"TestEntity\" (list\n  (query_not_among \"x\" (list -4 5))\n  (query_not_among \"y\" (list -4 0))\n))"
	},

	{
		"parameter" : "query_max string label_name [number entities_returned] [bool numeric]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects a number of entities with the highest values in the specified label.  If entities_returned is specified, it will return that many entities, otherwise will return 1.  If numeric is true, its default value, then it only considers numeric values; if false, will consider all types.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_max \"TargetLabel\" 3)\n))"
	},

	{
		"parameter" : "query_min string label_name [number entities_returned] [bool numeric]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects a number of entities with the lowest values in the specified label.  If entities_returned is specified, it will return that many entities, otherwise will return 1.  If numeric is true, its default value, then it only considers numeric values; if false, will consider all types.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_min \"TargetLabel\" 3)\n))"
	},

	{
		"parameter" : "query_sum string label_name [string weight_label_name]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, returns the sum of all entities over the specified label.  If weight_label_name is specified, it will find the weighted sum, which is the same as a dot product.",
		"example" : "(compute_on_contained_entities \"TestEntity\" (list\n (query_sum \"TargetLabel\")\n))"
	},

	{
		"parameter" : "query_mode string label_name [string weight_label_name]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, finds the statistical mode of label_name for numerical data.  If weight_label_name is specified, it will find the weighted mode.",
		"example" : "(compute_on_contained_entities \"TestEntity\" (list\n (query_mode \"TargetLabel\")\n))"
	},

	{
		"parameter" : "query_quantile string label_name [number q] [string weight_label_name]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, finds the statistical quantile of label_name for numerical data, using q as the parameter to the quantile (default 0.5, median).  If weight_label_name is specified, it will find the weighted quantile, otherwise weight is 1.",
		"example" : "(compute_on_contained_entities \"TestEntity\" (list\n (query_quantile \"TargetLabel\" 0.75)\n))"
	},

	{
		"parameter" : "query_generalized_mean string label_name [number p] [string weight_label_name] [number center] [bool calculate_moment] [bool absolute_value]",
		"output" : "query",
		"new value" : "partial",
		"description": "When used as a query argument, computes the generalized mean over the label_name for numeric data.  If p is specified (which defaults to 1), it is the parameter that can control the type of mean from minimum (negative infinity) to harmonic mean (-1) to geometric mean (0) to arithmetic mean (1) to maximum (infinity).  If weight_label_name is specified, it will normalize the weights and compute a weighted mean.  If center is specified, calculations will use that as central point, default is 0.0.  If calculate_moment is true, results will not be raised to 1/p.  If absolute_value is true, the differences will take the absolute value.  Various parameterizations of generalized_mean can be used to compute moments about the mean, especially setting the calculate_moment parameter to true and using the mean as the center.",
		"example" : "(compute_on_contained_entities \"TestEntity\" (list\n (query_generalized_mean \"TargetLabel\" 0.5)\n))"
	},

	{
		"parameter" : "query_min_difference string label_name [number cyclic_range] [bool include_zero_difference]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, finds the smallest difference between any two values for the specified label. If cyclic_range is null, the default value, then it will assume the values are not cyclic; if it is a number, then it will assume the range is from 0 to cyclic_range.  If include_zero_difference is true, its default value, then it will return 0 if the smallest gap between any two numbers is 0; if false, it will return the smallest nonzero value.",
		"example" : "(compute_on_contained_entities \"TestEntity\" (list\n (query_min_difference \"TargetLabel\")\n))"
	},

	{
		"parameter" : "query_max_difference string label_name [number cyclic_range]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, finds the largest difference between any two values for the specified label. If cyclic_range is null, the default value, then it will assume the values are not cyclic; if it is a number, then it will assume the range is from 0 to cyclic_range.",
		"example" : "(compute_on_contained_entities \"TestEntity\" (list\n (query_max_difference \"TargetLabel\")\n))"
	},

	{
		"parameter" : "query_value_masses string label_name [string weight_label_name]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, computes the counts for each value of the label and returns an assoc with the keys being the label values and the values being the counts or weights of the values.  If weight_label_name is specified, then it will accumulate that weight for each value, otherwise it will use a weight of 1 for each yielding a count.",
		"example" : "(compute_on_contained_entities \"TestEntity\" (list\n (query_value_masses \"TargetLabel\")\n))"
	},

	{
		"parameter" : "query_less_or_equal_to string label_name * max_value",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities with a value in the specified label less than or equal to the specified *.",
		"example" : "(contained_entities \"TestEntity\" (list\n (query_less_or_equal_to \"TargetLabel\" 3)\n))"
	},

	{
		"parameter" : "query_greater_or_equal_to string label_name * min_value",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities with a value in the specified label greater than or equal to the specified *.",
		"example" : "(contained_entities \"TestEntity\" (list\n  (query_greater_or_equal_to \"TargetLabel\" 3)\n))"
	},

	{
		"parameter" : "query_within_generalized_distance number max_distance list axis_labels list|string axis_values_or_entity_id [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [string weights_selection_feature] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects entities which represent a point within a certain generalized norm to a given point. axis_labels specifies the names of the coordinate axes (as labels on the target entity), and axis_values_or_entity_id specifies the corresponding values for the point to test from or if a string the entity to collect the labels from.  The parameter p_value is the generalized norm parameter, where the value of 1 is probability space and Manhattan distance, the default, 2 being Euclidean distance, etc.  The weights parameter specifies how to weight the different dimensions.  If weights is a list, each value maps to its respective element in the vectors.  If weights is null, then it will assume that the weights are 1.  If weights is an assoc, then the parameter value_names will select the weights from the assoc.  If weights is an assoc of assocs, then the parameter weights_selection_feature will select which set of weights to use and redistribute and normalize any probability masses for unused features to features that are used.  The parameter distance_types is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are \"nominal_bool\", \"nominal_number\", \"nominal_string\", \"nominal_code\", \"continuous_number\", \"continuous_number_cyclic\", \"continuous_string\", and \"continuous_code\".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around.  For attributes, the particular distance_types specifies what is expected.  For a nominal distance_type, a number indicates the nominal count, whereas null will infer from the values available.  For continuous, a null means unbounded where distance for a null will be computed automatically from the relevant data; a single number indicates the difference between a value and a null, a specified uncertainty.  Cyclic requires either a single value or a list of two values; a list of two values indicates that the first value, the lower bound, will wrap around to the upper bound, the second value specified; if only a single number is provided instead of a list, then it will assume that number for the upper bound and 0 for the lower bound.  For the string distance type, the value specified can be a number indicating the maximum possible string length, inferred if null is provided.  For code, the value specified can be a number indicating the maximum number of nodes in the code (including labels), inferred if null is provided.  Deviations contains numbers that are used during the distance calculation, per-element, prior to exponentiation.  Specifying null as deviations is equivalent to setting each deviation to 0. max_distance is the maximum distance allowed. The optional radius_label parameter represents the label name of the radius of the entity (if the radius is within the distance, the entity is selected). The optional numerical_precision represents one of three values: \"precise\", which computes every distance with high numerical precision, \"fast\", which computes every distance with lower but faster numerical precision, and \"recompute_precise\", which computes distances quickly with lower precision but then recomputes any distance values that will be returned with higher precision. If called last with compute_on_contained_entities, then it returns an assoc of the entity ids with their distances.  A transform will be applied to these distances based on distance_transform.  If distance_transform is \"surprisal\" then distances will be calculated as surprisals, and weights will not be applied to the values.  If distance_transform is \"surprisal_to_prob\" then distances will be calculated as surprisals and will be transformed back into probabilities for aggregating, and then transformed back to surprisals.  If distance_transform is a number or omitted, which will default to 1.0, then it will be treated as a distance weight exponent, and will be applied to each distance as distance^distance_weight_exponent, only using entity weights for nonpositive values of distance_transform.  If entity_weight_label_name is specified, it will multiply the resulting value for each entity (after distance_weight_exponent, etc. have been applied) by the value in the label of entity_weight_label_name. If output_sorted_list is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if output_sorted_list is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether distance_weight_exponent is positive or negative respectively). If output_sorted_list is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity. If output_sorted_list is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.",
		"example" : "(contained_entities \"TestContainerExec\" (list (query_within_generalized_distance 3 (list \"x\" \"y\") (list 0.0 0.0) 0.01 (list 2 1) (list \"nominal_number\" \"continuous_number_cyclic\") (list 1 360) (null) (null) 1 (null) \"random seed 1234\" \"radius\") ) )"
	},

	{
		"parameter" : "query_nearest_generalized_distance list|number entities_returned list axis_labels list|string axis_values_or_entity_id [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [string weights_selection_feature] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list]",
		"output" : "query",
		"new value" : "partial",
		"description" : "When used as a query argument, selects the closest entities which represent a point within a certain generalized norm to a given point. axis_labels specifies the names of the coordinate axes (as labels on the target entity), and axis_values_or_entity_id specifies the corresponding values for the point to test from or if a string the entity to collect the labels from. The parameter p_value is the generalized norm parameter, where the value of 1 is probability space and Manhattan distance, the default, 2 being Euclidean distance, etc.  The weights parameter specifies how to weight the different dimensions.  If weights is a list, each value maps to its respective element in the vectors.  If weights is null, then it will assume that the weights are 1.  If weights is an assoc, then the parameter value_names will select the weights from the assoc.  If weights is an assoc of assocs, then the parameter weights_selection_feature will select which set of weights to use and redistribute and normalize any probability masses for unused features to features that are used.  The parameter distance_types is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are \"nominal_bool\", \"nominal_number\", \"nominal_string\", \"nominal_code\", \"continuous_number\", \"continuous_number_cyclic\", \"continuous_string\", and \"continuous_code\".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around.  \nFor attributes, the particular distance_types specifies what particular attributes are expected.  For a nominal distance_type, a number indicates the nominal count, whereas null will infer from the values given.  Cyclic requires a single value, which is the upper bound of the difference for the cycle range (e.g., if the value is 360, then the supremum difference between two values will be 360, leading 1 and 359 to have a difference of 2).\n  Deviations are used during distance calculation to specify uncertainty per-element, the minimum difference between two values prior to exponentiation.  Specifying null as a deviation is equivalent to setting each deviation to 0, unless distance_transform is \"surprisal_to_prob\", in which case it will attempt to infer a deviation.  Each deviation for each feature can be a single value or a list.  If it is a single value, that value is used as the deviation and differences and deviations for null values will automatically computed from the data based on the maximum difference.  If a deviation is provided as a list, then the first value is the deviation, the second value is the difference to use when one of the values being compared is null, and the third value is the difference to use when both of the values are null.  If the third value is omitted, it will use the second value for both.  If both of the null values are omitted, then it will compute the maximum difference and use that for both.  For nominal types, the value for each feature can be a numeric deviation, an assoc, or a list.  If the value is an assoc it specifies deviation information, where each key of the assoc is the nominal value, and each value of the assoc can be a numeric deviation value, a list, or an assoc, with the list specifying either an assoc followed optionally by the default deviation.  This inner assoc, regardless of whether it is in a list, maps the value to each actual value's deviation.  The parameter entities_returned specifies either the number of entities to return, or is a list.  If entities_returned is a list, the first element of the list specifies the minimum incremental probability or percent of mass that the next largest entity would comprise (e.g., 0.05 would return at most 20 entities if they were all equal in percent of mass), and the other elements are optional.  The second element is the minimum number of entities to return, the third element is the maximum number of entities to return, and the fourth indicates the number of additional entities to include after any of the aforementioned thresholds (defaulting to zero).  If there is disagreement among the constraints for entities_returned, the constraint yielding the fewest entities will govern the number of entities returned. The optional radius_label parameter represents the label name of the radius of the entity (if the radius is within the distance, the entity is selected). The optional numerical_precision represents one of three values: \"precise\", which computes every distance with high numerical precision, \"fast\", which computes every distance with lower but faster numerical precision, and \"recompute_precise\", which computes distances quickly with lower precision but then recomputes any distance values that will be returned with higher precision.  If called last with compute_on_contained_entities, then it returns an assoc of the entity ids with their distances.  A transform will be applied to these distances based on distance_transform.  If distance_transform is \"surprisal\" then distances will be calculated as surprisals, and weights will not be applied to the values.  If distance_transform is \"surprisal_to_prob\" then distances will be calculated as surprisals and will be transformed back into probabilities for aggregating, and then transformed back to surprisals.  If distance_transform is a number or omitted, which will default to 1.0, then it will be treated as a distance weight exponent, and will be applied to each distance as distance^distance_weight_exponent, only using entity weights for nonpositive values of distance_transform.  If entity_weight_label_name is specified, it will multiply the resulting value for each entity (after distance_weight_exponent, etc. have been applied) by the value in the label of entity_weight_label_name. If output_sorted_list is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if output_sorted_list is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether distance_weight_exponent is positive or negative respectively).  If output_sorted_list is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity. If output_sorted_list is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity. If output_sorted_list is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.",
		"example" : "(contained_entities \"TestContainerExec\" (list (query_nearest_generalized_distance 3 (list \"x\" \"y\") (list 0.0 0.0) 0.01 (list 2 1) (list \"nominal_number\" \"continuous_number_cyclic\") (list 1 360) (null) (null) 1 (null) \"random seed 1234\" \"radius\") ) )"
	},
	
	{
		"parameter" : "query_distance_contributions list|number entities_returned list feature_labels list axis_value_lists [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [string weights_selection_feature] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list]",
		"output" : "query",
		"new value" : "partial",
		"concurrency" : true,
		"description" : "When used as a query argument, computes the case conviction for every case given in case_ids_to_compute with respect to *all* cases in the contained entities set input during a query.  If case_ids_to_compute is null or an empty list, case conviction is computed for all cases.  feature_labels specifies the names of the features to consider the during computation.  axis_value_lists specifies a list of lists, where each inner list is the set of values for each axis, and a distance contribution will be computed for each outer list.  The parameter p_value is the generalized norm parameter, where the value of 1 is probability space and Manhattan distance, the default, 2 being Euclidean distance, etc.  The weights parameter specifies how to weight the different dimensions.  If weights is a list, each value maps to its respective element in the vectors.  If weights is null, then it will assume that the weights are 1.  If weights is an assoc, then the parameter value_names will select the weights from the assoc.  If weights is an assoc of assocs, then the parameter weights_selection_feature will select which set of weights to use and redistribute and normalize any probability masses for unused features to features that are used.  The parameter distance_types is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are \"nominal_bool\", \"nominal_number\", \"nominal_string\", \"nominal_code\", \"continuous_number\", \"continuous_number_cyclic\", \"continuous_string\", and \"continuous_code\".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around.  \nFor attributes, the particular distance_types specifies what particular attributes are expected.  For a nominal distance_type, a number indicates the nominal count, whereas null will infer from the values given.  Cyclic requires a single value, which is the upper bound of the difference for the cycle range (e.g., if the value is 360, then the supremum difference between two values will be 360, leading 1 and 359 to have a difference of 2).\n  Deviations are used during distance calculation to specify uncertainty per-element, the minimum difference between two values prior to exponentiation.  Specifying null as a deviation is equivalent to setting each deviation to 0, unless distance_transform is \"surprisal\" or \"surprisal_to_prob\", in which case it will attempt to infer a deviation.  Each deviation for each feature can be a single value or a list.  If it is a single value, that value is used as the deviation and differences and deviations for null values will automatically computed from the data based on the maximum difference.  If a deviation is provided as a list, then the first value is the deviation, the second value is the difference to use when one of the values being compared is null, and the third value is the difference to use when both of the values are null.  If the third value is omitted, it will use the second value for both.  If both of the null values are omitted, then it will compute the maximum difference and use that for both.  For nominal types, the value for each feature can be a numeric deviation, an assoc, or a list.  If the value is an assoc it specifies deviation information, where each key of the assoc is the nominal value, and each value of the assoc can be a numeric deviation value, a list, or an assoc, with the list specifying either an assoc followed optionally by the default deviation.  This inner assoc, regardless of whether it is in a list, maps the value to each actual value's deviation.  The parameter entities_returned specifies either the number of entities to return, or is a list.  If entities_returned is a list, the first element of the list specifies the minimum incremental probability or percent of mass that the next largest entity would comprise (e.g., 0.05 would return at most 20 entities if they were all equal in percent of mass), and the other elements are optional.  The second element is the minimum number of entities to return, the third element is the maximum number of entities to return, and the fourth indicates the number of additional entities to include after any of the aforementioned thresholds (defaulting to zero).  If there is disagreement among the constraints for entities_returned, the constraint yielding the fewest entities will govern the number of entities returned. The optional radius_label parameter represents the label name of the radius of the entity (if the radius is within the distance, the entity is selected). The optional numerical_precision represents one of three values: \"precise\", which computes every distance with high numerical precision, \"fast\", which computes every distance with lower but faster numerical precision, and \"recompute_precise\", which computes distances quickly with lower precision but then recomputes any distance values that will be returned with higher precision.  If called last with compute_on_contained_entities, then it returns an assoc of the entity ids with their convictions.  A transform will be applied to these distances based on distance_transform.  If distance_transform is \"surprisal\" then distances will be calculated as surprisals, and weights will not be applied to the values.  If distance_transform is \"surprisal_to_prob\" then distances will be calculated as surprisals and will be transformed back into probabilities for aggregating, and then transformed back to surprisals.  If distance_transform is a number or omitted, which will default to 1.0, then it will be used as a parameter for a generalized mean (e.g., -1 yields the harmonic mean) to average the distances, only using entity weights for nonpositive values of distance_transform.  If entity_weight_label_name is specified, it will multiply the resulting value for each entity (after distance_weight_exponent, etc. have been applied) by the value in the label of entity_weight_label_name. If output_sorted_list is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if output_sorted_list is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether distance_weight_exponent is positive or negative respectively).  If output_sorted_list is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity. If output_sorted_list is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.",
		"example" : "(compute_on_contained_entities \"SurprisalTransformContainer\" (list (query_distance_contributions 4 (list \"x\") [[0]] 1 (null) (null) (null) (list 0.25) (null) \"surprisal\" (null) \"fixed_seed\" (null) \"precise\") ))"
	},

	{
		"parameter" : "query_entity_convictions list|number entities_returned list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [string weights_selection_feature] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [bool conviction_of_removal] [* output_sorted_list]",
		"output" : "query",
		"new value" : "partial",
		"concurrency" : true,
		"description" : "When used as a query argument, computes the case conviction for every case given in case_ids_to_compute with respect to *all* cases in the contained entities set input during a query.  If case_ids_to_compute is null or an empty list, case conviction is computed for all cases.  feature_labels specifies the names of the features to consider the during computation. The parameter p_value is the generalized norm parameter, where the value of 1 is probability space and Manhattan distance, the default, 2 being Euclidean distance, etc.  The weights parameter specifies how to weight the different dimensions.  If weights is a list, each value maps to its respective element in the vectors.  If weights is null, then it will assume that the weights are 1.  If weights is an assoc, then the parameter value_names will select the weights from the assoc.  If weights is an assoc of assocs, then the parameter weights_selection_feature will select which set of weights to use and redistribute and normalize any probability masses for unused features to features that are used.  The parameter distance_types is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are \"nominal_bool\", \"nominal_number\", \"nominal_string\", \"nominal_code\", \"continuous_number\", \"continuous_number_cyclic\", \"continuous_string\", and \"continuous_code\".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around.  \nFor attributes, the particular distance_types specifies what particular attributes are expected.  For a nominal distance_type, a number indicates the nominal count, whereas null will infer from the values given.  Cyclic requires a single value, which is the upper bound of the difference for the cycle range (e.g., if the value is 360, then the supremum difference between two values will be 360, leading 1 and 359 to have a difference of 2).\n  Deviations are used during distance calculation to specify uncertainty per-element, the minimum difference between two values prior to exponentiation.  Specifying null as a deviation is equivalent to setting each deviation to 0, unless distance_transform is \"surprisal\" or \"surprisal_to_prob\", in which case it will attempt to infer a deviation.  Each deviation for each feature can be a single value or a list.  If it is a single value, that value is used as the deviation and differences and deviations for null values will automatically computed from the data based on the maximum difference.  If a deviation is provided as a list, then the first value is the deviation, the second value is the difference to use when one of the values being compared is null, and the third value is the difference to use when both of the values are null.  If the third value is omitted, it will use the second value for both.  If both of the null values are omitted, then it will compute the maximum difference and use that for both.  For nominal types, the value for each feature can be a numeric deviation, an assoc, or a list.  If the value is an assoc it specifies deviation information, where each key of the assoc is the nominal value, and each value of the assoc can be a numeric deviation value, a list, or an assoc, with the list specifying either an assoc followed optionally by the default deviation.  This inner assoc, regardless of whether it is in a list, maps the value to each actual value's deviation.  The parameter entities_returned specifies either the number of entities to return, or is a list.  If entities_returned is a list, the first element of the list specifies the minimum incremental probability or percent of mass that the next largest entity would comprise (e.g., 0.05 would return at most 20 entities if they were all equal in percent of mass), and the other elements are optional.  The second element is the minimum number of entities to return, the third element is the maximum number of entities to return, and the fourth indicates the number of additional entities to include after any of the aforementioned thresholds (defaulting to zero).  If there is disagreement among the constraints for entities_returned, the constraint yielding the fewest entities will govern the number of entities returned. The optional radius_label parameter represents the label name of the radius of the entity (if the radius is within the distance, the entity is selected). The optional numerical_precision represents one of three values: \"precise\", which computes every distance with high numerical precision, \"fast\", which computes every distance with lower but faster numerical precision, and \"recompute_precise\", which computes distances quickly with lower precision but then recomputes any distance values that will be returned with higher precision.  If called last with compute_on_contained_entities, then it returns an assoc of the entity ids with their convictions.  A transform will be applied to these distances based on distance_transform.  If distance_transform is \"surprisal\" then distances will be calculated as surprisals, and weights will not be applied to the values.  If distance_transform is \"surprisal_to_prob\" then distances will be calculated as surprisals and will be transformed back into probabilities for aggregating, and then transformed back to surprisals.  If distance_transform is a number or omitted, which will default to 1.0, then it will be used as a parameter for a generalized mean (e.g., -1 yields the harmonic mean) to average the distances, only using entity weights for nonpositive values of distance_transform.  If entity_weight_label_name is specified, it will multiply the resulting value for each entity (after distance_weight_exponent, etc. have been applied) by the value in the label of entity_weight_label_name. If conviction_of_removal is true, then it will compute the conviction as if the entities specified by entity_ids_to_compute were removed; if false (the default), then will compute the conviction as if those entities were added or included. If output_sorted_list is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if output_sorted_list is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether distance_weight_exponent is positive or negative respectively).  If output_sorted_list is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity. If output_sorted_list is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.",
		"example" : "(compute_on_contained_entities (list (query_entity_convictions 2 (list \"alpha\" \"b\" \"c\") (null) 0.1 (null) (list 0 0 1) ) ))"
	},

	{
		"parameter" : "query_entity_group_kl_divergence list|number entities_returned list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [string weights_selection_feature] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [bool conviction_of_removal]",
		"output" : "query",
		"new value" : "partial",
		"concurrency" : true,
		"description" : "When used as a query argument, computes the case kl divergence for every case given in case_ids_to_compute as a group with respect to *all* cases in the contained entities set input during a query.  If case_ids_to_compute is null or an empty list, case conviction is computed for all cases.  feature_labels specifies the names of the features to consider the during computation. The parameter p_value is the generalized norm parameter, where the value of 1 is probability space and Manhattan distance, the default, 2 being Euclidean distance, etc.  The weights parameter specifies how to weight the different dimensions.  If weights is a list, each value maps to its respective element in the vectors.  If weights is null, then it will assume that the weights are 1.  If weights is an assoc, then the parameter value_names will select the weights from the assoc.  If weights is an assoc of assocs, then the parameter weights_selection_feature will select which set of weights to use and redistribute and normalize any probability masses for unused features to features that are used.  The parameter distance_types is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are \"nominal_bool\", \"nominal_number\", \"nominal_string\", \"nominal_code\", \"continuous_number\", \"continuous_number_cyclic\", \"continuous_string\", and \"continuous_code\".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around.  \nFor attributes, the particular distance_types specifies what particular attributes are expected.  For a nominal distance_type, a number indicates the nominal count, whereas null will infer from the values given.  Cyclic requires a single value, which is the upper bound of the difference for the cycle range (e.g., if the value is 360, then the supremum difference between two values will be 360, leading 1 and 359 to have a difference of 2).\n  Deviations are used during distance calculation to specify uncertainty per-element, the minimum difference between two values prior to exponentiation.  Specifying null as a deviation is equivalent to setting each deviation to 0, unless distance_transform is \"surprisal\" or \"surprisal_to_prob\", in which case it will attempt to infer a deviation.  Each deviation for each feature can be a single value or a list.  If it is a single value, that value is used as the deviation and differences and deviations for null values will automatically computed from the data based on the maximum difference.  If a deviation is provided as a list, then the first value is the deviation, the second value is the difference to use when one of the values being compared is null, and the third value is the difference to use when both of the values are null.  If the third value is omitted, it will use the second value for both.  If both of the null values are omitted, then it will compute the maximum difference and use that for both.  For nominal types, the value for each feature can be a numeric deviation, an assoc, or a list.  If the value is an assoc it specifies deviation information, where each key of the assoc is the nominal value, and each value of the assoc can be a numeric deviation value, a list, or an assoc, with the list specifying either an assoc followed optionally by the default deviation.  This inner assoc, regardless of whether it is in a list, maps the value to each actual value's deviation.  The parameter entities_returned specifies either the number of entities to return, or is a list.  If entities_returned is a list, the first element of the list specifies the minimum incremental probability or percent of mass that the next largest entity would comprise (e.g., 0.05 would return at most 20 entities if they were all equal in percent of mass), and the other elements are optional.  The second element is the minimum number of entities to return, the third element is the maximum number of entities to return, and the fourth indicates the number of additional entities to include after any of the aforementioned thresholds (defaulting to zero).  If there is disagreement among the constraints for entities_returned, the constraint yielding the fewest entities will govern the number of entities returned. The optional radius_label parameter represents the label name of the radius of the entity (if the radius is within the distance, the entity is selected). The optional numerical_precision represents one of three values: \"precise\", which computes every distance with high numerical precision, \"fast\", which computes every distance with lower but faster numerical precision, and \"recompute_precise\", which computes distances quickly with lower precision but then recomputes any distance values that will be returned with higher precision.  If called last with compute_on_contained_entities, then it returns an assoc of the entity ids with their convictions.  A transform will be applied to these distances based on distance_transform.  If distance_transform is \"surprisal\" then distances will be calculated as surprisals, and weights will not be applied to the values.  If distance_transform is \"surprisal_to_prob\" then distances will be calculated as surprisals and will be transformed back into probabilities for aggregating, and then transformed back to surprisals.  If distance_transform is a number or omitted, which will default to 1.0, then it will be used as a parameter for a generalized mean (e.g., -1 yields the harmonic mean) to average the distances, only using entity weights for nonpositive values of distance_transform.  If entity_weight_label_name is specified, it will multiply the resulting value for each entity (after distance_weight_exponent, etc. have been applied) by the value in the label of entity_weight_label_name. If conviction_of_removal is true, then it will compute the conviction as if the entities specified by entity_ids_to_compute were removed; if false (the default), then will compute the conviction as if those entities were added or included.",
		"example" : "(compute_on_contained_entities (list (query_entity_group_kl_divergence 2 (list \"x\" \"y\") obj2_verts 2 (null) (null) (null) (null) (null) -1 (null) \"random seed 1234\") ))"
	},

	{
		"parameter" : "query_entity_distance_contributions list|number entities_returned list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [string weights_selection_feature] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list]",
		"output" : "query",
		"new value" : "partial",
		"concurrency" : true,
		"description" : "When used as a query argument, computes the case conviction for every case given in case_ids_to_compute with respect to *all* cases in the contained entities set input during a query.  If case_ids_to_compute is null or an empty list, case conviction is computed for all cases.  feature_labels specifies the names of the features to consider the during computation. The parameter p_value is the generalized norm parameter, where the value of 1 is probability space and Manhattan distance, the default, 2 being Euclidean distance, etc.  The weights parameter specifies how to weight the different dimensions.  If weights is a list, each value maps to its respective element in the vectors.  If weights is null, then it will assume that the weights are 1.  If weights is an assoc, then the parameter value_names will select the weights from the assoc.  If weights is an assoc of assocs, then the parameter weights_selection_feature will select which set of weights to use and redistribute and normalize any probability masses for unused features to features that are used.  The parameter distance_types is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are \"nominal_bool\", \"nominal_number\", \"nominal_string\", \"nominal_code\", \"continuous_number\", \"continuous_number_cyclic\", \"continuous_string\", and \"continuous_code\".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around.  \nFor attributes, the particular distance_types specifies what particular attributes are expected.  For a nominal distance_type, a number indicates the nominal count, whereas null will infer from the values given.  Cyclic requires a single value, which is the upper bound of the difference for the cycle range (e.g., if the value is 360, then the supremum difference between two values will be 360, leading 1 and 359 to have a difference of 2).\n  Deviations are used during distance calculation to specify uncertainty per-element, the minimum difference between two values prior to exponentiation.  Specifying null as a deviation is equivalent to setting each deviation to 0, unless distance_transform is \"surprisal\" or \"surprisal_to_prob\", in which case it will attempt to infer a deviation.  Each deviation for each feature can be a single value or a list.  If it is a single value, that value is used as the deviation and differences and deviations for null values will automatically computed from the data based on the maximum difference.  If a deviation is provided as a list, then the first value is the deviation, the second value is the difference to use when one of the values being compared is null, and the third value is the difference to use when both of the values are null.  If the third value is omitted, it will use the second value for both.  If both of the null values are omitted, then it will compute the maximum difference and use that for both.  For nominal types, the value for each feature can be a numeric deviation, an assoc, or a list.  If the value is an assoc it specifies deviation information, where each key of the assoc is the nominal value, and each value of the assoc can be a numeric deviation value, a list, or an assoc, with the list specifying either an assoc followed optionally by the default deviation.  This inner assoc, regardless of whether it is in a list, maps the value to each actual value's deviation.  The parameter entities_returned specifies either the number of entities to return, or is a list.  If entities_returned is a list, the first element of the list specifies the minimum incremental probability or percent of mass that the next largest entity would comprise (e.g., 0.05 would return at most 20 entities if they were all equal in percent of mass), and the other elements are optional.  The second element is the minimum number of entities to return, the third element is the maximum number of entities to return, and the fourth indicates the number of additional entities to include after any of the aforementioned thresholds (defaulting to zero).  If there is disagreement among the constraints for entities_returned, the constraint yielding the fewest entities will govern the number of entities returned. The optional radius_label parameter represents the label name of the radius of the entity (if the radius is within the distance, the entity is selected). The optional numerical_precision represents one of three values: \"precise\", which computes every distance with high numerical precision, \"fast\", which computes every distance with lower but faster numerical precision, and \"recompute_precise\", which computes distances quickly with lower precision but then recomputes any distance values that will be returned with higher precision.  If called last with compute_on_contained_entities, then it returns an assoc of the entity ids with their convictions.  A transform will be applied to these distances based on distance_transform.  If distance_transform is \"surprisal\" then distances will be calculated as surprisals, and weights will not be applied to the values.  If distance_transform is \"surprisal_to_prob\" then distances will be calculated as surprisals and will be transformed back into probabilities for aggregating, and then transformed back to surprisals.  If distance_transform is a number or omitted, which will default to 1.0, then it will be used as a parameter for a generalized mean (e.g., -1 yields the harmonic mean) to average the distances, only using entity weights for nonpositive values of distance_transform.  If entity_weight_label_name is specified, it will multiply the resulting value for each entity (after distance_weight_exponent, etc. have been applied) by the value in the label of entity_weight_label_name. If output_sorted_list is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if output_sorted_list is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether distance_weight_exponent is positive or negative respectively).  If output_sorted_list is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity. If output_sorted_list is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.",
		"example" : "(compute_on_contained_entities (list (query_entity_distance_contributions 2 (list \"x\" \"y\") (null) 2 (null) (null) (null) (null) (null) -1 (null) \"random seed 1234\") ))"
	},

	{
		"parameter" : "query_entity_kl_divergences list|number entities_returned list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [string weights_selection_feature] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [bool conviction_of_removal] [* output_sorted_list]",
		"output" : "query",
		"new value" : "partial",
		"concurrency" : true,
		"description" : "When used as a query argument, computes the case conviction for every case given in case_ids_to_compute with respect to *all* cases in the contained entities set input during a query.  If case_ids_to_compute is null or an empty list, case conviction is computed for all cases.  feature_labels specifies the names of the features to consider the during computation. The parameter p_value is the generalized norm parameter, where the value of 1 is probability space and Manhattan distance, the default, 2 being Euclidean distance, etc.  The weights parameter specifies how to weight the different dimensions.  If weights is a list, each value maps to its respective element in the vectors.  If weights is null, then it will assume that the weights are 1.  If weights is an assoc, then the parameter value_names will select the weights from the assoc.  If weights is an assoc of assocs, then the parameter weights_selection_feature will select which set of weights to use and redistribute and normalize any probability masses for unused features to features that are used.  The parameter distance_types is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are \"nominal_bool\", \"nominal_number\", \"nominal_string\", \"nominal_code\", \"continuous_number\", \"continuous_number_cyclic\", \"continuous_string\", and \"continuous_code\".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around.  \nFor attributes, the particular distance_types specifies what particular attributes are expected.  For a nominal distance_type, a number indicates the nominal count, whereas null will infer from the values given.  Cyclic requires a single value, which is the upper bound of the difference for the cycle range (e.g., if the value is 360, then the supremum difference between two values will be 360, leading 1 and 359 to have a difference of 2).\n  Deviations are used during distance calculation to specify uncertainty per-element, the minimum difference between two values prior to exponentiation.  Specifying null as a deviation is equivalent to setting each deviation to 0, unless distance_transform is \"surprisal\" or \"surprisal_to_prob\", in which case it will attempt to infer a deviation.  Each deviation for each feature can be a single value or a list.  If it is a single value, that value is used as the deviation and differences and deviations for null values will automatically computed from the data based on the maximum difference.  If a deviation is provided as a list, then the first value is the deviation, the second value is the difference to use when one of the values being compared is null, and the third value is the difference to use when both of the values are null.  If the third value is omitted, it will use the second value for both.  If both of the null values are omitted, then it will compute the maximum difference and use that for both.  For nominal types, the value for each feature can be a numeric deviation, an assoc, or a list.  If the value is an assoc it specifies deviation information, where each key of the assoc is the nominal value, and each value of the assoc can be a numeric deviation value, a list, or an assoc, with the list specifying either an assoc followed optionally by the default deviation.  This inner assoc, regardless of whether it is in a list, maps the value to each actual value's deviation.  The parameter entities_returned specifies either the number of entities to return, or is a list.  If entities_returned is a list, the first element of the list specifies the minimum incremental probability or percent of mass that the next largest entity would comprise (e.g., 0.05 would return at most 20 entities if they were all equal in percent of mass), and the other elements are optional.  The second element is the minimum number of entities to return, the third element is the maximum number of entities to return, and the fourth indicates the number of additional entities to include after any of the aforementioned thresholds (defaulting to zero).  If there is disagreement among the constraints for entities_returned, the constraint yielding the fewest entities will govern the number of entities returned. The optional radius_label parameter represents the label name of the radius of the entity (if the radius is within the distance, the entity is selected). The optional numerical_precision represents one of three values: \"precise\", which computes every distance with high numerical precision, \"fast\", which computes every distance with lower but faster numerical precision, and \"recompute_precise\", which computes distances quickly with lower precision but then recomputes any distance values that will be returned with higher precision.  If called last with compute_on_contained_entities, then it returns an assoc of the entity ids with their convictions.  A transform will be applied to these distances based on distance_transform.  If distance_transform is \"surprisal\" then distances will be calculated as surprisals, and weights will not be applied to the values.  If distance_transform is \"surprisal_to_prob\" then distances will be calculated as surprisals and will be transformed back into probabilities for aggregating, and then transformed back to surprisals.  If distance_transform is a number or omitted, which will default to 1.0, then it will be used as a parameter for a generalized mean (e.g., -1 yields the harmonic mean) to average the distances, only using entity weights for nonpositive values of distance_transform.  If entity_weight_label_name is specified, it will multiply the resulting value for each entity (after distance_weight_exponent, etc. have been applied) by the value in the label of entity_weight_label_name. If conviction_of_removal is true, then it will compute the conviction as if the entities specified by entity_ids_to_compute were removed; if false (the default), then will compute the conviction as if those entities were added or included. If output_sorted_list is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if output_sorted_list is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether distance_weight_exponent is positive or negative respectively).  If output_sorted_list is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity. If output_sorted_list is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.",
		"example" : "(compute_on_contained_entities (list (query_entity_kl_divergences 2 (list \"x\" \"y\") (null) 2 (null) (null) (null) (null) (null) -1 (null) \"random seed 1234\")))"
	},

	{
		"parameter" : "contains_label [id_path entity] string label_name",
		"output" : "bool",
		"new value" : "new",
		"description" : "Evaluates to true if the label represented by string exists for the entity specified by id_path for a contained entity.  If id_path is omitted, then it uses the current entity.",
		"example" : "(print (contains_label \"MyEntity\" \"some_label\"))"
	},

	{
		"parameter" : "assign_to_entities [id_path entity_1] assoc variable_value_pairs_1 [id_path entity_2] [assoc variable_value_pairs_2] [...]",
		"output" : "bool",
		"new value" : "new",
		"permissions" : "entity",
		"description" : "For each index-value pair of variable_value_pairs, assigns the value to the labeled variable on the contained entity represented by the respective entity, itself if no id_path specified, while retaining the original labels. If none found, it will not cause an assignment. When the value is assigned, any labels will be cleared out and the root of the value will be assigned the comments and labels of the previous root at the label. Will perform an assignment for each of the entities referenced, returning .true if all assignments were successful, .false if not.",
		"example" : "(null #asgn_test1 12)\n(assign_to_entities (assoc asgn_test1 4))\n(print (retrieve_from_entity \"asgn_test1\"))\n\n"
	},

	{
		"parameter" : "accum_to_entities [id_path entity_1] assoc variable_value_pairs_1 [id_path entity_2] [assoc variable_value_pairs_2] [...]",
		"output" : "bool",
		"new value" : "new",
		"permissions" : "entity",
		"description" : "For each index-value pair of assoc, retrieves the labeled variable from the respective entity, accumulates it by the corresponding value in variable_value_pairs, then assigns the value to the labeled variable on the contained entity represented by the id_path, itself if no id_path specified, while retaining the original labels. If none found, it will not cause an assignment. When the value is assigned, any labels will be cleared out and the root of the value will be assigned the comments and labels of the previous root at the label.  Accumulation is performed differently based on the type: for numeric values it adds, for strings, it concatenates, for lists it appends, and for assocs it appends based on the pair. Will perform an accum for each of the entities referenced, returning .true if all assignments were successful, .false if not.",
		"example" : "(null #asgn_test1 12)\n(accum_to_entities (assoc asgn_test1 4))\n(print (retrieve_from_entity \"asgn_test1\"))\n\n"
	},

	{
		"parameter" : "direct_assign_to_entities [id_path entity_1] assoc variable_value_pairs_1 [id_path entity_2] [assoc variable_value_pairs_2] [...]",
		"output" : "bool",
		"new value" : "new",
		"permissions" : "entity",
		"description" : "Like assign_to_entities, except retains any/all labels, comments, etc.",
		"example" : "(create_entities \"DRFE\" (lambda (null ##a 12)) )\n(print (direct_retrieve_from_entity \"DRFE\" \"a\"))\n(print (direct_assign_to_entities \"DRFE\" (assoc a 7)))\n(print (direct_retrieve_from_entity \"DRFE\" \"a\"))"
	},

	{
		"parameter" : "retrieve_from_entity [id_path entity] [string|list|assoc label_names]",
		"output" : "*",
		"new value" : "conditional",
		"permissions" : "entity",
		"description" : "If string specified, returns the value of the contained entity id_path, itself if no id_path specified, at the label specified by the string. If list specified, returns the value of the contained entity id_path, itself if no id_path specified, returns a list of the values on the stack specified by each element of the list interpreted as a string label. If assoc specified, returns the value of the contained entity id_path, itself if no id_path specified, returns an assoc with the indices of the assoc passed in with the values being the appropriate values of the label represented by each index.",
		"example" : "(null #asgn_test1 12)\n(assign_to_entities (assoc asgn_test1 4))\n(print (retrieve_from_entity \"asgn_test1\"))\n\n(null #asgn_test2 12)\n(assign_to_entities (assoc asgn_test2 4))\n(print (retrieve_from_entity \"asgn_test2\"))\n(create_entities \"RCT\" (lambda (null ##a 12 ##b 13)) )\n(print (retrieve_from_entity \"RCT\" \"a\"))\n(print (retrieve_from_entity \"RCT\" (list \"a\" \"b\") ))\n(print (retrieve_from_entity \"RCT\" (zip (list \"a\" \"b\") null) ))\n"
	},

	{
		"parameter" : "direct_retrieve_from_entity [id_path entity] [string|list|assoc label_names]",
		"output" : "*",
		"new value" : "conditional",
		"permissions" : "entity",
		"description" : "Like retrieve_from_entity, except retains labels.",
		"example" : "(create_entities \"DRFE\" (lambda (null ##a 12)) )\n(print (direct_retrieve_from_entity \"DRFE\" \"a\"))\n(print (direct_assign_to_entities \"DRFE\" (assoc a 7)))\n(print (direct_retrieve_from_entity \"DRFE\" \"a\"))"
	},

	{
		"parameter" : "call_entity id_path entity [string label_name] [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [number max_contained_entities] [number max_contained_entity_depth] [number max_entity_id_length] [return_warnings]",
		"output" : "*",
		"new value" : "conditional",
		"permissions" : "entity",
		"new scope" : true,
		"description" : "Calls the contained entity specified by id_path, using the entity as the new entity context.  It will evaluate to the return value of the call, null if not found.  If string is specified, then it will call the label specified by string.  If assoc is specified, then it will pass assoc as the arguments on the scope stack.  If operation_limit is specified, it represents the number of operations that are allowed to be performed. If operation_limit is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations. The root entity has infinite computing cycles.  If max_node_allocations is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If max_node_allocations is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If max_opcode_execution_depth is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise max_opcode_execution_depth limits how deep nested opcodes will be called.  The parameters max_contained_entities, max_contained_entity_depth, and max_entity_id_length constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream. If any performance constraints are given or return_warnings is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is an assoc mapping all warnings to their number of occurrences, and perf_constraint violation is a string denoting the constraint exceeded (or (null) if none)), unless return_warnings is false, in which case just the value will be returned.",
		"example" : "(create_entities \"TestContainerExec\"\n  (lambda (parallel\n  ##d (print \"hello \" x)\n  )) \n)\n\n(print (call_entity \"TestContainerExec\" \"d\" (assoc x \"goodbye\")))"
	},

	{
		"parameter" : "call_entity_get_changes id_path entity [string label_name] [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [number max_contained_entities] [number max_contained_entity_depth] [number max_entity_id_length] [bool return_warnings]",
		"output" : "list of *1 *2",
		"new value" : "conditional",
		"permissions" : "entity",
		"new scope" : true,
		"description" : "Like call_entity returning the value in *1.  However, it also returns a list of direct_assign_to_entities calls with respective data in *2, holding a log of all of the changes that have elapsed.  The log may be evaluated to apply or re-apply the changes to any id_path passed in to the log as _. If any performance constraints are given or return_warnings is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is an assoc mapping all warnings to their number of occurrences, and perf_constraint violation is a string denoting the constraint exceeded (or (null) if none)), unless return_warnings is false, in which case just the value will be returned.",
		"example" : "(create_entities \"CEGCTest\" (lambda\n    (null ##a_assign\n    (seq \n      (create_entities \"Contained\" (lambda\n        (null ##a 4 )\n      ))\n      (print (retrieve_from_entity \"Contained\" \"a\") )\n      (assign_to_entities \"Contained\" (assoc a 6) )\n      (print (retrieve_from_entity \"Contained\" \"a\") )\n      (set_entity_rand_seed \"Contained\" \"bbbb\")\n      (destroy_entities \"Contained\")\n    )\n  )\n))\n\n(print (call_entity_get_changes \"CEGCTest\" \"a_assign\"))\n"
	},

	{
		"parameter" : "call_container string parent_label_name [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [bool return_warnings]",
		"output" : "*",
		"new value" : "new",
		"new scope" : true,
		"description" : "Attempts to call the container associated with the label that begins with a caret (^); the caret indicates that the label is allowed to be accessed by contained entities.  It will evaluate to the return value of the call, null if not found.  The call is made on the label specified by string.  If assoc is specified, then it will pass assoc as the arguments on the scope stack.  The parameter accessing_entity will automatically be set to the id of the caller, regardless of the arguments.  If operation_limit is specified, it represents the number of operations that are allowed to be performed. If operation_limit is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations. The root entity has infinite computing cycles.  If max_node_allocations is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If max_node_allocations is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If max_opcode_execution_depth is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise max_opcode_execution_depth limits how deep nested opcodes will be called.  The execution performed will use a random number stream created from the entity's random number stream. If any performance constraints are given or return_warnings is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is an assoc mapping all warnings to their number of occurrences, and perf_constraint violation is a string denoting the constraint exceeded (or (null) if none)), unless return_warnings is false, in which case just the value will be returned.",
		"permissions" : "entity",
		"example" : "(create_entities \"TestContainerExec\"\n  (lambda (parallel\n  ##^a 3\n  ##b (contained_entities)\n  ##c (+ x 1)\n  ##d (call_entity \"TCEc\" \"q\" (assoc x x))\n  ##x 4\n  ##y 5\n  )) \n)\n(create_entities (list \"TestContainerExec\" \"TCEc\")\n  (lambda (parallel\n  ##p 3\n  ##q (+ x (call_container \"a\"))\n  ##bar \"foo\"\n  ))\n)\n\n(print (call_entity \"TestContainerExec\" \"d\" (assoc x 4)))"
	}
];

// Help Node out by setting up define.
if (typeof exports === 'object' && typeof define !== 'function') {
	define = function (factory) {
		factory(require, exports, module);
	};
}

if (typeof define === 'function') {
	define(function (require, exports, module) {
		exports.language = data;
	});
}
