# Amalgam&reg; Overview
Amalgam is a domain specific language (DSL) developed primarily for genetic programming and instance based machine learning, but also for simulation, agent based modeling, data storage and retrieval, the mathematics of probability theory and information theory, and game content and AI. The language format is somewhat LISP-like in that it uses parenthesized list format with prefix notation and is geared toward functional programming, where there is a one-to-one mapping between the code and the corresponding parse tree.

Whereas virtually all practical programming languages are primarily designed for some combination of programmer productivity and computational performance, Amalgam prioritizes code matching and merging, as well as a deep equivalence of code and data. Amalgam uses entities to store code and data, with a rich query system to find entities by their labels. The language uses a variable stack, but all attributes and methods are stored directly as labels in entities. There is no separate class versus instance, but entities can be used as prototypes to be copied and modified. Though code and data are represented as trees from the root of each entity, graphs in code and data structures are permitted and are flattened to code using special references. Further, instead of failing early when there is an error, Amalgam supports genetic programming and code mixing by being extremely weakly typed, and attempts to find a way to execute code no matter whether types match or not.

Amalgam takes inspiration from many programming languages, but those with the largest influence are LISP, Scheme, Haskell, Perl, Smalltalk, and Python. Despite being much like LISP, there is deliberately no macro system. This is to make sure that code is semantically similar whenever the code is similar, regardless of context. It makes it easy to find the difference between x and y as an executable patch, and then apply that patch to z as `(call (difference x y) {_ z})`, or semantically mix blocks of code a and b as `(mix a b)`. Amalgam is not a purely functional language. It has imperative and object oriented capabilities, but is primarily optimized for functional programming with relatively few opcodes that are functionally flexible based on parameters to maximize flexibility with code mixing and matching.

Genetic programming can create arbitrary code, so there is always a chance that an evolved program ends up consuming more CPU or memory resources than desired, or may attempt to affect the system outside of the interpreter. For these reasons, there are many strict sandboxing aspects of the language with optional constraints on access, CPU, and memory. Amalgam also has a rich permissions system, which controls what entities and code are able to do, whether writing to the console or executing system commands.

The Amalgam interpreter was designed to be used a standalone interpreter and to build functionality for other programming languages and environments. It does not currently have rich support for linking C libraries into the language, but that is planned for future functionality.

Initial development on Amalgam began in 2011. It was first offered as a commercial product in 2014 at Hazardous Software Inc. and was open sourced in September 2023 by Howso Incorporated (formerly known as Diveplane Corporation, a company spun out of Hazardous Software Inc.).

When referencing the language: "Amalgam", "amalgam", "amalgam-lang", and "amalgam language" are used interchangeably with Amalgam being preferred. When referencing the interpreter: "Amalgam interpreter", "interpreter", "Amalgam app", and "Amalgam lib" are used interchangeably.
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

If an operator is preceeded by an @ symbol, then it will be evaluated with regard to data format on load, useful for storing self-referential or graph data structures.  In particular, the target and get opcodes are primarily used with the @ symbol, and can reference locally or from the top of the stack.  For example, `@(target .true "inlined_referenced_method")` will point directly to the top level inlined_reference_method for an inline-like method.

Comments nor annotations do not affect execution directly, but can be read by code and thus influence execution.  They can also be used to store metadata.  An entity's root node's comment specifies the name and description of the Entity.  The first line of the comment is its name, the remainder of the lines of the comment are the description.  Annotations and comments are almost identical in how they are handled, but are accessed with different opcodes and annotations are intended for code-like metadata.  Annotations will always be printed before comments, enabling the classic Unix shebang #! operation for amalgam scripts as the first line.

In-order evaluation of parameters of most opcodes are not guaranteed to execute in order, or be executed at all if not necessary, unless otherwise specified in the opcode (e.g., seq, declare, let, etc. all execute in order).  It is generally not recommended practice to have side effects (variable or entity writes) in opcodes whose parameters are not guaranteed to be sequential.

If the concurrent/parallel symbol, ||, is specified then the opcode's computations will be executed concurrently if possible.  The concurrent execution will be interpreted with regard to the specific opcode, but any function calls may be executed in any order and possibly concurrently.

Each entity contains code and/or data via a root node that is executed every call to the entity.  An entity has complete abilities to perform reads and writes to any other Entity contained within it; it is also allowed to create, destroy, access, or modify other entities.  Entities have a set of permissions which include std_out_and_std_err, std_in, load (from filesystem), store (to filesystem), environment, alter_performance, system (run system commands), and entity (create and manage contained entities).

Entities may be explicitly named and may be used as code libraries.  For example, a library named MyLibrary with function MyFunction can be called as `(call_entity "MyLibrary" "MyFunction" { parameter_a 1 parameter_b 2 })`.  Entity names are called entity ids, and nested entities can be accessed via an entity walk path, which is a list of ids.  When using null as the entity name, it will refer to the current entity.  Entities that begin with an underscore are unnamed, and when entities are merged, unnamed entities are compared to other unnamed entities to potentially merge.

Numbers are represented via numeric characters, as well as `.`, `-`, and `e` for base-ten exponents, and are stored and processed as double precision floating point format.  Further, infinity and negative infinity are represented as `.infinity` and `-.infinity` respectively.  Not-a-number and non-string results are represented via the opcode `(null)`.  The type of number is the string "number".

Strings begin and end with double quotes.  Certain characters can be encoded by preceding with a backslash, which are backslash, null represented as 0, double quote, tab represented as t, newline represented as n, and carriage return represented as r.  The type of string is the string "string".  Comparisons and sorting on any strings are done in "natural order", meaning that 1x comes before 10, and m20x comes after m2a.  Bare strings, that is, strings that do not need to be surrounded by quotes, are only allowed with a couple operators, such as assoc, and standalone bare strings are interpreted as symbols.  

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
 - require_version_compatibility:   If true, will fail on a load if the version of Amalgam is not compatible with the file version.
## Amalgam Opcodes
### System and Runtime
  - [help](./system_and_runtime.md#opcode-help)
  - [print](./system_and_runtime.md#opcode-print)
  - [system_time](./system_and_runtime.md#opcode-system_time)
  - [system](./system_and_runtime.md#opcode-system)
  - [reclaim_resources](./system_and_runtime.md#opcode-reclaim_resources)
### Primitive Types
  - [null](./primitive_types.md#opcode-null)
  - [bool](./primitive_types.md#opcode-bool)
  - [number](./primitive_types.md#opcode-number)
  - [string](./primitive_types.md#opcode-string)
  - [list](./primitive_types.md#opcode-list)
  - [unordered_list](./primitive_types.md#opcode-unordered_list)
  - [assoc](./primitive_types.md#opcode-assoc)
### Variable Definition and Modification
  - [symbol](./variable_definition_and_modification.md#opcode-symbol)
  - [let](./variable_definition_and_modification.md#opcode-let)
  - [declare](./variable_definition_and_modification.md#opcode-declare)
  - [assign](./variable_definition_and_modification.md#opcode-assign)
  - [accum](./variable_definition_and_modification.md#opcode-accum)
  - [retrieve](./variable_definition_and_modification.md#opcode-retrieve)
  - [target](./variable_definition_and_modification.md#opcode-target)
  - [stack](./variable_definition_and_modification.md#opcode-stack)
  - [args](./variable_definition_and_modification.md#opcode-args)
  - [get_type](./variable_definition_and_modification.md#opcode-get_type)
  - [get_type_string](./variable_definition_and_modification.md#opcode-get_type_string)
  - [set_type](./variable_definition_and_modification.md#opcode-set_type)
  - [format](./variable_definition_and_modification.md#opcode-format)
### Control Flow
  - [if](./control_flow.md#opcode-if)
  - [seq](./control_flow.md#opcode-seq)
  - [lambda](./control_flow.md#opcode-lambda)
  - [call](./control_flow.md#opcode-call)
  - [call_sandboxed](./control_flow.md#opcode-call_sandboxed)
  - [while](./control_flow.md#opcode-while)
  - [conclude](./control_flow.md#opcode-conclude)
  - [return](./control_flow.md#opcode-return)
  - [apply](./control_flow.md#opcode-apply)
  - [opcode_stack](./control_flow.md#opcode-opcode_stack)
### Logic and Comparison
  - [and](./logic_and_comparison.md#opcode-and)
  - [or](./logic_and_comparison.md#opcode-or)
  - [xor](./logic_and_comparison.md#opcode-xor)
  - [not](./logic_and_comparison.md#opcode-not)
  - [=](./logic_and_comparison.md#opcode-=)
  - [!=](./logic_and_comparison.md#opcode-!=)
  - [<](./logic_and_comparison.md#opcode-<)
  - [<=](./logic_and_comparison.md#opcode-<=)
  - [>](./logic_and_comparison.md#opcode->)
  - [>=](./logic_and_comparison.md#opcode->=)
  - [~](./logic_and_comparison.md#opcode-~)
  - [!~](./logic_and_comparison.md#opcode-!~)
### Basic Math
  - [+](./basic_math.md#opcode-+)
  - [-](./basic_math.md#opcode--)
  - [*](./basic_math.md#opcode-*)
  - [/](./basic_math.md#opcode-/)
  - [mod](./basic_math.md#opcode-mod)
  - [get_digits](./basic_math.md#opcode-get_digits)
  - [set_digits](./basic_math.md#opcode-set_digits)
  - [floor](./basic_math.md#opcode-floor)
  - [ceil](./basic_math.md#opcode-ceil)
  - [round](./basic_math.md#opcode-round)
  - [abs](./basic_math.md#opcode-abs)
  - [max](./basic_math.md#opcode-max)
  - [min](./basic_math.md#opcode-min)
  - [index_max](./basic_math.md#opcode-index_max)
  - [index_min](./basic_math.md#opcode-index_min)
### Advanced Math
  - [exp](./advanced_math.md#opcode-exp)
  - [log](./advanced_math.md#opcode-log)
  - [erf](./advanced_math.md#opcode-erf)
  - [tgamma](./advanced_math.md#opcode-tgamma)
  - [lgamma](./advanced_math.md#opcode-lgamma)
  - [sqrt](./advanced_math.md#opcode-sqrt)
  - [pow](./advanced_math.md#opcode-pow)
  - [dot_product](./advanced_math.md#opcode-dot_product)
  - [normalize](./advanced_math.md#opcode-normalize)
  - [mode](./advanced_math.md#opcode-mode)
  - [quantile](./advanced_math.md#opcode-quantile)
  - [generalized_mean](./advanced_math.md#opcode-generalized_mean)
  - [generalized_distance](./advanced_math.md#opcode-generalized_distance)
  - [entropy](./advanced_math.md#opcode-entropy)
### Trigonometry
  - [sin](./trigonometry.md#opcode-sin)
  - [asin](./trigonometry.md#opcode-asin)
  - [cos](./trigonometry.md#opcode-cos)
  - [acos](./trigonometry.md#opcode-acos)
  - [tan](./trigonometry.md#opcode-tan)
  - [atan](./trigonometry.md#opcode-atan)
  - [sinh](./trigonometry.md#opcode-sinh)
  - [asinh](./trigonometry.md#opcode-asinh)
  - [cosh](./trigonometry.md#opcode-cosh)
  - [acosh](./trigonometry.md#opcode-acosh)
  - [tanh](./trigonometry.md#opcode-tanh)
  - [atanh](./trigonometry.md#opcode-atanh)
### String Operations
  - [explode](./string_operations.md#opcode-explode)
  - [split](./string_operations.md#opcode-split)
  - [substr](./string_operations.md#opcode-substr)
  - [concat](./string_operations.md#opcode-concat)
  - [parse](./string_operations.md#opcode-parse)
  - [unparse](./string_operations.md#opcode-unparse)
### Container Manipulation
  - [first](./container_manipulation.md#opcode-first)
  - [tail](./container_manipulation.md#opcode-tail)
  - [last](./container_manipulation.md#opcode-last)
  - [trunc](./container_manipulation.md#opcode-trunc)
  - [append](./container_manipulation.md#opcode-append)
  - [size](./container_manipulation.md#opcode-size)
  - [get](./container_manipulation.md#opcode-get)
  - [set](./container_manipulation.md#opcode-set)
  - [replace](./container_manipulation.md#opcode-replace)
  - [indices](./container_manipulation.md#opcode-indices)
  - [values](./container_manipulation.md#opcode-values)
  - [contains_index](./container_manipulation.md#opcode-contains_index)
  - [contains_value](./container_manipulation.md#opcode-contains_value)
  - [remove](./container_manipulation.md#opcode-remove)
  - [keep](./container_manipulation.md#opcode-keep)
### Iteration and Container Transform
  - [range](./iteration_and_container_transform.md#opcode-range)
  - [rewrite](./iteration_and_container_transform.md#opcode-rewrite)
  - [map](./iteration_and_container_transform.md#opcode-map)
  - [filter](./iteration_and_container_transform.md#opcode-filter)
  - [weave](./iteration_and_container_transform.md#opcode-weave)
  - [reduce](./iteration_and_container_transform.md#opcode-reduce)
  - [associate](./iteration_and_container_transform.md#opcode-associate)
  - [zip](./iteration_and_container_transform.md#opcode-zip)
  - [unzip](./iteration_and_container_transform.md#opcode-unzip)
  - [reverse](./iteration_and_container_transform.md#opcode-reverse)
  - [sort](./iteration_and_container_transform.md#opcode-sort)
  - [current_index](./iteration_and_container_transform.md#opcode-current_index)
  - [current_value](./iteration_and_container_transform.md#opcode-current_value)
  - [previous_result](./iteration_and_container_transform.md#opcode-previous_result)
### Entity Lifecycle and Storage
  - [create_entities](./entity_lifecycle_and_storage.md#opcode-create_entities)
  - [clone_entities](./entity_lifecycle_and_storage.md#opcode-clone_entities)
  - [move_entities](./entity_lifecycle_and_storage.md#opcode-move_entities)
  - [destroy_entities](./entity_lifecycle_and_storage.md#opcode-destroy_entities)
  - [load](./entity_lifecycle_and_storage.md#opcode-load)
  - [load_entity](./entity_lifecycle_and_storage.md#opcode-load_entity)
  - [store](./entity_lifecycle_and_storage.md#opcode-store)
  - [store_entity](./entity_lifecycle_and_storage.md#opcode-store_entity)
  - [contains_entity](./entity_lifecycle_and_storage.md#opcode-contains_entity)
  - [flatten_entity](./entity_lifecycle_and_storage.md#opcode-flatten_entity)
  - [retrieve_entity_root](./entity_lifecycle_and_storage.md#opcode-retrieve_entity_root)
  - [assign_entity_roots](./entity_lifecycle_and_storage.md#opcode-assign_entity_roots)
  - [get_entity_permissions](./entity_lifecycle_and_storage.md#opcode-get_entity_permissions)
  - [set_entity_permissions](./entity_lifecycle_and_storage.md#opcode-set_entity_permissions)
### Entity Access and Manipulation
  - [contains_label](./entity_access_and_manipulation.md#opcode-contains_label)
  - [assign_to_entities](./entity_access_and_manipulation.md#opcode-assign_to_entities)
  - [accum_to_entities](./entity_access_and_manipulation.md#opcode-accum_to_entities)
  - [remove_from_entities](./entity_access_and_manipulation.md#opcode-remove_from_entities)
  - [retrieve_from_entity](./entity_access_and_manipulation.md#opcode-retrieve_from_entity)
  - [call_entity](./entity_access_and_manipulation.md#opcode-call_entity)
  - [call_entity_get_changes](./entity_access_and_manipulation.md#opcode-call_entity_get_changes)
  - [call_on_entity](./entity_access_and_manipulation.md#opcode-call_on_entity)
  - [call_container](./entity_access_and_manipulation.md#opcode-call_container)
### Entity Query Engine
  - [contained_entities](./entity_query_engine.md#opcode-contained_entities)
  - [compute_on_contained_entities](./entity_query_engine.md#opcode-compute_on_contained_entities)
  - [query_select](./entity_query_engine.md#opcode-query_select)
  - [query_sample](./entity_query_engine.md#opcode-query_sample)
  - [query_in_entity_list](./entity_query_engine.md#opcode-query_in_entity_list)
  - [query_not_in_entity_list](./entity_query_engine.md#opcode-query_not_in_entity_list)
  - [query_exists](./entity_query_engine.md#opcode-query_exists)
  - [query_not_exists](./entity_query_engine.md#opcode-query_not_exists)
  - [query_equals](./entity_query_engine.md#opcode-query_equals)
  - [query_not_equals](./entity_query_engine.md#opcode-query_not_equals)
  - [query_between](./entity_query_engine.md#opcode-query_between)
  - [query_not_between](./entity_query_engine.md#opcode-query_not_between)
  - [query_among](./entity_query_engine.md#opcode-query_among)
  - [query_not_among](./entity_query_engine.md#opcode-query_not_among)
  - [query_max](./entity_query_engine.md#opcode-query_max)
  - [query_min](./entity_query_engine.md#opcode-query_min)
  - [query_sum](./entity_query_engine.md#opcode-query_sum)
  - [query_mode](./entity_query_engine.md#opcode-query_mode)
  - [query_quantile](./entity_query_engine.md#opcode-query_quantile)
  - [query_generalized_mean](./entity_query_engine.md#opcode-query_generalized_mean)
  - [query_min_difference](./entity_query_engine.md#opcode-query_min_difference)
  - [query_max_difference](./entity_query_engine.md#opcode-query_max_difference)
  - [query_value_masses](./entity_query_engine.md#opcode-query_value_masses)
  - [query_greater_or_equal_to](./entity_query_engine.md#opcode-query_greater_or_equal_to)
  - [query_less_or_equal_to](./entity_query_engine.md#opcode-query_less_or_equal_to)
  - [query_within_generalized_distance](./entity_query_engine.md#opcode-query_within_generalized_distance)
  - [query_nearest_generalized_distance](./entity_query_engine.md#opcode-query_nearest_generalized_distance)
  - [query_distance_contributions](./entity_query_engine.md#opcode-query_distance_contributions)
  - [query_entity_convictions](./entity_query_engine.md#opcode-query_entity_convictions)
  - [query_entity_group_kl_divergence](./entity_query_engine.md#opcode-query_entity_group_kl_divergence)
  - [query_entity_distance_contributions](./entity_query_engine.md#opcode-query_entity_distance_contributions)
  - [query_entity_kl_divergences](./entity_query_engine.md#opcode-query_entity_kl_divergences)
  - [query_entity_cumulative_nearest_entity_weights](./entity_query_engine.md#opcode-query_entity_cumulative_nearest_entity_weights)
### Metadata
  - [get_annotations](./metadata.md#opcode-get_annotations)
  - [set_annotations](./metadata.md#opcode-set_annotations)
  - [get_comments](./metadata.md#opcode-get_comments)
  - [set_comments](./metadata.md#opcode-set_comments)
  - [get_concurrency](./metadata.md#opcode-get_concurrency)
  - [set_concurrency](./metadata.md#opcode-set_concurrency)
  - [get_value](./metadata.md#opcode-get_value)
  - [set_value](./metadata.md#opcode-set_value)
  - [get_entity_annotations](./metadata.md#opcode-get_entity_annotations)
  - [get_entity_comments](./metadata.md#opcode-get_entity_comments)
### Code Comparison and Evolution
  - [total_size](./code_comparison_and_evolution.md#opcode-total_size)
  - [mutate](./code_comparison_and_evolution.md#opcode-mutate)
  - [get_mutation_defaults](./code_comparison_and_evolution.md#opcode-get_mutation_defaults)
  - [commonality](./code_comparison_and_evolution.md#opcode-commonality)
  - [edit_distance](./code_comparison_and_evolution.md#opcode-edit_distance)
  - [intersect](./code_comparison_and_evolution.md#opcode-intersect)
  - [union](./code_comparison_and_evolution.md#opcode-union)
  - [difference](./code_comparison_and_evolution.md#opcode-difference)
  - [mix](./code_comparison_and_evolution.md#opcode-mix)
### Entity Comparison and Evolution
  - [total_entity_size](./entity_comparison_and_evolution.md#opcode-total_entity_size)
  - [mutate_entity](./entity_comparison_and_evolution.md#opcode-mutate_entity)
  - [commonality_entities](./entity_comparison_and_evolution.md#opcode-commonality_entities)
  - [edit_distance_entities](./entity_comparison_and_evolution.md#opcode-edit_distance_entities)
  - [intersect_entities](./entity_comparison_and_evolution.md#opcode-intersect_entities)
  - [union_entities](./entity_comparison_and_evolution.md#opcode-union_entities)
  - [difference_entities](./entity_comparison_and_evolution.md#opcode-difference_entities)
  - [mix_entities](./entity_comparison_and_evolution.md#opcode-mix_entities)
### Random
  - [rand](./random.md#opcode-rand)
  - [get_rand_seed](./random.md#opcode-get_rand_seed)
  - [set_rand_seed](./random.md#opcode-set_rand_seed)
  - [get_entity_rand_seed](./random.md#opcode-get_entity_rand_seed)
  - [set_entity_rand_seed](./random.md#opcode-set_entity_rand_seed)
### Cryptography
  - [crypto_sign](./cryptography.md#opcode-crypto_sign)
  - [crypto_sign_verify](./cryptography.md#opcode-crypto_sign_verify)
  - [encrypt](./cryptography.md#opcode-encrypt)
  - [decrypt](./cryptography.md#opcode-decrypt)
# Distance and Surprisal Calculations
Amalgam has a number of opcodes that compute distances, and surprisals as distance, across various data types.  The opcode `generalized_distance` calculates these values based on two containers, whereas opcodes like `query_within_generalized_distance` and `query_nearest_generalized_distance` compute the distances on entity labels, and opcodes like `query_entity_convictions` use distance or surprisal calculations to compute more advanced metrics.  For full information on how these distances are calculated, see the paper "A Theory of the Mechanics of Information: Generalization Through Measurement of Uncertainty (Learning is Measuring)" by Hazard et. al <https://arxiv.org/abs/2510.22809v1>.

These opcodes all contain a set of common parameters that start with the containers or labels from which to compute the distance.  Following these parameters, the distance opcodes have the optional parameters in the order as follows, though not all opcodes have all of these parameters.
 - list|number `selection_bandwidth`:        The parameter `selection_bandwidth` specifies either the number of entities to return, or is a list of parameters for more sophisticated bandwidth selection.  If `selection_bandwidth` is a list, the first element of the list specifies the minimum incremental probability or percent of mass that the next largest entity would comprise (e.g., 0.05 would return at most 20 entities if they were all equal in percent of mass), and the other elements of the list are optional.  The second element is the minimum number of entities to return, the third element is the maximum number of entities to return, and the fourth indicates the number of additional entities to include after any of the aforementioned thresholds (defaulting to zero).  If there is disagreement among the constraints for `selection_bandwidth`, the constraint yielding the fewest entities will govern the number of entities returned.
 - list `feature_labels`:                    The names of the labels of the features from which to compute the distances.
 - number `p_value`:                     	 The parameter `p_value` is the generalized norm parameter, where the value of 1 is probability space and Manhattan distance, the default, 2 being Euclidean distance, etc.  For surprisal space, using a value of 1 is generally most appropriate.
 - list|assoc|assoc of assoc `weights`:  	 If `weights` is a list, each value maps to its respective element in the vectors.  If `weights` is null, then it will assume that the `weights` are 1 and additionally will ignore null values for the vectors instead of treating them as unknown differences.  If `weights` is an assoc, then the parameter `value_names` will select the `weights` from the assoc.  If `weights` is an assoc of assocs, additionally the parameter `weights_selection_features` will select which set of `weights` to use.
 - list|assoc `distance_types`:              The parameter `distance_types` is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are "nominal_bool", "nominal_number", "nominal_string", "nominal_code", "continuous_number", "continuous_number_cyclic", "continuous_string", and "continuous_code".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around. 
 - list|assoc `attributes`:                  For `attributes`, the corresponding element of `distance_types` specifies what particular `attributes` are expected.  For a nominal distance type, a number indicates the nominal count, whereas null will infer from the values given.  Cyclic requires a single value, which is the upper bound of the difference for the cycle range (e.g., if the value is 360, then the supremum difference between two values will be 360, leading 1 and 359 to have a difference of 2).  If the feature type is "continuous_code", then the parameter will be an assoc that may contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of node1 to node2.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.
 - list|assoc `deviations`:              	 The values in the parameter `deviations` are used during distance calculation to specify uncertainty per-element, the minimum difference between two values prior to exponentiation.  Specifying null as a deviation is equivalent to setting each deviation to 0.  Each deviation for each feature can be a single value or a list.  If it is a single value, that value is used as the deviation and differences and deviations for null values will automatically computed from the data based on the maximum difference.  If a deviation is provided as a list, then the first value is the deviation, the second value is the difference to use when one of the values being compared is null, and the third value is the difference to use when both of the values are null.  If the third value is omitted, it will use the second value for both.  If both of the null values are omitted, then it will compute the maximum difference and use that for both.  For nominal types, the value for each feature can be a numeric deviation, an assoc, or a list.  If the value is an assoc it specifies deviation information, where each key of the assoc is the nominal value, and each value of the assoc can be a numeric deviation value, a list, or an assoc, with the list specifying either an assoc followed optionally by the default deviation.  This inner assoc, regardless of whether it is in a list, maps the value to each actual value's deviation.
 - list|string `weights_selection_features`: If `weights_selection_features` is a string and `weights` is an assoc, then it will select the `weights` for the given feature and rebalance `weights` for any unused features.
 - string|number `distance_transform`:       A transform will be applied to the distances based on `distance_transform`.  If `distance_transform` is "surprisal" then distances will be calculated as surprisals, and weights will not be applied to the values.  If `distance_transform` is "surprisal_to_prob" then distances will be calculated as surprisals and will be transformed back into probabilities for aggregating, and then transformed back to surprisals.  If `distance_transform` is a number or omitted, which will default to 1.0, then it will be treated as a distance weight exponent, and will be applied to each distance as distance^distance_weight_exponent, only using entity weights for nonpositive values of `distance_transform`.  Note that the corresponding parameter for `generalized_distance` is bool `surprisal_space`, and is true then all distance computations will be performed in surprisal space.
 - number `random_seed`:                     If `random_seed` is specified, it uses a stream from this seed to break ties when selecting entities.
 - string `radius_label`:           		 The parameter `radius_label` parameter represents the label name of the radius of the entity, which effectively operates as a negative distance so that one point can be inside the hypersphere of another.
 - string `numerical_precision`:           	 The parameter `numerical_precision` can be specified as one of three values: "precise", which computes every distance with high numerical precision, "fast", which computes every distance with lower but faster numerical precision, and "recompute_precise", which computes distances quickly with lower precision but then recomputes any distance values that will be returned with higher precision.
