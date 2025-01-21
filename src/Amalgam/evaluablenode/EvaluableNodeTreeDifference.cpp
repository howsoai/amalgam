//project headers:
#include "EvaluableNodeTreeDifference.h"
#include "EvaluableNodeTreeFunctions.h"

//system headers:

EvaluableNode *EvaluableNodeTreeDifference::NodesMergeForDifferenceMethod::MergeValues(EvaluableNode *a, EvaluableNode *b, bool must_merge)
{
	EvaluableNode *result = MergeTrees(this, a, b);

	//record what was included
	if(result != nullptr)
	{
		if(a != nullptr)
			aNodesIncluded[a] = result;
		if(b != nullptr)
			bNodesIncluded[b] = result;
	}
	
	return result;
}

EvaluableNode *EvaluableNodeTreeDifference::DifferenceTrees(EvaluableNodeManager *enm, EvaluableNode *tree1, EvaluableNode *tree2)
{
	//find commonality
	NodesMergeForDifferenceMethod mm(enm);
	EvaluableNode *anded_trees = mm.MergeValues(tree1, tree2);
	auto &tree1_to_merged_node = mm.GetANodesIncluded();
	auto &tree2_to_merged_node = mm.GetBNodesIncluded();

	//////////
	//build replace code

	//update difference function to: (declare )
	EvaluableNode *difference_function = enm->AllocNode(ENT_DECLARE);

	//update difference function to: (declare (assoc _ null) )
	EvaluableNode *df_vars = enm->AllocNode(ENT_ASSOC);
	df_vars->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI__), enm->AllocNode(ENT_NULL));
	difference_function->AppendOrderedChildNode(df_vars);

	//update difference function to: (declare (assoc _ null) (replace _ ) )
	EvaluableNode *df_replace = enm->AllocNode(ENT_REPLACE);
	difference_function->AppendOrderedChildNode(df_replace);
	df_replace->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, GetStringIdFromBuiltInStringId(ENBISI__)));

	//////////
	//find nodes that are mutually exclusive and create lookup tables

	//traverse tree1 looking for any references not included
	// if not included, then find path to node and create set of delete instructions
	std::vector<EvaluableNode *> tree1_top_nodes_excluded;
	EvaluableNode::ReferenceAssocType tree1_to_parent_node;
	FindTopNodesExcluded(tree1, tree1_to_merged_node, tree1_top_nodes_excluded, tree1_to_parent_node);

	//traverse tree2 looking for any references not included
	// if not included, then find path to node and create set of insert instructions
	std::vector<EvaluableNode *> tree2_top_nodes_excluded;
	EvaluableNode::ReferenceAssocType tree2_to_parent_node;
	FindTopNodesExcluded(tree2, tree2_to_merged_node, tree2_top_nodes_excluded, tree2_to_parent_node);

	EvaluableNode::ReferenceAssocType merged_references_with_parents;
	FindParentReferences(anded_trees, merged_references_with_parents);

	EvaluableNode::ReferenceAssocType merged_to_tree1_node;
	for(auto &[n1, n2] : tree1_to_merged_node)
		merged_to_tree1_node[n2] = n1;

	EvaluableNode::ReferenceAssocType merged_to_tree2_node;
	for(auto &[n1, n2] : tree2_to_merged_node)
		merged_to_tree2_node[n2] = n1;

	//find unique parent nodes that need to be replaced, but keep them in order
	std::vector<EvaluableNode *> merged_nodes_need_replacing;
	for(auto &parent : tree1_top_nodes_excluded)
	{
		EvaluableNode *merged_parent = nullptr;
		if(parent != nullptr)
			merged_parent = tree1_to_merged_node[parent];

		//don't modify the node more than once
		if(std::find(begin(merged_nodes_need_replacing), end(merged_nodes_need_replacing), merged_parent) == end(merged_nodes_need_replacing))
			merged_nodes_need_replacing.push_back(merged_parent);
	}
	for(auto &parent : tree2_top_nodes_excluded)
	{
		EvaluableNode *merged_parent = nullptr;
		if(parent != nullptr)
			merged_parent = tree2_to_merged_node[parent];

		//don't modify the node more than once
		if(std::find(begin(merged_nodes_need_replacing), end(merged_nodes_need_replacing), merged_parent) == end(merged_nodes_need_replacing))
			merged_nodes_need_replacing.push_back(merged_parent);
	}
	//start from bottom of tree and work way back up to top to ensure nodes are in original order
	std::reverse(begin(merged_nodes_need_replacing), end(merged_nodes_need_replacing));

	//////////
	//perform replacements

	//for all nodes that need to be replaced, replace with tree2's version, but retrieve all relevant child nodes from the tree2 version
	for(auto &node_to_replace : merged_nodes_need_replacing)
	{
		if(node_to_replace != nullptr)
		{
			EvaluableNode *path_to_replace = GetTraversalPathListFromAToB(enm, tree1_to_parent_node, merged_to_tree1_node[anded_trees], merged_to_tree1_node[node_to_replace]);
			df_replace->AppendOrderedChildNode(path_to_replace);
		}
		else //pointing to top-most node, so leave list access blank
			df_replace->AppendOrderedChildNode(enm->AllocNode(ENT_LIST));

		EvaluableNode *replacement_function = enm->AllocNode(ENT_LAMBDA);
		df_replace->AppendOrderedChildNode(replacement_function);

		//if node to replace is nullptr, then replace the parent object
		if(node_to_replace == nullptr)
		{
			replacement_function->AppendOrderedChildNode(tree2);
			break;
		}

		//make sure node replacing is actually in tree2 and find it
		auto tree2_node_reference = merged_to_tree2_node.find(node_to_replace);
		if(tree2_node_reference == end(merged_to_tree2_node))
			continue;
		EvaluableNode *tree2_node = merged_to_tree2_node[node_to_replace]; //need to reverse look up node_to_replace to get tree2's node
		if(tree2_node == nullptr)
			continue;
		//make a copy and make sure labels are escaped, then clear any child node lists
		// which will make sure there is a lower chance of reallocation when adding child nodes
		EvaluableNode *replacement = enm->AllocNode(tree2_node, EvaluableNodeManager::ENMM_LABEL_ESCAPE_INCREMENT);
		replacement->ClearOrderedChildNodes();

		//make sure it is of a data containing type, otherwise need to convert and then set_type
		auto replacement_type = replacement->GetType();
		if(replacement_type == ENT_LIST || replacement_type == ENT_ASSOC)
		{
			replacement_function->AppendOrderedChildNode(replacement);
		}
		else //need to create a list and transform it into (set_type ... type)
		{
			replacement->SetType(ENT_LIST, enm, false);
			EvaluableNode *set_type = enm->AllocNode(ENT_SET_TYPE);
			set_type->AppendOrderedChildNode(replacement);
			set_type->AppendOrderedChildNode(enm->AllocNode(ENT_STRING, GetStringFromEvaluableNodeType(replacement_type)));

			replacement_function->AppendOrderedChildNode(set_type);
		}

		//replace any ordered
		for(auto &cn : tree2_node->GetOrderedChildNodes())
		{
			auto merged = tree2_to_merged_node.find(cn);
			if(merged == end(tree2_to_merged_node) || (cn != nullptr && cn->GetType() == ENT_SYMBOL))
			{
				//use whatever was given for tree2
				replacement->AppendOrderedChildNode(cn);
			}
			else
			{
				//build (get (current_value 1) ...)
				EvaluableNode *retrieval = enm->AllocNode(ENT_GET);
				replacement->AppendOrderedChildNode(retrieval);
				EvaluableNode *target = enm->AllocNode(ENT_CURRENT_VALUE);
				target->AppendOrderedChildNode(enm->AllocNode(1.0));
				retrieval->AppendOrderedChildNode(target);

				//match up to tree1
				EvaluableNode *tree1_node = merged_to_tree1_node[node_to_replace];
				EvaluableNode *tree1_cn = merged_to_tree1_node[merged->second];

				//This should not happen.  Something weird happened.
				if(tree1_node == nullptr || tree1_cn == nullptr)
					continue;

				//get position from tree1
				auto position_in_merged = std::find(begin(tree1_node->GetOrderedChildNodes()), end(tree1_node->GetOrderedChildNodes()), tree1_cn);
				size_t index = std::distance(begin(tree1_node->GetOrderedChildNodes()), position_in_merged);
				retrieval->AppendOrderedChildNode(enm->AllocNode(static_cast<double>(index)));
			}
		}

		//replace any mapped
		for(auto &[cn_id, cn] : tree2_node->GetMappedChildNodes())
		{
			auto merged = tree2_to_merged_node.find(cn);
			if(merged == end(tree2_to_merged_node))
			{
				//use whatever was given for tree2
				replacement->SetMappedChildNode(cn_id, cn, true);
			}
			else
			{
				//build (get (current_value 1) ...)
				EvaluableNodeReference retrieval(enm->AllocNode(ENT_GET), true);
				replacement->SetMappedChildNode(cn_id, retrieval, true);
				EvaluableNode *target = enm->AllocNode(ENT_CURRENT_VALUE);
				target->AppendOrderedChildNode(enm->AllocNode(1.0));
				retrieval->AppendOrderedChildNode(target);

				EvaluableNodeReference key_node = Parser::ParseFromKeyStringId(cn_id, enm);
				retrieval->AppendOrderedChildNode(key_node);
				retrieval.UpdatePropertiesBasedOnAttachedNode(key_node);
			}
		}
	}

	return difference_function;
}

void EvaluableNodeTreeDifference::FindParentReferences(EvaluableNode *tree, EvaluableNode::ReferenceAssocType &references_with_parents, EvaluableNode *parent)
{
	if(tree == nullptr)
		return;
	
	//attempt to record the reference, but if already processed, skip
	if(references_with_parents.emplace(tree, parent).second == false)
		return;

	for(auto &cn : tree->GetOrderedChildNodes())
		FindParentReferences(cn, references_with_parents, tree);
	for(auto &[_, cn] : tree->GetMappedChildNodes())
		FindParentReferences(cn, references_with_parents, tree);
}

void EvaluableNodeTreeDifference::FindTopNodesExcluded(EvaluableNode *tree, EvaluableNode::ReferenceAssocType &nodes_included,
	std::vector<EvaluableNode *> &top_nodes_excluded, EvaluableNode::ReferenceAssocType &references_with_parents, EvaluableNode *parent)
{
	if(tree == nullptr)
		return;

	//attempt to record the reference, but if already processed, skip (also prevents infinite recursion in graph structures)
	if(references_with_parents.emplace(tree, parent).second == false)
		return;
	
	//if included, traverse tree, if not, insert as top excluded node
	auto included_node_found = nodes_included.find(tree);
	if(included_node_found == end(nodes_included))
		top_nodes_excluded.push_back(parent);
	else //node *itself* is included, now check to see if it is included with all its respective keys, and also its child nodes
	{
		//make sure matches; if any of the keys don't match, then it's excluded
		EvaluableNode *matching = (*included_node_found).second;
		if(matching == nullptr)
		{
			top_nodes_excluded.push_back(tree);
			//can't continue because matching is null
			return;
		}

		auto &tree_ocn = tree->GetOrderedChildNodes();
		auto &tree_mcn = tree->GetMappedChildNodes();

		if(matching->GetOrderedChildNodes().size() != tree_ocn.size())
			top_nodes_excluded.push_back(tree);

		auto &matching_mcn = matching->GetMappedChildNodes();

		if(matching_mcn.size() != tree_mcn.size())
			top_nodes_excluded.push_back(tree);

		//if any missing keys then also it is excluded -- needs to be recreated
		for(auto &[cn_id, _] : tree_mcn)
		{
			if(matching_mcn.find(cn_id) == end(matching_mcn))
			{
				top_nodes_excluded.push_back(tree);
				break;
			}
		}

		//check child nodes
		for(auto &cn : tree_ocn)
			FindTopNodesExcluded(cn, nodes_included, top_nodes_excluded, references_with_parents, tree);
		for(auto &[_, cn] : tree_mcn)
			FindTopNodesExcluded(cn, nodes_included, top_nodes_excluded, references_with_parents, tree);
	}
}
