//project headers:
#include "ImportEntityStatus.h"

ImportEntityStatus::ImportEntityStatus()
{
	SetStatus(true);
}

ImportEntityStatus::ImportEntityStatus(bool loaded, std::string message, std::string version)
{
	SetStatus(loaded, message, version);
}

void ImportEntityStatus::SetStatus(bool loaded_in, std::string message_in, std::string version_in)
{
	loaded = loaded_in;
	message = std::move(message_in);
	version = std::move(version_in);
}
