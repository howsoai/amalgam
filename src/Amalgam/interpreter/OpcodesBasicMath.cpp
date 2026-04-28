//project headers:
#include "Interpreter.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Basic Math";

static OpcodeInitializer _ENT_ADD(ENT_ADD, &Interpreter::InterpretNode_ENT_ADD, []() {
	OpcodeDetails d;
	d.parameters = R"([number x1] [number x2] ... [number xN])";
	d.returns = R"(number)";
	d.allowsConcurrency = true;
	d.description = R"(Sums all numbers.)";
	d.examples = MakeAmalgamExamples({
		{R"((+ 1 2 3 4))", R"(10)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 18.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ADD(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	double value = 0.0;

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
		std::vector<EvaluableNodeReference> interpreted_nodes;
		if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
		{
			for(auto &cn : interpreted_nodes)
				value += ConvertNodeIntoNumberValueAndFreeIfPossible(cn);

			return AllocReturn(value, immediate_result);
		}
	}
#endif

	for(auto &cn : ocn)
		value += InterpretNodeIntoNumberValue(cn);

	return AllocReturn(value, immediate_result);
}

static OpcodeInitializer _ENT_SUBTRACT(ENT_SUBTRACT, &Interpreter::InterpretNode_ENT_SUBTRACT, []() {
	OpcodeDetails d;
	d.parameters = R"([number x1] [number x2] ... [number xN])";
	d.returns = R"(number)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to `x1` - `x2` - ... - `xN`.  If only one parameter is passed, then it is treated as negative)";
	d.examples = MakeAmalgamExamples({
		{R"((- 1 2 3 4))", R"(-8)"},
		{R"((- 3))", R"(-3)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 15.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SUBTRACT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
		std::vector<EvaluableNodeReference> interpreted_nodes;
		if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
		{
			double value = ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[0]);
			for(size_t i = 1; i < ocn.size(); i++)
				value -= ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[i]);

			return AllocReturn(value, immediate_result);
		}
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

static OpcodeInitializer _ENT_MULTIPLY(ENT_MULTIPLY, &Interpreter::InterpretNode_ENT_MULTIPLY, []() {
	OpcodeDetails d;
	d.parameters = R"([number x1] [number x2] ... [number xN])";
	d.returns = R"(number)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to the product of all numbers.)";
	d.examples = MakeAmalgamExamples({
		{R"((* 1 2 3 4))", R"(24)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 9.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_MULTIPLY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	double value = 1.0;

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
		std::vector<EvaluableNodeReference> interpreted_nodes;
		if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes, true))
		{
			for(auto &cn : interpreted_nodes)
				value *= ConvertNodeIntoNumberValueAndFreeIfPossible(cn);

			return AllocReturn(value, immediate_result);
		}
	}
#endif

	for(auto &cn : ocn)
		value *= InterpretNodeIntoNumberValue(cn);

	return AllocReturn(value, immediate_result);
}

static OpcodeInitializer _ENT_DIVIDE(ENT_DIVIDE, &Interpreter::InterpretNode_ENT_DIVIDE, []() {
	OpcodeDetails d;
	d.parameters = R"([number x1] [number x2] ... [number xN])";
	d.returns = R"(number)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to `x1` / `x2` / ... / `xN`.)";
	d.examples = MakeAmalgamExamples({
		{R"((/ 1.0 2 3 4))", R"(0.041666666666666664)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 12.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_DIVIDE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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

static OpcodeInitializer _ENT_MODULUS(ENT_MODULUS, &Interpreter::InterpretNode_ENT_MODULUS, []() {
	OpcodeDetails d;
	d.parameters = R"([number x1] [number x2] ... [number xN])";
	d.returns = R"(number)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates the modulus of `x1` % `x2` % ... % `xN`.)";
	d.examples = MakeAmalgamExamples({
		{R"((mod 1 2 3 4))", R"(1)"},
		{R"((mod 5 3))", R"(2)"},
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_MODULUS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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

static OpcodeInitializer _ENT_GET_DIGITS(ENT_GET_DIGITS, &Interpreter::InterpretNode_ENT_GET_DIGITS, []() {
	OpcodeDetails d;
	d.parameters = R"(number value [number base] [number start_digit] [number end_digit] [bool relative_to_zero])";
	d.returns = R"(list of number)";
	d.description = R"(Evaluates to a list of the number of each digit of `value` for the given `base`.  If `base` is omitted, 10 is the default.  The parameters `start_digit` and `end_digit` can be used to get a specific set of digits, but can also be infinite or null to catch all the digits on one side of the number.  The interpretation of `start_digit` and `end_digit` are with respect to relative_to_zero, which defaults to true.  If relative_to_zero is true, then the digits are indexed from their distance to zero, such as "5 4 3 2 1 0 . -1 -2".  If relative_to_zero is false, then the digits are indexed from their most significant digit, such as "0 1 2 3 4 5 . 6  7".  The default values of `start_digit` and `end_digit` are the most and least significant digits respectively.)";
	d.examples = MakeAmalgamExamples({
		{R"&((get_digits 1234567.8 10))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	0
	0
	0
])"},
			{R"&((get_digits 1234567.89 10))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	8
	9
	9
])"},
			{R"&((get_digits 1234.5678 10 -1 -.infinity))&", R"([
	5
	6
	7
	8
	0
	0
	0
	0
	0
	0
	0
])"},
			{R"&((get_digits 7 2 .infinity 0))&", R"([1 1 1])"},
			{R"&((get_digits 16 2 .infinity 0))&", R"([1 0 0 0 0])"},
			{R"&((get_digits 24 4 .infinity 0))&", R"([1 2 0])"},
			{R"&((get_digits 40 3 .infinity 0))&", R"([1 1 1 1])"},
			{R"&((get_digits 16 2 .infinity 0))&", R"([1 0 0 0 0])"},
			{R"&((get_digits 16 8 .infinity 0))&", R"([2 0])"},
			{R"&((get_digits 3 2 5 0))&", R"([0 0 0 0 1 1])"},
			{R"&((get_digits 1.5 1.5 .infinity 0))&", R"([1 0])"},
			{R"&((get_digits 3.75 1.5 .infinity -7))&", R"([
	1
	0
	0
	0
	0
	0
	1
	0
	0
	0
	1
])"},
			{R"&((get_digits 1234567.8 10 0 4 .false))&", R"([1 2 3 4 5])"},
			{R"&((get_digits 1234567.8 10 4 8 .false))&", R"([5 6 7 8 0])"},
			{R"&((get_digits 1.2345678e+100 10 0 4 .false))&", R"([1 2 3 4 5])"},
			{R"&((get_digits 1.2345678e+100 10 4 8 .false))&", R"([5 6 7 8 0])"},
			{R"&(;should print empty list for these
(get_digits 0 2.714 1 3 .false))&", R"([])"},
			{R"&((get_digits 0 2.714 1 3 .true))&", R"([])"},
			{R"&((get_digits 0 10 0 10 .false))&", R"([])"},
			{R"&(;4 followed by zeros
(get_digits 0.4 10 0 10 .false))&", R"([
	4
	0
	0
	0
	0
	0
	0
	0
	0
	0
	0
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

//helper method for InterpretNode_ENT_GET_DIGITS and InterpretNode_ENT_SET_DIGITS
//if relative_to_zero the digits are indexed as
// 5 4 3 2 1 0 . -1 -2
//if not relative_to_zero, the digits are indexed as
// 0 1 2 3 4 5 . 6  7
//for a given value and a base of the digits, sets first_digit, start_digit, and end_digit to be relative to zero
//accepts infinities and NaNs and still sets them appropriately
//first_digit is the first digit in the number (most significant), start_digit and end_digit are the digits selected
//if first_digit does not need to be computed, then it will be left unchanged
static inline void NormalizeStartAndEndDigitToZerosPlace(double value, double base, bool relative_to_zero,
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_DIGITS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
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
	auto &digits_ocn = digits->GetOrderedChildNodesReference();
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

static OpcodeInitializer _ENT_SET_DIGITS(ENT_SET_DIGITS, &Interpreter::InterpretNode_ENT_SET_DIGITS, []() {
	OpcodeDetails d;
	d.parameters = R"(number value [number base] [list|number|null digits] [number start_digit] [number end_digit] [bool relative_to_zero])";
	d.returns = R"(number)";
	d.description = R"(Evaluates to `value` having each of the values in the list of `digits` replace each of the relative digits in `value` for the given base.  If a digit is null in `digits`, then that digit is not set.  If `base` is omitted, 10 is the default.  The parameters `start_digit` and `end_digit` can be used to get a specific set of digits, but can also be infinite or null to catch all the digits on one side of the number.  The interpretation of `start_digit` and `end_digit` are with respect to `relative_to_zero`, which defaults to true.  If `relative_to_zero` is true, then the digits are indexed from their distance to zero, such as "5 4 3 2 1 0 . -1 -2".  If `relative_to_zer`o is false, then the digits are indexed from their most significant digit, such as "0 1 2 3 4 5 . 6  7".  The default values of `start_digit` and `end_digit` are the most and least significant digits respectively.)";
	d.examples = MakeAmalgamExamples({
		{R"&((set_digits
	1234567.8
	10
	[5 5 5]
))&", R"(5554567.8)"},
			{R"&((set_digits
	1234567.8
	10
	[5 5 5]
	-1
	-.infinity
))&", R"(1234567.555)"},
			{R"&((set_digits
	7
	2
	[1 0 0]
	.infinity
	0
))&", R"(4)"},
			{R"&((set_digits
	1.5
	1.5
	[1]
	.infinity
	0
))&", R"(1.5)"},
			{R"&((set_digits
	1.5
	1.5
	[2]
	.infinity
	0
))&", R"(3)"},
			{R"&((set_digits
	1.5
	1.5
	[1 0]
	1
	0
))&", R"(1.5)"},
			{R"&((set_digits
	1234567.8
	10
	[5 5 5]
	10
))&", R"(55501234567.8)"},
			{R"&((set_digits
	1.5
	1.5
	[1 0 0]
	2
	0
))&", R"(2.25)"},
			{R"&((set_digits
	1234567.8
	10
	[5 5 5 5 5]
	0
	4
	.false
))&", R"(5555567.8)"},
			{R"&((set_digits
	1234567.8
	10
	[5 5 5 5 5]
	4
	8
	.false
))&", R"(1234555.55)"},
			{R"&((set_digits
	1.2345678e+100
	10
	[5 5 5 5 5]
	0
	4
	.false
))&", R"(5.555567800000001e+100)"},
			{R"&((set_digits
	1.2345678e+100
	10
	[5 5 5 5 5]
	4
	8
	.false
))&", R"(1.2345555499999999e+100)"},
			{R"&((set_digits
	1.2345678e+100
	10
	[5 .null 5 .null 5]
	4
	8
	.false
))&", R"(1.23456585e+100)"},
			{R"&(;these should all print (list 1 0 1)
(get_digits
	(set_digits
		1234567.8
		10
		[1 0 1 0]
		2
		5
		.false
	)
	10
	2
	5
	.false
))&", R"([1 0 1 0])"},
			{R"&((get_digits
	(set_digits
		1234567.8
		2
		[1 0 1 0]
		2
		5
		.false
	)
	2
	2
	5
	.false
))&", R"([1 0 1 0])"},
			{R"&((get_digits
	(set_digits
		1234567.8
		3.1
		[1 0 1 0]
		2
		5
		.false
	)
	3.1
	2
	5
	.false
))&", R"([1 0 1 0])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_DIGITS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
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

	auto &digits_ocn = digits->GetOrderedChildNodesReference();
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

static OpcodeInitializer _ENT_FLOOR(ENT_FLOOR, &Interpreter::InterpretNode_ENT_FLOOR, []() {
	OpcodeDetails d;
	d.parameters = R"(number x)";
	d.returns = R"(int)";
	d.description = R"(Evaluates to the mathematical floor of x.)";
	d.examples = MakeAmalgamExamples({
		{R"((floor 1.5))", R"(1)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_FLOOR(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::floor(value);	});
}

static OpcodeInitializer _ENT_CEILING(ENT_CEILING, &Interpreter::InterpretNode_ENT_CEILING, []() {
	OpcodeDetails d;
	d.parameters = R"(number x)";
	d.returns = R"(int)";
	d.description = R"(Evaluates to the mathematical ceiling of x.)";
	d.examples = MakeAmalgamExamples({
		{R"((ceil 1.5))", R"(2)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CEILING(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::ceil(value);	});
}

static OpcodeInitializer _ENT_ROUND(ENT_ROUND, &Interpreter::InterpretNode_ENT_ROUND, []() {
	OpcodeDetails d;
	d.parameters = R"(number x [number significant_digits] [number significant_digits_after_decimal])";
	d.returns = R"(int)";
	d.description = R"(Rounds the value `x` and evaluates to the new value.  If only one parameter is specified, it rounds to the nearest integer.  If `significant_digits` is specified, then it rounds to the specified number of significant digits.  If `significant_digits_after_decimal` is specified, then it ensures that `x` will be rounded at least to the number of decimal points past the integer as specified, and takes priority over `significant_digits`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((round 12.7))&", R"(13)"},
		{R"&((round 12.7 1))&", R"(10)"},
		{R"&((round 123.45678 5))&", R"(123.46)"},
		{R"&((round 123.45678 2))&", R"(120)"},
		{R"&((round 123.45678 2 2))&", R"(120)"},
		{R"&((round 123.45678 6 2))&", R"(123.46)"},
		{R"&((round 123.45678 4 0))&", R"(123)"},
		{R"&((round 123.45678 0 0))&", R"(0)"},
		{R"&((round 1.2345678 2 4))&", R"(1.2)"},
		{R"&((round 1.2345678 0 4))&", R"(0)"},
		{R"&((round 0.012345678 2 4))&", R"(0.012)"},
		{R"&((round 0.012345678 4 2))&", R"(0.01)"},
		{R"&((round 0.012345678 0 0))&", R"(0)"},
		{R"&((round 0.012345678 100 100))&", R"(0.012345678)"},
		{R"&((round 0.6 2))&", R"(0.6)"},
		{R"&((round 0.6 32 2))&", R"(0.6)"},
		{R"&((round
	(/ 1 3)
	32
	1
))&", R"(0.3)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ROUND(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t num_params = ocn.size();
	if(num_params == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference retval = EvaluableNodeReference::Null();
	double number_value = 0.0;

	if(immediate_result.AnyImmediateType())
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

static OpcodeInitializer _ENT_ABS(ENT_ABS, &Interpreter::InterpretNode_ENT_ABS, []() {
	OpcodeDetails d;
	d.parameters = R"(number x)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to absolute value of `x`)";
	d.examples = MakeAmalgamExamples({
		{R"((abs -0.5))", R"(0.5)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ABS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::abs(value);	});
}

static OpcodeInitializer _ENT_MAX(ENT_MAX, &Interpreter::InterpretNode_ENT_MAX, []() {
	OpcodeDetails d;
	d.parameters = R"([number x1] [number x2] ... [number xN])";
	d.returns = R"(number)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to the maximum of all of parameters.)";
	d.examples = MakeAmalgamExamples({
		{R"&((max 0.5 1 7 9 -5))&", R"(9)"},
		{R"&((max .null 4 8))&", R"(8)"},
		{R"&((max .null))&", R"(.null)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 2.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_MAX(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool value_found = false;
	double result_value = -std::numeric_limits<double>::infinity();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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

static OpcodeInitializer _ENT_MIN(ENT_MIN, &Interpreter::InterpretNode_ENT_MIN, []() {
	OpcodeDetails d;
	d.parameters = R"([number x1] [number x2] ... [number xN])";
	d.returns = R"(number)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to the minimum of all of the numbers.)";
	d.examples = MakeAmalgamExamples({
		{R"&((min 0.5 1 7 9 -5))&", R"(-5)"},
		{R"&((min .null 4 8))&", R"(4)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 2.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool value_found = false;
	double result_value = std::numeric_limits<double>::infinity();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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

static OpcodeInitializer _ENT_INDEX_MAX(ENT_INDEX_MAX, &Interpreter::InterpretNode_ENT_INDEX_MAX, []() {
	OpcodeDetails d;
	d.parameters = R"([[number x1] [number x2] [number x3] ... [number xN]] | assoc|list values)";
	d.returns = R"([any])";
	d.allowsConcurrency = true;
	d.description = R"(If given multiple arguments, returns a list of the indices of the arguments with the maximum value.  If given a single argument that is an assoc, it returns the a list of keys associated with the maximum values; the list will be a single value unless there are ties.  If given a single argument that is a list, it returns a list of list indices with the maximum value.)";
	d.examples = MakeAmalgamExamples({
		{R"&((index_max 0.5 -12 3 5 7))&", R"([4])"},
		{R"&((index_max
	[1 1 3 2 1 3]
))&", R"([2 5])"},
			{R"&((index_max .null 34 -66))&", R"([1])"},
			{R"&((index_max .null .null .null))&", R"(.null)"},
			{R"&((index_max
	{1 2 3 5 tomato 4444}
))&", R"(["tomato"])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

template<typename Compare>
EvaluableNodeReference GetIndexMinMaxFromAssoc(EvaluableNodeReference interpreted_assoc,
	EvaluableNodeManager *enm, Compare compare, double compare_limit, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &mapped_child_nodes = interpreted_assoc->GetMappedChildNodesReference();
	double candidate_value = compare_limit;
	bool value_found = false;

	std::vector<StringInternPool::StringID> max_keys;

	for(auto [cur_key, cur_child] : mapped_child_nodes)
	{
		double cur_value = EvaluableNode::ToNumber(cur_child);

		if(cur_value == candidate_value)
		{
			max_keys.push_back(cur_key);
			// If all child nodes are the max/min value, we never fall into the other case.
			// So we need to set value_found here.
			value_found = true;
		}
		else if(compare(cur_value, candidate_value))
		{
			max_keys.clear();
			candidate_value = cur_value;
			max_keys.push_back(cur_key);
			value_found = true;
		}
	}

	if(value_found)
	{
		EvaluableNodeReference index_list(enm->AllocNode(ENT_LIST), false);
		auto &index_list_ocn = index_list->GetOrderedChildNodesReference();
		index_list_ocn.reserve(max_keys.size());

		for(StringInternPool::StringID max_key : max_keys)
		{
			EvaluableNodeReference parsedKey = Parser::ParseFromKeyStringId(max_key, enm);
			index_list.UpdatePropertiesBasedOnAttachedNode(parsedKey);
			index_list_ocn.push_back(parsedKey);
		}

		return index_list;
	}

	return EvaluableNodeReference::Null();
}

template<typename Compare>
EvaluableNodeReference GetIndexMinMaxFromList(EvaluableNode *en, EvaluableNodeManager *enm,
	Compare compare, double compare_limit, EvaluableNodeRequestedValueTypes immediate_result)
{
	bool value_found = false;
	double result_value = compare_limit;
	std::vector<size_t> max_indices;

	auto &orderedChildNodes = en->GetOrderedChildNodesReference();

	if(orderedChildNodes.size() == 0)
		return EvaluableNodeReference::Null();

	for(size_t i = 0; i < orderedChildNodes.size(); i++)
	{
		double cur_value = EvaluableNode::ToNumber(orderedChildNodes[i]);

		if(cur_value == result_value)
		{
			max_indices.push_back(i);
			// If all child nodes are the max/min value, we never fall into the other case.
			// So we need to set value_found here.
			value_found = true;
		}
		else if(compare(cur_value, result_value))
		{
			max_indices.clear();

			value_found = true;
			result_value = cur_value;
			max_indices.push_back(i);
		}
	}

	if(value_found)
		return CreateListOfNumbersFromIteratorAndFunction(max_indices, enm,
			[](auto val) { return static_cast<double>(val); });

	return EvaluableNodeReference::Null();
}

template<typename Compare>
EvaluableNodeReference GetIndexMinMaxFromRemainingArgList(EvaluableNode *en, Interpreter *interpreter,
	Compare compare, double compare_limit, EvaluableNodeRequestedValueTypes immediate_result)
{
	double result_value = compare_limit;
	std::vector<size_t> max_indices;
	bool value_found = false;

	auto &orderedChildNodes = en->GetOrderedChildNodesReference();

	if(orderedChildNodes.size() == 0)
		return EvaluableNodeReference::Null();

	// First node has already been interpreted and thus needs different handling
	double candidate_zero = EvaluableNode::ToNumber(orderedChildNodes[0]);
	if(!FastIsNaN(candidate_zero))
	{
		max_indices.push_back(0);
		value_found = true;
		result_value = candidate_zero;
	}

	for(size_t i = 1; i < orderedChildNodes.size(); i++)
	{
		double cur_value = interpreter->InterpretNodeIntoNumberValue(orderedChildNodes[i]);

		if(cur_value == result_value)
		{
			max_indices.push_back(i);
			// If all child nodes are the max/min value, we never fall into the other case.
			// So we need to set value_found here.
			value_found = true;
		}
		else if(compare(cur_value, result_value))
		{
			max_indices.clear();
			result_value = cur_value;
			max_indices.push_back(i);
			value_found = true;
		}
	}

	if(value_found)
		return CreateListOfNumbersFromIteratorAndFunction(max_indices, interpreter->evaluableNodeManager,
			[](size_t val) { return static_cast<double>(val); });

	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INDEX_MAX(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference ocn_zero = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(ocn_zero);

	EvaluableNodeReference result;
	if(ocn_zero != nullptr && ocn_zero->GetType() == ENT_ASSOC && ocn.size() == 1)
		result = GetIndexMinMaxFromAssoc(ocn_zero, evaluableNodeManager,
			std::greater(), -std::numeric_limits<double>::infinity(), immediate_result);
	else if(ocn_zero != nullptr && ocn_zero->GetType() == ENT_LIST && ocn.size() == 1)
		result = GetIndexMinMaxFromList(ocn_zero, evaluableNodeManager,
			std::greater(), -std::numeric_limits<double>::infinity(), immediate_result);
	else
		return GetIndexMinMaxFromRemainingArgList(en, this,
			std::greater(), -std::numeric_limits<double>::infinity(), immediate_result);

	evaluableNodeManager->FreeNodeTreeIfPossible(ocn_zero);
	return result;
}

static OpcodeInitializer _ENT_INDEX_MIN(ENT_INDEX_MIN, &Interpreter::InterpretNode_ENT_INDEX_MIN, []() {
	OpcodeDetails d;
	d.parameters = R"([[number x1] [number x2] [number x3] ... [number xN]] | assoc values | list values)";
	d.returns = R"([any])";
	d.allowsConcurrency = true;
	d.description = R"(If given multiple arguments, returns a list of the indices of the arguments with the minimum value.  If given a single argument that is an assoc, it returns the a list of keys associated with the minimum values; the list will be a single value unless there are ties.  If given a single argument that is a list, it returns a list of list indices with the minimum value.)";
	d.examples = MakeAmalgamExamples({
		{R"&((index_min 0.5 -12 3 5 7))&", R"([1])"},
		{R"&((index_min
	[1 1 3 2 1 3]
))&", R"([0 1 4])"},
			{R"&((index_min .null 34 -66))&", R"([2])"},
			{R"&((index_min
	{1 2 3 5 tomato 4444}
))&", R"([1])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_INDEX_MIN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference ocn_zero = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(ocn_zero);

	EvaluableNodeReference result;
	if(ocn_zero != nullptr && ocn_zero->GetType() == ENT_ASSOC && ocn.size() == 1)
		result = GetIndexMinMaxFromAssoc(ocn_zero, evaluableNodeManager,
			std::less(), std::numeric_limits<double>::infinity(), immediate_result);
	else if(ocn_zero != nullptr && ocn_zero->GetType() == ENT_LIST && ocn.size() == 1)
		result = GetIndexMinMaxFromList(ocn_zero, evaluableNodeManager,
			std::less(), std::numeric_limits<double>::infinity(), immediate_result);
	else
		return GetIndexMinMaxFromRemainingArgList(en, this,
			std::less(), std::numeric_limits<double>::infinity(), immediate_result);

	evaluableNodeManager->FreeNodeTreeIfPossible(ocn_zero);
	return result;
}
