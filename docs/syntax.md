# Amalgam Syntax
Using <>'s to enclose optional elements, the general syntax is:
```amalgam
#annotation line 1...
#annotation line N
;comments until the end of the line
;and can continue on to additional lines
<||><@>(opcode <parameter1> <parameter2> ...)
```

The language generally follows a parse tree in a manner similar to Lisp and Scheme, with opcodes surrounded by parenthesis and including parameters in a recursive fashion.  The exceptions are that the opcodes list and assoc (associative array, sometimes referred to as a hashmap or dict) may use `[]` and `{}` respectively and omit the opcode, though are still considered identical to `(list)` and `(assoc)`.

Entities always have a top level data element which is an assoc.  These assoc elements at the top of an entity are handled specially and their keys are called labels.  Labels are how entities can be accessed from outside the entity.  If a label starts with a caret, e.g. `^method_for_contained_entities`, then it can be accessed by contained entities, and they do not need to specify the caret.  Parent entities do need to specify the caret. For example, if `^foo` is a label of a container, a contained entity within could call the label `"foo"`.  This adds a layer of security and prevents contained entities from affecting parts of the container that are exposed for its own container's access.  Labels starting with an exclamation point, e.g. `!private_method`, are not accessible by the container and can only be accessed by the entity itself except for by the contained entity getting all of the code, acting as a private label. A label cannot be simultaneously private to its container and accessible to contained entities.  If an entity's root node is set to be something other than an assoc, it is set to the null key, indicating the main method.  If an entity is executed directly, the value at its null key is what is executed.

Variables are accessed in from the closest immediate scope, which means if there is a global variable named x and a function parameter named x, the function parameter will be used.  Entity labels are considered the global-most scope.  If a variable name cannot be found, then it will look at the entity's labels instead.  Scope is handled as a stack, and some opcodes may modify the scope.

In addition to the stack scope, there is a target scope, which can be accessed via the target opcodes to access the data being iterated over.  Some opcodes will add one or more layers to the target stack, so care must be taken to count back up the target stack an appropriate number of levels if the target is being used directly as opposed to being accessed via a variable.

If an operator is preceeded by an `@` symbol, then it will be evaluated on load rather than during execution, but only on a limited set of opcodes.  The `@` symbol is particularly useful for storing self-referential or graph data structures or for loading additional code as modules.  The opcodes `@` is permitted on consists of `target`, `get`, `append`, and `load`, and the opcodes' parameters must be resolvable without additional code execution.  The opcades `target` and `get` can reference locally or from the top of the stack.  For example, `@(target .true "inlined_referenced_method")` will point directly to the top level inlined_reference_method for an inline-like method.  The opcodes `load` and `append` are designed to make it easy to include modules, and the common idiom for including modules within an entity is
```amalgam
@(append
@(load "module_1.amlg")
@(load "module_2.amlg")
{
	...code...
}
```
where the two modules would be included as additional labels on the entity.

Neither comments nor annotations affect execution directly.  However, they can be read by code and thus influence execution, and can be 6used to store metadata.  An entity's root node's comment specifies the name and description of the Entity.  The first line of the comment is its name, the remainder of the lines of the comment are the description.  Annotations and comments are almost identical in how they are handled, but are accessed with different opcodes and annotations are intended for code-like metadata.  Annotations will always be printed before comments, enabling the classic Unix shebang #! operation for amalgam scripts as the first line.

In-order evaluation of parameters of most opcodes are not guaranteed to execute in order, or be executed at all if not necessary, unless otherwise specified in the opcode (e.g., seq, declare, let, etc. all execute in order).  It is generally not recommended practice to have side effects (variable or entity writes) in opcodes whose parameters are not guaranteed to be sequential.

If the concurrent/parallel symbol, \|\|, is specified then the opcode's computations will be executed concurrently if possible.  The concurrent execution will be interpreted with regard to the specific opcode, but any function calls may be executed in any order and possibly concurrently.

Each entity contains code and/or data via a root node that is executed every call to the entity.  An entity has complete abilities to perform reads and writes to any other Entity contained within it; it is also allowed to create, destroy, access, or modify other entities.  Entities have a set of permissions which include std_out_and_std_err, std_in, load (from filesystem), store (to filesystem), environment, alter_performance, system (run system commands), and entity (create and manage contained entities).

Entities may be explicitly named and may be used as code libraries.  For example, a library named MyLibrary with function MyFunction can be called as `(call_entity "MyLibrary" "MyFunction" { parameter_a 1 parameter_b 2 })`.  Entity names are called entity ids, and nested entities can be accessed via an entity walk path, which is a list of ids.  When using null as the entity name, it will refer to the current entity.  Entities that begin with an underscore are unnamed, and when entities are merged, unnamed entities are compared to other unnamed entities to potentially merge.

Null is represented as `.null`.

Numbers are represented via numeric characters, as well as `.`, `-`, and `e` for base-ten exponents, and are stored and processed as double precision floating point format.  Further, infinity and negative infinity are represented as `.infinity` and `-.infinity` respectively.  Not-a-number and non-string results are handled as `.null`.  The type of number is the string "number".

Strings begin and end with double quotes.  Certain characters can be encoded by preceding with a backslash, which are backslash, the null character (0), double quote, tab represented as t, newline represented as n, and carriage return represented as r.  The type of string is the string "string".  Comparisons and sorting on any strings are done in "natural order", meaning that 1x comes before 10, and m20x comes after m2a.  Bare strings, that is, strings that do not need to be surrounded by quotes, are only allowed with a couple operators, such as assoc, and standalone bare strings are interpreted as symbols.  

Boolean values can be represented as immediate values `.true` and `.false` respectively.  The type of a bool is the string "bool".

All regular expressions are EMCA-standard regular expressions.  See https://en.cppreference.com/w/cpp/regex/ecmascript or https://262.ecma-international.org/5.1/#sec-15.10 for further details on the regular expression syntax allowed.

The argument vector passed in on the command line is passed in as the variable argv, with any arguments consumed by the interpreter removed. This includes the standard 0th argument which is the Amalgam script being run. The interpreter path and name are passed in as the variable interpreter.

When attempting to load an asset, whether a .amlg file or another type, the interpreter will look for a file of the same name but with the extension .madm. The .mdam extension stands for metadata of Amalgam. This file consists of simple code of an associative array where the data within is immediate values representing the metadata.

File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string. Note that loading from a non-'.amlg' extension will only ever provide lists, assocs, numbers, and strings. 
For file I/O, the following parameters apply to load and store opcodes and API calls:
 - include_rand_seeds:              If true, attempts to include random seeds when storing and loading.
 - escape_resource_name:            If true, will escape any characters in the resource or file name that are not universally supported across platforms.
 - escape_contained_resource_names: If true, then for any contained entities and their resource or file paths that extend the base file path, it will escape any characters in the names that are not universally supported across platforms. This only applies to file formats where the entities are not flattened into one file.
 - transactional:                   If true, attempts to load and store files in a transactional manner, such that any interruption will not corrupt the file. Not applicable to all file types.
 - pretty_print:                    If true, then any code stored will be pretty printed.
 - sort_keys:                       If true, then any associative arrays will be sorted by keys.
 - flatten:                         If true, then will attempt to flatten all contained entities into one executable object and thus one file.
 - parallel_create:                 If true, will attempt use concurrency to store and load entities in parallel.
 - execute_on_load:                 If true, will execute the code upon load, which is required when entities are stored using flatten in order to create all of the entity structures.
 - load_external_files:             If true, upon parsing, will allow `@(load...)` statements to load external files.  It is true by default for parsing `.amlg` files, but false for all other file types.
 - require_version_compatibility:   If true, will fail on a load if the version of Amalgam is not compatible with the file version.
