#pragma once

//project headers:
#include "EvaluableNode.h"

//forward declarations:
class Interpreter;

class EvaluableNodeTreeAlgebra
{
public:

	//simplifies the node at en and assumes en is writable
	//enm is required.  if nodes_freeable is true, then it will free any nodes not needed
	//if interpreter is specified, it will allow additional simplification
	static void SimplifyNode(EvaluableNode *en, EvaluableNodeManager *enm,
		bool nodes_freeable = false, Interpreter *interpreter = nullptr);

	//returns a simplified tree; it assumes the tree is unique and protected from garbage collection,
	//incase this method must interpret some nodes
	//enm is required.  if interpreter is specified, it will allow additional simplification
	static EvaluableNode *SimplifyTree(EvaluableNode *tree, EvaluableNodeManager *enm, Interpreter *interpreter = nullptr);
};
