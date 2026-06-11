#include "Interpreter.h"

struct NoValueOp
{
	using AccumType = bool;

	inline AccumType Init() const
	{
		return true;
	}

	template<bool interpret_result>
	inline bool Step(Interpreter &interpreter, EvaluableNode *function, AccumType & /*acc*/) const
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

struct SizeOp
{
	inline size_t Init() const
	{
		return 0;
	}

	template<bool interpret_result>
	inline bool Step(Interpreter &interpreter, EvaluableNode *function, size_t &acc) const
	{
		acc++;
		return true;
	}

	inline EvaluableNodeReference Finish(size_t acc) const
	{
		return EvaluableNodeReference(static_cast<double>(acc));
	}
};

struct SumOp
{
	inline double Init() const
	{
		return 0.0;
	}

	template<bool interpret_result>
	inline bool Step(Interpreter &interpreter, EvaluableNode *function, double &acc) const
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
	inline bool Step(Interpreter &interpreter, EvaluableNode *function, double &acc) const
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
	inline bool Step(Interpreter &interpreter, EvaluableNode *function, double &acc)
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
	inline bool Step(Interpreter &interpreter, EvaluableNode *function, double &acc)
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
	inline bool Step(Interpreter &interpreter, EvaluableNode *function, std::string &acc) const
	{
		bool valid;
		std::string s;

		if constexpr(interpret_result)
			std::tie(valid, s) = interpreter.InterpretNodeIntoStringValue(function);
		else
			std::tie(valid, s) = EvaluableNode::ToValidString(function);

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
	//use a switch statement because the specialized interpret requests must only allow a single type
	switch(immediate_result.requestedValueTypes)
	{
	case EvaluableNodeRequestedValueTypes::Type::NULL_VALUE:
		return std::make_pair(true, iteration_function(NoValueOp{}));

	case EvaluableNodeRequestedValueTypes::Type::SIZE_AS_NUMBER:
		return std::make_pair(true, iteration_function(SizeOp{}));

	case EvaluableNodeRequestedValueTypes::Type::SUM_AS_NUMBER:
		return std::make_pair(true, iteration_function(SumOp{}));

	case EvaluableNodeRequestedValueTypes::Type::PRODUCT_AS_NUMBER:
		return std::make_pair(true, iteration_function(ProductOp{}));

	case EvaluableNodeRequestedValueTypes::Type::MIN_AS_NUMBER:
		return std::make_pair(true, iteration_function(MinOp{}));

	case EvaluableNodeRequestedValueTypes::Type::MAX_AS_NUMBER:
		return std::make_pair(true, iteration_function(MaxOp{}));

	case EvaluableNodeRequestedValueTypes::Type::CONCAT_AS_STRING_ID:
		return std::make_pair(true, iteration_function(ConcatOp{}));

	default:
		return std::make_pair(false, EvaluableNodeReference::Null());
	}
}
