#include "Interpreter.h"

struct NoValueOp
{
	using Acc = bool;

	inline Acc Init() const
	{
		return true;
	}

	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, Acc & /*acc*/) const
	{
		interpreter.InterpretNodeForImmediateUse(function,
			EvaluableNodeRequestedValueTypes::Type::NULL_VALUE);
		return true;
	}

	inline EvaluableNodeReference Finish(Acc /*acc*/) const
	{
		return EvaluableNodeReference::Null();
	}
};

struct SumOp
{
	inline double Init() const
	{
		return 0.0;
	}

	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, double &acc) const
	{
		double v = interpreter.InterpretNodeIntoNumberValue(function);
		acc += v;
		return true; //never aborts early
	}

	inline EvaluableNodeReference Finish(double acc) const
	{
		return EvaluableNodeReference(acc);
	}
};

struct ProductOp
{
	inline double Init() const
	{
		return 1.0;
	}

	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, double &acc) const
	{
		double v = interpreter.InterpretNodeIntoNumberValue(function);
		acc *= v;
		return true;
	}

	inline EvaluableNodeReference Finish(double acc) const
	{
		return EvaluableNodeReference(acc);
	}
};

struct MinOp
{
	inline double Init() const
	{
		return std::numeric_limits<double>::infinity();
	}

	bool value_found = false;

	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, double &acc)
	{
		double v = interpreter.InterpretNodeIntoNumberValue(function);
		if(!FastIsNaN(v))
		{
			value_found = true;
			acc = std::min(v, acc);
		}
		return true;
	}

	inline EvaluableNodeReference Finish(double acc) const
	{
		return value_found ? EvaluableNodeReference(acc) : EvaluableNodeReference::Null();
	}
};

struct MaxOp
{
	inline double Init() const
	{
		return -std::numeric_limits<double>::infinity();
	}

	bool value_found = false;

	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, double &acc)
	{
		double v = interpreter.InterpretNodeIntoNumberValue(function);
		if(!FastIsNaN(v))
		{
			value_found = true;
			acc = std::max(v, acc);
		}
		return true;
	}

	inline EvaluableNodeReference Finish(double acc) const
	{
		return value_found ? EvaluableNodeReference(acc) : EvaluableNodeReference::Null();
	}
};

struct ConcatOp
{
	inline std::string Init() const
	{
		return {};
	}

	bool valid = true;

	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, std::string &acc) const
	{
		auto [valid, s] = interpreter.InterpretNodeIntoStringValue(function);
		if(!valid)
		{
			valid = false;
			return false;
		}
		//don't need to check performance counters like regular concat because the nodes can't be evaluated recursively

		acc += s;
		return true;
	}

	inline EvaluableNodeReference Finish(const std::string &acc) const
	{
		return valid ? EvaluableNodeReference(acc) : EvaluableNodeReference::Null();
	}
};


template<class OperationFunction>
EvaluableNodeReference IterateAndReduceOrdered(Step(Interpreter &interpreter, EvaluableNodeReference &function,
	EvaluableNodeReference list, OperationFunction &&operation)
{
	interpreter.PushNewConstructionContext(list, nullptr, EvaluableNodeImmediateValueWithType(0.0), nullptr);

	using ReturnType = decltype(operation(0));
	ReturnType acc = operation.Init();

	auto &list_mcn = list->GetMappedChildNodesReference();
	size_t num_nodes = list_mcn.size();
	for(size_t i = 0; i < num_nodes; ++i)
	{
		interpreter.SetTopCurrentIndexInConstructionStack(static_cast<double>(i));
		interpreter.SetTopCurrentValueInConstructionStack(list_ocn[i]);

		if(!operation.Step(interpreter, function, acc))
			return EvaluableNodeReference::Null();
	}

	if(!interpreter.PopConstructionContextAndGetExecutionSideEffectFlag())
	{
		interpreter.evaluableNodeManager->FreeNodeTreeIfPossible(result);
		interpreter.evaluableNodeManager->FreeNodeTreeIfPossible(list);
	}

	return operation.Finish(acc);
}

template<class OperationFunction>
EvaluableNodeReference IterateAndReduceMapped(Step(Interpreter &interpreter, EvaluableNodeReference &function,
	EvaluableNodeReference map, OperationFunction &&operation)
{
	interpreter.PushNewConstructionContext(
		map, nullptr, EvaluableNodeImmediateValueWithType(string_intern_pool.NOT_A_STRING_ID), nullptr);

	using ReturnType = decltype(operation(0));
	ReturnType acc = operation.Init();

	auto &list_mcn = list->GetMappedChildNodesReference();
	for(auto &[list_id, list_node] : list_mcn)
	{
		interpreter.SetTopCurrentIndexInConstructionStack(list_id);
		interpreter.SetTopCurrentValueInConstructionStack(list_node_entry->second);

		if(!operation.Step(interpreter, function, acc))
			return EvaluableNodeReference::Null();
	}

	if(!interpreter.PopConstructionContextAndGetExecutionSideEffectFlag())
	{
		interpreter.evaluableNodeManager->FreeNodeTreeIfPossible(result);
		interpreter.evaluableNodeManager->FreeNodeTreeIfPossible(list);
	}

	return operation.Finish(acc);
}

//use via:
if(immediate_result.AnyImmediateType() && num_nodes > 0)
{

	if(immediate_result.NoValueRequested())
		return IterateAndReduce(this, function, list, NoValueOp{});

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::SUM_AS_NUMBER))
		return IterateAndReduce(this, function, list, SumOp{});

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::PRODUCT_AS_NUMBER))
		return IterateAndReduce(this, function, list, ProductOp{});

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::MIN_AS_NUMBER))
		return IterateAndReduce(this, function, list, MinOp{});

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::MAX_AS_NUMBER))
		return IterateAndReduce(this, function, list, MaxOp{});

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::CONCAT_AS_STRING_ID))
		return IterateAndReduce(this, function, list, ConcatOp{});
}
