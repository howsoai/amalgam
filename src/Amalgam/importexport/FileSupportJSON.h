#pragma once

//project headers:
#include "EntityExternalInterface.h"
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"

//system headers:
#include <string_view>

namespace EvaluableNodeJSONTranslation
{
	//converts JSON string_view to EvaluableNode tree
	EvaluableNode *JsonToEvaluableNode(EvaluableNodeManager *enm, std::string_view json_str);

	//converts EvaluableNode tree to JSON string. Returns false if EN cannot be converted to JSON
	// if sort_keys is true, it will sort all of the assoc keys
	std::pair<std::string, bool> EvaluableNodeToJson(EvaluableNode *code, bool sort_keys = false);

	//loads json file to EvaluableNode tree
	EvaluableNode *Load(const std::string &resource_path, EvaluableNodeManager *enm, ImportEntityStatus &status);
	
	//stores EvaluableNode tree to json file
	bool Store(EvaluableNode *code, const std::string &resource_path, EvaluableNodeManager *enm, bool sort_keys);
};
