#pragma once

//project headers:
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"

class FileSupportCSV
{
public:
	static EvaluableNode *Load(const std::string &resource_path, EvaluableNodeManager *enm);
	static bool Store(EvaluableNode *code, const std::string &resource_path, EvaluableNodeManager *enm);
};
