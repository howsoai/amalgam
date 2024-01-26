//project headers:
#include "FileSupportJSON.h"

#include "EvaluableNodeTreeFunctions.h"
#include "FastMath.h"
#include "PlatformSpecific.h"
#include "StringManipulation.h"

//3rd party headers:
#include "simdjson/simdjson.h"

//system headers:
#include <fstream>
#include <iostream>
#include <vector>

//per simdjson documentation, for multithreading, there should be one of these per thread
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
simdjson::ondemand::parser json_parser;

//transform json to an Amalgam node tree.  Only lists and assocs, and immediates  are supported.
EvaluableNode *JsonToEvaluableNodeRecurse(EvaluableNodeManager *enm, simdjson::ondemand::value element)
{
	switch(element.type())
	{
	case simdjson::ondemand::json_type::array:
	{
		EvaluableNode *node = enm->AllocNode(ENT_LIST);
		for(auto e : element.get_array())
			node->AppendOrderedChildNode(JsonToEvaluableNodeRecurse(enm, e.value()));

		return node;
	}

	case simdjson::ondemand::json_type::object:
	{
		EvaluableNode *node = enm->AllocNode(ENT_ASSOC);
		for(auto e : element.get_object())
		{
			std::string_view key_view = e.unescaped_key();
			std::string key(key_view);
			node->SetMappedChildNode(key, JsonToEvaluableNodeRecurse(enm, e.value()));
		}

		return node;
	}

	case simdjson::ondemand::json_type::number:
		return enm->AllocNode(element.get_double());

	case simdjson::ondemand::json_type::string:
	{
		std::string_view str_view = element.get_string();
		std::string str(str_view);
		return enm->AllocNode(ENT_STRING, str);
	}

	case simdjson::ondemand::json_type::boolean:
	{
		if(element.get_bool())
			return enm->AllocNode(ENT_TRUE);
		else
			return enm->AllocNode(ENT_FALSE);
	}

	case simdjson::ondemand::json_type::null:
		return nullptr;
	}

	return nullptr;
}

//escapes str with json standards and appends to json_str
inline void EscapeAndAppendStringToJsonString(const std::string &str, std::string &json_str)
{
	json_str += '"';

	for(size_t i = 0; i < str.size(); i++)
	{
		auto c = str[i];
		switch(c)
		{
			case '"':	json_str += "\\\"";	break;
			case '\\':	json_str += "\\\\";	break;
			case '\b':	json_str += "\\b";	break;
			case '\f':	json_str += "\\f";	break;
			case '\n':	json_str += "\\n";	break;
			case '\r':	json_str += "\\r";	break;
			case '\t':	json_str += "\\t";	break;
			default:
			{
				if(static_cast<uint8_t>(c) <= 0x1f)
				{
					//escape control characters
					char buffer[8];
					snprintf(&buffer[0], sizeof(buffer), "\\u%04x", c);
					json_str += &buffer[0];
					break;
				}
				
				//the ECMA 404 standard for json makes no mention about LS and PS characters, but they are known
				// to be a problem with some systems that use ECMA 262 versions prior to 10 (released in the year 2019)
				// so we escape these two code points just to be safe on all systems
				if(i + 3 < str.size())
				{
					if(static_cast<uint8_t>(c) == 0xe2)
					{
						//escape utf-8 line separator https://www.fileformat.info/info/unicode/char/2028/index.htm
						if(static_cast<uint8_t>(str[i + 1]) == 0x80 && static_cast<uint8_t>(str[i + 2]) == 0xa8)
						{
							json_str += "\\u2028";
							i += 2;
							break;
						}
						//escape utf-8 paragraph separator https://www.fileformat.info/info/unicode/char/2029/index.htm
						else if(static_cast<uint8_t>(str[i + 1]) == 0x80 && static_cast<uint8_t>(str[i + 2]) == 0xa9)
						{
							json_str += "\\u2029";
							i += 2;
							break;
						}
					}
				}

				//wasn't a special character, just concatenate
				json_str += c;
			}
		}
	}

	json_str += '"';
}

//transform en to a json string
//en must be guaranteed to not be nullptr
//if sort_keys is true, it will sort all of the assoc keys
//returns true if it was able to create a json correctly, false if there was problematic data
bool EvaluableNodeToJsonStringRecurse(EvaluableNode *en, std::string &json_str, bool sort_keys)
{
	if(en->IsAssociativeArray())
	{
		json_str += '{';

		auto &mcn = en->GetMappedChildNodesReference();

		if(!sort_keys)
		{
			bool first_cn = true;
			for(auto &[cn_id, cn] : mcn)
			{
				if(!first_cn)
					json_str += ',';
				else
					first_cn = false;

				auto str = string_intern_pool.GetStringFromID(cn_id);
				EscapeAndAppendStringToJsonString(str, json_str);

				json_str += ':';

				if(cn == nullptr)
					json_str += "null";
				else
				{
					if(!EvaluableNodeToJsonStringRecurse(cn, json_str, sort_keys))
						return false;
				}
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

				if(i > 0)
					json_str += ',';

				auto str = string_intern_pool.GetStringFromID(key_sids[i]);
				EscapeAndAppendStringToJsonString(str, json_str);

				json_str += ':';

				if(k->second == nullptr)
					json_str += "null";
				else
				{
					if(!EvaluableNodeToJsonStringRecurse(k->second, json_str, sort_keys))
						return false;
				}
			}
		}

		json_str += '}';
	}
	else if(!en->IsImmediate())
	{
		auto node_type = en->GetType();
		if(node_type == ENT_NULL)
		{
			json_str += "null";
			return true;
		}
		else if(node_type == ENT_TRUE)
		{
			json_str += "true";
			return true;
		}
		else if(node_type == ENT_FALSE)
		{
			json_str += "false";
			return true;
		}
		else if(node_type != ENT_LIST)
		{
			//must be a list, so return false as can't build
			return false;
		}

		json_str += '[';

		bool first_cn = true;
		for(auto &cn : en->GetOrderedChildNodesReference())
		{
			if(!first_cn)
				json_str += ',';
			else
				first_cn = false;

			if(cn == nullptr)
			{
				json_str += "null";
			}
			else
			{
				if(!EvaluableNodeToJsonStringRecurse(cn, json_str, sort_keys))
					return false;
			}
		}

		json_str += ']';
	}
	else //immediate
	{
		if(DoesEvaluableNodeTypeUseNumberData(en->GetType()))
		{
			double number = en->GetNumberValueReference();

			if(number == std::numeric_limits<double>::infinity())
				json_str += StringManipulation::NumberToString(std::numeric_limits<double>::max());
			else if(number == -std::numeric_limits<double>::infinity())
				json_str += StringManipulation::NumberToString(std::numeric_limits<double>::lowest());
			else if(FastIsNaN(number))
				return false;
			else
				json_str += StringManipulation::NumberToString(number);
		}
		else
		{
			const auto &str_value = en->GetStringValue();
			EscapeAndAppendStringToJsonString(str_value, json_str);
		}
	}

	return true;
}

EvaluableNode *EvaluableNodeJSONTranslation::JsonToEvaluableNode(EvaluableNodeManager *enm, std::string_view json_str)
{
	auto json_padded = simdjson::padded_string(json_str);
	auto json_top_element = json_parser.iterate(json_padded);

	try
	{
		//simdjson needs special handling if the top element is a scalar
		if(json_top_element.is_scalar())
		{
			switch(json_top_element.type())
			{
			case simdjson::ondemand::json_type::number:
				return enm->AllocNode(json_top_element.get_double());

			case simdjson::ondemand::json_type::string:
			{
				std::string_view str_view = json_top_element.get_string();
				std::string str(str_view);
				return enm->AllocNode(ENT_STRING, str);
			}

			case simdjson::ondemand::json_type::boolean:
			{
				if(json_top_element.get_bool())
					return enm->AllocNode(ENT_TRUE);
				else
					return enm->AllocNode(ENT_FALSE);
			}

			default:
				return nullptr;
			}
		}

		return JsonToEvaluableNodeRecurse(enm, json_top_element);
	}
	catch(simdjson::simdjson_error &e)
	{
		//get rid of unused variable warning
		(void)e;
		return nullptr;
	}
}

std::pair<std::string, bool> EvaluableNodeJSONTranslation::EvaluableNodeToJson(EvaluableNode *code, bool sort_keys)
{
	if(code == nullptr)
		return std::make_pair("null", true);

	//if need cycle check, double-check
	if(!EvaluableNode::CanNodeTreeBeFlattened(code))
		return std::make_pair("", false);

	std::string json_str;
	if(EvaluableNodeToJsonStringRecurse(code, json_str, sort_keys))
		return std::make_pair(json_str, true);
	else
		return std::make_pair("", false);
}

EvaluableNode *EvaluableNodeJSONTranslation::Load(const std::string &resource_path, EvaluableNodeManager *enm)
{
	std::string error_string;
	if(!Platform_IsResourcePathAccessible(resource_path, true, error_string))
	{
		std::cerr << "Error loading JSON: " << error_string << std::endl;
		return nullptr;
	}

	auto json_str = simdjson::padded_string::load(resource_path);
	auto json_top_element = json_parser.iterate(json_str);

	try
	{
		return JsonToEvaluableNodeRecurse(enm, json_top_element);
	}
	catch(simdjson::simdjson_error &e)
	{
		//get rid of unused variable warning
		(void)e;
		std::cerr << "Error loading JSON, malformatted file " << resource_path << std::endl;
		return nullptr;
	}
}

// Save node tree to disk as JSON.
bool EvaluableNodeJSONTranslation::Store(EvaluableNode *code, const std::string &resource_path, EvaluableNodeManager *enm, bool sort_keys)
{
	std::string error_string;
	if(!Platform_IsResourcePathAccessible(resource_path, false, error_string))
	{
		std::cerr << "Error storing JSON: " << error_string << std::endl;
		return false;
	}

	auto [result, converted] = EvaluableNodeToJson(code, sort_keys);
	if(!converted)
	{
		std::cerr << "Error storing JSON: cannot convert node to JSON" << std::endl;
		return false;
	}
	std::ofstream file(resource_path);
	file << result;

	return true;
}
