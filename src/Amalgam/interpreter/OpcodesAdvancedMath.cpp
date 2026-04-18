//project headers:
#include "EntityQueryBuilder.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Advanced Math";

static OpcodeInitializer _ENT_EXPONENT(ENT_EXPONENT, &Interpreter::InterpretNode_ENT_EXPONENT, []() {
	OpcodeDetails d;
	d.parameters = R"(number x)";
	d.returns = R"(number)";
	d.description = R"(e^x)";
	d.examples = MakeAmalgamExamples({
		{R"((exp 0.5))", R"(1.6487212707001282)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_EXPONENT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::exp(value);	});
}

static OpcodeInitializer _ENT_LOG(ENT_LOG, &Interpreter::InterpretNode_ENT_LOG, []() {
	OpcodeDetails d;
	d.parameters = R"(number x [number base])";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the logarithm of `x`.  If `base` is specified, uses that base, otherwise defaults to natural log.)";
	d.examples = MakeAmalgamExamples({
		{R"((log 0.5))", R"(-0.6931471805599453)"},
		{R"((log 0.5 2))", R"(-1)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

static OpcodeInitializer _ENT_ERF(ENT_ERF, &Interpreter::InterpretNode_ENT_ERF, []() {
	OpcodeDetails d;
	d.parameters = R"(number errno)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the error function on `errno`.)";
	d.examples = MakeAmalgamExamples({
		{R"((erf 0.5))", R"(0.5204998778130465)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ERF(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::erf(value);	});
}

static OpcodeInitializer _ENT_TGAMMA(ENT_TGAMMA, &Interpreter::InterpretNode_ENT_TGAMMA, []() {
	OpcodeDetails d;
	d.parameters = R"(number z)";
	d.returns = R"(number)";
	d.description = R"(Evaluates the true (complete) gamma function on `z`.)";
	d.examples = MakeAmalgamExamples({
		{R"((tgamma 0.5))", R"(1.772453850905516)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TGAMMA(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::tgamma(value);	});
}

static OpcodeInitializer _ENT_LGAMMA(ENT_LGAMMA, &Interpreter::InterpretNode_ENT_LGAMMA, []() {
	OpcodeDetails d;
	d.parameters = R"(number z)";
	d.returns = R"(number)";
	d.description = R"(Evaluates the log-gamma function function on `z`.)";
	d.examples = MakeAmalgamExamples({
		{R"((lgamma 0.5))", R"(0.5723649429247001)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_LGAMMA(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::lgamma(value);	});
}

static OpcodeInitializer _ENT_SQRT(ENT_SQRT, &Interpreter::InterpretNode_ENT_SQRT, []() {
	OpcodeDetails d;
	d.parameters = R"(number x)";
	d.returns = R"(number)";
	d.description = R"(Returns the square root of `x`.)";
	d.examples = MakeAmalgamExamples({
		{R"((sqrt 0.5))", R"(0.7071067811865476)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SQRT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::sqrt(value);	});
}

static OpcodeInitializer _ENT_POW(ENT_POW, &Interpreter::InterpretNode_ENT_POW, []() {
	OpcodeDetails d;
	d.parameters = R"(number base number exponent)";
	d.returns = R"(number)";
	d.description = R"(Returns `base` raised to the `exponent` power.)";
	d.examples = MakeAmalgamExamples({
		{R"((pow 0.5 2))", R"(0.25)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_POW(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	double f1 = InterpretNodeIntoNumberValue(ocn[0]);
	double f2 = InterpretNodeIntoNumberValue(ocn[1]);
	return AllocReturn(std::pow(f1, f2), immediate_result);
}

static OpcodeInitializer _ENT_DOT_PRODUCT(ENT_DOT_PRODUCT, &Interpreter::InterpretNode_ENT_DOT_PRODUCT, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc x1 list|assoc x2)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the sum of all corresponding element-wise products of `x1` and `x2`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((dot_product
	[0.5 0.25 0.25]
	[4 8 8]
))&", R"(6)"},
			{R"&((dot_product
	(associate "a" 0.5 "b" 0.25 "c" 0.25)
	(associate "a" 4 "b" 8 "c" 8)
))&", R"(6)"},
			{R"&((dot_product
	(associate 0 0.5 1 0.25 2 0.25)
	[4 8 8]
))&", R"(6)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

static OpcodeInitializer _ENT_NORMALIZE(ENT_NORMALIZE, &Interpreter::InterpretNode_ENT_NORMALIZE, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc values [number p])";
	d.returns = R"(list|assoc)";
	d.description = R"(Evaluates to a container of the values with the elements normalized, where `p` represents the order of the Lebesgue space to normalize the vector (e.g., 1 is Manhattan or surprisal space, 2 is Euclidean) and defaults to 1.)";
	d.examples = MakeAmalgamExamples({
		{R"&((normalize
	[0.5 0.5 0.5 0.5]
))&", R"([0.25 0.25 0.25 0.25])"},
			{R"&((normalize
	[0.5 0.5 0.5 .infinity]
))&", R"([0 0 0 1])"},
			{R"&((normalize
	{
		a 1
		b 1
		c 1
		d 1
	}
	2
))&", R"({
	a 0.5
	b 0.5
	c 0.5
	d 0.5
})"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

static OpcodeInitializer _ENT_MODE(ENT_MODE, &Interpreter::InterpretNode_ENT_MODE, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc values [list|assoc weights])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to mode of the `values`.  If `values` is an assoc, it will return the key.  If `weights` is specified and both `values` and `weights` are lists, then the corresponding elements will be weighted by `weights`.  If weights is specified and is an assoc, then each value will be looked up in the `weights`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((mode
	[1 1 2 3 4 5]
))&", R"(1)"},
			{R"&((mode
	[
		1
		1
		2
		3
		4
		5
		5
		5
	]
))&", R"(5)"},
			{R"&((mode
	[
		1
		1
		[]
		[]
		[]
		{}
		{}
	]
))&", R"([])"},
			{R"&((mode
	[
		1
		1
		2
		3
		4
		5
		.null
	]
))&", R"(1)"},
			{R"&((mode
	[1 1 2 3 4 5]
))&", R"(1)"},
			{R"&((mode
	[1 1 2 3 4 5]
	[0.5 0.1 0.1 0.1 0.1]
))&", R"(1)"},
			{R"&((mode
	{
		a 1
		b 1
		c 3
		d 4
		e 5
	}
	{
		a 0.5
		b 0.1
		c 0.1
		d 0.1
		e 0.1
	}
))&", R"(1)"},
			{R"&((mode
	[1 1 2 3 4 5]
	{0 0.75 4 0.125}
))&", R"(1)"},
			{R"&((mode
	{
		0 1
		1 1
		2 2
		3 3
		4 4
		5 5
	}
	[0.75 0 0 0 0.125]
))&", R"(1)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

static OpcodeInitializer _ENT_QUANTILE(ENT_QUANTILE, &Interpreter::InterpretNode_ENT_QUANTILE, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc values number quantile [list|assoc weights])";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the quantile of the `values` specified by `quantile` ranging from 0 to 1.  If `weights` is specified and both `values` and `weights` are lists, then the corresponding elements will be weighted by `weights`.  If `weights` is specified and is an assoc, then each value will be looked up in the `weights`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((quantile
	[1 2 3 4 5]
	0.5
))&", R"(3)"},
			{R"&((quantile
	[1 2 3 4 5 .null]
	0.5
))&", R"(3)"},
			{R"&((quantile
	[1 2 3 4 5]
	0.5
))&", R"(3)"},
			{R"&((quantile
	[1 2 3 4 5]
	0.5
	[0.5 0.1 0.1 0.1 0.1]
))&", R"(1.6666666666666667)"},
			{R"&((quantile
	{
		a 1
		b 2
		c 3
		d 4
		e 5
	}
	0.5
	{
		a 0.5
		b 0.1
		c 0.1
		d 0.1
		e 0.1
	}
))&", R"(1.6666666666666667)"},
			{R"&((quantile
	[1 2 3 4 5]
	0.5
	{0 0.75 4 0.125}
))&", R"(1.5714285714285716)"},
			{R"&((quantile
	{
		0 1
		1 2
		2 3
		3 4
		4 5
		5 .null
	}
	0.5
	[0.75 0 0 0 0.125]
))&", R"(1.1666666666666667)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_QUANTILE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
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

static OpcodeInitializer _ENT_GENERALIZED_MEAN(ENT_GENERALIZED_MEAN, &Interpreter::InterpretNode_ENT_GENERALIZED_MEAN, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc values [number p] [list|assoc weights] [number center] [bool calculate_moment] [bool absolute_value])";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the generalized mean of the `values`.  If `p` is specified (which defaults to 1), it is the parameter that can control the type of mean from minimum (negative infinity) to harmonic mean (-1) to geometric mean (0) to arithmetic mean (1) to maximum (infinity).  If `weights` are specified, it uses those when calculating the corresponding values for the generalized mean.  If `center` is specified, calculations will use that as central point, and the default center is is 0.0.  If `calculate_moment` is true, which defaults to false, then the results will not be raised to 1/`p` at the end.  If `absolute_value` is true, which defaults to false, the differences will take the absolute value.  Various parameterizations of generalized_mean can be used to compute moments about the mean, especially setting the calculate_moment parameter to true and using the mean as the center.)";
	d.examples = MakeAmalgamExamples({
		{R"&((generalized_mean
	[1 2 3 4 5]
))&", R"(3)"},
			{R"&((generalized_mean
	[1 2 3 4 5 .null]
))&", R"(3)"},
			{R"&((generalized_mean
	[1 2 3 4 5]
	2
))&", R"(3.3166247903554)"},
			{R"&((generalized_mean
	[1 2 3 4 5]
	1
	[0.5 0.1 0.1 0.1 0.1]
))&", R"(2.111111111111111)", R"(2.1111111111111\d\d)"},
			{R"&((generalized_mean
	{
		a 1
		b 2
		c 3
		d 4
		e 5
	}
	1
	{
		a 0.5
		b 0.1
		c 0.1
		d 0.1
		e 0.1
	}
))&", R"(2.111111111111111)", R"(2.1111111111111\d\d)"},
			{R"&((generalized_mean
	[1 2 3 4 5]
	1
	{0 0.75 4 0.125}
))&", R"(1.5714285714285714)"},
			{R"&((generalized_mean
	{
		0 1
		1 2
		2 3
		3 4
		4 5
		5 .null
	}
	1
	[0.75 0 0 0 0.125]
))&", R"(1.5714285714285714)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 2.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

			result = GeneralizedMean(size_t{ 0 }, weights_ocn.size(),
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

			result = GeneralizedMean(size_t{ 0 }, weights_ocn.size(),
				[&values_ocn](auto i, auto &value) { return GetValueFromIndex(values_ocn, i, value); },
				true, [&weights_ocn](auto i, auto &value) { return GetValueFromIndex(weights_ocn, i, value); },
				p, center, calculate_moment, absolute_value);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(weights);
	evaluableNodeManager->FreeNodeTreeIfPossible(values);

	return AllocReturn(result, immediate_result);
}

static OpcodeInitializer _ENT_GENERALIZED_DISTANCE(ENT_GENERALIZED_DISTANCE, &Interpreter::InterpretNode_ENT_GENERALIZED_DISTANCE, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc|* vector1 [list|assoc|* vector2] [number p_value] [list|assoc|assoc of assoc|number weights] [list|assoc attributes] [list|assoc|number deviations] [list value_names] [list|string weights_selection_features] [bool surprisal_space])";
	d.returns = R"(number)";
	d.description = R"(Computes the generalized norm between `vector1` and `vector2` (or an equivalent zero vector if unspecified) using the numerical distance or edit distance as appropriate.  The parameter `value_names`, if specified as a list of the names of the values, will transform via unzipping any assoc into a list for the respective parameter in the order of the `value_names`, or if a number will use the number repeatedly for every element.  If any vector value is null or any of the differences between `vector1` and `vector2` evaluate to null, then it will compute a corresponding maximum distance value based on the properties of the feature.  If `surprisal_space` is true, which defaults to false, it will perform all computations in surprisal space.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.)";
	d.examples = MakeAmalgamExamples({
		{R"&((generalized_distance
	(map
		10000
		(range 0 200)
	)
	.null
	0.01
))&", R"(2.0874003024080013e+234)"},
			{R"&((generalized_distance
	[1 2 3]
	[0 2 3]
	0.01
))&", R"(1)"},
			{R"&((generalized_distance
	[3 4]
	.null
	2
))&", R"(5)"},
			{R"&((generalized_distance
	[3 4]
	.null
	-.infinity
))&", R"(3)"},
			{R"&((generalized_distance
	[1 2 3]
	[0 2 3]
	0.01
	[0.3333 0.3333 0.3333]
))&", R"(1.9210176984148622e-48)"},
			{R"&((generalized_distance
	[3 4]
	.null
	2
	[1 1]
))&", R"(5)"},
			{R"&((generalized_distance
	[3 4]
	.null
	2
	[0.5 0.5]
))&", R"(3.5355339059327378)"},
			{R"&((generalized_distance
	[3 4]
	.null
	1
	[0.5 0.5]
))&", R"(3.5)"},
			{R"&((generalized_distance
	[3 4]
	.null
	0.5
	[0.5 0.5]
))&", R"(3.482050807568877)"},
			{R"&((generalized_distance
	[3 4]
	.null
	0.1
	[0.5 0.5]
))&", R"(3.467687001077147)"},
			{R"&((generalized_distance
	[3 4]
	.null
	0.01
	[0.5 0.5]
))&", R"(3.4644599990846436)"},
			{R"&((generalized_distance
	[3 4]
	.null
	0.001
	[0.5 0.5]
))&", R"(3.4641374518767565)"},
			{R"&((generalized_distance
	[3 4]
	.null
	0
	[0.5 0.5]
))&", R"(3.4641016151377544)"},
			{R"&((generalized_distance
	[.null 4]
	.null
	2
	[1 1]
))&", R"(.infinity)"},
			{R"&((generalized_distance
	[.null 4]
	.null
	0
	[1 1]
))&", R"(.infinity)"},
			{R"&((generalized_distance
	[.null 4]
	.null
	2
	[0.5 0.5]
))&", R"(.infinity)"},
			{R"&((generalized_distance
	[.null 4]
	.null
	0
	[0.5 0.5]
))&", R"(.infinity)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 4]
	1
	.null
	[{difference_type "nominal" data_type "number" nominal_count 1}]
))&", R"(2)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	.null
	[{difference_type "nominal" data_type "number" nominal_count 1}]
))&", R"(8)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	.null
	[{difference_type "nominal" data_type "number" nominal_count 1}]
))&", R"(8)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 4]
	1
	[0.3333 0.3333 0.3333]
	[{difference_type "nominal" data_type "number" nominal_count 1}]
))&", R"(0.6666)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	[{difference_type "nominal" data_type "number" nominal_count 1}]
))&", R"(2.6664)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	[{difference_type "nominal" data_type "number" nominal_count 1}]
))&", R"(2.6664)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	[
		{difference_type "nominal" data_type "number" nominal_count 1}
		{difference_type "continous" data_type "number" cycle_range 360}
		{difference_type "continous" data_type "number" cycle_range 12}
	]
))&", R"(1.9997999999999998)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	[{difference_type "nominal" data_type "number" nominal_count 1.1}]
	[0.25 180 -12]
))&", R"(92.57407500000001)"},
			{R"&((generalized_distance
	[4 4 .null]
	[2 .null .null]
	2
	[1 0 1]
	[
		{difference_type "continous" data_type "number"}
		{difference_type "nominal" data_type "number" nominal_count 5}
		{difference_type "nominal" data_type "number" nominal_count 5}
	]
	[0.1 0.1 0.1]
))&", R"(2.227195548101088)"},
			{R"&((generalized_distance
	[4 4 .null]
	[2 .null .null]
	2
	[1 0 1]
	[
		{difference_type "continous" data_type "number"}
		{difference_type "nominal" data_type "number" nominal_count 5}
		{difference_type "nominal" data_type "number" nominal_count 5}
	]
))&", R"(2.23606797749979)"},
			{R"&((generalized_distance
	[4 4 .null 4]
	[2 .null .null 2]
	2
	[1 0 1 1]
	[
		{difference_type "continous" data_type "number"}
		{difference_type "nominal" data_type "number" nominal_count 5}
		{difference_type "nominal" data_type "number" nominal_count 5}
	]
	[0.1 0.1 0.1 0.1]
))&", R"(2.9933927271513525)"},
			{R"&((generalized_distance
	[4 4 .null 4]
	[2 .null .null 2]
	2
	[1 0 1 1]
	[
		{difference_type "continous" data_type "number"}
		{difference_type "nominal" data_type "number" nominal_count 5}
		{difference_type "nominal" data_type "number" nominal_count 5}
	]
))&", R"(3)"},
			{R"&((generalized_distance
	[4 4 4 4 4]
	[2 .null 2 2 2]
	1
	[1 0 1 1 1]
))&", R"(.null)"},
			{R"&((generalized_distance
	[4 4 4]
	[2 2 2]
	1
	{x 1 y 1 z 1}
	{x "nominal_number" y "continuous_number" z "continuous_number"}
	{z 5}
	.null
	.null
	.null
	["x" "y" "z"]
))&", R"(6)"},
			{R"&((generalized_distance
	[4 4 .null]
	[2 2 .null]
	1
	[1 1 1]
	[
		{difference_type "continous" data_type "number"}
		{difference_type "nominal" data_type "number" nominal_count 5}
		{difference_type "nominal" data_type "number" nominal_count 5}
	]
))&", R"(4)"},
			{R"&((generalized_distance
	[4 4 4 4]
	[2 2 2 .null]
	0
	[1 1 1 1]
	[
		{difference_type "continous" data_type "number"}
		{difference_type "nominal" data_type "number" nominal_count 5}
		{difference_type "nominal" data_type "number" nominal_count 5}
		{difference_type "continous" data_type "number"}
	]
	[
		[0 2]
		.null
		.null
		[0 2]
	]
))&", R"(4)"},
			{R"&((generalized_distance
	[4 "s" "s" 4]
	[2 "s" 2 .null]
	1
	[1 1 1 1]
	[
		{difference_type "continous" data_type "number"}
		{difference_type "nominal" data_type "number" nominal_count 5}
		{difference_type "nominal" data_type "number" nominal_count 5}
		{difference_type "continous" data_type "number"}
	]
	[
		[0 1]
		.null
		.null
		[0 1]
	]
))&", R"(4)"},
			{R"&((generalized_distance
	[
		[1 2 3 4 5]
		"s"
	]
	[
		[1 2 3]
		"s"
	]
	1
	[1 1]
	[
		{difference_type "continous" data_type "code"}
		{difference_type "nominal" data_type "number" nominal_count 5}
	]
))&", R"(2)"},
			{R"&((generalized_distance
	[
		[1.5 2 3 4 5]
		"s"
	]
	[
		[1 2 3]
		"s"
	]
	1
	[1 1]
	[
		{difference_type "continous" data_type "code"}
		{difference_type "nominal" data_type "number" nominal_count 5}
	]
))&", R"(3.3255881193876142)"},
			{R"&((generalized_distance
	[1 1]
	[1 1]
	1
	[1 1]
	[
		{difference_type "continous" data_type "number"}
		{difference_type "continous" data_type "number"}
	]
	[0.5 0.5]
	.null
	.null
	.true
))&", R"(0)"},
			{R"&((generalized_distance
	[1 1]
	[1 1]
	1
	[1 1]
	[
		{difference_type "nominal" data_type "number"}
		{difference_type "nominal" data_type "number"}
	]
	[0.5 0.5]
	.null
	.null
	.true
))&", R"(0)"},
			{R"&((generalized_distance
	[1 1]
	[2 2]
	1
	[1 1]
	[
		{difference_type "continous" data_type "number"}
		{difference_type "continous" data_type "number"}
	]
	[0.5 0.5]
	.null
	.null
	.true
))&", R"(1.6766764161830636)"},
			{R"&((generalized_distance
	[1 1]
	[2 2]
	1
	[1 1]
	[
		{difference_type "nominal" data_type "number" nominal_count 2}
		{difference_type "nominal" data_type "number" nominal_count 2}
	]
	[0.25 0.25]
	.null
	.null
	.true
))&", R"(2.197224577336219)"},
			{R"&((generalized_distance
	;vector1
	["b"]
	;vector2
	["c"]
	;p
	1
	;weights
	[1 1]
	;attributes
	[{difference_type "nominal" data_type "string" nominal_count 4}]
	;deviations
	[
		{
			a {a 0.00744879 b 0.996275605 c 0.996275605}
			b {a 0.501736111 b 0.501736111 c 0.996527778}
			c {a 0.996539792 b 0.996539792 c 0.006920415}
		}
	]
	;value_names
	.null
	;weights_selection_feature
	.null
	;surpisal_space
	.true
))&", R"(4.966335099422683)"},
			{R"&((generalized_distance
	;vector1
	["b"]
	;vector2
	["a"]
	;p
	1
	;weights
	[1 1]
	;attributes
	[{difference_type "nominal" data_type "string" nominal_count 4}]
	;deviations
	[
		{
			a {a 0.00744879 b 0.996275605 c 0.996275605}
			b {a 0.501736111 b 0.501736111 c 0.996527778}
			c {a 0.996539792 b 0.996539792 c 0.006920415}
		}
	]
	;value_names
	.null
	;weights_selection_feature
	.null
	;surpisal_space
	.true
))&", R"(0)"},
			{R"&((generalized_distance
	;vector1
	["b"]
	;vector2
	["q"]
	;p
	1
	;weights
	[1 1]
	;attributes
	[{difference_type "nominal" data_type "string" nominal_count 4}]
	;deviations
	[
{
			a {a 0.00744879 b 0.996275605 c 0.996275605}
			b [
					{a 0.501736111 b 0.501736111 c 0.996527778}
					0.8
				]
			c {a 0.996539792 b 0.996539792 c 0.006920415}
		}
	]
	;value_names
	.null
	;weights_selection_feature
	.null
	;surpisal_space
	.true
))&", R"(0.9128124677208268)"},
			{R"&((generalized_distance
	;vector1
	["q"]
	;vector2
	["u"]
	;p
	1
	;weights
	[1 1]
	;attributes
	[{difference_type "nominal" data_type "string" nominal_count 2}]
	;deviations
	[ 0.2 ]
	;value_names
	.null
	;weights_selection_feature
	.null
	;surpisal_space
	.true
))&", R"(1.3862943611198906)"},
			{R"&((generalized_distance
	;vector1
	["q"]
	;vector2
	["u"]
	;p
	1
	;weights
	[1 1]
	;attributes
	[{difference_type "nominal" data_type "string" nominal_count 4}]
	;deviations
	[
		[
			{
				a {a 0.00744879 b 0.996275605 c 0.996275605}
				b [
						{a 0.501736111 b 0.501736111 c 0.996527778}
						0.8
					]
				c {a 0.996539792 b 0.996539792 c 0.006920415}
			}
			0.2
		]
	]
	;value_names
	.null
	;weights_selection_feature
	.null
	;surpisal_space
	.true
))&", R"(1.3862943611198906)"},
			{R"&((generalized_distance
	;vector1
	["q"]
	;vector2
	["u"]
	;p
	1
	;weights
	[1 1]
	;attributes
	[{difference_type "nominal" data_type "string" nominal_count 4}]
	;deviations
	[
		[
			[
				{
					a {a 0.00744879 b 0.996275605 c 0.996275605}
					b [
							{a 0.501736111 b 0.501736111 c 0.996527778}
							0.8
						]
					c {a 0.996539792 b 0.996539792 c 0.006920415}
				}
				0.2
			]
			0.2
		]
	]
	;value_names
	.null
	;weights_selection_feature
	.null
	;surpisal_space
	.true
))&", R"(1.3862943611198906)"},
			{R"&((generalized_distance
	;vector1
	{
		A1 1
		A2 1
		A3 1
		B 1
	}
	;vector2
	{
		A1 2
		A2 2
		A3 2
		B 2
	}
	;p
	1
	;weights
	{
		A1 {
				A1 0
				A2 0.372145984
				A3 0.370497589
				B 0.082723928
				sum 0.174632499
			}
		A2 {
				A1 0.371518433
				A2 0
				A3 0.370520996
				B 0.082668725
				sum 0.175291846
			}
		A3 {
				A1 0.370319458
				A2 0.370968492
				A3 0
				B 0.085480882
				sum 0.173231167
			}
		B {
				A1 0.061363751
				A2 0.049512288
				A3 0.05628626
				B 0
				sum 0.832837701
			}
		sum {
				A1 0.114003407
				A2 0.106173002
				A3 0.100958636
				B 0.678864956
				sum 0
			}
	}
	;attributes
	[{difference_type "continuous" data_type "number"}]
	;deviations
	0.5
	;value_names
	["A2" "A3" "B"]
	;weights_selection_features
	"sum"
	;surprisal_space
	.true
))&", R"(0.8383382080915319)"},
			{R"&((generalized_distance
	[
		[1.5 2 3 4 5 "s12"]
	]
	[
		[1 2 3 "s21"]
	]
	1
	[1]
	[{difference_type "continuous" data_type "code"}]
))&", R"(5.325588119387614)"},
			{R"&((generalized_distance
	[
		[1.5 2 3 4 5 "s12"]
	]
	[
		[1 2 3 "s21"]
	]
	1
	[1]
	[{difference_type "continuous" data_type "code" nominal_strings .false types_must_match .false}]
))&", R"(3.697640774259515)"},
			{R"&((generalized_distance
	{
		A1 1
		A2 1
		A3 1
		B 1
	}
	
	;vector 1
	{
		A1 2
		A2 2
		A3 2
		B 2
	}
	
	;vector 2
	1
	
	;p
	{
		A1 {
				A1 0
				A2 0.372145984
				A3 0.370497589
				B 0.082723928
				sum 0.174632499
			}
		A2 {
				A1 0.371518433
				A2 0
				A3 0.370520996
				B 0.082668725
				sum 0.175291846
			}
		A3 {
				A1 0.370319458
				A2 0.370968492
				A3 0
				B 0.085480882
				sum 0.173231167
			}
		B {
				A1 0.061363751
				A2 0.049512288
				A3 0.05628626
				B 0
				sum 0.832837701
			}
		sum {
				A1 0.114003407
				A2 0.106173002
				A3 0.100958636
				B 0.678864956
				sum 0
			}
	}
	
	;weights
	["continuous_number"]
	
	;types
	.null
	
	;attributes
	0.5
	
	;deviations
	["A2" "A3"]
	
	;names
	["sum" "A1" "B"]
	
	;weights_selection_feature
	.true
))&", R"(0.8383382080915318)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

	//get feature attributes if applicable
	EvaluableNodeReference attributes_node;
	if(ocn.size() > 4)
	{
		attributes_node = InterpretNodeForImmediateUse(ocn[4]);
		if(attributes_node != nullptr)
			node_stack.PushEvaluableNode(attributes_node);
	}

	//get deviations if applicable
	EvaluableNodeReference deviations_node;
	if(ocn.size() > 5)
	{
		deviations_node = InterpretNodeForImmediateUse(ocn[5]);
		if(deviations_node != nullptr)
			node_stack.PushEvaluableNode(deviations_node);
	}

	//get value_names if applicable
	std::vector<StringInternPool::StringID> value_names;
	if(ocn.size() > 6)
	{
		EvaluableNodeReference value_names_node = InterpretNodeForImmediateUse(ocn[6]);
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
	if(ocn.size() > 7)
	{
		weights_selection_features_node = InterpretNodeForImmediateUse(ocn[7]);
		if(weights_selection_features_node != nullptr)
			node_stack.PushEvaluableNode(weights_selection_features_node);
	}

	dist_eval.computeSurprisal = false;
	if(ocn.size() > 8)
		dist_eval.computeSurprisal = InterpretNodeIntoBoolValue(ocn[8], false);

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
		weights_node, weights_selection_features_node, attributes_node, deviations_node);

	//done with all values
	evaluableNodeManager->FreeNodeTreeIfPossible(weights_node);
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

static OpcodeInitializer _ENT_ENTROPY(ENT_ENTROPY, &Interpreter::InterpretNode_ENT_ENTROPY, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc|number p [list|assoc|number q] [number p_exponent] [number q_exponent])";
	d.returns = R"(number)";
	d.description = R"(Computes a form of entropy on the specified vectors `p` and `q` using nats (natural log, not bits) in the form of -sum p_i ln (p_i^p_exponent * q_i^q_exponent).  For both `p` and `q`, if `p` or `q` is a list of numbers, then it will treat each entry as being the probability of that element.  If it is an associative array, then elements with matching keys will be matched.  If `p` or `q` is a number then it will use that value in place of each element.  If `p` or `q` is null or not specified, it will be calculated as the reciprocal of the size of the other element (p_i would be 1/|q| or q_i would be 1/|p|).  If either `p_exponent` or `q_exponent` is 0, then that exponent will be ignored.  Shannon entropy can be computed by ignoring the q parameters by specifying it as null, setting `p_exponent` to 1 and `q_exponent` to 0. KL-divergence can be computed by providing both `p` and `q` and setting `p_exponent` to -1 and `q_exponent` to 1.  Cross-entropy can be computed by setting `p_exponent` to 0 and `q_exponent` to 1.)";
	d.examples = MakeAmalgamExamples({
		{R"&((entropy
	[0.5 0.5]
))&", R"(0.6931471805599453)"},
			{R"&((entropy
	[0.5 0.5]
	[0.25 0.75]
	-1
	1
))&", R"(0.14384103622589045)"},
			{R"&((entropy
	[0.5 0.5]
	[0.25 0.75]
))&", R"(0.14384103622589045)"},
			{R"&((entropy
	0.5
	[0.25 0.75]
	-1
	1
))&", R"(0.14384103622589045)"},
			{R"&((entropy
	0.5
	[0.25 0.75]
	0
	1
))&", R"(1.6739764335716716)"},
			{R"&((entropy
	{A 0.5 B 0.5}
	{A 0.75 B 0.25}
))&", R"(0.14384103622589045)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

