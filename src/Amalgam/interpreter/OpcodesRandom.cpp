//project headers:
#include "Interpreter.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Random";

static OpcodeInitializer _ENT_RAND(ENT_RAND, &Interpreter::InterpretNode_ENT_RAND, []() {
	OpcodeDetails d;
	d.parameters = R"([list|assoc|number range] [number number_to_generate] [bool unique])";
	d.returns = R"(any)";
	d.description = R"(Generates random values based on its parameters.  The random values are drawn from a random stream specific to each execution flow for each entity.  With no range, evaluates to a random number between 0.0 and 1.0.  If range is a list, it will uniformly randomly choose and evaluate to one element of the list.  If range is a number, it will evaluate to a value greater than or equal to zero and less than the number specified.  If range is an assoc, then it will randomly evaluate to one of the keys using the values as the weights for the probabilities.  If  number_to_generate is specified, it will generate a list of multiple values (even if number_to_generate is 1).  If unique is true (it defaults to false), then it will only return unique values, the same as selecting from the list or assoc without replacement.  Note that if unique only applies to list and assoc ranges.  If unique is true and there are not enough values in a list or assoc, it will only generate the number of elements in range.)";
	d.examples = MakeAmalgamExamples({
		{R"&((rand))&", R"(0.4153759082605256)"},
		{R"&((rand 50))&", R"(20.768795413026282)"},
		{R"&((rand
	[1 2 4 5 7]
))&", R"(1)"},
			{R"&((rand
	(range 0 10)
))&", R"(4)"},
			{R"&((rand
	(range 0 10)
	0
))&", R"([])"},
			{R"&((rand
	(range 0 10)
	1
))&", R"([4])"},
			{R"&((rand
	(range 0 10)
	10
	.true
))&", R"([
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
])"},
			{R"&((rand 50 4))&", R"([20.768795413026282 23.51742714184096 6.034392211178502 29.777315548569128])"},
			{R"&((rand
	(associate "a" 0.25 "b" 0.75)
))&", R"("b")"},
			{R"&((rand
	(associate "a" 0.25 "b" 0.75)
	16
))&", "", R"&(\[\s*
    "(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
\])&"
},
			{R"&((rand
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
))&", R"(["c" "c" "c" "d"])",
R"&(\[\s*
    "(?:c|d)"\s*
    "(?:c|d)"\s*
    "(?:c|d)"\s*
    "(?:c|d)"\s*
\])&"
},
			{R"&(;should come out somewhere near the correct proportion
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
))&", R"({a 30 b 50 c 20})",
			R"&(\{\s*
    a\s+(\d+)\s+
    b\s+(\d+)\s+
    c\s+(\d+)\s*
\})&"
},
			{R"&(;these should be weighted toward smaller numbers
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
))&", R"([2 6 1])",
			R"&(\[\s*
    (\d+)\s*
    (\d+)\s*
    (\d+)\s*
\])&"
}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 6.5;
	d.opcodeGroup = _opcode_group;
	return d;
});


//given an assoc of StringID -> value representing the probability weight of each, and a random stream, it randomly selects from the assoc
// if it can't find an appropriate probability, it returns an empty string
// if normalize is true, then it will accumulate the probability and then normalize
static StringInternPool::StringID GetRandomWeightedKey(EvaluableNode::AssocType &assoc, RandomStream &rs, bool normalize)
{
	double probability_target = rs.RandFull();
	double accumulated_probability = 0.0;
	double total_probability = 1.0;

	if(normalize)
	{
		total_probability = 0;
		for(auto &[_, prob] : assoc)
			total_probability += std::max(0.0, EvaluableNode::ToNumber(prob, 0.0));

		//if no probabilities, just choose uniformly
		if(total_probability <= 0.0)
		{
			//find index to return
			size_t index_to_return = static_cast<size_t>(assoc.size() * probability_target);

			//iterate over pairs until find the index
			size_t cur_index = 0;
			for(auto &[prob_id, _] : assoc)
			{
				if(cur_index == index_to_return)
					return prob_id;

				cur_index++;
			}

			return StringInternPool::NOT_A_STRING_ID;
		}

		if(total_probability == std::numeric_limits<double>::infinity())
		{
			//start over, count infinities
			size_t inf_count = 0;
			for(auto &[_, prob] : assoc)
			{
				if(EvaluableNode::ToNumber(prob, 0.0) == std::numeric_limits<double>::infinity())
					inf_count++;
			}

			//get the infinity to use
			inf_count = static_cast<size_t>(inf_count * probability_target);

			//count down until the infinite pair is found
			for(auto &[prob_id, prob] : assoc)
			{
				if(EvaluableNode::ToNumber(prob, 0.0) == std::numeric_limits<double>::infinity())
				{
					if(inf_count == 0)
						return prob_id;
					inf_count--;
				}
			}

			//shouldn't make it here
			return StringInternPool::NOT_A_STRING_ID;
		}
	}

	for(auto &[prob_id, prob] : assoc)
	{
		accumulated_probability += (EvaluableNode::ToNumber(prob, 0.0) / total_probability);
		if(probability_target < accumulated_probability)
			return prob_id;
	}

	//probability mass didn't add up, just grab the first one with a probability greater than zero
	for(auto &[prob_id, prob] : assoc)
	{
		if(EvaluableNode::ToNumber(prob, 0.0) > 0)
			return prob_id;
	}

	//nothing valid to return
	return StringInternPool::NOT_A_STRING_ID;
}

//Generates an EvaluableNode containing a random value based on the random parameter param, using enm and random_stream
// if any part of param is preserved in the return value, then can_free_param will be set to false, otherwise it will be left alone
static EvaluableNodeReference GenerateRandomValueBasedOnRandParam(EvaluableNodeReference param, Interpreter *interpreter,
	RandomStream &random_stream, bool &can_free_param, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(EvaluableNode::IsNull(param))
		return interpreter->AllocReturn(random_stream.RandFull(), immediate_result);

	if(param->GetNumChildNodes() > 0)
	{
		if(param->IsAssociativeArray())
		{
			StringInternPool::StringID id_selected = GetRandomWeightedKey(param->GetMappedChildNodesReference(),
				random_stream, true);
			return Parser::ParseFromKeyStringId(id_selected, interpreter->evaluableNodeManager);
		}
		else if(param->IsOrderedArray())
		{
			auto &ocn = param->GetOrderedChildNodesReference();
			size_t selection = random_stream.RandSize(ocn.size());
			can_free_param = false;
			return EvaluableNodeReference(ocn[selection], param.unique);
		}
	}
	else if(DoesEvaluableNodeTypeUseNumberData(param->GetType()))
	{
		double value = random_stream.RandFull() * param->GetNumberValueReference();
		return interpreter->AllocReturn(value, immediate_result);
	}

	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_RAND(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
	{
		double r = randomStream.RandFull();
		return AllocReturn(r, immediate_result);
	}

	//get number to generate
	bool generate_list = false;
	size_t number_to_generate = 1;
	if(ocn.size() >= 2)
	{
		double num_value = InterpretNodeIntoNumberValue(ocn[1]);
		if(FastIsNaN(num_value) || num_value < 0)
			return EvaluableNodeReference::Null();
		number_to_generate = static_cast<size_t>(num_value);
		generate_list = true;
		//because generating a list, can no longer return an immediate
		immediate_result = EvaluableNodeRequestedValueTypes::Type::NONE;
	}
	//make sure not eating up too much memory
	if(ConstrainedAllocatedNodes())
	{
		if(interpreterConstraints->WouldNewAllocatedNodesExceedConstraint(
			evaluableNodeManager->GetNumberOfUsedNodes() + number_to_generate))
			return EvaluableNodeReference::Null();
	}

	//get whether it needs to be unique
	bool generate_unique_values = false;
	if(ocn.size() >= 3)
		generate_unique_values = InterpretNodeIntoBoolValue(ocn[2]);

	//get random param
	auto param = InterpretNodeForImmediateUse(ocn[0]);

	//if generating a single value
	if(!generate_list)
	{
		bool can_free_param = true;
		EvaluableNodeReference rand_value = GenerateRandomValueBasedOnRandParam(param,
				this, randomStream, can_free_param, immediate_result);

		if(can_free_param)
			evaluableNodeManager->FreeNodeTreeIfPossible(param);
		else
			evaluableNodeManager->FreeNodeIfPossible(param);
		return rand_value;
	}

	if(generate_unique_values && !EvaluableNode::IsNull(param) && param->GetNumChildNodes() > 0)
	{
		//clamp to the maximum number that can possibly be generated
		size_t num_elements = (param == nullptr ? 0 : param->GetNumChildNodes());
		number_to_generate = std::min(number_to_generate, num_elements);

		if(param->IsAssociativeArray())
		{
			//want to generate multiple values, so return a list
			EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);
			auto &retval_ocn = retval->GetOrderedChildNodesReference();
			retval_ocn.reserve(number_to_generate);

			//make a copy of all of the probabilities so they can be removed one at a time
			EvaluableNode::AssocType assoc(param->GetMappedChildNodesReference());

			for(size_t i = 0; i < number_to_generate; i++)
			{
				StringInternPool::StringID selected_sid = GetRandomWeightedKey(assoc, randomStream, true);
				EvaluableNodeReference selected_value = Parser::ParseFromKeyStringId(selected_sid, evaluableNodeManager);
				retval_ocn.push_back(selected_value);
				retval.UpdatePropertiesBasedOnAttachedNode(selected_value, i == 0);

				//remove the element so it won't be reselected
				assoc.erase(selected_sid);
			}

			evaluableNodeManager->FreeNodeTreeIfPossible(param);
			return retval;
		}

		//want to generate multiple values, so return a list
		//try to reuse param if can so don't need to allocate more memory
		EvaluableNodeReference retval;
		if(param.unique)
		{
			retval = param;
		}
		else
		{
			retval = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);
			retval->SetOrderedChildNodes(param->GetOrderedChildNodesReference(),
				param->GetNeedCycleCheck(), param->GetIsIdempotent());

			retval.UpdatePropertiesBasedOnAttachedNode(param, true);
		}

		//shuffle ordered child nodes
		auto &retval_ocn = retval->GetOrderedChildNodesReference();
		for(size_t i = 0; i < number_to_generate; i++)
		{
			//make sure to only shuffle things that haven't been shuffled before,
			//otherwise bias toward lower elements
			size_t to_swap_with = randomStream.RandSize(num_elements - i);
			std::swap(retval_ocn[i], retval_ocn[i + to_swap_with]);
		}

		//free unneeded nodes that weren't part of the shuffle
		if(param.unique && !param->GetNeedCycleCheck())
		{
			for(size_t i = number_to_generate; i < num_elements; i++)
				evaluableNodeManager->FreeNodeTree(retval_ocn[i]);
		}

		//get rid of unneeded extra nodes
		retval->SetOrderedChildNodesSize(number_to_generate);
		retval->ReleaseOrderedChildNodesExtraMemory();

		return retval;
	}

	//want to generate multiple values, so return a list
	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);

	//just generate a list of values with replacement; either generate_unique_values was not set or the distribution "always" generates unique values
	retval->ReserveOrderedChildNodes(number_to_generate);

	bool can_free_param = true;

	//get information to determine which mechanism to use to generate
	size_t num_weighted_values = 0;
	if(EvaluableNode::IsAssociativeArray(param))
		num_weighted_values = param->GetMappedChildNodesReference().size();

	if(num_weighted_values > 0
		&& (number_to_generate > 10 || (number_to_generate > 3 && num_weighted_values > 200)))
	{
		//use fast repeated generation technique
		WeightedDiscreteRandomStreamTransform<StringInternPool::StringID,
			EvaluableNode::AssocType, EvaluableNodeAsDouble> wdrst(param->GetMappedChildNodesReference(), false);
		for(size_t i = 0; i < number_to_generate; i++)
		{
			EvaluableNodeReference rand_value(Parser::ParseFromKeyStringId(wdrst.WeightedDiscreteRand(randomStream), evaluableNodeManager));
			retval->AppendOrderedChildNode(rand_value);
		}
	}
	else //perform simple generation
	{
		for(size_t i = 0; i < number_to_generate; i++)
		{
			EvaluableNodeReference rand_value = GenerateRandomValueBasedOnRandParam(param,
				this, randomStream, can_free_param, immediate_result);
			retval->AppendOrderedChildNode(rand_value);
			retval.UpdatePropertiesBasedOnAttachedNode(rand_value, i == 0);
		}
	}

	if(can_free_param)
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(param);
	}
	else
	{
		//if used the parameters, a parameter might be used more than once
		retval->SetNeedCycleCheck(true);
		evaluableNodeManager->FreeNodeIfPossible(param);
	}

	return retval;
}

static OpcodeInitializer _ENT_GET_RAND_SEED(ENT_GET_RAND_SEED, &Interpreter::InterpretNode_ENT_GET_RAND_SEED, []() {
	OpcodeDetails d;
	d.parameters = R"()";
	d.returns = R"(string)";
	d.description = R"(Evaluates to a string representing the current state of the random number generator.  Note that the string will be a string of bytes that may not be valid as UTF-8.)";
	d.examples = MakeAmalgamExamples({
		{R"&((format (get_rand_seed) "string" "base64"))&", R"("X6f8e5JTT5kuHHGZUu7r6/8=")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_RAND_SEED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	std::string rand_state_string = randomStream.GetState();
	return AllocReturn(rand_state_string, immediate_result);
}

static OpcodeInitializer _ENT_SET_RAND_SEED(ENT_SET_RAND_SEED, &Interpreter::InterpretNode_ENT_SET_RAND_SEED, []() {
	OpcodeDetails d;
	d.parameters = R"(string seed)";
	d.returns = R"(string)";
	d.description = R"(Initializes the random number stream for the given `seed` without affecting any entity.  If the seed is already a string in the proper format output by `get_entity_rand_seed` or `get_rand_seed`, then it will set the random generator to that current state, picking up where the previous state left off.  If it is anything else, it uses the value as a random seed to start the generator.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
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
))&", R"([0.4153759082605256 0.47034854283681926 0.4153759082605256 0.47034854283681926])"},
			{R"&((seq
	(set_rand_seed "12345")
	(rand)
))&", R"(0.5507987428849511)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_RAND_SEED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto seed_node = InterpretNodeForImmediateUse(ocn[0]);
	std::string seed_string;
	if(seed_node != nullptr && seed_node->GetType() == ENT_STRING)
		seed_string = seed_node->GetStringValue();
	else
		seed_string = Parser::Unparse(seed_node, false, false, true);

	randomStream.SetState(seed_string);

	return seed_node;
}

static OpcodeInitializer _ENT_GET_ENTITY_RAND_SEED(ENT_GET_ENTITY_RAND_SEED, &Interpreter::InterpretNode_ENT_GET_ENTITY_RAND_SEED, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity])";
	d.returns = R"(string)";
	d.description = R"(Evaluates to a string representing the current state of the random number generator for `entity` used for seeding the random streams of any calls to the entity.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
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
))&", R"("nHKVcHddHVaqvcDt3AYbD/8=")", "", R"((destroy_entities "Rand"))"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_RAND_SEED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	EntityReadReference entity;
	if(ocn.size() > 0)
		entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		entity = EntityReadReference(curEntity);

	if(entity == nullptr)
		return EvaluableNodeReference::Null();

	std::string rand_state_string = entity->GetRandomState();

	return AllocReturn(rand_state_string, immediate_result);
}

static OpcodeInitializer _ENT_SET_ENTITY_RAND_SEED(ENT_SET_ENTITY_RAND_SEED, &Interpreter::InterpretNode_ENT_SET_ENTITY_RAND_SEED, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity] * node [bool deep])";
	d.returns = R"(string)";
	d.description = R"(Sets the random number seed and state for the random number generator of `entity`, or the current entity if null or not specified, to the state specified by `node`.  If `node` is already a string in the proper format output by `(get_entity_rand_seed)`, then it will set the random generator to that current state, picking up where the previous state left off.  If `node` is anything else, it uses the value as a random seed to start the generator.  Note that this will not affect the state of the current random number stream, only future random streams created by `entity` for new calls.  The parameter `deep` defaults to false, but if it is true, all contained entities are recursively set with random seeds based on the specified random seed and a hash of their relative id path to the entity being set.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
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
))&", R"([
	[0.9512993766655248 0.3733350484591008]
	[0.9512993766655248 0.3733350484591008]
])", "", R"((destroy_entities "Rand"))"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_ENTITY_RAND_SEED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(!CanModifyEntityFromConstraints())
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();
	size_t num_params = ocn.size();
	if(num_params < 1)
		return EvaluableNodeReference::Null();

	//retrieve parameter to determine whether to deep set the seeds, if applicable
	bool deep_set = true;
	if(num_params == 3)
		deep_set = InterpretNodeIntoBoolValue(ocn[2], true);

	//the opcode parameter index of the seed
	auto seed_node = InterpretNode(ocn[num_params > 1 ? 1 : 0]);
	std::string seed_string;
	if(seed_node != nullptr && seed_node->GetType() == ENT_STRING)
		seed_string = seed_node->GetStringValue();
	else
		seed_string = Parser::Unparse(seed_node, false, false, true);
	auto node_stack = CreateOpcodeStackStateSaver(seed_node);

	//get the entity
	EntityWriteReference entity;
	if(num_params > 1)
		entity = InterpretNodeIntoRelativeSourceEntityWriteReference(ocn[0]);
	else
		entity = EntityWriteReference(curEntity);

	if(entity == nullptr)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(deep_set)
	{
		auto contained_entities = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityWriteReference>();
		if(contained_entities == nullptr)
			return EvaluableNodeReference::Null();

		entity->SetRandomState(seed_string, true, writeListeners, &contained_entities);
	}
	else
	#endif
		entity->SetRandomState(seed_string, deep_set, writeListeners);

	return seed_node;
}
