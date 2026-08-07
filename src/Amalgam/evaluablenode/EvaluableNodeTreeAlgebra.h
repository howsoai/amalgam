#pragma once

//project headers:
#include "EvaluableNode.h"

//forward declarations:
class Interpreter;

class EvaluableNodeTreeAlgebra
{
public:
	//returns a simplified tree; it assumes the tree is unique and protected from garbage collection,
	//incase this method must interpret some nodes
	static EvaluableNode *SimplifyTree(Interpreter *interpreter, EvaluableNode *tree);
};
