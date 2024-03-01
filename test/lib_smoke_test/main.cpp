//
// Test driver for Amalgam shared libraries (dll/so/dylib)
//

//project headers:
#include "Amalgam.h"

//system headers:
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
	// Print info and clean-up memory:
	auto version = GetVersionString();
	auto type = GetConcurrencyTypeString();
	std::cout << "version='" << std::string(version) << "', type='" << std::string(type) << std::endl;
	DeleteString(version);
	DeleteString(type);

	// Load+execute+delete entity:
	char handle[] = "tes";
	char* file = (argc > 1) ? argv[1] : (char*)"test.amlg";
	auto status = LoadEntity(handle, file);

	// Clean-up status strings:
	DeleteString(status.message);
	DeleteString(status.version);

	// Process if loaded:
	if(status.loaded)
	{
		ExecuteEntity(handle, "test");
		DestroyEntity(handle);

		return 0;
	}

	return 1;
}