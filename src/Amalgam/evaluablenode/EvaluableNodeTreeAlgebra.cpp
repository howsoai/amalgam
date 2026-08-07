//project headers:
#include "EvaluableNodeTreeAlgebra.h"
#include "Interpreter.h"

//system headers:
#include <array>
#include <utility>

//converts a stateless rule type into a pair of plain function pointers
template<class R>
constexpr auto MakeRuleEntry()
{
	//the lambdas are capture‑less; the leading + forces them to decay
	// to a plain function pointer (no std::function, no allocation).
	return std::pair{
		+[](EvaluableNode *en) -> bool { return R{}.Match(en); },
		+[](Interpreter *ip, EvaluableNode *en) { R{}.Rewrite(ip, en); }
	};
}

template<class... Rules>
constexpr auto MakeRuleRegistry()
{
	using entry_t = std::pair<
		bool(*)(EvaluableNode *),
		void(*)(Interpreter *, EvaluableNode *)>;

	return std::array<entry_t, sizeof...(Rules)>{ MakeRuleEntry<Rules>()... };
}

struct FlattenAssociations final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		return IsOpcodeAssociative(node_type);
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en)
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
				if(!ocn[i]->GetNeedCycleCheck())
					interpreter->evaluableNodeManager->FreeNode(ocn[i]);
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

struct DeadCodeElimination final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		return node_type == ENT_IF;
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en)
	{
		auto enm = interpreter->evaluableNodeManager;
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
				if(!ocn[i]->GetNeedCycleCheck())
					enm->FreeNodeTree(ocn[i]);
				if(!ocn[i + 1]->GetNeedCycleCheck())
					enm->FreeNodeTree(ocn[i + 1]);

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

				if(!child_node->GetNeedCycleCheck())
					enm->FreeNode(child_node);
			}
		}
	}
};

struct SimplifySelfContainedWithImmediates final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		return (!en->IsTerminal() && IsEvaluableNodeTypeOfSimpleExecution(node_type));
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en)
	{
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
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		size_t max_num_params = GetOpcodeMaxNumValidParameters(node_type);
		return (en->IsOrderedArray() && en->GetOrderedChildNodesReference().size() > max_num_params);
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en)
	{
		size_t max_num_params = GetOpcodeMaxNumValidParameters(en->GetType());
		auto &ocn = en->GetOrderedChildNodesReference();
		if(!en->GetNeedCycleCheck())
		{
			for(size_t i = max_num_params; i < ocn.size(); i++)
				interpreter->evaluableNodeManager->FreeNodeTree(ocn[i]);
		}
		ocn.resize(max_num_params);
	}
};

struct SortParameters final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		return (GetChildNodeStructureType(node_type) == OpcodeDetails::ChildNodeStructureType::UNORDERED);
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		std::sort(begin(ocn), end(ocn), EvaluableNode::IsStrictlyLessThan);
	}
};

static constexpr auto rule_registry = MakeRuleRegistry<
	FlattenAssociations,
	DeadCodeElimination,
	SimplifySelfContainedWithImmediates,
	//TODO 25662: make sure consolidated results are computed, e.g., addition of numbers and symbols
	TruncateToValidParameters,
	SortParameters
>();

inline static void ApplyRewriteRules(Interpreter *interpreter, EvaluableNode *en)
{
	for(const auto &entry : rule_registry)
	{
		const auto &[match_fn, rewrite_fn] = entry;
		if(match_fn(en))
			rewrite_fn(interpreter, en);
	}
}

EvaluableNode *EvaluableNodeTreeAlgebra::SimplifyTree(Interpreter *interpreter, EvaluableNode *tree)
{
	if(tree == nullptr)
		return nullptr;

	//node and bool indicating whether its child nodes have been processed yet
	std::vector<std::pair<EvaluableNode *, bool>> node_stack;
	EvaluableNode::ReferenceSetType visited;

	node_stack.emplace_back(tree, false);

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

		ApplyRewriteRules(interpreter, cur);
	}

	EvaluableNodeManager::UpdateFlagsForNodeTree(tree);

	return tree;
}
