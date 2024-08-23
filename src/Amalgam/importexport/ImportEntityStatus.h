#pragma once

//system headers:
#include <string>

class ImportEntityStatus
{
public:
	ImportEntityStatus();
	ImportEntityStatus(bool loaded, std::string message = std::string(), std::string version = std::string());
	void SetStatus(bool loaded_in, std::string message_in = std::string(), std::string version_in = std::string());

	bool loaded;
	std::string message;
	std::string version;
};

