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
			enm->FreeNode(en);
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
	// to a plain function pointer (no std::function, no allocation).
	return std::pair{
		+[](EvaluableNode *en) -> bool { return R{}.Match(en); },
		+[](EvaluableNode *en, RuleContext &rc) -> bool
		{ return R{}.Rewrite(en, rc); }
	};
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
		std::vector<EvaluableNode *> associative_child_nodes;
		auto &ocn = en->GetOrderedChildNodesReference();
		for(size_t i = 0; i < ocn.size(); i++)
		{
			if(EvaluableNode::IsNull(ocn[i]) || !ocn[i]->IsOrderedArray())
				continue;

			if(ocn[i]->GetType() == en->GetType())
			{
				for(EvaluableNode *add_cn : ocn[i]->GetOrderedChildNodesReference())
					associative_child_nodes.push_back(add_cn);

				//erase the node
				rc.FreeNodeIfPossible(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//need to recheck index since erased one
				i--;
			}
		}

		//append all at the end
		if(associative_child_nodes.size() != 0)
		{
			en->AppendOrderedChildNodes(associative_child_nodes);
			return true;
		}

		return false;
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

			//found it, replace with this
			if(EvaluableNode::ToBool(ocn[i]))
			{
				en->CopyNodeFrom(ocn[i + 1]);

				node_replaced = true;
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

			any_changes = true;
		}

		//check for low number of parameters
		if(!node_replaced)
		{
			if(ocn.size() == 0)
			{
				rc.NullifyNode(en);
				any_changes = true;
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
			}

			any_changes = true;
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

				continue;
			}

			//if two non-immediates in a row, see if the same
			if(i + 1 < ocn.size() && ocn[i + 1] != nullptr && !ocn[i + 1]->IsImmediate())
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
						if(!subtraction)
							nonimmediate_accum_multiplicand = 2.0;
						else
						{
							//first two cancel out, otherwise negative
							if(positive_sign)
								nonimmediate_accum_multiplicand = 0.0;
							else
								nonimmediate_accum_multiplicand = -2.0;
						}
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

			//a group at index zero contains the distinguished minuend and keeps its sign
			if(currently_accumulating_nonimmediate)
			{
				double grouped_multiplicand = ( (subtraction && i > 0)
					? -nonimmediate_accum_multiplicand : nonimmediate_accum_multiplicand);
				if(grouped_multiplicand == 0.0)
				{
					if(i == 0)
					{
						//need to keep a zero at the start
						rc.FreeNodeTreeIfPossible(ocn[i]);
						ocn[i] = rc.enm->AllocNode(0.0);
					}
					else
					{
						rc.FreeNodeTreeIfPossible(ocn[i]);
						ocn.erase(begin(ocn) + i);
						i--;
					}
				}
				else if(grouped_multiplicand != 1.0)
				{
					EvaluableNode *new_term = rc.enm->AllocNode(ENT_MULTIPLY);
					new_term->AppendOrderedChildNode(rc.enm->AllocNode(grouped_multiplicand));
					new_term->AppendOrderedChildNode(ocn[i]);
					ocn[i] = new_term;
				}
			}

			//start over
			currently_accumulating_nonimmediate = false;
			nonimmediate_accum_multiplicand = 0.0;
		}

		//append the positive magnitude subtracted from the first operand
		if(accumulated_immediate != 0.0)
		{
			if(ocn.size() > 0)
				ocn.push_back(rc.enm->AllocNode(subtraction ? -accumulated_immediate : accumulated_immediate));
			else
				en->SetTypeViaNumberValue(accumulated_immediate);
		}

		if(subtraction && original_term_count > 1)
		{
			if(ocn.size() == 0)
			{
				rc.FreeNodeChildNodesIfPossible(en);
				en->SetTypeViaNumberValue(0.0);
			}
			else if(ocn.size() == 1)
			{
				en->SetType(ENT_MULTIPLY, false);
			}
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

		bool multiplicand = true;
		bool division = (en->GetType() == ENT_DIVIDE);

		double product_immediate = 1.0;

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
				//runtime division stops at a zero divisor, so it cannot be regrouped
				if(division && !multiplicand && value == 0.0)
					continue;
				if(multiplicand)
					product_immediate *= value;
				else
					product_immediate /= value;

				//erase the node
				rc.FreeNodeTreeIfPossible(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;

				continue;
			}

			//if two non-immediates in a row, see if the same
			if(i + 1 < ocn.size() && ocn[i + 1] != nullptr && !ocn[i + 1]->IsImmediate())
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
						if(!division)
							nonimmediate_accum_exponent = 2.0;
						else
						{
							//first two cancel out, otherwise negative
							if(multiplicand)
								nonimmediate_accum_exponent = 0.0;
							else
								nonimmediate_accum_exponent = -2.0;
						}
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

			//a group at index zero contains the distinguished dividend and keeps its exponent
			if(currently_accumulating_nonimmediate)
			{
				double grouped_exponent = ( (division && i > 0)
					? -nonimmediate_accum_exponent : nonimmediate_accum_exponent);
				if(grouped_exponent == 0.0)
				{
					if(i == 0)
					{
						rc.FreeNodeTreeIfPossible(ocn[i]);
						ocn[i] = rc.enm->AllocNode(1.0);
					}
					else
					{
						rc.FreeNodeTreeIfPossible(ocn[i]);
						ocn.erase(begin(ocn) + i);
						i--;
					}
				}
				else if(grouped_exponent != 1.0)
				{
					EvaluableNode *new_term = rc.enm->AllocNode(ENT_POW);
					new_term->AppendOrderedChildNode(ocn[i]);
					new_term->AppendOrderedChildNode(rc.enm->AllocNode(grouped_exponent));
					ocn[i] = new_term;
				}
			}

			//start over
			currently_accumulating_nonimmediate = false;
			nonimmediate_accum_exponent = 0.0;
		}

		//add on accumulated_immediate at end if nonzero
		if(product_immediate != 1.0)
		{
			if(ocn.size() > 0)
			{
				ocn.push_back(rc.enm->AllocNode(division ? 1.0 / product_immediate : product_immediate));
			}
			else
			{
				rc.FreeNodeChildNodesIfPossible(en);
				en->SetTypeViaNumberValue(product_immediate);
			}
		}

		if(division && original_term_count > 1)
		{
			if(ocn.size() == 0)
			{
				rc.FreeNodeChildNodesIfPossible(en);
				en->SetTypeViaNumberValue(1.0);
			}
			else if(ocn.size() == 1)
			{
				en->SetType(ENT_MULTIPLY, false);
			}
		}

		return (any_changes || ocn.size() != original_term_count);
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
		if(EvaluableNode::IsNull(ocn[1]))
		{
			rc.NullifyNode(en);
			return true;
		}

		if(ocn[1]->GetType() == ENT_NUMBER && ocn[1]->GetNumberValue() == 1.0)
		{
			EvaluableNode *child = ocn[0];
			en->CopyNodeFrom(child);
			rc.FreeNodeIfPossible(child);
			return true;
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

		EvaluableNode *child = ocn[0];
		if(EvaluableNode::IsNull(child))
		{
			rc.NullifyNode(en);
			return true;
		}

		if(child->GetType() == ENT_LOG)
		{
			auto &log_ocn = child->GetOrderedChildNodesReference();
			if(log_ocn.size() == 1)
			{
				en->CopyNodeFrom(log_ocn[0]);
				rc.FreeNodeIfPossible(child);
				return true;
			}
		}

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

		if(child->GetType() == ENT_EXPONENT)
		{
			auto &log_ocn = child->GetOrderedChildNodesReference();
			if(log_ocn.size() == 1)
			{
				en->CopyNodeFrom(log_ocn[0]);
				rc.FreeNodeIfPossible(child);
				return true;
			}
		}

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
			for(auto &[_, cn] : en->GetMappedChildNodesReference())
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
			std::sort(begin(ocn) + 1, end(ocn), EvaluableNode::IsStrictlyLessThan);

		//sorting alone shouldn't trigger a another analysis
		return false;
	}
};

static constexpr auto rule_registry = MakeRuleRegistry<
	FlattenAssociations,
	DeadCodeElimination,
	SimplifySelfContainedWithImmediates,
	ShortCircuitBooleans,
	SortParameters,
	FoldAdditionAndSubtraction,
	FoldMultiplicationAndDivision,
	ConsolidateConstantsForConcat,
	PowerSimplification,
	EulerExponentSimplification,
	LogSimplification,
	TruncateToValidParameters,
	//sort again to yield canonical output
	SortParameters
>();

void EvaluableNodeTreeAlgebra::SimplifyNode(EvaluableNode *en, EvaluableNodeManager *enm,
	bool nodes_freeable, Interpreter *interpreter)
{
	bool any_changes = false;
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
	} while(any_changes);
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
				for(auto &[_, cn] : cur->GetMappedChildNodesReference())
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
