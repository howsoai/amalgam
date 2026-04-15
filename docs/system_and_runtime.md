### Opcode: `help`
#### Parameters
`[string topic]`
#### Description
If no parameter is specified it returns a string of the topics that can be used.  For given a `topic`, returns a string or relevant data that describes the given topic.
#### Details
 - Permissions required:  all
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(help "+")
```
Output:
```amalgam
{
	allows_concurrency .true
	description "Sums all numbers."
	examples [
			{example "(+ 1 2 3 4)" output "10"}
		]
	frequency_per_10000_opcodes 18
	new_scope .false
	new_target_scope .false
	opcode_group "Basic Math"
	parameters "[number x1] [number x2] ... [number xN]"
	permissions "none"
	requires_entity .false
	returns "number"
	value_newness "new"
}
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `print`
#### Parameters
`[* node1] [* node2] ... [* nodeN]`
#### Description
Prints each of the parameters in order in a manner interpretable as if they were code, except strings are printed without quotes.  Output is pretty-printed.
#### Details
 - Permissions required:  std_out_and_std_err
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): null
#### Examples
Example:
```amalgam
(print "hello world\n")
```
Output:
```amalgam
.null
```
Example:
```amalgam
(print
	1
	2
	[3 4]
	"5"
	"\n"
)
```
Output:
```amalgam
.null
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `system_time`
#### Parameters
``
#### Description
Evaluates to the current system time since epoch in seconds (including fractions of seconds).
#### Details
 - Permissions required:  environment
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): existing
#### Examples
Example:
```amalgam
(system_time)
```
Output:
```amalgam
1773855306.4474
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `system`
#### Parameters
`string command [* optional1] ... [* optionalN]`
#### Description
Executes system command specified by `command`.  The available system commands are as follows:
 - exit:                Exits the application.
 - readline:            Reads a line of input from the terminal and returns the string.
 - printline:           Prints a line of string output of the second argument directly to the terminal and returns null.
 - cwd:                 If no additional parameter is specified, returns the current working directory. If an additional parameter is specified, it attempts to change the current working directory to that parameter, returning true on success and false on failure.
 - system:              Executes the the second argument as a system command (i.e., a string that would normally be run on the command line). Returns `.null` if the command was not found. If found, it returns a list, where the first value is the exit code and the second value is a string containing everything printed to stdout.
 - os:                  Returns a string describing the operating system.
 - sleep:               Sleeps for the amount of seconds specified by the second argument.
 - version:             Returns a string representing the current Amalgam version.
 - est_mem_reserved:    Returns data involving the estimated memory reserved.
 - est_mem_used:        Returns data involving the estimated memory used (excluding memory management overhead, caching, etc.).
 - mem_diagnostics:     Returns data involving memory diagnostics.
 - rand:                Returns the number of bytes specified by the additional parameter of secure random data intended for cryptographic use.
 - sign_key_pair:       Returns a list of two values, first a public key and second a secret key, for use with cryptographic signatures using the Ed25519 algorithm, generated via securely generated random numbers.
 - encrypt_key_pair:    Returns a list of two values, first a public key and second a secret key, for use with cryptographic encryption using the XSalsa20 and Curve25519 algorithms, generated via securely generated random numbers.
 - debugging_info:      Returns a list of two values. The first is true if a debugger is present, false if it is not. The second is true if debugging sources is enabled, which means that source code location information is prepended to opcodes comments for any opcodes loaded from a file.
 - get_max_num_threads: Returns the current maximum number of threads.
 - set_max_num_threads: Attempts to set the current maximum number of threads, where 0 means to use the number of processor cores reported by the operating system. Returns the maximum number of threads after it has been set.
 - built_in_data:       Returns built-in data compiled along with the version information.
#### Details
 - Permissions required:  all
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(system "debugging_info")
```
Output:
```amalgam
[.false .false]
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

### Opcode: `reclaim_resources`
#### Parameters
`[id_path entity] [bool apply_to_all_contained_entities] [bool|list clear_query_caches] [bool collect_garbage] [bool force_free_memory]`
#### Description
Frees resources of the specified types on `entity`, which is the current entity if null.  Will include all contained entities if `apply_to_all_contained_entities` is true, which defaults to false, though the opcode will be unable to complete if there are concurrent threads running on any of the contained entities.  The parameter `clear_query_caches` will remove the query caches, which will make it faster to add, remove, or edit contained entities, but the cache will be rebuilt once a query is called.  If `clear_query_caches` is a boolean, then it will either clear all the caches or none.  If `clear_query_caches` is a list of strings, then it will only clear caches for the labels corresponding to the strings in the list.  The parameter `collect_garbage` will perform garbage collection on the entity, and if `force_free_memory` is true, it will reallocate memory buffers to their current size, after garbage collection if both are specified.
#### Details
 - Permissions required:  alter_performance
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): null
#### Examples
Example:
```amalgam
(reclaim_resources .null .true ["x"] .true .true )
```
Output:
```amalgam
.null
```
Example:
```amalgam
(reclaim_resources .null .true .true .true .true )
```
Output:
```amalgam
.null
```

[Amalgam Opcodes](./index.md#amalgam-opcodes)

