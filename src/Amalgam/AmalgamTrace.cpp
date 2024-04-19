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
	std::string clone_handle;
	std::string command;
	std::string data;
	std::string persistent;
	std::string use_contained;
	std::string escape_filename;
	std::string escape_contained_filenames;
	std::string print_listener_path;
	std::string transaction_listener_path;
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
			if(command_tokens.size() >= 6)
			{
				handle = command_tokens[0];
				data = command_tokens[1];  // path to amlg file
				persistent = command_tokens[2];
				use_contained = command_tokens[3];

				if(command_tokens.size() >= 5)
					escape_filename = command_tokens[4];
				else
					escape_filename = "true";

				if(command_tokens.size() >= 6)
					escape_contained_filenames = command_tokens[5];
				else
					escape_contained_filenames = "true";

				if(command_tokens.size() >= 7)
					transaction_listener_path = command_tokens[6];
				else
					transaction_listener_path = "";

				if(command_tokens.size() >= 8)
					print_listener_path = command_tokens[7];
				else
					print_listener_path = "";

				std::string new_rand_seed = random_stream.CreateOtherStreamStateViaString("trace");
				auto status = entint.LoadEntity(handle, data, persistent == "true", use_contained == "true",
					escape_filename == "true", escape_contained_filenames == "true", transaction_listener_path, print_listener_path, new_rand_seed);
				response = status.loaded ? SUCCESS_RESPONSE : FAILURE_RESPONSE;
			}
			else
			{
				//Insufficient arguments
				response = FAILURE_RESPONSE;
			}
		}
		else if(command == "CLONE_ENTITY")
		{
			std::vector<std::string> command_tokens = StringManipulation::SplitArgString(input);
			if(command_tokens.size() >= 2)
			{
				handle = command_tokens[0];
				clone_handle = command_tokens[1];

				if(command_tokens.size() >= 3)
					data = command_tokens[2];  // path to amlg file

				if(command_tokens.size() >= 4)
					persistent = command_tokens[3];

				if(command_tokens.size() >= 5)
					transaction_listener_path = command_tokens[4];
				else
					transaction_listener_path = "";

				if(command_tokens.size() >= 6)
					print_listener_path = command_tokens[5];
				else
					print_listener_path = "";

				bool result = entint.CloneEntity(handle, clone_handle, data, persistent == "true", transaction_listener_path, print_listener_path);
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
			if(command_tokens.size() >= 4)
			{
				handle = command_tokens[0];
				data = command_tokens[1];  // path to amlg file
				persistent = command_tokens[2];
				use_contained = command_tokens[3];

				entint.StoreEntity(handle, data, persistent == "true", use_contained == "true");
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
			data = input;  // json data
			bool result = entint.SetJSONToLabel(handle, label, data);
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
			data = input;  // json data
			response = entint.ExecuteEntityJSON(handle, label, data);
		}
		else if(command == "SET_RANDOM_SEED")
		{
			handle = StringManipulation::RemoveFirstToken(input);
			data = input;
			bool result = entint.SetRandomSeed(handle, data);
			response = result ? SUCCESS_RESPONSE : FAILURE_RESPONSE;
		}
		else if(command == "VERSION")
		{
			response = AMALGAM_VERSION_STRING;
		}
		else if(command == "EXIT")
		{
			break;
		}
		else if(command == "#" || command == "")
		{
			// Commment or blank lines used in execution dumps.
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
