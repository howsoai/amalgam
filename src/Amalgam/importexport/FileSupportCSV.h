#pragma once

//project headers:
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"

//forward declarations:
class ImportEntityStatus;

namespace FileSupportCSV
{
	EvaluableNode *Load(const std::string &resource_path, EvaluableNodeManager *enm, ImportEntityStatus &status);
	bool Store(EvaluableNode *code, const std::string &resource_path, EvaluableNodeManager *enm);
};
