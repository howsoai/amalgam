//project headers:
#include "EvaluableNodeTreeAlgebra.h"
#include "Interpreter.h"

//system headers:
#include <array>
#include <cmath>
#include <utility>

//contextual information when applying a rule
class RuleContext
{
public:

	inline void NullifyNode(EvaluableNode *en)
	{
		FreeNodeChildNodesIfPossible(en);
		en->ClearMetadata();
		en->SetType(ENT_NULL, false);
	}

	inline void FreeNodeIfPossible(EvaluableNode *en)
	{
		if(nodesFreeable)
			enm->FreeNode(en);
	}

	inline void FreeNodeTreeIfPossible(EvaluableNode *en)
	{
		if(nodesFreeable)
			enm->FreeNodeTree(en);
	}

	inline void FreeNodeChildNodesIfPossible(EvaluableNode *en)
	{
		if(nodesFreeable)
			enm->FreeNodeChildNodes(en);
	}

	EvaluableNodeManager *enm;
	bool nodesFreeable;
	Interpreter *interpreter;
};

//converts a stateless rule type into a pair of plain function pointers
//implementations en is a valid pointer and guaranteed to not be nullptr
//Rewrite should return true if it made any changes, false if not
template<class R>
constexpr auto MakeRuleEntry()
{
	//the lambdas are capture‑less; the leading + forces them to decay
	// to a plain function pointer (no std::function, no allocation)
	return std::pair{
		+[](EvaluableNode *en) -> bool { return R{}.Match(en); },
		+[](EvaluableNode *en, RuleContext &rc) -> bool
		{ return R{}.Rewrite(en, rc); }
	};
}

//returns an iterator to the first divisor of ocn that is an immediate zero, or the end iterator if
//there is none; a runtime division stops at a zero divisor, leaving the remaining divisors unevaluated,
//so no divisor can be moved across one
static EvaluableNode::OrderedType::iterator FindFirstZeroDivisor(EvaluableNode::OrderedRef ocn)
{
	return std::find_if(begin(ocn) + 1, end(ocn),
		[](EvaluableNode *cn) { return (EvaluableNode::IsImmediate(cn) && EvaluableNode::ToNumber(cn) == 0.0); });
}

template<class... Rules>
constexpr auto MakeRuleRegistry()
{
	using entry_t = std::pair<
		bool (*)(EvaluableNode *),
		bool (*)(EvaluableNode *, RuleContext &)>;

	return std::array<entry_t, sizeof...(Rules)>{ MakeRuleEntry<Rules>()... };
}

struct FlattenAssociations final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return IsOpcodeAssociative(en->GetType());
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		bool any_changes = false;
		for(size_t i = 0; i < ocn.size(); i++)
		{
			if(EvaluableNode::IsNull(ocn[i]) || !ocn[i]->IsOrderedArray())
				continue;

			if(ocn[i]->GetType() != en->GetType())
				continue;

			EvaluableNode *child = ocn[i];

			//a node that contains itself can't be flattened into itself
			if(child == en)
				continue;

			//splice the child's child nodes in at the position the child occupied, so that opcodes that
			//are associative but not commutative, such as concat, retain their original ordering
			auto &child_ocn = child->GetOrderedChildNodesReference();
			ocn.erase(begin(ocn) + i);
			ocn.insert(begin(ocn) + i, begin(child_ocn), end(child_ocn));

			//free only the child node itself; its child nodes have been moved up and are still in use
			rc.FreeNodeIfPossible(child);

			//need to recheck the position now occupied by the first spliced-in child node
			i--;

			any_changes = true;
		}

		return any_changes;
	}
};

struct DeadCodeElimination final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_IF);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		bool node_replaced = false;
		bool any_changes = false;

		//check each condition
		for(size_t i = 0; i + 1 < ocn.size(); i += 2)
		{
			//if not immediate, then skip
			if(ocn[i] != nullptr && !ocn[i]->IsImmediate())
				continue;

			//will either eliminate branch or replace code above with it
			any_changes = true;

			//found it, replace with this
			if(EvaluableNode::ToBool(ocn[i]))
			{
				en->CopyNodeFrom(ocn[i + 1]);

				node_replaced = true;
				any_changes = true;
				break;
			}
			else //remove this branch
			{
				//erase the node and the following
				rc.FreeNodeTreeIfPossible(ocn[i]);
				rc.FreeNodeTreeIfPossible(ocn[i + 1]);

				ocn.erase(begin(ocn) + i, begin(ocn) + i + 2);

				//recheck this position next iteration
				i -= 2;
			}
		}

		//check for low number of parameters
		if(!node_replaced)
		{
			if(ocn.size() == 0)
			{
				rc.NullifyNode(en);
				return true;
			}

			if(ocn.size() == 1)
			{
				EvaluableNode *child_node = ocn[0];
				en->CopyNodeFrom(child_node);

				rc.FreeNodeIfPossible(child_node);

				any_changes = true;
			}
		}

		return any_changes;
	}
};

struct ShortCircuitBooleans final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_AND || en->GetType() == ENT_OR);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() == 0)
		{
			rc.NullifyNode(en);
			return true;
		}

		bool is_or = (en->GetType() == ENT_OR);

		bool short_circuit = false;
		//default as appropriate for operation
		bool short_circuit_value = (is_or ? false : true);
		bool any_changes = false;

		//check each condition
		for(size_t i = 0; i < ocn.size(); i++)
		{
			//if not immediate, then skip
			if(ocn[i] != nullptr && !ocn[i]->IsImmediate())
				continue;

			bool immediate_value = EvaluableNode::ToBool(ocn[i]);

			any_changes = true;

			//eliminate or short-circuit based on value
			if((!is_or && immediate_value) ||  (is_or && !immediate_value))
			{
				rc.FreeNodeTreeIfPossible(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;
			}
			else //can short-circuit
			{
				short_circuit = true;
				short_circuit_value = is_or;
				break;
			}
		}

		//if have eliminated all true values or short circuited, replace with a single value
		if(ocn.size() == 0 || short_circuit)
		{
			rc.FreeNodeChildNodesIfPossible(en);
			en->SetTypeViaBoolValue(short_circuit_value);
			return true;
		}

		return any_changes;
	}
};

struct FoldAdditionAndSubtraction final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_ADD || en->GetType() == ENT_SUBTRACT);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() == 0)
		{
			if(en->GetType() == ENT_SUBTRACT)
			{
				rc.NullifyNode(en);
			}
			else
			{
				rc.FreeNodeChildNodesIfPossible(en);
				en->ClearMetadata();
				en->SetTypeViaNumberValue(0.0);
			}
			return true;
		}

		bool positive_sign = true;
		bool subtraction = (en->GetType() == ENT_SUBTRACT);

		double accumulated_immediate = 0.0;

		size_t original_term_count = ocn.size();

		bool currently_accumulating_nonimmediate = false;
		double nonimmediate_accum_multiplicand = 0.0;

		bool any_changes = false;

		//check each condition; need extra counter to see how many variables have been accum'd in case one is removed
		size_t loop_iteration = 0;
		for(size_t i = 0; i < ocn.size(); i++, loop_iteration++)
		{
			//any null short circuits to a null
			if(EvaluableNode::IsNull(ocn[i]))
			{
				rc.NullifyNode(en);
				return true;
			}

			if(subtraction && loop_iteration == 1)
				positive_sign = false;

			if(ocn[i]->IsImmediate())
			{
				if(subtraction && loop_iteration == 0)
					continue;

				double value = EvaluableNode::ToNumber(ocn[i]);
				//keep a negative-zero subtrahend because removing it changes signed-zero results
				if(subtraction && loop_iteration > 0 && value == 0.0 && std::signbit(value))
					continue;
				if(positive_sign)
					accumulated_immediate += value;
				else
					accumulated_immediate -= value;

				//erase the node
				rc.FreeNodeTreeIfPossible(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;

				any_changes = true;
				continue;
			}

			//the minuend can't be combined with the terms that follow it, because a term minus an
			//equal term is only zero for finite numbers
			//if two non-immediates in a row, see if the same
			if(!(subtraction && i == 0)
				&& i + 1 < ocn.size() && ocn[i + 1] != nullptr && !ocn[i + 1]->IsImmediate())
			{
				if(EvaluableNode::AreDeepEqual(ocn[i], ocn[i + 1]))
				{
					//if currently accumulating, count another one, otherwise count first two
					if(currently_accumulating_nonimmediate)
					{
						if(positive_sign)
							nonimmediate_accum_multiplicand += 1.0;
						else
							nonimmediate_accum_multiplicand -= 1.0;
					}
					else
					{
						currently_accumulating_nonimmediate = true;
						nonimmediate_accum_multiplicand = (subtraction ? -2.0 : 2.0);
					}

					//erase the node
					rc.FreeNodeTreeIfPossible(ocn[i]);
					ocn.erase(begin(ocn) + i);

					//recheck this position next iteration
					i--;

					any_changes = true;
					continue;
				}
			}

			//a group of a subtraction is always part of the subtrahend, so it keeps its magnitude
			if(currently_accumulating_nonimmediate)
			{
				double grouped_multiplicand = (subtraction
					? -nonimmediate_accum_multiplicand : nonimmediate_accum_multiplicand);

				EvaluableNode *new_term = rc.enm->AllocNode(ENT_MULTIPLY);
				new_term->AppendOrderedChildNode(rc.enm->AllocNode(grouped_multiplicand));
				new_term->AppendOrderedChildNode(ocn[i]);
				ocn[i] = new_term;
			}

			//start over
			currently_accumulating_nonimmediate = false;
			nonimmediate_accum_multiplicand = 0.0;
		}

		//append the positive magnitude subtracted from the first operand
		if(accumulated_immediate != 0.0 && ocn.size() > 0)
			ocn.push_back(rc.enm->AllocNode(subtraction ? -accumulated_immediate : accumulated_immediate));

		//if every term folded away, the node is just the accumulated value
		if(ocn.size() == 0)
		{
			rc.FreeNodeChildNodesIfPossible(en);
			en->SetTypeViaNumberValue(accumulated_immediate);
			return true;
		}

		if(original_term_count > 1 && ocn.size() == 1)
		{
			//replace en with the remaining child node
			EvaluableNode *child = ocn[0];
			en->CopyNodeFrom(child);
			rc.FreeNodeIfPossible(child);
			return true;
		}

		return (any_changes || ocn.size() != original_term_count);
	}
};

struct FoldMultiplicationAndDivision final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_MULTIPLY || en->GetType() == ENT_DIVIDE);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() == 0)
		{
			if(en->GetType() == ENT_DIVIDE)
			{
				rc.NullifyNode(en);
			}
			else
			{
				rc.FreeNodeChildNodesIfPossible(en);
				en->ClearMetadata();
				en->SetTypeViaNumberValue(1.0);
			}
			return true;
		}

		bool division = (en->GetType() == ENT_DIVIDE);

		//a zero divisor ends the division, so the divisors can neither be regrouped nor reordered
		if(division && FindFirstZeroDivisor(ocn) != end(ocn))
			return false;

		bool multiplicand = true;

		//immediates that multiply the result
		double numerator_immediate = 1.0;
		//immediates that divide the result, accumulated as a direct product of the denominators
		//rather than as a reciprocal, so that (/ a 7 7) yields (/ a 49) exactly
		double denominator_immediate = 1.0;

		size_t original_term_count = ocn.size();

		bool currently_accumulating_nonimmediate = false;
		double nonimmediate_accum_exponent = 0.0;

		bool any_changes = false;

		//check each condition; need extra counter to see how many variables have been accum'd in case one is removed
		size_t loop_iteration = 0;
		for(size_t i = 0; i < ocn.size(); i++, loop_iteration++)
		{
			//any null short circuits to a null
			if(EvaluableNode::IsNull(ocn[i]))
			{
				rc.NullifyNode(en);
				return true;
			}

			if(division && loop_iteration == 1)
				multiplicand = false;

			if(ocn[i]->IsImmediate())
			{
				if(division && loop_iteration == 0)
					continue;

				double value = EvaluableNode::ToNumber(ocn[i]);
				if(multiplicand)
					numerator_immediate *= value;
				else
					denominator_immediate *= value;

				//erase the node
				rc.FreeNodeTreeIfPossible(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;

				any_changes = true;
				continue;
			}

			//the dividend can't be combined with the divisors that follow it, because a term divided
			//by an equal term is only one for finite nonzero numbers
			//if two non-immediates in a row, see if the same
			if(!(division && i == 0)
				&& i + 1 < ocn.size() && ocn[i + 1] != nullptr && !ocn[i + 1]->IsImmediate())
			{
				if(EvaluableNode::AreDeepEqual(ocn[i], ocn[i + 1]))
				{
					//if currently accumulating, count another one, otherwise count first two
					if(currently_accumulating_nonimmediate)
					{
						if(multiplicand)
							nonimmediate_accum_exponent += 1.0;
						else
							nonimmediate_accum_exponent -= 1.0;
					}
					else
					{
						currently_accumulating_nonimmediate = true;
						nonimmediate_accum_exponent = (division ? -2.0 : 2.0);
					}

					//erase the node
					rc.FreeNodeTreeIfPossible(ocn[i]);
					ocn.erase(begin(ocn) + i);

					//recheck this position next iteration
					i--;

					any_changes = true;
					continue;
				}
			}

			//a group of a division is always part of the divisor, so it keeps its exponent's magnitude
			if(currently_accumulating_nonimmediate)
			{
				double grouped_exponent = (division
					? -nonimmediate_accum_exponent : nonimmediate_accum_exponent);

				EvaluableNode *new_term = rc.enm->AllocNode(ENT_POW);
				new_term->AppendOrderedChildNode(ocn[i]);
				new_term->AppendOrderedChildNode(rc.enm->AllocNode(grouped_exponent));
				ocn[i] = new_term;
			}

			//start over
			currently_accumulating_nonimmediate = false;
			nonimmediate_accum_exponent = 0.0;
		}

		//if every term folded away, the node is just the accumulated value
		if(ocn.size() == 0)
		{
			rc.FreeNodeChildNodesIfPossible(en);
			en->SetTypeViaNumberValue(division
				? numerator_immediate / denominator_immediate : numerator_immediate);
			return true;
		}

		//add on the accumulated immediate at the end if it isn't the identity; for division this is
		//the product of the denominators, so it divides once by an exact value instead of
		//multiplying by a rounded reciprocal
		double remaining_immediate = (division ? denominator_immediate : numerator_immediate);
		if(remaining_immediate != 1.0)
			ocn.push_back(rc.enm->AllocNode(remaining_immediate));

		bool term_count_changed = (ocn.size() != original_term_count);

		if(original_term_count > 1 && ocn.size() == 1)
		{
			//replace en with the remaining child node
			EvaluableNode *child = ocn[0];
			en->CopyNodeFrom(child);
			rc.FreeNodeIfPossible(child);
			return true;
		}

		return (any_changes || term_count_changed);
	}
};

struct PowerSimplification final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_POW);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() == 0)
		{
			rc.NullifyNode(en);
			return true;
		}

		if(ocn.size() == 1)
		{
			EvaluableNode *child = ocn[0];
			en->CopyNodeFrom(child);
			rc.FreeNodeIfPossible(child);
			return true;
		}

		//ocn.size() > 1
		if(EvaluableNode::IsNull(ocn[0]) || EvaluableNode::IsNull(ocn[1]))
		{
			rc.NullifyNode(en);
			return true;
		}

		if(ocn[1]->GetType() == ENT_NUMBER)
		{
			double exponent = ocn[1]->GetNumberValue();
			if(exponent == 1.0)
			{
				EvaluableNode *child = ocn[0];
				en->CopyNodeFrom(child);
				rc.FreeNodeIfPossible(child);
				return true;
			}
			else if(exponent == 0.0 && ocn[0]->GetType() == ENT_NUMBER && ocn[0]->GetNumberValueReference() != 0.0)
			{
				rc.FreeNodeChildNodesIfPossible(en);
				en->SetTypeViaNumberValue(1.0);
				return true;
			}
		}

		return false;
	}
};

struct EulerExponentSimplification final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_EXPONENT);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() < 1)
			return false;

		if(EvaluableNode::IsNull(ocn[0]))
		{
			rc.NullifyNode(en);
			return true;
		}

		//an exp of a log can't be reduced to its parameter, because the log of a nonpositive number isn't a number
		return false;
	}
};

struct LogSimplification final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_LOG);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() != 1)
			return false;

		EvaluableNode *child = ocn[0];
		if(EvaluableNode::IsNull(child))
		{
			rc.NullifyNode(en);
			return true;
		}

		//ln(1) -> 0
		if(child->IsImmediate() && EvaluableNode::ToNumber(child) == 1.0)
		{
			rc.FreeNodeChildNodesIfPossible(en);
			en->SetTypeViaNumberValue(0.0);
			return true;
		}

		//ln(e) -> 1
		if(child->GetType() == ENT_EXPONENT && child->GetOrderedChildNodesReference().size() == 0)
		{
			rc.FreeNodeChildNodesIfPossible(en);
			en->SetTypeViaNumberValue(1.0);
			return true;
		}

		//a log of an exp can't be reduced to its parameter, because the exp may overflow to infinity
		//or underflow to zero
		return false;
	}
};

struct ConsolidateConstantsForConcat final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_CONCAT);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() == 0)
		{
			rc.NullifyNode(en);
			return true;
		}

		size_t original_term_count = ocn.size();

		//control variables for string accumulation
		std::string accumulating_string;
		bool currently_accumulating = false;

		//check each condition
		for(size_t i = 0; i < ocn.size(); i++)
		{
			//any null short circuits to a null
			if(EvaluableNode::IsNull(ocn[i]))
			{
				rc.NullifyNode(en);
				return true;
			}

			//if not immediate with another immediate following, then skip
			if(i + 1 >= ocn.size() || !ocn[i]->IsImmediate() || !ocn[i + 1]->IsImmediate())
			{
				if(currently_accumulating)
				{
					//since made it here, the previous value must have been immediate, so concat and overwrite
					accumulating_string += EvaluableNode::ToString(ocn[i]);
					ocn[i]->SetTypeViaStringIdValue(accumulating_string);
					currently_accumulating = false;
					accumulating_string.clear();
				}
			}
			else //immediate, followed by another immediate; delete and accumulate
			{
				currently_accumulating = true;
				accumulating_string += EvaluableNode::ToString(ocn[i]);

				//erase the node
				rc.FreeNodeTreeIfPossible(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;
			}
		}

		return (ocn.size() != original_term_count);
	}
};

struct SimplifySelfContainedWithImmediates final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (!en->IsTerminal() && IsEvaluableNodeTypeOfSimpleExecution(en->GetType()));
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		//need interpreter
		if(rc.interpreter == nullptr)
			return false;

		//check for any non-immediate child node
		bool any_non_immediate = false;
		if(en->IsAssociativeArray())
		{
			for(auto &cn : en->GetMappedChildNodesViewOnAssoc() | std::views::values)
			{
				if(cn != nullptr && !cn->IsImmediate())
				{
					any_non_immediate = true;
					break;
				}
			}
		}
		else if(!en->IsTerminal())
		{
			for(EvaluableNode *cn : en->GetOrderedChildNodesReference())
			{
				if(cn != nullptr && !cn->IsImmediate())
				{
					any_non_immediate = true;
					break;
				}
			}
		}

		if(!any_non_immediate)
		{
			EvaluableNodeReference result = rc.interpreter->InterpretNode(en);
			if(result.unique)
				rc.FreeNodeChildNodesIfPossible(en);

			en->CopyNodeFrom(result);
			return true;
		}

		return false;
	}
};

struct TruncateToValidParameters final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		size_t max_num_params = GetOpcodeMaxNumValidParameters(en->GetType());
		return (en->IsOrderedArray() && en->GetOrderedChildNodesReference().size() > max_num_params);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		size_t max_num_params = GetOpcodeMaxNumValidParameters(en->GetType());
		auto &ocn = en->GetOrderedChildNodesReference();
		bool extra_params = (ocn.size() >= max_num_params);
		if(extra_params && rc.nodesFreeable)
		{
			for(size_t i = max_num_params; i < ocn.size(); i++)
				rc.FreeNodeTreeIfPossible(ocn[i]);
		}
		ocn.resize(max_num_params);

		return extra_params;
	}
};

struct SortParameters final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto child_structure_type = GetChildNodeStructureType(en->GetType());
		return (child_structure_type == OpcodeDetails::ChildNodeStructureType::UNORDERED ||
				child_structure_type == OpcodeDetails::ChildNodeStructureType::ONE_POSITION_THEN_UNORDERED_OR_ONE_UNORDERED);
	}

	bool Rewrite(EvaluableNode *en, RuleContext &rc)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(GetChildNodeStructureType(en->GetType()) == OpcodeDetails::ChildNodeStructureType::UNORDERED)
			std::sort(begin(ocn), end(ocn), EvaluableNode::IsStrictlyLessThan);
		else if(ocn.size() > 2)
		{
			//a zero divisor ends the division, so the divisors from the first zero onward keep their order
			auto sort_end = (en->GetType() == ENT_DIVIDE ? FindFirstZeroDivisor(ocn) : end(ocn));
			std::sort(begin(ocn) + 1, sort_end, EvaluableNode::IsStrictlyLessThan);
		}

		//sorting alone shouldn't trigger a another analysis
		return false;
	}
};

static constexpr auto rule_registry = MakeRuleRegistry<
	TruncateToValidParameters,
	DeadCodeElimination,
	ShortCircuitBooleans,
	FlattenAssociations,
	SimplifySelfContainedWithImmediates,

	LogSimplification,
	EulerExponentSimplification,
	PowerSimplification,

	SortParameters,
	FoldAdditionAndSubtraction,
	FoldMultiplicationAndDivision,
	ConsolidateConstantsForConcat,	
	
	//sort again to yield canonical output
	SortParameters
>();

void EvaluableNodeTreeAlgebra::SimplifyNode(EvaluableNode *en, EvaluableNodeManager *enm,
	bool nodes_freeable, Interpreter *interpreter)
{
	bool any_changes = false;
	size_t total_count = 0;
	RuleContext rc{ enm, nodes_freeable, interpreter };
	do
	{
		any_changes = false;
		for(const auto &entry : rule_registry)
		{
			const auto &[match_fn, rewrite_fn] = entry;
			//rewrite_fn not applicable for nullptr node
			if(en != nullptr && match_fn(en))
				any_changes |= rewrite_fn(en, rc);

			if(interpreter != nullptr && interpreter->AreExecutionResourcesExhausted(true))
				return;
		}
		//limit in case it goes into an infinite loop for some reason
	} while(any_changes && ++total_count < 50);
}

EvaluableNode *EvaluableNodeTreeAlgebra::SimplifyTree(EvaluableNode *tree, EvaluableNodeManager *enm,
	Interpreter *interpreter)
{
	if(tree == nullptr)
		return nullptr;

	//node and bool indicating whether its child nodes have been processed yet
	std::vector<std::pair<EvaluableNode *, bool>> node_stack;
	EvaluableNode::ReferenceSetType visited;

	node_stack.emplace_back(tree, false);
	//can't free nodes if different parts of the tree may refer to each other
	bool nodes_freeable = !tree->GetNeedCycleCheck();

	while(!node_stack.empty())
	{
		auto [cur, child_nodes_seen] = node_stack.back();
		node_stack.pop_back();

		if(!child_nodes_seen)
		{
			if(!visited.insert(cur).second)
				continue;

			//put the node back on before its child nodes and indicate that its child nodes have been seen
			node_stack.emplace_back(cur, true);

			//traverse further down the tree
			if(cur->IsAssociativeArray())
			{
				for(auto &cn : cur->GetMappedChildNodesViewOnAssoc() | std::views::values)
				{
					if(cn && !cn->IsTerminal())
						node_stack.emplace_back(cn, false);
				}
			}
			else if(!cur->IsTerminal())
			{
				for(EvaluableNode *cn : cur->GetOrderedChildNodesReference())
				{
					if(cn && !cn->IsTerminal())
						node_stack.emplace_back(cn, false);
				}
			}

			continue;
		}

		SimplifyNode(cur, enm, nodes_freeable, interpreter);
	}

	EvaluableNodeManager::UpdateFlagsForNodeTree(tree);

	return tree;
}
