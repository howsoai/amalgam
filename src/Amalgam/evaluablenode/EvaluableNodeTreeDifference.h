#pragma once

//project headers:
#include "EvaluableNodeTreeManipulation.h"

class EvaluableNodeTreeDifference : public EvaluableNodeTreeManipulation
{
public:

	//functionality to merge two nodes
	class NodesMergeForDifferenceMethod : public NodesMergeMethod
	{
	public:
		NodesMergeForDifferenceMethod(EvaluableNodeManager *_enm)
			: NodesMergeMethod(_enm, false, true, false)
		{	}

		virtual EvaluableNode *MergeValues(EvaluableNode *a, EvaluableNode *b, bool must_merge = false);

		constexpr EvaluableNode::ReferenceAssocType &GetANodesIncluded()
		{		return aNodesIncluded;		}
		constexpr EvaluableNode::ReferenceAssocType &GetBNodesIncluded()
		{		return bNodesIncluded;		}

	protected:
		//key is the node from tree a or b, value is the node from the merged tree
		EvaluableNode::ReferenceAssocType aNodesIncluded;
		EvaluableNode::ReferenceAssocType bNodesIncluded;
	};

	//returns code that will transform tree1 into tree2, using allocations from enm
	static EvaluableNode *DifferenceTrees(EvaluableNodeManager *enm, EvaluableNode *tree1, EvaluableNode *tree2);

protected:

	//given a set of nodes, will traverse and populate each with a reference to its parent, in traversal order
	static void FindParentReferences(EvaluableNode *tree, EvaluableNode::ReferenceAssocType &references_with_parents, EvaluableNode *parent = nullptr);

	//given a set of nodes to be included (nodes_included, with the values being their matching original tree counterparts),
	// will traverse tree to find the topmost nodes excluded (top_nodes_excluded, with the values being their matching original tree counterparts) which is the parent of all of the subtrees that will be excluded
	// adds any nodes encountered to references_with_parents, as to be used for finding the paths to any of the nodes for creation and deletion
	static void FindTopNodesExcluded(EvaluableNode *tree, EvaluableNode::ReferenceAssocType &nodes_included,
		std::vector<EvaluableNode *> &top_nodes_excluded, EvaluableNode::ReferenceAssocType &references_with_parents, EvaluableNode *parent = nullptr);
};
