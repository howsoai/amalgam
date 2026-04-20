//project headers:
#include "Entity.h"
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"
#include "OpcodeDetails.h"

//system headers:
#include <regex>

EvaluableNode *ExecutionPermissions::GetPermissionsAsEvaluableNode(EvaluableNodeManager *enm)
{
	EvaluableNode *permissions_en = enm->AllocNode(ENT_ASSOC);
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err),
		enm->AllocNode(HasPermission(Permission::STD_OUT_AND_STD_ERR)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_in),
		enm->AllocNode(HasPermission(Permission::STD_IN)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_load),
		enm->AllocNode(HasPermission(Permission::LOAD)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_store),
		enm->AllocNode(HasPermission(Permission::STORE)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_environment),
		enm->AllocNode(HasPermission(Permission::ENVIRONMENT)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_alter_performance),
		enm->AllocNode(HasPermission(Permission::ALTER_PERFORMANCE)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_system),
		enm->AllocNode(HasPermission(Permission::SYSTEM)));

	return permissions_en;
}

std::pair<ExecutionPermissions, ExecutionPermissions> ExecutionPermissions::EvaluableNodeToPermissions(EvaluableNode *en)
{
	ExecutionPermissions permissions_to_set;
	ExecutionPermissions permission_values;

	if(EvaluableNode::IsAssociativeArray(en))
	{
		for(auto [permission_type, allow_en] : en->GetMappedChildNodesReference())
		{
			bool allow = EvaluableNode::ToBool(allow_en);

			if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err))
			{
				permissions_to_set.SetPermission(Permission::STD_OUT_AND_STD_ERR, true);
				permission_values.SetPermission(Permission::STD_OUT_AND_STD_ERR, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_in))
			{
				permissions_to_set.SetPermission(Permission::STD_IN, true);
				permission_values.SetPermission(Permission::STD_IN, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_load))
			{
				permissions_to_set.SetPermission(Permission::LOAD, true);
				permission_values.SetPermission(Permission::LOAD, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_store))
			{
				permissions_to_set.SetPermission(Permission::STORE, true);
				permission_values.SetPermission(Permission::STORE, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_environment))
			{
				permissions_to_set.SetPermission(Permission::ENVIRONMENT, true);
				permission_values.SetPermission(Permission::ENVIRONMENT, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_alter_performance))
			{
				permissions_to_set.SetPermission(Permission::ALTER_PERFORMANCE, true);
				permission_values.SetPermission(Permission::ALTER_PERFORMANCE, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_system))
			{
				permissions_to_set.SetPermission(Permission::SYSTEM, true);
				permission_values.SetPermission(Permission::SYSTEM, allow);
			}
		}
	}
	else if(EvaluableNode::ToBool(en))
	{
		permissions_to_set = AllPermissions();
		permission_values = AllPermissions();
	}

	return std::make_pair(permissions_to_set, permission_values);
}


//returns a copy of s where each consecutive whitespace block is replaced
//by a single space, any leading and trailing spaces are removed,
//numbers with large precision are truncated to remove errors of
//insignificant rounding across platforms, and unnamed entity ids are replaced with
//underscores since they are not reliable based on differences of randomization
//across platforms
static std::string NormalizeTestValidationString(std::string_view s)
{
	std::string out;
	out.reserve(s.size());

	bool in_whitespace = false;
	bool after_dot = false;
	int frac_count = 0;
	for(size_t i = 0; i < s.size(); i++)
	{
		char ch = s[i];

		if(std::isspace(static_cast<unsigned char>(ch)))
		{
			//if first whitespace, change to space
			if(!in_whitespace)
			{
				out.push_back(' ');
				in_whitespace = true;
			}
			continue;
		}

		in_whitespace = false;

		if(ch == '.' && i + 1 < s.size()
			&& std::isdigit(static_cast<unsigned char>(s[i + 1])))
		{
			after_dot = true;
			frac_count = 0;
			out.push_back(ch);
			continue;
		}

		//decimal digit truncation
		if(after_dot)
		{
			if(std::isdigit(static_cast<unsigned char>(ch)))
			{
				frac_count++;
				//keep only the first 6 digits after the decimal place
				if(frac_count <= 6)
					out.push_back(ch);

				continue;
			}
			else //any other character ends the number
			{
				after_dot = false;
				frac_count = 0;
			}
		}

		//detect a quoted string that starts with an underscore as an unnamed entity reference
		if(ch == '"' && i + 1 < s.size())
		{
			size_t closing_quote_pos = i + 1;
			while(closing_quote_pos < s.size() && s[closing_quote_pos] != '"')
				closing_quote_pos++;

			//if found closing quote, then normalize
			if(closing_quote_pos < s.size() && s[i + 1] == '_')
			{
				out.append("\"_____\"");
				i = closing_quote_pos;
				continue;
			}

			//otherwise normal character
		}

		out.push_back(ch);
	}

	//trim spaces
	if(!out.empty() && out.front() == ' ')
		out.erase(out.begin());
	if(!out.empty() && out.back() == ' ')
		out.pop_back();

	return out;
}

//returns true if a and b are equal ignoring subtle differences due to differing platforms
inline static bool EqualGivenValidationNormalization(std::string_view a, std::string_view b)
{
	return NormalizeTestValidationString(a) == NormalizeTestValidationString(b);
}

bool AmalgamExample::ValidateExample(Entity *entity)
{
	bool test_succeeded = true;
	std::cout << "Initializing... ";

	entity->SetRandomState("12345", true);

	auto [code, warnings, char_with_error, code_complete]
		= Parser::Parse(example, &entity->evaluableNodeManager, false, nullptr, false, true);

	if(warnings.size() > 0)
	{
		std::cerr << "Improper code: " << std::endl;
		for(auto &w : warnings)
			std::cerr << w << std::endl;

		return false;
	}

	std::cout << "Executing... ";
	auto result = entity->ExecuteOnEntity(code, nullptr);
	std::string result_str = Parser::Unparse(result, true, true, true);

	if(regexMatch.empty())
	{
		if(!EqualGivenValidationNormalization(result_str, output))
		{
			std::cerr << "Failed, ran code:" << std::endl;
			std::cerr << example << std::endl;
			std::cerr << "Expected result:" << std::endl;
			std::cerr << output << std::endl;
			std::cerr << "Observed result:" << std::endl;
			std::cerr << result_str << std::endl;
			test_succeeded = false;
		}
	}
	else //match with regular expression
	{
		//use the begin/end constructor since std::string_view isn't universally supported
		std::regex pattern(begin(regexMatch), end(regexMatch), std::regex::ECMAScript);
		if(std::regex_match(result_str, pattern))
		{
			std::cerr << "Failed, ran code:" << std::endl;
			std::cerr << example << std::endl;
			std::cerr << "Expected to match:" << std::endl;
			std::cerr << regexMatch << std::endl;
			std::cerr << "Observed:" << std::endl;
			std::cerr << result_str << std::endl;
			test_succeeded = false;
		}
	}

	std::cout << "Freeing Resources... ";
	if(!cleanup.empty())
	{
		auto [cleanup_code, cleanup_warnings, cleanup_char_with_error, cleanup_code_complete]
			= Parser::Parse(cleanup, &entity->evaluableNodeManager);

		entity->ExecuteOnEntity(cleanup_code, nullptr);
	}

	entity->ReclaimResources(false, true, false);

	auto query_caches = entity->GetQueryCaches();
	if(query_caches != nullptr)
		query_caches->sbfds.VerifyAllEntitiesForAllColumns();

	if(entity->GetLabelIndex().size() != 0)
	{
		std::cerr << "Failed: Labels remain in entity after test" << std::endl;
		test_succeeded = false;
	}

	if(entity->GetContainedEntities().size() > 0)
	{
		std::cerr << "Failed: One or more contained entities remain after test" << std::endl;
		test_succeeded = false;
	}

	return test_succeeded;
}

UninitializedArray<OpcodeDetails, NUM_ENT_OPCODES> _opcode_details;

static std::string _opcode_group = "global";

