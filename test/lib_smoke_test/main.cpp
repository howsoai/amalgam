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
	// Print version:
	std::cout << std::string(GetVersionString()) << std::endl;

	// Load+execute+delete entity:
	char handle[] = "1";
	char* file = (argc > 1) ? argv[1] : (char*)"test.amlg";
	char write_log[] = "";
	char print_log[] = "";
	auto status = LoadEntity(handle, file, false, true, false, false, write_log, print_log);
	if(status.loaded)
	{
		char label[] = "test";
		ExecuteEntity(handle, label);
		DestroyEntity(handle);
		return 0;
	}

	return 1;
}