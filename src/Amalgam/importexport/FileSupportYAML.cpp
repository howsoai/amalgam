//project headers:
#include "FileSupportYAML.h"

#include "EvaluableNodeTreeFunctions.h"
#include "FastMath.h"
#include "PlatformSpecific.h"
#include "StringManipulation.h"

//3rd party headers:
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "rapidyaml/rapidyaml-0.5.0.hpp"

//system headers:
#include <iostream>
#include <vector>

//transform yaml to an Amalgam node tree.  Only lists and assocs, and immediates  are supported.
EvaluableNode *YamlToEvaluableNodeRecurse(EvaluableNodeManager *enm, ryml::ConstNodeRef &element)
{
	if(element.is_seq())
	{
		EvaluableNode *node = enm->AllocNode(ENT_LIST);
		for(auto e : element.children())
			node->AppendOrderedChildNode(YamlToEvaluableNodeRecurse(enm, e));

		return node;
	}

	if(element.is_map())
	{
		EvaluableNode *node = enm->AllocNode(ENT_ASSOC);
		for(auto e : element.children())
		{
			auto key_value = e.key();
			std::string key(key_value.begin(), key_value.end());
			node->SetMappedChildNode(key, YamlToEvaluableNodeRecurse(enm, e));
		}

		return node;
	}

	if(element.val_is_null())
		return nullptr;

	auto value = element.val();
	std::string value_string(value.begin(), value.end());

	if(value.is_number())
	{
		auto [num, success] = Platform_StringToNumber(value_string);
		if(!success)
			return nullptr;

		return enm->AllocNode(num);
	}

	//must be a string
	return enm->AllocNode(ENT_STRING, value_string);
}

//transform en to a rapidyaml tree
//en must be guaranteed to not be nullptr
//if sort_keys is true, it will sort all of the assoc keys
//returns true if it was able to create a yaml correctly, false if there was problematic data
bool EvaluableNodeToYamlStringRecurse(EvaluableNode *en, ryml::NodeRef &built_element, bool sort_keys)
{
	if(en->IsAssociativeArray())
	{
		built_element |= ryml::MAP;
		auto &mcn = en->GetMappedChildNodesReference();
		if(!sort_keys)
		{
			for(auto &[cn_id, cn] : mcn)
			{
				const auto &str = string_intern_pool.GetStringFromID(cn_id);
				auto new_element = built_element.append_child();
				new_element << ryml::key(str);
				if(!EvaluableNodeToYamlStringRecurse(cn, new_element, sort_keys))
					return false;
			}
		}
		else //sort_keys
		{
			std::vector<StringInternPool::StringID> key_sids;
			key_sids.reserve(mcn.size());
			for(auto &[key, _] : mcn)
				key_sids.push_back(key);

			std::sort(begin(key_sids), end(key_sids), StringIDNaturalCompareSort);

			for(size_t i = 0; i < key_sids.size(); i++)
			{
				auto k = mcn.find(key_sids[i]);

				const auto &str = string_intern_pool.GetStringFromID(k->first);
				auto new_element = built_element.append_child();
				new_element << ryml::key(str);

				if(!EvaluableNodeToYamlStringRecurse(k->second, new_element, sort_keys))
					return false;
			}
		}
	}
	else if(!en->IsImmediate())
	{
		auto node_type = en->GetType();
		if(node_type == ENT_NULL)
		{
			//don't set anything
			return true;
		}
		else if(node_type == ENT_TRUE)
		{
			built_element << "true";
			return true;
		}
		else if(node_type == ENT_FALSE)
		{
			built_element << "false";
			return true;
		}
		else if(node_type != ENT_LIST)
		{
			//must be a list, so return false as can't build
			return false;
		}

		built_element |= ryml::SEQ;
		for(auto &cn : en->GetOrderedChildNodesReference())
		{
			auto new_element = built_element.append_child();
			EvaluableNodeToYamlStringRecurse(cn, new_element, sort_keys);
		}
	}
	else //immediate
	{
		if(DoesEvaluableNodeTypeUseNumberData(en->GetType()))
		{
			double number = en->GetNumberValue();
			built_element << number;
		}
		else
		{
			auto &str_value = en->GetStringValue();
			built_element << str_value;
		}
	}

	return true;
}

EvaluableNode *EvaluableNodeYAMLTranslation::YamlToEvaluableNode(EvaluableNodeManager *enm, std::string &yaml_str)
{
	ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yaml_str));

	ryml::ConstNodeRef yaml_top_element = tree.rootref();

	return YamlToEvaluableNodeRecurse(enm, yaml_top_element);
}

std::string EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(EvaluableNode *code, bool sort_keys)
{
	if(code == nullptr)
		return "null";

	//if need cycle check, double-check
	if(!EvaluableNode::CanNodeTreeBeFlattened(code))
		return "";

	ryml::Tree tree;
	auto top_node = tree.rootref();
	if(EvaluableNodeToYamlStringRecurse(code, top_node, sort_keys))
		return ryml::emitrs_yaml<std::string>(tree);
	else
		return "";
}

EvaluableNode *EvaluableNodeYAMLTranslation::Load(const std::string &resource_path, EvaluableNodeManager *enm)
{
	auto [data, data_success] = Platform_OpenFileAsString(resource_path);
	if(!data_success)
	{
		std::cerr << data << std::endl;
		return EvaluableNodeReference::Null();
	}

	ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(data));

	ryml::ConstNodeRef yaml_top_element = tree.rootref();

	return YamlToEvaluableNodeRecurse(enm, yaml_top_element);
}

bool EvaluableNodeYAMLTranslation::Store(EvaluableNode *code, const std::string &resource_path, EvaluableNodeManager *enm, bool sort_keys)
{
	std::string error_string;
	if(!Platform_IsResourcePathAccessible(resource_path, false, error_string))
	{
		std::cerr << "Error storing YAML: " << error_string << std::endl;
		return false;
	}

	std::ofstream file(resource_path);
	file << EvaluableNodeToYaml(code, sort_keys);

	return true;
}
