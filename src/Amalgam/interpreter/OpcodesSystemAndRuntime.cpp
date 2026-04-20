//project headers:
#include "AssetManager.h"
#include "Cryptography.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"

//system headers:
#include <regex>

static std::string _opcode_group = "System and Runtime";

static OpcodeInitializer _ENT_HELP(ENT_HELP, &Interpreter::InterpretNode_ENT_HELP, []() {
	OpcodeDetails d;
	d.parameters = R"([string topic])";
	d.returns = R"(any)";
	d.description = R"(If no parameter is specified it returns a string of the topics that can be used.  For given a `topic`, returns a string or relevant data that describes the given topic.)";
	d.examples = MakeAmalgamExamples({
		{R"((help "+"))", R"&({
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
})&"}
		});
	d.permissions = ExecutionPermissions::Permission::ALL;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

static std::string_view _help_options(R"(Pass either an opcode keyword or one of the following strings as the first parameter for help on the topic.
overview
syntax
distance
opcodes
For example, try: (print (help "overview"))
Or type (conclude) and press enter to exit.
)");

static std::string_view _help_overview(R"(# Amalgam&reg; Overview
Amalgam is a domain specific language (DSL) developed primarily for genetic programming and instance based machine learning, but also for simulation, agent based modeling, data storage and retrieval, the mathematics of probability theory and information theory, and game content and AI. The language format is somewhat LISP-like in that it uses parenthesized list format with prefix notation and is geared toward functional programming, where there is a one-to-one mapping between the code and the corresponding parse tree.

Whereas virtually all practical programming languages are primarily designed for some combination of programmer productivity and computational performance, Amalgam prioritizes code matching and merging, as well as a deep equivalence of code and data. Amalgam uses entities to store code and data, with a rich query system to find entities by their labels. The language uses a variable stack, but all attributes and methods are stored directly as labels in entities. There is no separate class versus instance, but entities can be used as prototypes to be copied and modified. Though code and data are represented as trees from the root of each entity, graphs in code and data structures are permitted and are flattened to code using special references. Further, instead of failing early when there is an error, Amalgam supports genetic programming and code mixing by being extremely weakly typed, and attempts to find a way to execute code no matter whether types match or not.

Amalgam takes inspiration from many programming languages, but those with the largest influence are LISP, Scheme, Haskell, Perl, Smalltalk, and Python. Despite being much like LISP, there is deliberately no macro system. This is to make sure that code is semantically similar whenever the code is similar, regardless of context. It makes it easy to find the difference between x and y as an executable patch, and then apply that patch to z as `(call (difference x y) {_ z})`, or semantically mix blocks of code a and b as `(mix a b)`. Amalgam is not a purely functional language. It has imperative and object oriented capabilities, but is primarily optimized for functional programming with relatively few opcodes that are functionally flexible based on parameters to maximize flexibility with code mixing and matching.

Genetic programming can create arbitrary code, so there is always a chance that an evolved program ends up consuming more CPU or memory resources than desired, or may attempt to affect the system outside of the interpreter. For these reasons, there are many strict sandboxing aspects of the language with optional constraints on access, CPU, and memory. Amalgam also has a rich permissions system, which controls what entities and code are able to do, whether writing to the console or executing system commands.

The Amalgam interpreter was designed to be used a standalone interpreter and to build functionality for other programming languages and environments. It does not currently have rich support for linking C libraries into the language, but that is planned for future functionality.

Initial development on Amalgam began in 2011. It was first offered as a commercial product in 2014 at Hazardous Software Inc. and was open sourced in September 2023 by Howso Incorporated (formerly known as Diveplane Corporation, a company spun out of Hazardous Software Inc.).

When referencing the language: "Amalgam", "amalgam", "amalgam-lang", and "amalgam language" are used interchangeably with Amalgam being preferred. When referencing the interpreter: "Amalgam interpreter", "interpreter", "Amalgam app", and "Amalgam lib" are used interchangeably.)");

static std::string_view _help_syntax(R"(# Amalgam Syntax
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
 - require_version_compatibility:   If true, will fail on a load if the version of Amalgam is not compatible with the file version.)");

static std::string_view _help_distance(R"&(# Distance and Surprisal Calculations
Amalgam has a number of opcodes that compute distances, and surprisals as distance, across various data types.  The opcode `generalized_distance` calculates these values based on two containers, whereas opcodes like `query_within_generalized_distance` and `query_nearest_generalized_distance` compute the distances on entity labels, and opcodes like `query_entity_convictions` use distance or surprisal calculations to compute more advanced metrics.  For full information on how these distances are calculated, see the paper "A Theory of the Mechanics of Information: Generalization Through Measurement of Uncertainty (Learning is Measuring)" by Hazard et. al <https://arxiv.org/abs/2510.22809v1>.

These opcodes all contain a set of common parameters that start with the containers or labels from which to compute the distance.  Following these parameters, the distance opcodes have the optional parameters in the order as follows, though not all opcodes have all of these parameters.
 - list\|number `selection_bandwidth`:        The parameter `selection_bandwidth` specifies either the number of entities to return, or is a list of parameters for more sophisticated bandwidth selection.  If `selection_bandwidth` is a list, the first element of the list specifies the minimum incremental probability or percent of mass that the next largest entity would comprise (e.g., 0.05 would return at most 20 entities if they were all equal in percent of mass), and the other elements of the list are optional.  The second element is the minimum number of entities to return, the third element is the maximum number of entities to return, and the fourth indicates the number of additional entities to include after any of the aforementioned thresholds (defaulting to zero).  If there is disagreement among the constraints for `selection_bandwidth`, the constraint yielding the fewest entities will govern the number of entities returned.
 - list `feature_labels`:                    The names of the labels of the features from which to compute the distances.
 - number `p_value`:                     	 The parameter `p_value` is the generalized norm parameter, where the value of 1 is probability space and Manhattan distance, the default, 2 being Euclidean distance, etc.  For surprisal space, using a value of 1 is generally most appropriate.
 - list\|assoc\|assoc of assoc `weights`:  	 If `weights` is a list, each value maps to its respective element in the vectors.  If `weights` is null, then it will assume that the `weights` are 1 and additionally will ignore null values for the vectors instead of treating them as unknown differences.  If `weights` is an assoc, then the parameter `value_names` will select the `weights` from the assoc.  If `weights` is an assoc of assocs, additionally the parameter `weights_selection_features` will select which set of `weights` to use.
 - list\|assoc `distance_types`:              The parameter `distance_types` is either a list strings or an assoc of strings indicating the type of distance for each feature.  Allowed values are "nominal_bool", "nominal_number", "nominal_string", "nominal_code", "continuous_number", "continuous_number_cyclic", "continuous_string", and "continuous_code".  Nominals evaluate whether the two values are the same and continuous evaluates the difference between the two values.  The numeric, string, or code modifier specifies how the difference is measured, and cyclic means it is a difference that wraps around. 
 - list\|assoc `attributes`:                  For `attributes`, the corresponding element of `distance_types` specifies what particular `attributes` are expected.  For a nominal distance type, a number indicates the nominal count, whereas null will infer from the values given.  Cyclic requires a single value, which is the upper bound of the difference for the cycle range (e.g., if the value is 360, then the supremum difference between two values will be 360, leading 1 and 359 to have a difference of 2).  If the feature type is "continuous_code", then the parameter will be an assoc that may contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of node1 to node2.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.
 - list\|assoc `deviations`:              	 The values in the parameter `deviations` are used during distance calculation to specify uncertainty per-element, the minimum difference between two values prior to exponentiation.  Specifying null as a deviation is equivalent to setting each deviation to 0.  Each deviation for each feature can be a single value or a list.  If it is a single value, that value is used as the deviation and differences and deviations for null values will automatically computed from the data based on the maximum difference.  If a deviation is provided as a list, then the first value is the deviation, the second value is the difference to use when one of the values being compared is null, and the third value is the difference to use when both of the values are null.  If the third value is omitted, it will use the second value for both.  If both of the null values are omitted, then it will compute the maximum difference and use that for both.  For nominal types, the value for each feature can be a numeric deviation, an assoc, or a list.  If the value is an assoc it specifies deviation information, where each key of the assoc is the nominal value, and each value of the assoc can be a numeric deviation value, a list, or an assoc, with the list specifying either an assoc followed optionally by the default deviation.  This inner assoc, regardless of whether it is in a list, maps the value to each actual value's deviation.
 - list\|string `weights_selection_features`: If `weights_selection_features` is a string and `weights` is an assoc, then it will select the `weights` for the given feature and rebalance `weights` for any unused features.
 - string\|number `distance_transform`:       A transform will be applied to the distances based on `distance_transform`.  If `distance_transform` is "surprisal" then distances will be calculated as surprisals, and weights will not be applied to the values.  If `distance_transform` is "surprisal_to_prob" then distances will be calculated as surprisals and will be transformed back into probabilities for aggregating, and then transformed back to surprisals.  If `distance_transform` is a number or omitted, which will default to 1.0, then it will be treated as a distance weight exponent, and will be applied to each distance as distance^distance_weight_exponent, only using entity weights for nonpositive values of `distance_transform`.  Note that the corresponding parameter for `generalized_distance` is bool `surprisal_space`, and is true then all distance computations will be performed in surprisal space.
 - number `random_seed`:                     If `random_seed` is specified, it uses a stream from this seed to break ties when selecting entities.
 - string `radius_label`:           		 The parameter `radius_label` parameter represents the label name of the radius of the entity, which effectively operates as a negative distance so that one point can be inside the hypersphere of another.
 - string `numerical_precision`:           	 The parameter `numerical_precision` can be specified as one of three values: "precise", which computes every distance with high numerical precision, "fast", which computes every distance with lower but faster numerical precision, and "recompute_precise", which computes distances quickly with lower precision but then recomputes any distance values that will be returned with higher precision.)&");

EvaluableNodeReference Interpreter::InterpretNode_ENT_HELP(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0 || EvaluableNode::IsNull(ocn[0]))
		return AllocReturn(_help_options, immediate_result);

	StringInternPool::StringID help_command_sid = InterpretNodeIntoStringIDValueIfExists(ocn[0]);
	if(help_command_sid == GetStringIdFromBuiltInStringId(ENBISI_overview))
	{
		return AllocReturn(_help_overview, immediate_result);
	}
	else if(help_command_sid == GetStringIdFromBuiltInStringId(ENBISI_syntax))
	{
		return AllocReturn(_help_syntax, immediate_result);
	}
	else if(help_command_sid == GetStringIdFromBuiltInStringId(ENBISI_distance))
	{
		return AllocReturn(_help_distance, immediate_result);
	}
	else if(help_command_sid == GetStringIdFromBuiltInStringId(ENBISI_opcodes))
	{
		EvaluableNodeReference opcode_list(evaluableNodeManager->AllocNode(ENT_LIST), true);
		auto &opcode_ocn = opcode_list->GetOrderedChildNodesReference();
		opcode_ocn.resize(NUM_VALID_ENT_OPCODES);

		for(size_t opcode_index = 0; opcode_index < NUM_VALID_ENT_OPCODES; opcode_index++)
			opcode_ocn[opcode_index] = evaluableNodeManager->AllocNode(ENT_STRING, GetStringIdFromNodeType(
				static_cast<EvaluableNodeType>(opcode_index)));

		return opcode_list;
	}
	else if(auto opcode_type = GetEvaluableNodeTypeFromStringId(help_command_sid);
		opcode_type != ENT_NOT_A_BUILT_IN_TYPE)
	{
		auto &od = _opcode_details[opcode_type];

		EvaluableNodeReference opcode_attribs(evaluableNodeManager->AllocNode(ENT_ASSOC), true);

		opcode_attribs->SetMappedChildNode("description", evaluableNodeManager->AllocNode(od.description));
		opcode_attribs->SetMappedChildNode("parameters", evaluableNodeManager->AllocNode(od.parameters));
		opcode_attribs->SetMappedChildNode("returns", evaluableNodeManager->AllocNode(od.returns));
		opcode_attribs->SetMappedChildNode("allows_concurrency", evaluableNodeManager->AllocNode(od.allowsConcurrency));
		opcode_attribs->SetMappedChildNode("requires_entity", evaluableNodeManager->AllocNode(od.requiresEntity));
		opcode_attribs->SetMappedChildNode("new_scope", evaluableNodeManager->AllocNode(od.newScope));
		opcode_attribs->SetMappedChildNode("new_target_scope", evaluableNodeManager->AllocNode(od.newTargetScope));
		opcode_attribs->SetMappedChildNode("frequency_per_10000_opcodes", evaluableNodeManager->AllocNode(od.frequencyPer10000Opcodes));
		opcode_attribs->SetMappedChildNode("opcode_group", evaluableNodeManager->AllocNode(od.opcodeGroup));

		std::string_view permissions_str;
		switch(od.permissions)
		{
		case ExecutionPermissions::Permission::NONE:				permissions_str = "none";					break;
		case ExecutionPermissions::Permission::STD_OUT_AND_STD_ERR:	permissions_str = "std_out_and_std_err";	break;
		case ExecutionPermissions::Permission::STD_IN:				permissions_str = "std_in";					break;
		case ExecutionPermissions::Permission::LOAD:				permissions_str = "load";					break;
		case ExecutionPermissions::Permission::STORE:				permissions_str = "store";					break;
		case ExecutionPermissions::Permission::ENVIRONMENT:			permissions_str = "environment";			break;
		case ExecutionPermissions::Permission::ALTER_PERFORMANCE:	permissions_str = "alter_performance";		break;
		case ExecutionPermissions::Permission::SYSTEM:				permissions_str = "system";					break;
		case ExecutionPermissions::Permission::ALL:					permissions_str = "all";					break;
		default:													permissions_str = "other";					break;
		}
		opcode_attribs->SetMappedChildNode("permissions", evaluableNodeManager->AllocNode(permissions_str));

		std::string_view new_value_str;
		switch(od.valueNewness)
		{
		case OpcodeDetails::OpcodeReturnNewnessType::NEW:			new_value_str = "new";			break;
		case OpcodeDetails::OpcodeReturnNewnessType::PARTIAL:		new_value_str = "partial";		break;
		case OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL:	new_value_str = "conditional";	break;
		case OpcodeDetails::OpcodeReturnNewnessType::EXISTING:		new_value_str = "existing";		break;
		case OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE:	new_value_str = "null";			break;
		}
		opcode_attribs->SetMappedChildNode("value_newness", evaluableNodeManager->AllocNode(new_value_str));

		EvaluableNode *example_output_pairs = evaluableNodeManager->AllocNode(ENT_LIST);
		auto &examples_output_pairs_ocn = example_output_pairs->GetOrderedChildNodesReference();
		examples_output_pairs_ocn.reserve(od.examples.size());
		for(auto &example : od.examples)
		{
			EvaluableNode *example_with_output_node = evaluableNodeManager->AllocNode(ENT_ASSOC);
			example_with_output_node->SetMappedChildNode("example",
				evaluableNodeManager->AllocNode(example.example));
			example_with_output_node->SetMappedChildNode("output",
				evaluableNodeManager->AllocNode(example.output));
			example_output_pairs->AppendOrderedChildNode(example_with_output_node);
		}

		opcode_attribs->SetMappedChildNode("examples", example_output_pairs);

		return opcode_attribs;
	}

	return AllocReturn(_help_options, immediate_result);
}

static OpcodeInitializer _ENT_PRINT(ENT_PRINT, &Interpreter::InterpretNode_ENT_PRINT, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(.null)";
	d.description = R"(Prints each of the parameters in order in a manner interpretable as if they were code, except strings are printed without quotes.  Output is pretty-printed.)";
	d.examples = MakeAmalgamExamples({
		{R"&((print "hello world\n"))&", R"(.null)"},
		{R"&((print
	1
	2
	[3 4]
	"5"
	"\n"
))&", R"(.null)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.permissions = ExecutionPermissions::Permission::STD_OUT_AND_STD_ERR;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 43.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_PRINT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.HasPermission(ExecutionPermissions::Permission::STD_OUT_AND_STD_ERR))
		return EvaluableNodeReference::Null();

	for(auto &cn : en->GetOrderedChildNodesReference())
	{
		auto cur = InterpretNodeForImmediateUse(cn);

		std::string s;
		if(cur == nullptr)
		{
			s = ".null";
		}
		else
		{
			if(DoesEvaluableNodeTypeUseNullData(cur->GetType()))
				s = ".null";
			else if(DoesEvaluableNodeTypeUseBoolData(cur->GetType()))
				s = EvaluableNode::BoolToString(cur->GetBoolValueReference());
			else if(DoesEvaluableNodeTypeUseStringData(cur->GetType()))
				s = cur->GetStringValue();
			else if(DoesEvaluableNodeTypeUseNumberData(cur->GetType()))
				s = EvaluableNode::NumberToString(cur->GetNumberValueReference());
			else //only print attributes if not debugSources
				s = Parser::Unparse(cur, true, false, true);

			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
		}

		if(writeListeners != nullptr)
		{
			for(auto &wl : *writeListeners)
				wl->LogPrint(s);
		}
		if(printListener != nullptr)
			printListener->LogPrint(s);
	}

	if(writeListeners != nullptr)
	{
		for(auto &wl : *writeListeners)
			wl->FlushLogFile();
	}
	if(printListener != nullptr)
		printListener->FlushLogFile();

	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_SYSTEM_TIME(ENT_SYSTEM_TIME, &Interpreter::InterpretNode_ENT_SYSTEM_TIME, []() {
	OpcodeDetails d;
	d.parameters = R"()";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the current system time since epoch in seconds (including fractions of seconds).)";
	d.examples = MakeAmalgamExamples({
		{R"&((system_time))&", R"(1773855306.4474)",
		R"&(^\s*
    (                                   # start of the number
        (?:\d+\.\d*|\.\d+|\d+)          # integer part with optional fraction
        (?:[eE][+-]?\d+)?               # optional exponent
    )
    \s*$)&"
}
		});
	d.permissions = ExecutionPermissions::Permission::ENVIRONMENT;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 4.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SYSTEM_TIME(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.HasPermission(ExecutionPermissions::Permission::ENVIRONMENT))
		return EvaluableNodeReference::Null();

	std::chrono::time_point tp = std::chrono::system_clock::now();
	std::chrono::system_clock::duration duration_us = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch());
	std::chrono::duration<double, std::ratio<1>> double_duration_us = duration_us;
	double sec = double_duration_us.count();

	return AllocReturn(sec, immediate_result);
}

static OpcodeInitializer _ENT_SYSTEM(ENT_SYSTEM, &Interpreter::InterpretNode_ENT_SYSTEM, []() {
	OpcodeDetails d;
	d.parameters = R"(string command [* optional1] ... [* optionalN])";
	d.returns = R"(any)";
	d.description = R"(Executes system command specified by `command`.  The available system commands are as follows:
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
 - built_in_data:       Returns built-in data compiled along with the version information.)";
	d.examples = MakeAmalgamExamples({
		{R"((system "debugging_info"))", R"([.false .false])"}
		});
	d.permissions = ExecutionPermissions::Permission::ALL;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 2.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

//Used only for deep debugging of entity memory and garbage collection
static std::string GetEntityMemorySizeDiagnostics(Entity *e)
{
	if(e == nullptr)
		return "";

	static CompactHashMap<Entity *, size_t> entity_core_allocs;
	static CompactHashMap<Entity *, size_t> entity_temp_unused;

	//initialize to zero if not already in the list
	auto prev_used = entity_core_allocs.emplace(e, 0);
	auto prev_unused = entity_temp_unused.emplace(e, 0);

	size_t cur_used = e->evaluableNodeManager.GetNumberOfUsedNodes();
	size_t cur_unused = e->evaluableNodeManager.GetNumberOfUnusedNodes();

	std::string result;

	if(cur_used > prev_used.first->second || cur_unused > prev_unused.first->second)
	{
		result += e->GetId() + " (used, free): " + EvaluableNode::NumberToString(cur_used - prev_used.first->second) + ", "
			+ EvaluableNode::NumberToString(cur_unused - prev_unused.first->second) + "\n";

		prev_used.first->second = cur_used;
		prev_unused.first->second = cur_unused;
	}

	for(auto entity : e->GetContainedEntities())
		result += GetEntityMemorySizeDiagnostics(entity);

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SYSTEM(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);

	std::string command = InterpretNodeIntoStringValueEmptyNull(ocn[0]);

	if(writeListeners != nullptr)
	{
		for(auto &wl : *writeListeners)
			wl->LogSystemCall(ocn[0]);
	}

	if(command == "exit" && permissions.HasPermission(ExecutionPermissions::Permission::SYSTEM))
	{
		exit(0);
	}
	else if(command == "readline" && permissions.HasPermission(ExecutionPermissions::Permission::STD_IN))
	{
		std::string input;
		std::getline(std::cin, input);

		//exit if have no more input
		if(std::cin.bad() || std::cin.eof())
			exit(0);

		return AllocReturn(input, immediate_result);
	}
	else if(command == "printline" && ocn.size() > 1 && permissions.HasPermission(ExecutionPermissions::Permission::STD_OUT_AND_STD_ERR))
	{
		std::string output = InterpretNodeIntoStringValueEmptyNull(ocn[1]);
		printListener->LogPrint(output);
		printListener->FlushLogFile();
		return EvaluableNodeReference::Null();
	}
	else if(command == "cwd" && permissions.HasPermission(ExecutionPermissions::Permission::ENVIRONMENT))
	{
		//if no parameter specified, return the directory
		if(ocn.size() == 1)
		{
			auto path = std::filesystem::current_path();
			return AllocReturn(path.string(), immediate_result);
		}

		std::string directory = InterpretNodeIntoStringValueEmptyNull(ocn[1]);
		std::filesystem::path path(directory);

		//try to set the directory
		std::error_code error;
		std::filesystem::current_path(directory, error);
		bool error_value = static_cast<bool>(error);
		return AllocReturn(error_value, immediate_result);
	}
	else if(command == "system" && ocn.size() > 1 && permissions.HasPermission(ExecutionPermissions::Permission::SYSTEM))
	{
		std::string sys_command = InterpretNodeIntoStringValueEmptyNull(ocn[1]);

		bool successful_run = false;
		int exit_code = 0;
		std::string stdout_data = Platform_RunSystemCommand(sys_command, successful_run, exit_code);

		if(!successful_run)
			return EvaluableNodeReference::Null();

		EvaluableNode *list = evaluableNodeManager->AllocNode(ENT_LIST);
		list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(static_cast<double>(exit_code)));
		list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, stdout_data));

		return EvaluableNodeReference(list, true);
	}
	else if(command == "os" && permissions.HasPermission(ExecutionPermissions::Permission::ENVIRONMENT))
	{
		std::string os = Platform_GetOperatingSystemName();
		return AllocReturn(os, immediate_result);
	}
	else if(command == "sleep" && permissions.HasPermission(ExecutionPermissions::Permission::SYSTEM))
	{
		std::chrono::microseconds sleep_time_usec(1);
		if(ocn.size() > 1)
		{
			double sleep_time_sec = InterpretNodeIntoNumberValue(ocn[1]);
			sleep_time_usec = std::chrono::microseconds(static_cast<size_t>(1000000.0 * sleep_time_sec));
		}

		Platform_Sleep(sleep_time_usec);
	}
	else if(command == "version")
	{
		std::string version_string = AMALGAM_VERSION_STRING;
		return AllocReturn(version_string, immediate_result);
	}
	else if(command == "version_compatible")
	{
		if(ocn.size() < 2)
			return EvaluableNodeReference::Null();

		std::string version_requested = InterpretNodeIntoStringValueEmptyNull(ocn[1]);
		auto [error_message, success] = AssetManager::ValidateVersionAgainstAmalgam(version_requested, false);
		EvaluableNode *result = evaluableNodeManager->AllocNode(success);
		result->SetCommentsString(error_message);
		return EvaluableNodeReference(result, true);
	}
	else if(command == "est_mem_reserved" && permissions.HasPermission(ExecutionPermissions::Permission::ENVIRONMENT))
	{
		return AllocReturn(static_cast<double>(curEntity->GetEstimatedReservedDeepSizeInBytes()), immediate_result);
	}
	else if(command == "est_mem_used" && permissions.HasPermission(ExecutionPermissions::Permission::ENVIRONMENT))
	{
		return AllocReturn(static_cast<double>(curEntity->GetEstimatedUsedDeepSizeInBytes()), immediate_result);
	}
	else if(command == "mem_diagnostics" && permissions.HasPermission(ExecutionPermissions::Permission::ENVIRONMENT))
	{

	#ifdef MULTITHREAD_SUPPORT
		auto lock = curEntity->CreateEntityLock<Concurrency::ReadLock>();
	#endif

		return AllocReturn(GetEntityMemorySizeDiagnostics(curEntity), immediate_result);
	}
	else if(command == "validate" && permissions.HasPermission(ExecutionPermissions::Permission::SYSTEM))
	{
		VerifyEvaluableNodeIntegrity();
		return AllocReturn(true, immediate_result);
	}
	else if(command == "rand" && ocn.size() > 1 && permissions.HasPermission(ExecutionPermissions::Permission::SYSTEM))
	{
		double num_bytes_raw = InterpretNodeIntoNumberValue(ocn[1]);
		size_t num_bytes = 0;
		if(num_bytes_raw > 0)
			num_bytes = static_cast<size_t>(num_bytes_raw);

		std::string rand_data(num_bytes, '\0');
		Platform_GenerateSecureRandomData(&rand_data[0], num_bytes);

		return AllocReturn(rand_data, immediate_result);
	}
	else if(command == "sign_key_pair" && permissions.HasPermission(ExecutionPermissions::Permission::SYSTEM))
	{
		auto [public_key, secret_key] = GenerateSignatureKeyPair();
		EvaluableNode *list = evaluableNodeManager->AllocNode(ENT_LIST);
		auto &list_ocn = list->GetOrderedChildNodesReference();
		list_ocn.resize(2);
		list_ocn[0] = evaluableNodeManager->AllocNode(public_key);
		list_ocn[1] = evaluableNodeManager->AllocNode(secret_key);

		return EvaluableNodeReference(list, true);

	}
	else if(command == "encrypt_key_pair" && permissions.HasPermission(ExecutionPermissions::Permission::SYSTEM))
	{
		auto [public_key, secret_key] = GenerateEncryptionKeyPair();
		EvaluableNode *list = evaluableNodeManager->AllocNode(ENT_LIST);
		auto &list_ocn = list->GetOrderedChildNodesReference();
		list_ocn.resize(2);
		list_ocn[0] = evaluableNodeManager->AllocNode(public_key);
		list_ocn[1] = evaluableNodeManager->AllocNode(secret_key);

		return EvaluableNodeReference(list, true);
	}
	else if(command == "debugging_info" && permissions.HasPermission(ExecutionPermissions::Permission::ENVIRONMENT))
	{
		EvaluableNode *debugger_info = evaluableNodeManager->AllocNode(ENT_LIST);
		auto &list_ocn = debugger_info->GetOrderedChildNodesReference();
		list_ocn.resize(2);
		list_ocn[0] = evaluableNodeManager->AllocNode(Interpreter::GetDebuggingState());
		list_ocn[1] = evaluableNodeManager->AllocNode(asset_manager.debugSources);

		return EvaluableNodeReference(debugger_info, true);
	}
#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
	else if(command == "get_max_num_threads" && permissions.HasPermission(ExecutionPermissions::Permission::ENVIRONMENT))
	{
		double max_num_threads = static_cast<double>(Concurrency::GetMaxNumThreads());
		return AllocReturn(max_num_threads, immediate_result);
	}
	else if(command == "set_max_num_threads" && ocn.size() > 1 && permissions.HasPermission(ExecutionPermissions::Permission::ALTER_PERFORMANCE))
	{
		double max_num_threads_raw = InterpretNodeIntoNumberValue(ocn[1]);
		size_t max_num_threads = 0;
		if(max_num_threads >= 0)
			max_num_threads = static_cast<size_t>(max_num_threads_raw);
		Concurrency::SetMaxNumThreads(max_num_threads);

		max_num_threads_raw = static_cast<double>(Concurrency::GetMaxNumThreads());
		return AllocReturn(max_num_threads_raw, immediate_result);
	}
#endif
	else if(command == "built_in_data" && permissions.HasPermission(ExecutionPermissions::Permission::ENVIRONMENT))
	{
		uint8_t built_in_data[] = AMALGAM_BUILT_IN_DATA;
		std::string built_in_data_s(reinterpret_cast<char *>(&built_in_data[0]), sizeof(built_in_data));
		return AllocReturn(built_in_data_s, immediate_result);
	}
	else if(permissions.HasPermission(ExecutionPermissions::Permission::STD_OUT_AND_STD_ERR))
	{
		std::cerr << "Invalid system opcode command \"" << command << "\" invoked" << std::endl;
	}

	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_RECLAIM_RESOURCES(ENT_RECLAIM_RESOURCES, &Interpreter::InterpretNode_ENT_RECLAIM_RESOURCES, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity] [bool apply_to_all_contained_entities] [bool|list clear_query_caches] [bool collect_garbage] [bool force_free_memory])";
	d.returns = R"(any)";
	d.description = R"(Frees resources of the specified types on `entity`, which is the current entity if null.  Will include all contained entities if `apply_to_all_contained_entities` is true, which defaults to false, though the opcode will be unable to complete if there are concurrent threads running on any of the contained entities.  The parameter `clear_query_caches` will remove the query caches, which will make it faster to add, remove, or edit contained entities, but the cache will be rebuilt once a query is called.  If `clear_query_caches` is a boolean, then it will either clear all the caches or none.  If `clear_query_caches` is a list of strings, then it will only clear caches for the labels corresponding to the strings in the list.  The parameter `collect_garbage` will perform garbage collection on the entity, and if `force_free_memory` is true, it will reallocate memory buffers to their current size, after garbage collection if both are specified.)";
	d.examples = MakeAmalgamExamples({
		{R"((reclaim_resources .null .true ["x"] .true .true ))", R"(.null)"},
		{R"((reclaim_resources .null .true .true .true .true ))", R"(.null)"}
		});
	d.permissions = ExecutionPermissions::Permission::ALTER_PERFORMANCE;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_RECLAIM_RESOURCES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.HasPermission(ExecutionPermissions::Permission::ALTER_PERFORMANCE))
		return EvaluableNodeReference::Null();

	bool apply_to_all_contained_entities = false;
	if(ocn.size() > 1)
		apply_to_all_contained_entities = InterpretNodeIntoBoolValue(ocn[1]);

	auto clear_query_caches_node = EvaluableNodeReference::Null();
	auto node_stack = CreateOpcodeStackStateSaver();
	if(ocn.size() > 2)
	{
		clear_query_caches_node = InterpretNode(ocn[2]);
		node_stack.PushEvaluableNode(clear_query_caches_node);
	}

	bool collect_garbage = true;
	if(ocn.size() > 3)
		collect_garbage = InterpretNodeIntoBoolValue(ocn[3]);

	bool force_free_memory = false;
	if(ocn.size() > 4)
		force_free_memory = InterpretNodeIntoBoolValue(ocn[4]);

	//get the entity last to reduce time under lock
	EntityWriteReference target_entity;
	if(ocn.size() > 1)
		target_entity = InterpretNodeIntoRelativeSourceEntityWriteReference(ocn[0]);
	else
		target_entity = EntityWriteReference(curEntity);

	bool clear_all_query_caches = false;
	bool clear_select_query_caches = false;
	if(EvaluableNode::IsOrderedArray(clear_query_caches_node))
		clear_select_query_caches = true;
	else
		clear_all_query_caches = EvaluableNode::ToBool(clear_query_caches_node);

	if(apply_to_all_contained_entities)
	{
		//lock all entities
		auto contained_entities = target_entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityWriteReference>();
		if(contained_entities == nullptr)
			return EvaluableNodeReference::Null();

		for(auto &e : *contained_entities)
		{
			e->ReclaimResources(clear_all_query_caches, collect_garbage, force_free_memory);
			if(clear_select_query_caches)
			{
				for(auto cn : clear_query_caches_node->GetOrderedChildNodesReference())
					target_entity->ClearQueryCacheForLabel(EvaluableNode::ToStringIDIfExists(cn));
			}
		}
	}
	else
	{
		target_entity->ReclaimResources(clear_all_query_caches, collect_garbage, force_free_memory);
		if(clear_select_query_caches)
		{
			for(auto cn : clear_query_caches_node->GetOrderedChildNodesReference())
				target_entity->ClearQueryCacheForLabel(EvaluableNode::ToStringIDIfExists(cn));
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(clear_query_caches_node);

	return EvaluableNodeReference::Null();
}
