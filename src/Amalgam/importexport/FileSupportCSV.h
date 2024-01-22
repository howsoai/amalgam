#pragma once

//project headers:
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"

namespace FileSupportCSV
{
	EvaluableNode *Load(const std::string &resource_path, EvaluableNodeManager *enm);
	bool Store(EvaluableNode *code, const std::string &resource_path, EvaluableNodeManager *enm);
};
