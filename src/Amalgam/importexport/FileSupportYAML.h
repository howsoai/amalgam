#pragma once

//project headers:
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"

//system headers:
#include <string_view>

namespace EvaluableNodeYAMLTranslation
{
	//converts YAML string_view to EvaluableNode tree
	EvaluableNode *YamlToEvaluableNode(EvaluableNodeManager *enm, std::string &yaml_str);

	//converts EvaluableNode tree to YAML string
	// if sort_keys is true, it will sort all of the assoc keys
	std::string EvaluableNodeToYaml(EvaluableNode *code, bool sort_keys = false);

	//loads yaml file to EvaluableNode tree
	EvaluableNode *Load(const std::string &resource_path, EvaluableNodeManager *enm);
	
	//stores EvaluableNode tree to yaml file
	bool Store(EvaluableNode *code, const std::string &resource_path, EvaluableNodeManager *enm, bool sort_keys);
};
