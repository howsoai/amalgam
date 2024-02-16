#pragma once

//project headers:
#include "EntityExternalInterface.h"
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"

namespace FileSupportCSV
{
	EvaluableNode *Load(const std::string &resource_path, EvaluableNodeManager *enm, EntityExternalInterface::LoadEntityStatus &status);
	bool Store(EvaluableNode *code, const std::string &resource_path, EvaluableNodeManager *enm);
};
