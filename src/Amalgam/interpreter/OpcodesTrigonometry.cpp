//project headers:
#include "Interpreter.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Trigonometry";

static OpcodeInitializer _ENT_SIN(ENT_SIN, &Interpreter::InterpretNode_ENT_SIN, []() {
	OpcodeDetails d;
	d.parameters = R"(number theta)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the sine of `theta`.)";
	d.examples = MakeAmalgamExamples({
		{R"((sin 0.5))", R"(0.479425538604203)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SIN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::sin(value);	});
}

static OpcodeInitializer _ENT_ASIN(ENT_ASIN, &Interpreter::InterpretNode_ENT_ASIN, []() {
	OpcodeDetails d;
	d.parameters = R"(number length)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the arc sine (inverse sine) of `length`.)";
	d.examples = MakeAmalgamExamples({
		{R"((sin 0.5))", R"(0.479425538604203)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASIN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::asin(value);	});
}

static OpcodeInitializer _ENT_COS(ENT_COS, &Interpreter::InterpretNode_ENT_COS, []() {
	OpcodeDetails d;
	d.parameters = R"(number theta)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the cosine of `theta`.)";
	d.examples = MakeAmalgamExamples({
		{R"((cos 0.5))", R"(0.8775825618903728)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_COS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::cos(value);	});
}

static OpcodeInitializer _ENT_ACOS(ENT_ACOS, &Interpreter::InterpretNode_ENT_ACOS, []() {
	OpcodeDetails d;
	d.parameters = R"(number length)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the arc cosine (inverse cosine) of `length`.)";
	d.examples = MakeAmalgamExamples({
		{R"((acos 0.5))", R"(1.0471975511965979)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ACOS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::acos(value);	});
}

static OpcodeInitializer _ENT_TAN(ENT_TAN, &Interpreter::InterpretNode_ENT_TAN, []() {
	OpcodeDetails d;
	d.parameters = R"(number theta)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the tangent of `theta`.)";
	d.examples = MakeAmalgamExamples({
		{R"((tan 0.5))", R"(0.5463024898437905)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TAN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::tan(value);	});
}

static OpcodeInitializer _ENT_ATAN(ENT_ATAN, &Interpreter::InterpretNode_ENT_ATAN, []() {
	OpcodeDetails d;
	d.parameters = R"(number num [number divisor])";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the arc tangent (inverse tangent) of `num`.  If two numbers are provided, then it evaluates to the arc tangent of `num` / `divisor`.)";
	d.examples = MakeAmalgamExamples({
		{R"((atan 0.5))", R"(0.4636476090008061)"}, {R"((atan 0.5 0.5))", R"(0.7853981633974483)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

static OpcodeInitializer _ENT_SINH(ENT_SINH, &Interpreter::InterpretNode_ENT_SINH, []() {
	OpcodeDetails d;
	d.parameters = R"(number z)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the hyperbolic sine of `z`.)";
	d.examples = MakeAmalgamExamples({
		{R"((sinh 0.5))", R"(0.5210953054937474)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.001;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SINH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::sinh(value);	});
}

static OpcodeInitializer _ENT_ASINH(ENT_ASINH, &Interpreter::InterpretNode_ENT_ASINH, []() {
	OpcodeDetails d;
	d.parameters = R"(number x)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the hyperbolic arc sine of `x`.)";
	d.examples = MakeAmalgamExamples({
		{R"((asinh 0.5))", R"(0.48121182505960347)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.001;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASINH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::asinh(value);	});
}

static OpcodeInitializer _ENT_COSH(ENT_COSH, &Interpreter::InterpretNode_ENT_COSH, []() {
	OpcodeDetails d;
	d.parameters = R"(number z)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the hyperbolic cosine of `z`.)";
	d.examples = MakeAmalgamExamples({
		{R"((cosh 0.5))", R"(1.1276259652063807)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.001;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_COSH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::cosh(value);	});
}

static OpcodeInitializer _ENT_ACOSH(ENT_ACOSH, &Interpreter::InterpretNode_ENT_ACOSH, []() {
	OpcodeDetails d;
	d.parameters = R"(number x)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the hyperbolic arc cosine of `x`.)";
	d.examples = MakeAmalgamExamples({
		{R"((acosh 1.5))", R"(0.9624236501192069)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.001;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ACOSH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::acosh(value);	});
}

static OpcodeInitializer _ENT_TANH(ENT_TANH, &Interpreter::InterpretNode_ENT_TANH, []() {
	OpcodeDetails d;
	d.parameters = R"(number z)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the hyperbolic tangent on `z`.)";
	d.examples = MakeAmalgamExamples({
		{R"((tanh 0.5))", R"(0.46211715726000974)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.001;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TANH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::tanh(value);	});
}

static OpcodeInitializer _ENT_ATANH(ENT_ATANH, &Interpreter::InterpretNode_ENT_ATANH, []() {
	OpcodeDetails d;
	d.parameters = R"(number x)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the hyperbolic arc tangent on `x`.)";
	d.examples = MakeAmalgamExamples({
		{R"((atanh 0.5))", R"(0.5493061443340549)", R"(0.54930614433405\d+)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.001;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ATANH(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	return InterpretNodeUnaryNumericOperation(ocn[0], immediate_result,
		[](double value) {	return std::atanh(value);	});
}
