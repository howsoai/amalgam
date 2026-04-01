//project headers:
#include "Interpreter.h"

#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "Concurrency.h"
#include "Cryptography.h"
#include "EntityManipulation.h"
#include "EntityWriteListener.h"
#include "EvaluableNodeManagement.h"
#include "EvaluableNodeTreeFunctions.h"
#include "OpcodeDetails.h"
#include "PerformanceProfiler.h"

//system headers:
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <utility>


static EvaluableNodeReference ConstraintViolationToString(InterpreterConstraints::ViolationType violation, EvaluableNodeManager *evaluable_node_manager)
{
	switch(violation)
	{
	case InterpreterConstraints::ViolationType::NoViolation:
		return EvaluableNodeReference::Null();
	case InterpreterConstraints::ViolationType::ContainedEntitiesDepth:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Contained entities depth exceeded")), true);
	case InterpreterConstraints::ViolationType::ContainedEntitiesNumber:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Contained entities number l)imit exceeded")), true);
	case InterpreterConstraints::ViolationType::ExecutionDepth:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Execution depth exceeded")), true);
	case InterpreterConstraints::ViolationType::ExecutionStep:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Execution step limit exceeded")), true);
	case InterpreterConstraints::ViolationType::NodeAllocation:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Node allocation limit exceeded")), true);
	default:
		//cases should be exhaustive, so this is unreachable
		assert(false);
	}

	assert(false);
	return ""; //unreachable
}

EvaluableNodeReference Interpreter::BundleResultWithWarningsIfNeeded(EvaluableNodeReference result, InterpreterConstraints *interpreter_constraints)
{
	if(interpreter_constraints == nullptr || !interpreter_constraints->collectWarnings)
		return result;

	EvaluableNodeReference warning_assoc = CreateAssocOfNumbersFromIteratorAndFunctions(
		interpreter_constraints->warnings, [](std::pair<std::string, size_t> warning_count)
	{ return warning_count.first; }, [](std::pair<std::string, size_t> warning_count)
	{ return static_cast<double>(warning_count.second); }, evaluableNodeManager);

	EvaluableNodeReference constraint_violation_string = ConstraintViolationToString(interpreter_constraints->constraintViolation, evaluableNodeManager);

	EvaluableNodeReference result_tuple(evaluableNodeManager->AllocNode(ENT_LIST), true);

	auto &result_tuple_ocn = result_tuple->GetOrderedChildNodesReference();

	result_tuple_ocn.reserve(3);
	result_tuple_ocn.push_back(result);
	result_tuple_ocn.push_back(warning_assoc);
	result_tuple_ocn.push_back(constraint_violation_string);

	result_tuple.UpdatePropertiesBasedOnAttachedNode(result);
	result_tuple.UpdatePropertiesBasedOnAttachedNode(warning_assoc);
	result_tuple.UpdatePropertiesBasedOnAttachedNode(constraint_violation_string);

	return result_tuple;
}

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_RAND_SEED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	std::string rand_state_string = randomStream.GetState();
	return AllocReturn(rand_state_string, immediate_result);
}

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_DEALLOCATED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	std::cerr << "ERROR: attempt to use freed memory\n";
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(false);
#endif
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_NOT_A_BUILT_IN_TYPE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	std::cerr << "ERROR: encountered an invalid instruction\n";
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(false);
#endif
	return EvaluableNodeReference::Null();
}

void Interpreter::VerifyEvaluableNodeIntegrity()
{
	for(EvaluableNode *en : *scopeStackNodes)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en);

	for(EvaluableNode *en : *opcodeStackNodes)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en, nullptr, false);

	for(EvaluableNode *en : *constructionStackNodes)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en);

	if(curEntity != nullptr)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(curEntity->GetRoot());

	{
		auto &nr = evaluableNodeManager->GetNodesReferenced();
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::Lock lock(nr.mutex);
	#endif
		for(auto &[en, _] : nr.nodesReferenced)
			EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en, nullptr, false);
	}

	if(callingInterpreter != nullptr)
		callingInterpreter->VerifyEvaluableNodeIntegrity();
}
