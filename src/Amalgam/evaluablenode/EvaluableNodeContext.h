#pragma once

//project headers:
#include "EvaluableNodeManagement.h"
#include "RandomStream.h"

class EvaluableNodeContext
{
public:
	EvaluableNodeContext(EvaluableNodeManager *enm, RandomStream rand_stream)
		: evaluableNodeManager{ enm }
		, randomStream{ rand_stream }
	{}

	//creates a stack state saver for the interpreterNodeStack, which will be restored back to its previous condition when this object is destructed
	__forceinline EvaluableNodeStackStateSaver CreateNodeStackStateSaver()
	{
		return EvaluableNodeStackStateSaver(nodeStackNodes);
	}

	//like CreateNodeStackStateSaver, but also pushes another node on the stack
	__forceinline EvaluableNodeStackStateSaver CreateNodeStackStateSaver(EvaluableNode *en)
	{
		//count on C++ return value optimization to not call the destructor
		return EvaluableNodeStackStateSaver(nodeStackNodes, en);
	}

protected:
	//a stack (list) of the current nodes being executed
	std::vector<EvaluableNode *> *nodeStackNodes{ nullptr };

public:
	//where to allocate new nodes
	EvaluableNodeManager *const evaluableNodeManager;

	//random stream to get random numbers from
	RandomStream randomStream;

	//ensures that there are no reachable nodes that are deallocated
	void VerifyEvaluableNodeIntegrity();
};

