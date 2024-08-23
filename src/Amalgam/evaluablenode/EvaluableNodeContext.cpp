//project headers:
#include "EvaluableNodeContext.h"

void EvaluableNodeContext::VerifyEvaluableNodeIntegrity()
{
	for(EvaluableNode *en : *nodeStackNodes)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en, nullptr, false);

	auto &nr = evaluableNodeManager->GetNodesReferenced();
	for(auto &[en, _] : nr.nodesReferenced)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en, nullptr, false);
}

