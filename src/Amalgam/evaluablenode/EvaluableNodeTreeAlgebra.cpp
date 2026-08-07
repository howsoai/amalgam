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
		+[](Interpreter *interpreter, EvaluableNode *en, bool nodes_freeable)
		{ R{}.Rewrite(interpreter, en, nodes_freeable); }
	};
}

template<class... Rules>
constexpr auto MakeRuleRegistry()
{
	using entry_t = std::pair<
		bool(*)(EvaluableNode *),
		void(*)(Interpreter *, EvaluableNode *, bool)>;

	return std::array<entry_t, sizeof...(Rules)>{ MakeRuleEntry<Rules>()... };
}

struct FlattenAssociations final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		return IsOpcodeAssociative(node_type);
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en, bool nodes_freeable)
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

struct DeadCodeEliminationInENT_IF final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		return (node_type == ENT_IF);
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en, bool nodes_freeable)
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

struct ShortCircuitENT_AND final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		return (node_type == ENT_AND);
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en, bool nodes_freeable)
	{
		auto enm = interpreter->evaluableNodeManager;
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

		//check each condition
		for(size_t i = 0; i < ocn.size(); i++)
		{
			//if not immediate, then skip
			if(ocn[i] != nullptr && !ocn[i]->IsImmediate())
				continue;

			//eliminate or short-circuit based on value
			if(EvaluableNode::ToBool(ocn[i]))
			{
				//erase the node and the following
				if(nodes_freeable)
					enm->FreeNodeTree(ocn[i]);
				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;
			}
			else //entirity is false
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

struct ShortCircuitENT_OR final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		return (node_type == ENT_OR);
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en, bool nodes_freeable)
	{
		auto enm = interpreter->evaluableNodeManager;
		auto &ocn = en->GetOrderedChildNodesReference();
		if(ocn.size() == 0)
		{
			en->ClearMetadata();
			en->SetType(ENT_NULL, false);
			return;
		}

		bool short_circuit = false;
		//default to false
		bool short_circuit_value = false;

		//check each condition
		for(size_t i = 0; i < ocn.size(); i++)
		{
			//if not immediate, then skip
			if(ocn[i] != nullptr && !ocn[i]->IsImmediate())
				continue;

			//eliminate or short-circuit based on value
			if(!EvaluableNode::ToBool(ocn[i]))
			{
				//erase the node and the following
				if(nodes_freeable)
					enm->FreeNodeTree(ocn[i]);

				ocn.erase(begin(ocn) + i);

				//recheck this position next iteration
				i--;
			}
			else //entirity is true
			{
				short_circuit = true;
				short_circuit_value = true;
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

struct SimplifySelfContainedWithImmediates final
{
	bool Match(EvaluableNode *en) const noexcept
	{
		auto node_type = (en != nullptr ? en->GetType() : ENT_NULL);
		return (!en->IsTerminal() && IsEvaluableNodeTypeOfSimpleExecution(node_type));
	}

	void Rewrite(Interpreter *interpreter, EvaluableNode *en, bool nodes_freeable)
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

	void Rewrite(Interpreter *interpreter, EvaluableNode *en, bool nodes_freeable)
	{
		size_t max_num_params = GetOpcodeMaxNumValidParameters(en->GetType());
		auto &ocn = en->GetOrderedChildNodesReference();
		if(nodes_freeable)
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

	void Rewrite(Interpreter *interpreter, EvaluableNode *en, bool nodes_freeable)
	{
		auto &ocn = en->GetOrderedChildNodesReference();
		std::sort(begin(ocn), end(ocn), EvaluableNode::IsStrictlyLessThan);
	}
};

static constexpr auto rule_registry = MakeRuleRegistry<
	FlattenAssociations,
	DeadCodeEliminationInENT_IF,
	SimplifySelfContainedWithImmediates,
	ShortCircuitENT_AND,
	ShortCircuitENT_OR,
	//TODO 25662: make sure consolidated results are computed, e.g., addition of numbers and symbols
	TruncateToValidParameters,
	SortParameters
>();

inline static void ApplyRewriteRules(Interpreter *interpreter, EvaluableNode *en, bool nodes_freeable)
{
	for(const auto &entry : rule_registry)
	{
		const auto &[match_fn, rewrite_fn] = entry;
		if(match_fn(en))
			rewrite_fn(interpreter, en, nodes_freeable);
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

		ApplyRewriteRules(interpreter, cur, nodes_freeable);
	}

	EvaluableNodeManager::UpdateFlagsForNodeTree(tree);

	return tree;
}
