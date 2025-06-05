//project headers:
#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "EntityExternalInterface.h"
#include "PlatformSpecific.h"
#include "RandomStream.h"

//system headers:
#include <fstream>
#include <iostream>
#include <string>

extern EntityExternalInterface entint;

const std::string SUCCESS_RESPONSE = std::string("success");
const std::string FAILURE_RESPONSE = std::string("failure");

//runs a loop processing commands in the same manner as the API
// Message structure: <COMMAND> [ADDITIONAL ARGS] [DATA]
int32_t RunAmalgamTrace(std::istream *in_stream, std::ostream *out_stream, std::string &random_seed)
{
	if(in_stream == nullptr)
		return 0;

	RandomStream random_stream(random_seed);

	//set default store to be compressed
	asset_manager.defaultEntityExtension = FILE_EXTENSION_COMPRESSED_AMALGAM_CODE;

	// Define all these variables outside the main loop to reduce memory churn.
	std::string input;
	std::string handle;
	std::string label;
	std::string amlg;
	std::string clone_handle;
	std::string command;
	std::string path;
	std::string file_type;
	std::string json_payload;
	std::string persistent;
	std::string print_listener_path;
	std::string transaction_listener_path;
	std::string rand_seed;
	std::string response;

	// program loop
	while(in_stream->good())
	{
		// read external input
		getline(*in_stream, input, '\n');

		command = StringManipulation::RemoveFirstToken(input);
		response = "-";

		// perform specified operation
		if(command == "LOAD_ENTITY")
		{
			std::vector<std::string> command_tokens = StringManipulation::SplitArgString(input);
			if(command_tokens.size() >= 2)
			{
				handle = command_tokens[0];
				path = command_tokens[1];

				if(command_tokens.size() > 2)
					file_type = command_tokens[2];

				if(command_tokens.size() > 3)
					persistent = command_tokens[3];

				if(command_tokens.size() > 4)
					json_payload = command_tokens[4];

				if(command_tokens.size() > 5)
					transaction_listener_path = command_tokens[5];
				else
					transaction_listener_path = "";

				if(command_tokens.size() > 6)
					print_listener_path = command_tokens[6];
				else
					print_listener_path = "";

				if(command_tokens.size() > 7)
					rand_seed = command_tokens[7];
				else
					rand_seed = random_stream.CreateOtherStreamStateViaString("trace");

				auto status = entint.LoadEntity(handle, path, file_type,
					persistent == "true", json_payload, transaction_listener_path, print_listener_path, rand_seed);
				response = status.loaded ? SUCCESS_RESPONSE : FAILURE_RESPONSE;
			}
			else
			{
				//Insufficient arguments
				response = FAILURE_RESPONSE;
			}
		}
		else if(command == "GET_ENTITY_PERMISSIONS")
		{
			handle = StringManipulation::RemoveFirstToken(input);
			response = entint.GetEntityPermissions(handle);
		}
		else if(command == "SET_ENTITY_PERMISSIONS")
		{
			handle = StringManipulation::RemoveFirstToken(input);
			json_payload = input;  // json data
			entint.SetEntityPermissions(handle, json_payload);
			response = SUCCESS_RESPONSE;
		}
		else if(command == "CLONE_ENTITY")
		{
			std::vector<std::string> command_tokens = StringManipulation::SplitArgString(input);
			if(command_tokens.size() >= 2)
			{
				handle = command_tokens[0];
				clone_handle = command_tokens[1];

				if(command_tokens.size() > 2)
					path = command_tokens[2];

				if(command_tokens.size() > 3)
					file_type = command_tokens[3];

				if(command_tokens.size() > 4)
					persistent = command_tokens[4];

				if(command_tokens.size() > 5)
					json_payload = command_tokens[5];

				if(command_tokens.size() > 6)
					transaction_listener_path = command_tokens[6];
				else
					transaction_listener_path = "";

				if(command_tokens.size() > 7)
					print_listener_path = command_tokens[7];
				else
					print_listener_path = "";

				bool result = entint.CloneEntity(handle, clone_handle, path, file_type,
					persistent == "true", json_payload, transaction_listener_path, print_listener_path);
				response = result ? SUCCESS_RESPONSE : FAILURE_RESPONSE;
			}
			else
			{
				//Insufficient arguments
				response = FAILURE_RESPONSE;
			}
		}
		else if(command == "STORE_ENTITY")
		{
			std::vector<std::string> command_tokens = StringManipulation::SplitArgString(input);
			if(command_tokens.size() >= 2)
			{
				handle = command_tokens[0];
				path = command_tokens[1];

				if(command_tokens.size() > 2)
					file_type = command_tokens[2];

				if(command_tokens.size() > 3)
					persistent = command_tokens[3];

				if(command_tokens.size() > 4)
					json_payload = command_tokens[4];

				entint.StoreEntity(handle, path, file_type, persistent == "true", json_payload);
				response = SUCCESS_RESPONSE;
			}
			else
			{
				//Insufficient arguments
				response = FAILURE_RESPONSE;
			}
		}
		else if(command == "DESTROY_ENTITY")
		{
			handle = StringManipulation::RemoveFirstToken(input);

			entint.DestroyEntity(handle);
			response = SUCCESS_RESPONSE;
		}
		else if(command == "SET_JSON_TO_LABEL")
		{
			handle = StringManipulation::RemoveFirstToken(input);
			label = StringManipulation::RemoveFirstToken(input);
			json_payload = input;  // json data
			bool result = entint.SetJSONToLabel(handle, label, json_payload);
			response = result ? SUCCESS_RESPONSE : FAILURE_RESPONSE;
		}
		else if(command == "GET_JSON_FROM_LABEL")
		{
			handle = StringManipulation::RemoveFirstToken(input);
			label = StringManipulation::RemoveFirstToken(input);
			response = entint.GetJSONFromLabel(handle, label);
		}
		else if(command == "EXECUTE_ENTITY_JSON")
		{
			handle = StringManipulation::RemoveFirstToken(input);
			label = StringManipulation::RemoveFirstToken(input);
			json_payload = input;  // json data
			response = entint.ExecuteEntityJSON(handle, label, json_payload);
		}
		else if(command == "EXECUTE_ENTITY_JSON_LOGGED")
		{
			handle = StringManipulation::RemoveFirstToken(input);
			label = StringManipulation::RemoveFirstToken(input);
			json_payload = input;  // json data
			auto [json_response, log] = entint.ExecuteEntityJSONLogged(handle, label, json_payload);
			response = json_response + "\n# " + log;
		}
		else if(command == "EVAL_ON_ENTITY")
		{
			handle = StringManipulation::RemoveFirstToken(input);
			amlg = StringManipulation::RemoveFirstToken(input);
			response = entint.EvalOnEntity(handle, amlg);
		}
		else if(command == "SET_RANDOM_SEED")
		{
			handle = StringManipulation::RemoveFirstToken(input);
			json_payload = input;
			bool result = entint.SetRandomSeed(handle, json_payload);
			response = result ? SUCCESS_RESPONSE : FAILURE_RESPONSE;
		}
		else if(command == "VERSION")
		{
			response = AMALGAM_VERSION_STRING;
		}
		else if(command == "VERIFY_ENTITY")
		{
			std::vector<std::string> command_tokens = StringManipulation::SplitArgString(input);
			if(command_tokens.size() >= 1)
			{
				auto status = entint.VerifyEntity(command_tokens[0]);
				response = status.loaded ? SUCCESS_RESPONSE : FAILURE_RESPONSE;
			}
			else
			{
				response = FAILURE_RESPONSE;
			}
		}
		else if(command == "GET_MAX_NUM_THREADS")
		{
		#if defined(MULTITHREAD_SUPPORT)
			response = std::to_string(Concurrency::GetMaxNumThreads());
		#else
			response = FAILURE_RESPONSE;
		#endif
		}
		else if(command == "SET_MAX_NUM_THREADS")
		{
		#if defined(MULTITHREAD_SUPPORT)
			response = SUCCESS_RESPONSE;
			try
			{
				auto num_threads = std::stoll(input);
				if(num_threads >= 0)
					Concurrency::SetMaxNumThreads(static_cast<size_t>(num_threads));
				else
					response = FAILURE_RESPONSE;
			}
			catch(...)
			{
				response = FAILURE_RESPONSE;
			}
		#else
			response = FAILURE_RESPONSE;
		#endif
		}
		else if(command == "EXIT")
		{
			break;
		}
		else if(command == "#" || command == "")
		{
			// Comment or blank lines used in execution dumps.
		}
		else
		{
			response = "Unknown command: " + command;
		}

		// return response
		if(out_stream != nullptr)
			*out_stream << response << std::endl;
	}

	if(Platform_IsDebuggerPresent())
		std::cout << "Trace file complete." << std::endl;

	return 0;
}
