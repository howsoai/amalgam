//project headers:
#include "EvaluableNodeTreeAlgebra.h"
#include "Interpreter.h"

//system headers:
#include <array>
#include <utility>

//converts a stateless rule type into a pair of plain function pointers
//implementations en is a valid pointer and guaranteed to not be nullptr
template<class R>
constexpr auto MakeRuleEntry()
{
	//the lambdas are capture‑less; the leading + forces them to decay
	// to a plain function pointer (no std::function, no allocation).
	return std::pair{
		+[](EvaluableNode *en) -> bool { return R{}.Match(en); },
		+[](EvaluableNode *en, EvaluableNodeManager *enm, bool nodes_freeable, Interpreter *interpreter)
		{ R{}.Rewrite(en, enm, nodes_freeable, interpreter); }
	};
}

template<class... Rules>
constexpr auto MakeRuleRegistry()
{
	using entry_t = std::pair<
		bool(*)(EvaluableNode *),
		void(*)(EvaluableNode *, EvaluableNodeManager *, bool, Interpreter *)>;

	return std::array<entry_t, sizeof...(Rules)>{ MakeRuleEntry<Rules>()... };
}

struct FlattenAssociations final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return IsOpcodeAssociative(en->GetType());
	}

	void Rewrite(EvaluableNode *en, EvaluableNodeManager *enm, bool nodes_freeable, Interpreter *interpreter)
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
				if(nodes_freeable)
					enm->FreeNode(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//need to recheck index since erased one
				i--;
			}
		}

		//append all at the end
		if(associative_child_nodes.size() != 0)
			en->AppendOrderedChildNodes(associative_child_nodes);
	}
};

struct DeadCodeEliminationInENT_IF final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_IF);
	}

	void Rewrite(EvaluableNode *en, EvaluableNodeManager *enm, bool nodes_freeable, Interpreter *interpreter)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		bool node_replaced = false;

		//check each condition
		for(size_t i = 0; i + 1 < ocn.size(); i += 2)
		{
			//if not immediate, then skip
			if(ocn[i] != nullptr && !ocn[i]->IsImmediate())
				continue;

			//found it, replace with this
			if(EvaluableNode::ToBool(ocn[i]))
			{
				//copy metadata first so don't clobber
				en->CopyMetadataFrom(ocn[i + 1]);
				en->CopyValueFrom(ocn[i + 1]);

				node_replaced = true;
				break;
			}
			else //remove this branch
			{
				//erase the node and the following
				if(nodes_freeable)
				{
					enm->FreeNodeTree(ocn[i]);
					enm->FreeNodeTree(ocn[i + 1]);
				}

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
				en->ClearMetadata();
				en->SetType(ENT_NULL, false);
			}

			if(ocn.size() == 1)
			{
				EvaluableNode *child_node = ocn[0];
				en->CopyMetadataFrom(child_node);
				en->CopyValueFrom(child_node);

				if(nodes_freeable)
					enm->FreeNode(child_node);
			}
		}
	}
};

struct ShortCircuitBooleans final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_AND || en->GetType() == ENT_OR);
	}

	void Rewrite(EvaluableNode *en, EvaluableNodeManager *enm, bool nodes_freeable, Interpreter *interpreter)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() == 0)
		{
			en->ClearMetadata();
			en->SetType(ENT_NULL, false);
			return;
		}

		bool short_circuit = false;
		//default to true
		bool short_circuit_value = true;

		bool is_or = (en->GetType() == ENT_OR);

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
				//erase the node
				if(nodes_freeable)
					enm->FreeNodeTree(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;
			}
			else //result is false
			{
				short_circuit = true;
				short_circuit_value = false;
			}
		}

		//if have eliminated all true values or short circuited, replace with a single value
		if(ocn.size() == 0 || short_circuit)
		{
			if(nodes_freeable)
			{
				for(auto &cn : ocn)
					enm->FreeNodeTree(cn);
			}

			en->ClearMetadata();
			en->SetTypeViaBoolValue(short_circuit_value);
		}
	}
};

struct FoldENT_ADD_and_SUBTRACT final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_ADD || en->GetType() == ENT_SUBTRACT);
	}

	void Rewrite(EvaluableNode *en, EvaluableNodeManager *enm, bool nodes_freeable, Interpreter *interpreter)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() == 0)
		{
			en->ClearMetadata();
			en->SetTypeViaNumberValue(0.0);
			return;
		}

		bool positive_sign = true;
		bool subtraction = (en->GetType() == ENT_SUBTRACT);

		double accumulated_immediate = 0.0;

		bool currently_accumulating_nonimmediate = false;
		double nonimmediate_accum_multiplicand = 0.0;

		//check each condition; need extra counter to see how many variables have been accum'd in case one is removed
		size_t loop_iteration = 0;
		for(size_t i = 0; i < ocn.size(); i++, loop_iteration++)
		{
			//any null short circuits to a null
			if(EvaluableNode::IsNull(ocn[i]))
			{
				if(nodes_freeable)
				{
					for(auto &cn : ocn)
						enm->FreeNodeTree(cn);
				}

				en->ClearMetadata();
				en->SetType(ENT_NULL, false);
				return;
			}

			if(subtraction && loop_iteration == 1)
				positive_sign = false;

			if(ocn[i]->IsImmediate())
			{
				double value = EvaluableNode::ToNumber(ocn[i]);;
				if(positive_sign)
					accumulated_immediate += value;
				else
					accumulated_immediate -= value;

				//erase the node
				if(nodes_freeable)
					enm->FreeNodeTree(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;

				continue;
			}

			//if two non-immediates in a row, see if the same
			if(i + 1 < ocn.size() && !ocn[i + 1]->IsImmediate())
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
					if(nodes_freeable)
						enm->FreeNodeTree(ocn[i]);
					ocn.erase(begin(ocn) + i);

					//recheck this position next iteration
					i--;

					continue;
				}
			}

			//if made it here, need to put a multiplicand in front of the node
			if(currently_accumulating_nonimmediate)
			{
				if(nonimmediate_accum_multiplicand == 0.0)
				{
					//erase the node
					if(nodes_freeable)
						enm->FreeNodeTree(ocn[i]);
					ocn.erase(begin(ocn) + i);

					//recheck this position next iteration
					i--;
				}
				else if(nonimmediate_accum_multiplicand != 1.0)
				{
					EvaluableNode *new_term = enm->AllocNode(ENT_MULTIPLY);
					new_term->AppendOrderedChildNode(enm->AllocNode(nonimmediate_accum_multiplicand));
					new_term->AppendOrderedChildNode(ocn[i]);
					ocn[i] = new_term;
				}
			}

			//start over
			currently_accumulating_nonimmediate = false;
			nonimmediate_accum_multiplicand = 0.0;
		}

		//add on accumulated_immediate at end if nonzero
		if(accumulated_immediate != 0.0)
		{
			if(ocn.size() > 0)
			{
				if(!subtraction)
					ocn.push_back(enm->AllocNode(accumulated_immediate));
				else
					ocn.push_back(enm->AllocNode(-accumulated_immediate));
			}
			else
			{
				en->ClearMetadata();
				en->SetTypeViaNumberValue(accumulated_immediate);
			}
		}
	}
};

struct ConsolidateConstantsENT_CONCAT final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (en->GetType() == ENT_CONCAT);
	}

	void Rewrite(EvaluableNode *en, EvaluableNodeManager *enm, bool nodes_freeable, Interpreter *interpreter)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() == 0)
		{
			en->ClearMetadata();
			en->SetType(ENT_NULL, false);
			return;
		}

		//control variables for string accumulation
		std::string accumulating_string;
		bool currently_accumulating = false;

		//check each condition
		for(size_t i = 0; i < ocn.size(); i++)
		{
			//any null short circuits to a null
			if(EvaluableNode::IsNull(ocn[i]))
			{
				if(nodes_freeable)
				{
					for(auto &cn : ocn)
						enm->FreeNodeTree(cn);
				}

				en->ClearMetadata();
				en->SetType(ENT_NULL, false);
				return;
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
				if(nodes_freeable)
					enm->FreeNodeTree(ocn[i]);

				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;
			}
		}
	}
};

struct SimplifySelfContainedWithImmediates final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		return (!en->IsTerminal() && IsEvaluableNodeTypeOfSimpleExecution(en->GetType()));
	}

	void Rewrite(EvaluableNode *en, EvaluableNodeManager *enm, bool nodes_freeable, Interpreter *interpreter)
	{
		//need interpreter
		if(interpreter == nullptr)
			return;

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
			EvaluableNodeReference result = interpreter->InterpretNode(en);
			en->CopyValueFrom(result);
			en->CopyMetadataFrom(result);
		}
	}
};

struct TruncateToValidParameters final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		size_t max_num_params = GetOpcodeMaxNumValidParameters(en->GetType());
		return (en->IsOrderedArray() && en->GetOrderedChildNodesReference().size() > max_num_params);
	}

	void Rewrite(EvaluableNode *en, EvaluableNodeManager *enm, bool nodes_freeable, Interpreter *interpreter)
	{
		size_t max_num_params = GetOpcodeMaxNumValidParameters(en->GetType());
		auto &ocn = en->GetOrderedChildNodesReference();
		if(nodes_freeable)
		{
			for(size_t i = max_num_params; i < ocn.size(); i++)
				enm->FreeNodeTree(ocn[i]);
		}
		ocn.resize(max_num_params);
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

	void Rewrite(EvaluableNode *en, EvaluableNodeManager *enm, bool nodes_freeable, Interpreter *interpreter)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		if(GetChildNodeStructureType(en->GetType()) == OpcodeDetails::ChildNodeStructureType::UNORDERED)
			std::sort(begin(ocn), end(ocn), EvaluableNode::IsStrictlyLessThan);
		else if(ocn.size() > 2)
			std::sort(begin(ocn) + 1, end(ocn), EvaluableNode::IsStrictlyLessThan);
	}
};

static constexpr auto rule_registry = MakeRuleRegistry<
	FlattenAssociations,
	DeadCodeEliminationInENT_IF,
	SimplifySelfContainedWithImmediates,
	ShortCircuitBooleans,
	SortParameters,
	FoldENT_ADD_and_SUBTRACT,
	//TODO 25662: make sure consolidated results are computed for multiplication, subtraction, division; for multiplication, change -1 multiplication into (- value)
	//TODO 25662: consolidate already factored terms
	//TODO 25662: add logic to rerun engine if any change occurred
	//TODO 25662: properly account for execution count for rule engine
	ConsolidateConstantsENT_CONCAT,
	TruncateToValidParameters,
	SortParameters
>();

void EvaluableNodeTreeAlgebra::SimplifyNode(EvaluableNode *en, EvaluableNodeManager *enm,
	bool nodes_freeable, Interpreter *interpreter)
{
	for(const auto &entry : rule_registry)
	{
		const auto &[match_fn, rewrite_fn] = entry;
		//rewrite_fn not applicable for nullptr node
		if(en != nullptr && match_fn(en))
			rewrite_fn(en, enm, nodes_freeable, interpreter);
	}
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
