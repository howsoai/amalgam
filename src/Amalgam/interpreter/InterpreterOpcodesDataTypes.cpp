//project headers:
#include "Interpreter.h"

#include "AssetManager.h"
#include "Cryptography.h"
#include "DateTimeFormat.h"
#include "EvaluableNodeTreeManipulation.h"
#include "EntityWriteListener.h"
#include "FileSupportJSON.h"
#include "FileSupportYAML.h"
#include "PlatformSpecific.h"

//system headers:
#include <regex>
#include <utility>


void Interpreter::EmitOrLogUndefinedVariableWarningIfNeeded(StringInternPool::StringID not_found_variable_sid, EvaluableNode *en)
{
	std::string warning = "";

	warning.append("Warning: undefined symbol " + not_found_variable_sid->string);

	if(asset_manager.debugSources && en->HasComments())
	{
		std::string_view comment_string = en->GetCommentsString();
		size_t newline_index = comment_string.find("\n");

		std::string comment_string_first_line;

		if(newline_index != std::string::npos)
			comment_string_first_line = comment_string.substr(0, newline_index + 1);
		else
			comment_string_first_line = comment_string;

		warning.append(" at " + comment_string_first_line);
	}

	if(interpreterConstraints != nullptr)
	{
		if(interpreterConstraints->collectWarnings)
			interpreterConstraints->AddWarning(std::move(warning));
	}
	else if(asset_manager.warnOnUndefined)
	{
		ExecutionPermissions entity_permissions = asset_manager.GetEntityPermissions(curEntity);
		if(entity_permissions.HasPermission(ExecutionPermissions::Permission::STD_OUT_AND_STD_ERR))
			std::cerr << warning << std::endl;
	}
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CRYPTO_SIGN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	std::string message = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string secret_key = InterpretNodeIntoStringValueEmptyNull(ocn[1]);

	std::string signature = SignMessage(message, secret_key);

	return AllocReturn(signature, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CRYPTO_SIGN_VERIFY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 3)
		return EvaluableNodeReference::Null();

	std::string message = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string public_key = InterpretNodeIntoStringValueEmptyNull(ocn[1]);
	std::string signature = InterpretNodeIntoStringValueEmptyNull(ocn[2]);

	bool valid_sig = IsSignatureValid(message, public_key, signature);

	return AllocReturn(valid_sig, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ENCRYPT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	std::string plaintext = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string key_1 = InterpretNodeIntoStringValueEmptyNull(ocn[1]);

	std::string nonce = "";
	if(ocn.size() >= 3)
		nonce = InterpretNodeIntoStringValueEmptyNull(ocn[2]);

	std::string key_2 = "";
	if(ocn.size() >= 4)
		key_2 = InterpretNodeIntoStringValueEmptyNull(ocn[3]);

	std::string cyphertext = "";

	//if no second key, then use symmetric key encryption
	if(key_2.empty())
		cyphertext = EncryptMessage(plaintext, key_1, nonce);
	else //use public key encryption
		cyphertext = EncryptMessage(plaintext, key_1, key_2, nonce);

	return AllocReturn(cyphertext, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DECRYPT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	std::string cyphertext = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string key_1 = InterpretNodeIntoStringValueEmptyNull(ocn[1]);

	std::string nonce = "";
	if(ocn.size() >= 3)
		nonce = InterpretNodeIntoStringValueEmptyNull(ocn[2]);

	std::string key_2 = "";
	if(ocn.size() >= 4)
		key_2 = InterpretNodeIntoStringValueEmptyNull(ocn[3]);

	std::string plaintext = "";

	//if no second key, then use symmetric key encryption
	if(key_2.empty())
		plaintext = DecryptMessage(cyphertext, key_1, nonce);
	else //use public key encryption
		plaintext = DecryptMessage(cyphertext, key_1, key_2, nonce);

	return AllocReturn(plaintext, immediate_result);
}

