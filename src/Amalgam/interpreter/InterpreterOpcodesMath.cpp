//project headers:
#include "EntityQueryBuilder.h"
#include "EvaluableNode.h"
#include "Interpreter.h"

//system headers:
#include <cstdlib>
#include <functional>
#include <utility>

EvaluableNodeReference Interpreter::InterpretNode_ENT_EXPONENT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::exp(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LOG(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	double divisor = 1.0;
	if(ocn.size() > 1) //base is specified, need to scale
	{
		double log_base = InterpretNodeIntoNumberValue(ocn[1]);
		divisor = log(log_base);
	}

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[divisor](double value) {	return std::log(value) / divisor;	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SIN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::sin(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASIN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::asin(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_COS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::cos(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ACOS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::acos(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TAN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::tan(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ATAN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	if(ocn.size() == 1)
	{
		return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
			[](double value) {	return std::atan(value);	});
	}
	else if(ocn.size() >= 2)
	{
		double f1 = InterpretNodeIntoNumberValue(ocn[0]);
		double f2 = InterpretNodeIntoNumberValue(ocn[1]);
		return AllocReturn(std::atan2(f1, f2), immediate_result);
	}
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SINH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::sinh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASINH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::asinh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_COSH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::cosh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ACOSH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::acosh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TANH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::tanh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ATANH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::atanh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ERF(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::erf(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TGAMMA(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::tgamma(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LGAMMA(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::lgamma(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SQRT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::sqrt(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_POW(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	double f1 = InterpretNodeIntoNumberValue(ocn[0]);
	double f2 = InterpretNodeIntoNumberValue(ocn[1]);
	return AllocReturn(std::pow(f1, f2), immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DOT_PRODUCT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return AllocReturn(0.0, immediate_result);

	EvaluableNodeReference elements1 = InterpretNodeForImmediateUse(ocn[0]);
	if(EvaluableNode::IsNull(elements1))
		return AllocReturn(0.0, immediate_result);

	auto node_stack = CreateOpcodeStackStateSaver(elements1);
	EvaluableNodeReference elements2 = InterpretNodeForImmediateUse(ocn[1]);
	node_stack.PopEvaluableNode();

	if(EvaluableNode::IsNull(elements2))
		return AllocReturn(0.0, immediate_result);

	bool elements1_assoc = elements1->IsAssociativeArray();
	bool elements2_assoc = elements2->IsAssociativeArray();

	double dot_product = 0.0;

	if(!elements1_assoc && !elements2_assoc)
	{
		auto &ocn1 = elements1->GetOrderedChildNodes();
		auto &ocn2 = elements2->GetOrderedChildNodes();

		size_t num_elements = std::min(ocn1.size(), ocn2.size());
		for(size_t i = 0; i < num_elements; i++)
			dot_product += EvaluableNode::ToNumber(ocn1[i]) * EvaluableNode::ToNumber(ocn2[i]);
	}
	else //at least one is an assoc
	{
		//if not an assoc, then convert
		if(!elements1_assoc)
		{
			evaluableNodeManager->EnsureNodeIsModifiable(elements1);
			elements1->ConvertListToNumberedAssoc();
		}

		if(!elements2_assoc)
		{
			evaluableNodeManager->EnsureNodeIsModifiable(elements2);
			elements2->ConvertListToNumberedAssoc();
		}

		auto &mcn1 = elements1->GetMappedChildNodesReference();
		auto &mcn2 = elements2->GetMappedChildNodesReference();

		for(auto &[node1_id, node1] : mcn1)
		{
			//if a key isn't in both, then its value is zero
			auto node2 = mcn2.find(node1_id);
			if(node2 == end(mcn2))
				continue;

			dot_product += EvaluableNode::ToNumber(node1) * EvaluableNode::ToNumber(node2->second);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(elements1);
	evaluableNodeManager->FreeNodeTreeIfPossible(elements2);
	return AllocReturn(dot_product, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_NORMALIZE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	double p_value = 1.0;
	if(ocn.size() > 1)
	{
		double num_value = InterpretNodeIntoNumberValue(ocn[1]);
		if(!FastIsNaN(num_value))
			p_value = num_value;
	}

	auto container = InterpretNode(ocn[0]);
	if(EvaluableNode::IsNull(container) || container->IsImmediate())
		return EvaluableNodeReference::Null();

	bool allocate_child_nodes = (!container.unique);
	evaluableNodeManager->EnsureNodeIsModifiable(container, false, false);

	//ensure it's a list
	if(container->IsOrderedArray())
		container->SetType(ENT_LIST, evaluableNodeManager, false);
	container->SetIsIdempotent(true);
	container->SetNeedCycleCheck(false);

	if(container->IsAssociativeArray())
	{
		NormalizeVector(container->GetMappedChildNodesReference(), p_value,
			[](const auto &pair) { return EvaluableNode::ToNumber(pair.second); },
			[this, allocate_child_nodes](auto &pair, double new_val)
			{
				if(allocate_child_nodes || pair.second == nullptr)
				{
					pair.second = evaluableNodeManager->AllocNode(new_val);
				}
				else
				{
					pair.second->SetTypeViaNumberValue(new_val);
					pair.second->ClearMetadata();
				}
			}
		);
	}
	else //container->IsOrderedArray()
	{
		NormalizeVector(container->GetOrderedChildNodesReference(), p_value,
			[](const auto &cn) { return EvaluableNode::ToNumber(cn); },
			[this, allocate_child_nodes](auto &cn, double new_val)
			{
				if(allocate_child_nodes || cn == nullptr)
				{
					cn = evaluableNodeManager->AllocNode(new_val);
				}
				else
				{
					cn->SetTypeViaNumberValue(new_val);
					cn->ClearMetadata();
				}
			}
		);
	}

	return container;
}

//set of getter methods to help ENT_MODE, ENT_QUANTILE, and ENT_GENERALIZED_DISTANCE when retrieving values and weights
static inline bool GetValueFromIter(EvaluableNode::AssocType::iterator iter, double &value)
{
	value = EvaluableNode::ToNumber(iter->second);
	return !FastIsNaN(value);
};

static inline bool GetValueFromIter(EvaluableNode::AssocType::iterator iter, std::string &value)
{
	value = Parser::UnparseToKeyString(iter->second);
	return true;
};

static inline bool GetValueFromIndex(std::vector<EvaluableNode *> &ocn, size_t i, double &value)
{
	if(i >= ocn.size())
		return false;

	value = EvaluableNode::ToNumber(ocn[i]);
	return !FastIsNaN(value);
};

static inline bool GetValueFromIndex(std::vector<EvaluableNode *> &ocn, size_t i, std::string &value)
{
	if(i >= ocn.size())
		return false;

	value = Parser::UnparseToKeyString(ocn[i]);
	return true;
};

static inline bool GetValueFromWeightsIter(EvaluableNode::AssocType &values_mcn,
	EvaluableNode::AssocType::iterator iter, double &value)
{
	auto entry = values_mcn.find(iter->first);
	if(entry == end(values_mcn))
		return false;

	value = EvaluableNode::ToNumber(entry->second);
	return !FastIsNaN(value);
};

static inline bool GetValueFromWeightsIter(EvaluableNode::AssocType &values_mcn,
	EvaluableNode::AssocType::iterator iter, std::string &value)
{
	auto entry = values_mcn.find(iter->first);
	if(entry == end(values_mcn))
		return false;

	value = Parser::UnparseToKeyString(entry->second);
	return true;
};

static inline bool GetValueFromWeightsIter(std::vector<EvaluableNode *> &values_ocn,
	EvaluableNode::AssocType::iterator iter, double &value)
{
	double index_double = Parser::ParseNumberFromKeyStringId(iter->first);
	if(FastIsNaN(index_double))
		return false;
	size_t index = static_cast<size_t>(index_double);
	if(index >= values_ocn.size())
		return false;

	value = EvaluableNode::ToNumber(values_ocn[index]);
	return !FastIsNaN(value);
};

static inline bool GetValueFromWeightsIter(std::vector<EvaluableNode *> &values_ocn,
	EvaluableNode::AssocType::iterator iter, std::string &value)
{
	double index_double = Parser::ParseNumberFromKeyStringId(iter->first);
	if(FastIsNaN(index_double))
		return false;
	size_t index = static_cast<size_t>(index_double);
	if(index >= values_ocn.size())
		return false;

	value = Parser::UnparseToKeyString(values_ocn[index]);
	return true;
};

static inline bool GetValueFromWeightsIndex(EvaluableNode::AssocType &values_mcn,
	size_t index, double &value)
{
	auto key_sid = EvaluableNode::NumberToStringIDIfExists(index, true);
	if(key_sid == string_intern_pool.NOT_A_STRING_ID)
		return false;

	auto entry = values_mcn.find(key_sid);
	if(entry == end(values_mcn))
		return false;

	value = EvaluableNode::ToNumber(entry->second);
	return !FastIsNaN(value);
};

static inline bool GetValueFromWeightsIndex(EvaluableNode::AssocType &values_mcn,
	size_t index, std::string &value)
{
	auto key_sid = EvaluableNode::NumberToStringIDIfExists(index, true);
	if(key_sid == string_intern_pool.NOT_A_STRING_ID)
		return false;

	auto entry = values_mcn.find(key_sid);
	if(entry == end(values_mcn))
		return false;

	value = Parser::UnparseToKeyString(entry->second);
	return true;
};

EvaluableNodeReference Interpreter::InterpretNode_ENT_MODE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto values = InterpretNode(ocn[0]);
	if(EvaluableNode::IsNull(values) || values->IsImmediate())
		return values;

	EvaluableNodeReference weights = EvaluableNodeReference::Null();
	if(ocn.size() > 1)
	{
		auto node_stack = CreateOpcodeStackStateSaver(values);
		weights = InterpretNode(ocn[1]);
	}

	bool found = false;
	std::string unparsed_result;
	if(values->IsAssociativeArray())
	{
		auto &values_mcn = values->GetMappedChildNodesReference();

		if(EvaluableNode::IsNull(weights))
		{
			std::tie(found, unparsed_result) = ModeString(begin(values_mcn), end(values_mcn),
				[](auto iter, auto &value) { return GetValueFromIter(iter, value);},
				false, [](auto iter, auto &value) { return false;});
		}
		else if(weights->IsAssociativeArray())
		{
			auto &weights_mcn = weights->GetMappedChildNodesReference();

			std::tie(found, unparsed_result) = ModeString(begin(weights_mcn), end(weights_mcn),
				[&values_mcn](auto iter, auto &value) { return GetValueFromWeightsIter(values_mcn, iter, value); },
				true, [](auto iter, auto &value) {	return GetValueFromIter(iter, value); });

		}
		else //weights->IsOrderedArray())
		{
			auto &weights_ocn = weights->GetOrderedChildNodesReference();

			std::tie(found, unparsed_result) = ModeString(size_t{ 0 }, weights_ocn.size(),
				[&values_mcn](auto i, auto &value) { return GetValueFromWeightsIndex(values_mcn, i, value); },
				true, [&weights_ocn](auto i, auto &value) { return GetValueFromIndex(weights_ocn, i, value); });
		}
	}
	else //values->IsOrderedArray())
	{
		auto &values_ocn = values->GetOrderedChildNodesReference();

		if(EvaluableNode::IsNull(weights))
		{
			std::tie(found, unparsed_result) = ModeString(size_t{ 0 }, values_ocn.size(),
				[&values_ocn](auto i, auto &value) { return GetValueFromIndex(values_ocn, i, value); },
				false, [](auto iter, auto &value) { return false;});
		}
		else if(weights->IsAssociativeArray())
		{
			auto &weights_mcn = weights->GetMappedChildNodesReference();

			std::tie(found, unparsed_result) = ModeString(begin(weights_mcn), end(weights_mcn),
				[&values_ocn](auto iter, auto &value) { return GetValueFromWeightsIter(values_ocn, iter, value); },
				true, [](auto iter, auto &value) {	return GetValueFromIter(iter, value); });
		}
		else //weights->IsOrderedArray())
		{
			auto &weights_ocn = weights->GetOrderedChildNodesReference();

			std::tie(found, unparsed_result) = ModeString(size_t{ 0 }, weights_ocn.size(),
				[&values_ocn](auto i, auto &value) { return GetValueFromIndex(values_ocn, i, value); },
				true, [&weights_ocn](auto i, auto &value) { return GetValueFromIndex(weights_ocn, i, value); });
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(weights);
	evaluableNodeManager->FreeNodeTreeIfPossible(values);

	if(!found)
		return EvaluableNodeReference::Null();

	return Parser::ParseFromKeyString(unparsed_result, evaluableNodeManager);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_QUANTILE(EvaluableNode * en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	double quantile = InterpretNodeIntoNumberValue(ocn[1]);

	auto values = InterpretNode(ocn[0]);
	if(EvaluableNode::IsNull(values) || values->IsImmediate())
		return values;

	EvaluableNodeReference weights = EvaluableNodeReference::Null();
	if(ocn.size() > 2)
	{
		auto node_stack = CreateOpcodeStackStateSaver(values);
		weights = InterpretNode(ocn[2]);
	}

	double result = 0.0;
	if(values->IsAssociativeArray())
	{
		auto &values_mcn = values->GetMappedChildNodesReference();

		if(EvaluableNode::IsNull(weights))
		{
			result = Quantile(begin(values_mcn), end(values_mcn),
				[](auto iter, auto &value) { return GetValueFromIter(iter, value);},
				false, [](auto iter, auto &value) { return false;},
				quantile);
		}
		else if(weights->IsAssociativeArray())
		{
			auto &weights_mcn = weights->GetMappedChildNodesReference();

			result = Quantile(begin(weights_mcn), end(weights_mcn),
				[&values_mcn](auto iter, auto &value) { return GetValueFromWeightsIter(values_mcn, iter, value); },
				true, [](auto iter, auto &value) {	return GetValueFromIter(iter, value); },
				quantile);

		}
		else //weights->IsOrderedArray())
		{
			auto &weights_ocn = weights->GetOrderedChildNodesReference();

			result = Quantile(size_t{ 0 }, weights_ocn.size(),
				[&values_mcn](auto i, auto &value) { return GetValueFromWeightsIndex(values_mcn, i, value); },
				true, [&weights_ocn](auto i, auto &value) { return GetValueFromIndex(weights_ocn, i, value); },
				quantile);
		}
	}
	else //values->IsOrderedArray())
	{
		auto &values_ocn = values->GetOrderedChildNodesReference();

		if(EvaluableNode::IsNull(weights))
		{
			result = Quantile(size_t{ 0 }, values_ocn.size(),
				[&values_ocn](auto i, auto &value) { return GetValueFromIndex(values_ocn, i, value); },
				false, [](auto iter, auto &value) { return false;},
				quantile);
		}
		else if(weights->IsAssociativeArray())
		{
			auto &weights_mcn = weights->GetMappedChildNodesReference();

			result = Quantile(begin(weights_mcn), end(weights_mcn),
				[&values_ocn](auto iter, auto &value) { return GetValueFromWeightsIter(values_ocn, iter, value); },
				true, [](auto iter, auto &value) {	return GetValueFromIter(iter, value); },
				quantile);
		}
		else //weights->IsOrderedArray())
		{
			auto &weights_ocn = weights->GetOrderedChildNodesReference();

			result = Quantile(size_t{ 0 }, weights_ocn.size(),
				[&values_ocn](auto i, auto &value) { return GetValueFromIndex(values_ocn, i, value); },
				true, [&weights_ocn](auto i, auto &value) { return GetValueFromIndex(weights_ocn, i, value); },
				quantile);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(weights);
	evaluableNodeManager->FreeNodeTreeIfPossible(values);

	return AllocReturn(result, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GENERALIZED_MEAN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	double p = 1.0;
	if(ocn.size() > 1)
	{
		p = InterpretNodeIntoNumberValue(ocn[1]);
		if(FastIsNaN(p))
			p = 1.0;
	}

	double center = 0.0;
	if(ocn.size() > 3)
	{
		center = InterpretNodeIntoNumberValue(ocn[3]);
		if(FastIsNaN(center))
			center = 0.0;
	}

	bool calculate_moment = false;
	if(ocn.size() > 4)
		calculate_moment = InterpretNodeIntoBoolValue(ocn[4], false);

	bool absolute_value = false;
	if(ocn.size() > 5)
		absolute_value = InterpretNodeIntoBoolValue(ocn[5], false);

	auto values = InterpretNode(ocn[0]);
	if(EvaluableNode::IsNull(values) || values->IsImmediate())
		return values;

	EvaluableNodeReference weights = EvaluableNodeReference::Null();
	if(ocn.size() > 2)
	{
		auto node_stack = CreateOpcodeStackStateSaver(values);
		weights = InterpretNode(ocn[2]);
	}

	double result = 0.0;
	if(values->IsAssociativeArray())
	{
		auto &values_mcn = values->GetMappedChildNodesReference();

		if(EvaluableNode::IsNull(weights) || weights->IsImmediate())
		{
			result = GeneralizedMean(begin(values_mcn), end(values_mcn),
				[](auto iter, auto &value) { return GetValueFromIter(iter, value);},
				false, [](auto iter, auto &value) { return false;},
				p, center, calculate_moment, absolute_value);
		}
		else if(weights->IsAssociativeArray())
		{
			auto &weights_mcn = weights->GetMappedChildNodesReference();

			result = GeneralizedMean(begin(weights_mcn), end(weights_mcn),
				[&values_mcn](auto iter, auto &value) { return GetValueFromWeightsIter(values_mcn, iter, value); },
				true, [](auto iter, auto &value) {	return GetValueFromIter(iter, value); },
				p, center, calculate_moment, absolute_value);

		}
		else //weights->IsOrderedArray())
		{
			auto &weights_ocn = weights->GetOrderedChildNodesReference();

			result = GeneralizedMean(size_t{0}, weights_ocn.size(),
				[&values_mcn](auto i, auto &value) { return GetValueFromWeightsIndex(values_mcn, i, value); },
				true, [&weights_ocn](auto i, auto &value) { return GetValueFromIndex(weights_ocn, i, value); },
				p, center, calculate_moment, absolute_value);
		}
	}
	else //values->IsOrderedArray())
	{
		auto &values_ocn = values->GetOrderedChildNodesReference();

		if(EvaluableNode::IsNull(weights) || weights->IsImmediate())
		{
			result = GeneralizedMean(size_t{ 0 }, values_ocn.size(),
				[&values_ocn](auto i, auto &value) { return GetValueFromIndex(values_ocn, i, value); },
				false, [](auto iter, auto &value) { return false;},
				p, center, calculate_moment, absolute_value);
		}
		else if(weights->IsAssociativeArray())
		{
			auto &weights_mcn = weights->GetMappedChildNodesReference();

			result = GeneralizedMean(begin(weights_mcn), end(weights_mcn),
				[&values_ocn](auto iter, auto &value) { return GetValueFromWeightsIter(values_ocn, iter, value); },
				true, [](auto iter, auto &value) {	return GetValueFromIter(iter, value); },
				p, center, calculate_moment, absolute_value);
		}
		else //weights->IsOrderedArray())
		{
			auto &weights_ocn = weights->GetOrderedChildNodesReference();
			
			result = GeneralizedMean(size_t{0}, weights_ocn.size(),
				[&values_ocn](auto i, auto &value) { return GetValueFromIndex(values_ocn, i, value); },
				true, [&weights_ocn](auto i, auto &value) { return GetValueFromIndex(weights_ocn, i, value); },
				p, center, calculate_moment, absolute_value);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(weights);
	evaluableNodeManager->FreeNodeTreeIfPossible(values);

	return AllocReturn(result, immediate_result);
}

//builds a vector of the values in the node, using ordered or mapped child nodes as appropriate
// if node is mapped child nodes, it will use id_order to order populate out and use default_value if any given id is not found
static inline void GetChildNodesAsENImmediateValueArray(EvaluableNode *node, std::vector<StringInternPool::StringID> &id_order,
	std::vector<EvaluableNodeImmediateValue> &out, std::vector<EvaluableNodeImmediateValueType> &out_types)
{
	if(node != nullptr)
	{
		if(node->IsAssociativeArray())
		{
			auto &wn_mcn = node->GetMappedChildNodesReference();
			out.resize(id_order.size());
			out_types.resize(id_order.size());
			for(size_t i = 0; i < id_order.size(); i++)
			{
				auto found_node = wn_mcn.find(id_order[i]);
				if(found_node != end(wn_mcn))
				{
					out_types[i] = out[i].CopyValueFromEvaluableNode(found_node->second);
				}
				else //not found, use default
				{
					out[i] = EvaluableNodeImmediateValue(0.0);
					out_types[i] = ENIVT_NUMBER;
				}
			}
		}
		else if(node->IsImmediate())
		{
			//fill in with the node's value
			EvaluableNodeImmediateValue value;
			EvaluableNodeImmediateValueType value_type = value.CopyValueFromEvaluableNode(node);
			out.assign(id_order.size(), value);
			out_types.assign(id_order.size(), value_type);
		}
		else //must be ordered
		{
			auto &node_ocn = node->GetOrderedChildNodesReference();

			out.resize(node_ocn.size());
			out_types.resize(node_ocn.size());
			for(size_t i = 0; i < node_ocn.size(); i++)
				out_types[i] = out[i].CopyValueFromEvaluableNode(node_ocn[i]);
		}
	}
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GENERALIZED_DISTANCE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto node_stack = CreateOpcodeStackStateSaver();

	//get location
	auto location_node = InterpretNodeForImmediateUse(ocn[0]);
	if(location_node != nullptr)
		node_stack.PushEvaluableNode(location_node);

	//get origin if applicable
	EvaluableNodeReference origin_node = EvaluableNodeReference::Null();
	if(ocn.size() > 1)
	{
		origin_node = InterpretNodeForImmediateUse(ocn[1]);
		if(origin_node != nullptr)
			node_stack.PushEvaluableNode(origin_node);
	}

	GeneralizedDistanceEvaluator dist_eval;
	dist_eval.pValue = 1;
	if(ocn.size() > 2)
	{
		double val = InterpretNodeIntoNumberValue(ocn[2]);
		if(!FastIsNaN(val))
			dist_eval.pValue = val;
	}

	//get weights list if applicable
	EvaluableNodeReference weights_node;
	if(ocn.size() > 3)
	{
		weights_node = InterpretNodeForImmediateUse(ocn[3]);
		if(weights_node != nullptr)
			node_stack.PushEvaluableNode(weights_node);
	}

	//get distance types if applicable
	EvaluableNodeReference distance_types_node;
	if(ocn.size() > 4)
	{
		distance_types_node = InterpretNodeForImmediateUse(ocn[4]);
		if(distance_types_node != nullptr)
			node_stack.PushEvaluableNode(distance_types_node);
	}

	//get feature attributes if applicable
	EvaluableNodeReference attributes_node;
	if(ocn.size() > 5)
	{
		attributes_node = InterpretNodeForImmediateUse(ocn[5]);
		if(attributes_node != nullptr)
			node_stack.PushEvaluableNode(attributes_node);
	}

	//get deviations if applicable
	EvaluableNodeReference deviations_node;
	if(ocn.size() > 6)
	{
		deviations_node = InterpretNodeForImmediateUse(ocn[6]);
		if(deviations_node != nullptr)
			node_stack.PushEvaluableNode(deviations_node);
	}

	//get value_names if applicable
	std::vector<StringInternPool::StringID> value_names;
	if(ocn.size() > 7)
	{
		EvaluableNodeReference value_names_node = InterpretNodeForImmediateUse(ocn[7]);
		if(!EvaluableNode::IsNull(value_names_node))
		{
			//extract the names for each value into value_names
			auto &vnn_ocn = value_names_node->GetOrderedChildNodes();
			value_names.reserve(vnn_ocn.size());
			for(auto &vn : vnn_ocn)
			{
				StringInternPool::StringID label_sid = EvaluableNode::ToStringIDIfExists(vn);
				if(label_sid != string_intern_pool.NOT_A_STRING_ID)
					value_names.push_back(label_sid);
			}
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(value_names_node);
	}

	EvaluableNodeReference weights_selection_features_node = EvaluableNodeReference::Null();
	if(ocn.size() > 8)
	{
		weights_selection_features_node = InterpretNodeForImmediateUse(ocn[8]);
		if(weights_selection_features_node != nullptr)
			node_stack.PushEvaluableNode(weights_selection_features_node);
	}

	dist_eval.computeSurprisal = false;
	if(ocn.size() > 9)
		dist_eval.computeSurprisal = InterpretNodeIntoBoolValue(ocn[9], false);

	//get the origin and destination
	std::vector<EvaluableNodeImmediateValue> location;
	std::vector<EvaluableNodeImmediateValueType> location_types;
	GetChildNodesAsENImmediateValueArray(location_node, value_names, location, location_types);

	std::vector<EvaluableNodeImmediateValue> origin;
	std::vector<EvaluableNodeImmediateValueType> origin_types;
	GetChildNodesAsENImmediateValueArray(origin_node, value_names, origin, origin_types);

	//resize everything to the proper number of elements, fill in with zeros
	size_t num_elements = std::max(std::max(location.size(), origin.size()), value_names.size());
	location.resize(num_elements, 0.0);
	location_types.resize(num_elements, ENIVT_NUMBER);
	origin.resize(num_elements, 0.0);
	origin_types.resize(num_elements, ENIVT_NUMBER);

	EntityQueryBuilder::PopulateDistanceFeatureParameters(dist_eval, num_elements, value_names,
		weights_node, weights_selection_features_node, distance_types_node, attributes_node, deviations_node);

	//done with all values
	evaluableNodeManager->FreeNodeTreeIfPossible(weights_node);
	evaluableNodeManager->FreeNodeTreeIfPossible(distance_types_node);
	evaluableNodeManager->FreeNodeTreeIfPossible(attributes_node);
	evaluableNodeManager->FreeNodeTreeIfPossible(deviations_node);
	evaluableNodeManager->FreeNodeTreeIfPossible(weights_selection_features_node);

	//convert unknown differences into unknown distance terms
	for(size_t i = 0; i < num_elements; i++)
	{
		auto &feature_attribs = dist_eval.featureAttribs[i];

		//if one is nan and the other is not, the use the non-nan one for both
		if(FastIsNaN(feature_attribs.unknownToUnknownDistanceTerm.deviation))
		{
			if(!FastIsNaN(feature_attribs.knownToUnknownDistanceTerm.deviation))
				feature_attribs.unknownToUnknownDistanceTerm.deviation = feature_attribs.knownToUnknownDistanceTerm.deviation;
			else
				feature_attribs.unknownToUnknownDistanceTerm.deviation = dist_eval.GetMaximumDifference(i);
		}

		if(FastIsNaN(feature_attribs.knownToUnknownDistanceTerm.deviation))
			feature_attribs.knownToUnknownDistanceTerm.deviation = feature_attribs.unknownToUnknownDistanceTerm.deviation;
	}

	dist_eval.highAccuracyDistances = true;
	dist_eval.recomputeAccurateDistances = false;
	dist_eval.InitializeParametersAndFeatureParams();
	
	double value = dist_eval.ComputeMinkowskiDistance(location, location_types, origin, origin_types, true);
	evaluableNodeManager->FreeNodeTreeIfPossible(location_node);
	evaluableNodeManager->FreeNodeTreeIfPossible(origin_node);
	return AllocReturn(value, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ENTROPY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return AllocReturn(0.0, immediate_result);

	//get first list of probabilities, p
	bool p_is_constant = false;
	double p_constant_value = 0.0;

	bool p_is_assoc = false;
	size_t p_num_elements = std::numeric_limits<size_t>::max();

	//if the evaluable node for p is a list, then p_values will reference its list,
	// otherwise if it is an assoc array, it will populate p_copied_values and have p_values point to it
	std::vector<EvaluableNode *> *p_values = nullptr;
	std::vector<EvaluableNode *> p_copied_values;

	auto p_node = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(p_node);

	if(EvaluableNode::IsAssociativeArray(p_node))
	{
		auto &p_node_mcn = p_node->GetMappedChildNodesReference();
		p_is_assoc = true;
		p_num_elements = p_node_mcn.size();

		p_values = &p_copied_values;
		p_copied_values.reserve(p_num_elements);
		for(auto &[_, ce] : p_node_mcn)
			p_copied_values.push_back(ce);
	}
	else if(EvaluableNode::IsOrderedArray(p_node))
	{
		auto &p_node_ocn = p_node->GetOrderedChildNodesReference();
		p_num_elements = p_node_ocn.size();
		p_values = &p_node_ocn;
	}
	else //not an assoc or list, so treat as a constant probability instead
	{
		p_is_constant = true;
		p_constant_value = EvaluableNode::ToNumber(p_node);
	}

	//exponents are affected if we have two distributions specified
	bool have_q_distribution = false;

	//get second list of probabilities q
	bool q_is_constant = false;
	double q_constant_value = 0.0;

	size_t q_num_elements = std::numeric_limits<size_t>::max();

	//if the evaluable node for q is a list, then q_values will reference its list,
	// otherwise if it is an assoc array, it will populate q_copied_values and have q_values point to it
	std::vector<EvaluableNode *> *q_values = nullptr;
	std::vector<EvaluableNode *> q_copied_values;

	auto q_node = EvaluableNodeReference::Null();
	if(ocn.size() >= 2)
	{
		//comparison so use positive sign
		have_q_distribution = true;
		q_node = InterpretNodeForImmediateUse(ocn[1]);
		node_stack.PushEvaluableNode(q_node);
		
		if(EvaluableNode::IsAssociativeArray(q_node))
		{
			q_num_elements = q_node->GetMappedChildNodesReference().size();

			q_values = &q_copied_values;

			//because p is the parameter in front and if it is 0, then none of the rest of the term matters,
			// we should use p's index list to populate q's values
			if(p_is_assoc)
			{
				q_copied_values.reserve(p_num_elements);
				for(auto &[pce_id, _] : p_node->GetMappedChildNodesReference())
				{
					auto q_i = q_node->GetMappedChildNodesReference().find(pce_id);
					if(q_i == end(q_node->GetMappedChildNodesReference()))
						continue;
					q_copied_values.push_back(q_i->second);
				}
			}
			else if(p_is_constant)
			{
				q_copied_values.reserve(q_num_elements);
				for(auto &[_, ce] : q_node->GetMappedChildNodesReference())
					q_copied_values.push_back(ce);
			}
			else //p must be a list
			{
				q_copied_values.reserve(p_num_elements);
				for(size_t index = 0; index < p_num_elements; index++)
				{
					StringInternPool::StringID key_sid = EvaluableNode::ToStringIDIfExists((*p_values)[index], true);

					EvaluableNode **found = q_node->GetMappedChildNode(key_sid);
					if(found != nullptr)
						q_copied_values.push_back(*found);
				}
			}
		}
		else if(EvaluableNode::IsOrderedArray(q_node))
		{
			q_values = &q_node->GetOrderedChildNodesReference();
			q_num_elements = q_values->size();
		}
		else //not an assoc or list, so treat as a constant probability instead
		{
			q_is_constant = true;
			q_constant_value = EvaluableNode::ToNumber(q_node);
		}
	}

	//if both are constants, then have no entropy (no probability mass), so return 0
	if((p_is_constant || p_num_elements == std::numeric_limits<size_t>::max())
		&& (q_is_constant || q_num_elements == std::numeric_limits<size_t>::max()))
		return AllocReturn(0.0, immediate_result);

	//now that have the size of both p and q, can compute constant values if applicable
	//if p_node is null then compute a constant value
	if(EvaluableNode::IsNull(p_node))
	{
		p_is_constant = true;
		p_constant_value = 1.0 / q_num_elements;
	}

	//if p_node is null then compute a constant value
	if(EvaluableNode::IsNull(q_node))
	{
		q_is_constant = true;
		q_constant_value = 1.0 / p_num_elements;
	}

	//get optional exponent parameters
	double p_exponent = 1;
	//if have a second distribution, then default to kl divergence, with each term q_i/p_i
	if(have_q_distribution)
		p_exponent = -1;
	if(ocn.size() >= 3)
		p_exponent = InterpretNodeIntoNumberValue(ocn[2]);

	//if exponent is 0, then all values will be 1
	if(p_exponent == 0)
	{
		p_is_constant = true;
		p_constant_value = 1;
	}

	double q_exponent = 0;
	//default to KL divergence with each term of q_i/p_i
	if(ocn.size() >= 2)
		q_exponent = 1;
	//override if specified
	if(ocn.size() >= 4)
		q_exponent = InterpretNodeIntoNumberValue(ocn[3]);

	//if exponent is 0, then all values will be 1
	if(q_exponent == 0)
	{
		q_is_constant = true;
		q_constant_value = 1;
	}

	//finally can compute entropy
	size_t num_elements = std::min(p_num_elements, q_num_elements);
	double accumulated_entropy = 0.0;

	for(size_t i = 0; i < num_elements; i++)
	{
		//get the original p_i value to multiply out in front
		double p_i_first_term;
		if(p_is_constant)
			p_i_first_term = p_constant_value;
		else
			p_i_first_term = EvaluableNode::ToNumber((*p_values)[i]);

		//in entropy calculations, always exit early if p_i is 0 even if the subsequent terms blow up
		if(p_i_first_term <= 0)
			continue;

		//exponentiate p_i if applicable (note that exponent of 0 is covered earlier in the code)
		double p_i_exponentiated = p_i_first_term;
		if(p_exponent == -1)
			p_i_exponentiated = 1 / p_i_exponentiated;
		else if(p_exponent != 1)
			p_i_exponentiated = std::pow(p_i_exponentiated, p_exponent);

		double q_i;
		if(q_is_constant)
			q_i = q_constant_value;
		else
			q_i = EvaluableNode::ToNumber((*q_values)[i]);

		//exponentiate q_i if applicable (note that exponent of 0 is covered earlier in the code)
		if(q_exponent == 0)
			q_i = 1;
		else if(q_exponent == -1)
			q_i = 1 / q_i;
		else if(q_exponent != 1)
			q_i = std::pow(q_i, q_exponent);

		accumulated_entropy += p_i_first_term * std::log(p_i_exponentiated * q_i);
	}

	node_stack.PopEvaluableNode();

	//negate
	accumulated_entropy = -accumulated_entropy;

	//in rare cases where the values in either p or q may not add up exactly to 1 due to floating point precision, and where the values in q
	//are larger than the values in p, the resulting value may wind up being a tiny negative, but since information gain cannot be negative,
	//we take the max of the result and 0
	accumulated_entropy = std::max(0.0, accumulated_entropy);
	evaluableNodeManager->FreeNodeTreeIfPossible(p_node);
	evaluableNodeManager->FreeNodeTreeIfPossible(q_node);
	return AllocReturn(accumulated_entropy, immediate_result);
}
