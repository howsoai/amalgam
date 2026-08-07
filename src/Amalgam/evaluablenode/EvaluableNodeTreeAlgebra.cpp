//project headers:
#include "EvaluableNodeTreeAlgebra.h"
#include "Interpreter.h"

EvaluableNode *EvaluableNodeTreeAlgebra::NormalizeTree(Interpreter *interpreter, EvaluableNode *tree)
{
	if(tree == nullptr)
		return nullptr;

	auto *enm = interpreter->evaluableNodeManager;

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

		auto node_type = cur->GetType();

		//if associative and nested, then flatten
		if(IsOpcodeAssociative(node_type))
		{
			std::vector<EvaluableNode *> associative_child_nodes;
			auto &ocn = cur->GetOrderedChildNodesReference();
			for(size_t i = 0; i < ocn.size(); i++)
			{
				if(EvaluableNode::IsNull(ocn[i]) || !ocn[i]->IsOrderedArray())
					continue;

				if(ocn[i]->GetType() == node_type)
				{
					for(EvaluableNode *add_cn : ocn[i]->GetOrderedChildNodesReference())
						associative_child_nodes.push_back(add_cn);

					//erase the node
					if(!ocn[i]->GetNeedCycleCheck())
						enm->FreeNode(ocn[i]);
					ocn.erase(begin(ocn) + i);
					//need to recheck index since erased one
					i--;
				}
			}

			//append all at the end
			if(associative_child_nodes.size() != 0)
				cur->AppendOrderedChildNodes(associative_child_nodes);
		}

		//dead code elimination
		if(node_type == ENT_IF)
		{
			auto &ocn = cur->GetOrderedChildNodesReference();
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
					cur->CopyMetadataFrom(ocn[i + 1]);
					cur->CopyValueFrom(ocn[i + 1]);

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
					cur->ClearMetadata();
					cur->SetType(ENT_NULL, false);
				}

				if(ocn.size() == 1)
				{
					EvaluableNode *child_node = ocn[0];
					cur->CopyMetadataFrom(child_node);
					cur->CopyValueFrom(child_node);

					if(!child_node->GetNeedCycleCheck())
						enm->FreeNode(child_node);
				}
			}

			node_type = cur->GetType();
		}

		//if node_type is self-contained, and all child nodes are fully immediate, execute
		if(!cur->IsTerminal() && IsEvaluableNodeTypeOfSimpleExecution(node_type))
		{
			//check for any non-immediate child node
			bool any_non_immediate = false;
			if(cur->IsAssociativeArray())
			{
				for(auto &[_, cn] : cur->GetMappedChildNodesReference())
				{
					if(cn != nullptr && !cn->IsImmediate())
					{
						any_non_immediate = true;
						break;
					}
				}
			}
			else if(!cur->IsTerminal())
			{
				for(EvaluableNode *cn : cur->GetOrderedChildNodesReference())
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
				EvaluableNodeReference result = interpreter->InterpretNode(cur);
				cur->CopyValueFrom(result);
				cur->CopyMetadataFrom(result);
				node_type = cur->GetType();
			}
		}

		//TODO 25662: make sure consolidated results are computed, e.g., addition of numbers and symbols
		//TODO 25662: break out normalization on a single layer into its own method / rule so can be applied in mutate, or break into separate rules

		//truncate to maximum allowed child nodes
		size_t max_num_params = GetOpcodeMaxNumValidParameters(node_type);
		if(cur->IsOrderedArray() && cur->GetOrderedChildNodesReference().size() > max_num_params)
		{
			auto &ocn = cur->GetOrderedChildNodesReference();
			if(!cur->GetNeedCycleCheck())
			{
				for(size_t i = max_num_params; i < ocn.size(); i++)
					enm->FreeNodeTree(ocn[i]);
			}
			ocn.resize(max_num_params);
		}

		//sort to canonical order
		if(GetChildNodeStructureType(node_type) == OpcodeDetails::ChildNodeStructureType::UNORDERED)
		{
			auto &ocn = cur->GetOrderedChildNodesReference();
			std::sort(begin(ocn), end(ocn), EvaluableNode::IsStrictlyLessThan);
		}
	}

	return tree;
}

EvaluableNode *EvaluableNodeTreeAlgebra::SimplifyTree(Interpreter *interpreter, EvaluableNode *tree)
{
	tree = NormalizeTree(interpreter, tree);
	EvaluableNodeManager::UpdateFlagsForNodeTree(tree);
	return tree;
}
