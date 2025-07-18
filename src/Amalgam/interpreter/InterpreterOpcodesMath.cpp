//project headers:
#include "EntityQueryBuilder.h"
#include "EvaluableNode.h"
#include "Interpreter.h"

//system headers:
#include <cstdlib>
#include <functional>
#include <utility>

EvaluableNodeReference Interpreter::InterpretNode_ENT_ADD(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	double value = 0.0;

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
	{
		for(auto &cn : interpreted_nodes)
			value += ConvertNodeIntoNumberValueAndFreeIfPossible(cn);

		return AllocReturn(value, immediate_result);
	}
#endif

	for(auto &cn : ocn)
		value += InterpretNodeIntoNumberValue(cn);

	return AllocReturn(value, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SUBTRACT(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
	{
		double value = ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[0]);
		for(size_t i = 1; i < ocn.size(); i++)
			value -= ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[i]);

		return AllocReturn(value, immediate_result);
	}
#endif

	double value = InterpretNodeIntoNumberValue(ocn[0]);
	for(size_t i = 1; i < ocn.size(); i++)
		value -= InterpretNodeIntoNumberValue(ocn[i]);

	//if just one parameter, then treat as negative
	if(ocn.size() == 1)
		value = -value;

	return AllocReturn(value, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MULTIPLY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	double value = 1.0;

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
	{
		for(auto &cn : interpreted_nodes)
			value *= ConvertNodeIntoNumberValueAndFreeIfPossible(cn);

		return AllocReturn(value, immediate_result);
	}
#endif

	for(auto &cn : ocn)
		value *= InterpretNodeIntoNumberValue(cn);

	return AllocReturn(value, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DIVIDE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
	{
		double value = ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[0]);
		for(size_t i = 1; i < interpreted_nodes.size(); i++)
		{
			double divisor = ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[i]);

			if(divisor != 0.0)
				value /= divisor;
			else
			{
				if(value > 0.0)
					value = std::numeric_limits<double>::infinity();
				else if(value < 0.0)
					value = -std::numeric_limits<double>::infinity();
				else
					value = std::numeric_limits<double>::quiet_NaN();

				break;
			}
		}

		return AllocReturn(value, immediate_result);
	}
#endif

	double value = InterpretNodeIntoNumberValue(ocn[0]);
	for(size_t i = 1; i < ocn.size(); i++)
	{
		double divisor = InterpretNodeIntoNumberValue(ocn[i]);

		if(divisor != 0.0)
			value /= divisor;
		else
		{
			if(value > 0.0)
				value = std::numeric_limits<double>::infinity();
			else if(value < 0.0)
				value = -std::numeric_limits<double>::infinity();
			else
				value = std::numeric_limits<double>::quiet_NaN();

			break;
		}
	}

	return AllocReturn(value, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MODULUS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
	{
		double value = ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[0]);
		for(size_t i = 1; i < interpreted_nodes.size(); i++)
		{
			double mod = ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[i]);
			value = std::fmod(value, mod);
		}

		return AllocReturn(value, immediate_result);
	}
#endif

	double value = InterpretNodeIntoNumberValue(ocn[0]);
	for(size_t i = 1; i < ocn.size(); i++)
	{
		double mod = InterpretNodeIntoNumberValue(ocn[i]);
		value = std::fmod(value, mod);
	}

	return AllocReturn(value, immediate_result);
}

//helper method for InterpretNode_ENT_GET_DIGITS and InterpretNode_ENT_SET_DIGITS
//if relative_to_zero the digits are indexed as
// 5 4 3 2 1 0 . -1 -2
//if not relative_to_zero, the digits are indexed as
// 0 1 2 3 4 5 . 6  7
//for a given value and a base of the digits, sets first_digit, start_digit, and end_digit to be relative to zero
//accepts infinities and NaNs and still sets them appropriately
//first_digit is the first digit in the number (most significant), start_digit and end_digit are the digits selected
//if first_digit does not need to be computed, then it will be left unchanged
inline void NormalizeStartAndEndDigitToZerosPlace(double value, double base, bool relative_to_zero,
	double &first_digit, double &start_digit, double &end_digit)
{
	//compute max_num_digits using data on how the numbers are stored
	constexpr size_t max_num_storage_digits = std::numeric_limits<double>::digits;
	constexpr size_t storage_radix = std::numeric_limits<double>::radix;
	double max_num_digits = (storage_radix / base) * max_num_storage_digits;

	if(relative_to_zero)
	{
		//if start is infinite, start at top
		if(start_digit == std::numeric_limits<double>::infinity() || FastIsNaN(start_digit))
		{
			first_digit = std::floor(std::log(value) / std::log(base));
			start_digit = first_digit;
		}

		//if end is negative infinite, start at end
		if(end_digit == std::numeric_limits<double>::infinity() || FastIsNaN(end_digit))
			end_digit = start_digit - max_num_digits;
	}
	else //not relative to zero
	{
		first_digit = std::floor(std::log(value) / std::log(base));
		start_digit = first_digit - start_digit;

		if(end_digit == std::numeric_limits<double>::infinity() || FastIsNaN(end_digit))
			end_digit = start_digit - max_num_digits;
		else //valid position
			end_digit = first_digit - end_digit;
	}

	//make sure only use valid digits
	if(end_digit < start_digit - max_num_digits)
		end_digit = start_digit - max_num_digits;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_DIGITS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	size_t num_params = ocn.size();
	if(num_params == 0)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);

	double value = InterpretNodeIntoNumberValue(ocn[0]);
	//negative numbers have the same digits
	value = std::abs(value);
	if(FastIsNaN(value) || value == std::numeric_limits<double>::infinity())
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);

	double base = 10;
	if(num_params > 1)
	{
		base = InterpretNodeIntoNumberValue(ocn[1]);
		if(base <= 0)
			return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);
	}

	bool relative_to_zero = true;
	if(num_params > 4)
		relative_to_zero = InterpretNodeIntoBoolValue(ocn[4]);

	double start_digit = (relative_to_zero ? std::numeric_limits<double>::infinity() : 0);
	if(num_params > 2)
		start_digit = InterpretNodeIntoNumberValue(ocn[2]);

	double end_digit = (relative_to_zero ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
	if(num_params > 3)
		end_digit = InterpretNodeIntoNumberValue(ocn[3]);

	//leave first_digit as NaN; can check if non-NaN and lazily computed if needed later
	double first_digit = std::numeric_limits<double>::quiet_NaN();
	NormalizeStartAndEndDigitToZerosPlace(value, base, relative_to_zero, first_digit, start_digit, end_digit);

	EvaluableNodeReference digits(evaluableNodeManager->AllocNode(ENT_LIST), true);
	auto &digits_ocn = digits->GetOrderedChildNodes();
	if(std::isfinite(start_digit) && std::isfinite(end_digit) && start_digit >= end_digit)
	{
		size_t num_digits = static_cast<size_t>(std::floor(start_digit - end_digit + 1));
		digits_ocn.reserve(num_digits);

		//if doing an integer base, can be faster
		if(base - std::floor(base) == 0)
		{
			for(double cur_digit = start_digit; cur_digit >= end_digit; cur_digit--)
			{
				double place_value = std::pow(base, cur_digit);
				double value_shift_right = std::floor(value / place_value);
				double value_digit = std::fmod(value_shift_right, base);
				digits_ocn.emplace_back(evaluableNodeManager->AllocNode(value_digit));
			}
		}
		else //fractional base, need special logic
		{
			//need to compute first digits even if they're not used, so they can be subtracted from the number
			// this incurs extra performance and may reduce numerical accuracy slightly (hence not used for integer bases)
			if(FastIsNaN(first_digit))
				first_digit = std::floor(std::log(value) / std::log(base));

			//need to always start at most significant digit:
			for(double cur_digit = std::max(first_digit, start_digit); cur_digit >= end_digit; cur_digit--)
			{
				double place_value = std::pow(base, cur_digit);
				double value_shift_right = std::floor(value / place_value);
				double value_digit = std::fmod(value_shift_right, base);
				value -= value_digit * place_value;

				if(cur_digit <= start_digit)
					digits_ocn.emplace_back(evaluableNodeManager->AllocNode(value_digit));
			}
		}
	}

	return digits;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_DIGITS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	size_t num_params = ocn.size();
	if(num_params == 0)
		return AllocReturn(std::numeric_limits<double>::quiet_NaN(), immediate_result);

	double value = InterpretNodeIntoNumberValue(ocn[0]);
	if(FastIsNaN(value) || value == std::numeric_limits<double>::infinity())
		return AllocReturn(value, immediate_result);

	double base = 10;
	if(num_params > 1)
	{
		base = InterpretNodeIntoNumberValue(ocn[1]);
		if(base <= 0)
			return AllocReturn(value, immediate_result);
	}

	bool relative_to_zero = true;
	if(num_params > 5)
		relative_to_zero = InterpretNodeIntoBoolValue(ocn[5]);

	double start_digit = (relative_to_zero ? std::numeric_limits<double>::infinity() : 0);
	if(num_params > 3)
		start_digit = InterpretNodeIntoNumberValue(ocn[3]);

	double end_digit = (relative_to_zero ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
	if(num_params > 4)
		end_digit = InterpretNodeIntoNumberValue(ocn[4]);

	EvaluableNodeReference digits = EvaluableNodeReference::Null();
	if(num_params > 2)
		digits = InterpretNodeForImmediateUse(ocn[2]);

	if(digits == nullptr || digits->GetType() != ENT_LIST)
		return AllocReturn(value, immediate_result);

	bool negative = (value < 0);
	if(negative)
		value = -value;
	//value to modify
	double result_value = value;

	//leave first_digit as NaN; can check if non-NaN and lazily computed if needed later
	double first_digit = std::numeric_limits<double>::quiet_NaN();
	NormalizeStartAndEndDigitToZerosPlace(value, base, relative_to_zero, first_digit, start_digit, end_digit);

	auto &digits_ocn = digits->GetOrderedChildNodes();
	size_t cur_digit_index = 0;
	if(std::isfinite(start_digit) && std::isfinite(end_digit) && start_digit >= end_digit)
	{
		//if doing an integer base, can be faster
		if(base - std::floor(base) == 0)
		{
			for(double cur_digit = start_digit; cur_digit >= end_digit; cur_digit--)
			{
				double place_value = std::pow(base, cur_digit);
				double value_shift_right = std::floor(value / place_value);
				double value_digit = std::fmod(value_shift_right, base);

				if(cur_digit_index >= digits_ocn.size())
					break;

				//skip nulls
				if(EvaluableNode::IsNull(digits_ocn[cur_digit_index]))
				{
					cur_digit_index++;
					continue;
				}

				double new_digit = EvaluableNode::ToNumber(digits_ocn[cur_digit_index++]);

				result_value -= value_digit * place_value;
				result_value += new_digit * place_value;
			}
		}
		else //fractional base, need special logic
		{
			//need to compute first digits even if they're not used, so they can be subtracted from the number
			// this incurs extra performance and may reduce numerical accuracy slightly (hence not used for integer bases)
			if(FastIsNaN(first_digit))
				first_digit = std::floor(std::log(value) / std::log(base));

			//need to always start at most significant digit:
			for(double cur_digit = std::max(first_digit, start_digit); cur_digit >= end_digit; cur_digit--)
			{
				double place_value = std::pow(base, cur_digit);
				double value_shift_right = std::floor(value / place_value);
				double value_digit = std::fmod(value_shift_right, base);
				value -= value_digit * place_value;

				if(cur_digit <= start_digit)
				{
					if(cur_digit_index >= digits_ocn.size())
						break;

					//skip nulls
					if(EvaluableNode::IsNull(digits_ocn[cur_digit_index]))
					{
						cur_digit_index++;
						continue;
					}

					double new_digit = EvaluableNode::ToNumber(digits_ocn[cur_digit_index++]);

					result_value -= value_digit * place_value;
					result_value += new_digit * place_value;
				}
			}
		}
	}

	if(negative)
		result_value = -result_value;

	return AllocReturn(result_value, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_FLOOR(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::floor(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CEILING(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::ceil(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ROUND(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	size_t num_params = ocn.size();
	if(num_params == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference retval = EvaluableNodeReference::Null();
	double number_value = 0.0;

	if(immediate_result)
	{
		number_value = InterpretNodeIntoNumberValue(ocn[0]);
	}
	else
	{
		retval = InterpretNodeIntoUniqueNumberValueOrNullEvaluableNode(ocn[0]);
		number_value = EvaluableNode::ToNumber(retval);
	}

	if(num_params == 1)
	{
		//just round to the nearest integer
		number_value = std::round(number_value);
	}
	else 
	{
		auto node_stack = CreateOpcodeStackStateSaver(retval);

		//round to the specified number of significant digits or the specified number of digits after the decimal place, whichever is larger
		double num_significant_digits = InterpretNodeIntoNumberValue(ocn[1]);
		
		//assume don't want any digits after decimal (this will be ignored with negative infinity)
		double num_digits_after_decimal = std::numeric_limits<double>::infinity();
		if(num_params > 2)
			num_digits_after_decimal = InterpretNodeIntoNumberValue(ocn[2]);

		if(number_value != 0.0)
		{
			double starting_significant_digit = std::ceil(std::log10(std::fabs(number_value)));

			//decimal digits take priority over significant digits if they are specified
			num_significant_digits = std::min(starting_significant_digit + num_digits_after_decimal, num_significant_digits);

			double factor = std::pow(10.0, num_significant_digits - starting_significant_digit);
			number_value = std::round(number_value * factor) / factor;
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(retval);
	return AllocReturn(number_value, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EXPONENT(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::exp(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LOG(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_SIN(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::sin(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASIN(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::asin(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_COS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::cos(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ACOS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::acos(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TAN(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::tan(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ATAN(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_SINH(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::sinh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASINH(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::asinh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_COSH(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::cosh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ACOSH(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::acosh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TANH(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::tanh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ATANH(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::atanh(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ERF(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::erf(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TGAMMA(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::tgamma(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LGAMMA(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::lgamma(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SQRT(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::sqrt(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_POW(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	double f1 = InterpretNodeIntoNumberValue(ocn[0]);
	double f2 = InterpretNodeIntoNumberValue(ocn[1]);
	return AllocReturn(std::pow(f1, f2), immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ABS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::abs(value);	});
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MAX(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool value_found = false;
	double result_value = -std::numeric_limits<double>::infinity();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
	{
		for(size_t i = 0; i < interpreted_nodes.size(); i++)
		{
			//do the comparison and keep the greater
			double cur_value = ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[i]);
			if(cur_value > result_value)
			{
				value_found = true;
				result_value = cur_value;
			}
		}

		if(value_found)
			return AllocReturn(result_value, immediate_result);
		return EvaluableNodeReference::Null();
	}
#endif

	for(auto &cn : ocn)
	{
		double cur_value = InterpretNodeIntoNumberValue(cn);
		if(cur_value > result_value)
		{
			value_found = true;
			result_value = cur_value;
		}
	}

	if(value_found)
		return AllocReturn(result_value, immediate_result);
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIN(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool value_found = false;
	double result_value = std::numeric_limits<double>::infinity();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
	{
		for(size_t i = 0; i < interpreted_nodes.size(); i++)
		{
			//do the comparison and keep the greater
			double cur_value = ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[i]);
			if(cur_value < result_value)
			{
				value_found = true;
				result_value = cur_value;
			}
		}

		if(value_found)
			return AllocReturn(result_value, immediate_result);
		return EvaluableNodeReference::Null();
	}
#endif

	for(auto &cn : ocn)
	{
		auto cur_value = InterpretNodeIntoNumberValue(cn);
		if(cur_value < result_value)
		{
			value_found = true;
			result_value = cur_value;
		}
	}

	if(value_found)
		return AllocReturn(result_value, immediate_result);
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INDEX_MAX(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference ocn_zero = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(ocn_zero);

	EvaluableNodeReference result;
	if(ocn_zero != nullptr && ocn_zero->GetType() == ENT_ASSOC && ocn.size() == 1)
		result = GetIndexMinMaxFromAssoc(ocn_zero, std::greater(), -std::numeric_limits<double>::infinity(), immediate_result);
	else if(ocn_zero != nullptr && ocn_zero->GetType() == ENT_LIST && ocn.size() == 1)
		result = GetIndexMinMaxFromList(ocn_zero, std::greater(), -std::numeric_limits<double>::infinity(), immediate_result);
	else
		return GetIndexMinMaxFromRemainingArgList(en, std::greater(), -std::numeric_limits<double>::infinity(), immediate_result);

	evaluableNodeManager->FreeNodeTreeIfPossible(ocn_zero);
	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INDEX_MIN(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference ocn_zero = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(ocn_zero);

	EvaluableNodeReference result;
	if(ocn_zero != nullptr && ocn_zero->GetType() == ENT_ASSOC && ocn.size() == 1)
		result = GetIndexMinMaxFromAssoc(ocn_zero, std::less(), std::numeric_limits<double>::infinity(), immediate_result);
	else if(ocn_zero != nullptr && ocn_zero->GetType() == ENT_LIST && ocn.size() == 1)
		result = GetIndexMinMaxFromList(ocn_zero, std::less(), std::numeric_limits<double>::infinity(), immediate_result);
	else
		return GetIndexMinMaxFromRemainingArgList(en, std::less(), std::numeric_limits<double>::infinity(), immediate_result);

	evaluableNodeManager->FreeNodeTreeIfPossible(ocn_zero);
	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DOT_PRODUCT(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_NORMALIZE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	double p_value = 1.0;
	if(ocn.size() > 2)
	{
		double num_value = InterpretNodeIntoNumberValue(ocn[1]);
		if(!FastIsNaN(num_value))
			p_value = num_value;
	}

	auto container = InterpretNode(ocn[0]);
	if(EvaluableNode::IsNull(container))
		return EvaluableNodeReference::Null();

	if(!container.unique)
		container = evaluableNodeManager->DeepAllocCopy(container);

	//TODO 24093: finish this

	return EvaluableNodeReference::Null();
}

//builds a vector of the values in the node, using ordered or mapped child nodes as appropriate
// if node is mapped child nodes, it will use id_order to order populate out and use default_value if any given id is not found
inline void GetChildNodesAsENImmediateValueArray(EvaluableNode *node, std::vector<StringInternPool::StringID> &id_order,
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
			out.clear();
			out_types.clear();
			out.resize(id_order.size(), value);
			out_types.resize(id_order.size(), value_type);
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_GENERALIZED_DISTANCE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 6)
		return EvaluableNodeReference::Null();

	auto node_stack = CreateOpcodeStackStateSaver();

	//get weights list if applicable
	auto weights_node = InterpretNodeForImmediateUse(ocn[0]);
	if(weights_node != nullptr)
		node_stack.PushEvaluableNode(weights_node);

	//get distance types if applicable
	auto distance_types_node = InterpretNodeForImmediateUse(ocn[1]);
	if(distance_types_node != nullptr)
		node_stack.PushEvaluableNode(distance_types_node);

	//get feature attributes if applicable
	auto attributes_node = InterpretNodeForImmediateUse(ocn[2]);
	if(attributes_node != nullptr)
		node_stack.PushEvaluableNode(attributes_node);

	//get deviations if applicable
	auto deviations_node = InterpretNodeForImmediateUse(ocn[3]);
	if(deviations_node != nullptr)
		node_stack.PushEvaluableNode(deviations_node);

	GeneralizedDistanceEvaluator dist_eval;
	dist_eval.pValue = InterpretNodeIntoNumberValue(ocn[4]);

	//get location
	auto location_node = InterpretNodeForImmediateUse(ocn[5]);
	if(location_node != nullptr)
		node_stack.PushEvaluableNode(location_node);

	//get origin if applicable
	EvaluableNodeReference origin_node = EvaluableNodeReference::Null();
	if(ocn.size() > 6)
	{
		origin_node = InterpretNodeForImmediateUse(ocn[6]);
		if(origin_node != nullptr)
			node_stack.PushEvaluableNode(origin_node);
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
		weights_node, distance_types_node, attributes_node, deviations_node);

	//done with all values
	evaluableNodeManager->FreeNodeTreeIfPossible(weights_node);
	evaluableNodeManager->FreeNodeTreeIfPossible(distance_types_node);
	evaluableNodeManager->FreeNodeTreeIfPossible(attributes_node);
	evaluableNodeManager->FreeNodeTreeIfPossible(deviations_node);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_ENTROPY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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
			q_num_elements = q_node->GetOrderedChildNodes().size();
			q_values = &q_node->GetOrderedChildNodes();
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
