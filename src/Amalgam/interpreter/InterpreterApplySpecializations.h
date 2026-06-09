#include "Interpreter.h"

struct NoValueOp
{
	using AccumType = bool;

	inline AccumType Init() const
	{
		return true;
	}

	template<bool interpret_result>
	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, AccumType & /*acc*/) const
	{
		if constexpr(interpret_result)
			interpreter.InterpretNodeForImmediateUse(function,
				EvaluableNodeRequestedValueTypes::Type::NULL_VALUE);
		return true;
	}

	inline EvaluableNodeReference Finish(AccumType /*acc*/) const
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

	template<bool interpret_result>
	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, double &acc) const
	{
		if constexpr(interpret_result)
			acc += interpreter.InterpretNodeIntoNumberValue(function);
		else
			acc += EvaluableNode::ToNumber(function);

		return true;
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

	template<bool interpret_result>
	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, double &acc) const
	{
		if constexpr(interpret_result)
			acc *= interpreter.InterpretNodeIntoNumberValue(function);
		else
			acc *= EvaluableNode::ToNumber(function);

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

	template<bool interpret_result>
	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, double &acc)
	{
		double v;
		if constexpr(interpret_result)
			v = interpreter.InterpretNodeIntoNumberValue(function);
		else
			v = EvaluableNode::ToNumber(function);

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

	template<bool interpret_result>
	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, double &acc)
	{
		double v;
		if constexpr(interpret_result)
			v = interpreter.InterpretNodeIntoNumberValue(function);
		else
			v = EvaluableNode::ToNumber(function);

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

	template<bool interpret_result>
	inline bool Step(Interpreter &interpreter, EvaluableNodeReference &function, std::string &acc) const
	{
		bool valid;
		std::string s;

		if constexpr(interpret_result)
			std::tie(valid, s) = interpreter.InterpretNodeIntoStringValue(function);
		else
			std::tie(valid, s) = EvaluableNode::ToString(function);

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

template<typename IterationFunction>
inline std::pair<bool, EvaluableNodeReference> AttemptSpecializedInterpret(
	EvaluableNodeRequestedValueTypes immediate_result, IterationFunction &&iteration_function)
{
	if(immediate_result.NoValueRequested())
		return std::make_pair(true, iteration_function(NoValueOp{}));

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::SUM_AS_NUMBER))
		return std::make_pair(true, iteration_function(SumOp{}));

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::PRODUCT_AS_NUMBER))
		return std::make_pair(true, iteration_function(ProductOp{}));

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::MIN_AS_NUMBER))
		return std::make_pair(true, iteration_function(MinOp{}));

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::MAX_AS_NUMBER))
		return std::make_pair(true, iteration_function(MaxOp{}));

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::CONCAT_AS_STRING_ID))
		return std::make_pair(true, iteration_function(ConcatOp{}));

	return std::make_pair(false, EvaluableNodeReference::Null());
}
