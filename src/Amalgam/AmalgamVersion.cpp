//project headers:
#include "AmalgamVersion.h"
#include "StringManipulation.h"

#include <iostream>

std::pair<std::string, bool> ValidateVersionAgainstAmalgam(std::string &version)
{
	auto semver = StringManipulation::Split(version, '-'); //split on postfix
	auto version_split = StringManipulation::Split(semver[0], '.'); //ignore postfix
	if(version_split.size() != 3)
		return std::make_pair("Invalid version number", false);

	uint32_t major = atoi(version_split[0].c_str());
	uint32_t minor = atoi(version_split[1].c_str());
	uint32_t patch = atoi(version_split[2].c_str());
	auto dev_build = std::string(AMALGAM_VERSION_SUFFIX);
	if(!dev_build.empty()
			|| (AMALGAM_VERSION_MAJOR == 0 && AMALGAM_VERSION_MINOR == 0 && AMALGAM_VERSION_PATCH == 0))
		; // dev builds don't check versions
	else if(
		(major > AMALGAM_VERSION_MAJOR) ||
		(major == AMALGAM_VERSION_MAJOR && minor > AMALGAM_VERSION_MINOR) ||
		(major == AMALGAM_VERSION_MAJOR && minor == AMALGAM_VERSION_MINOR && patch > AMALGAM_VERSION_PATCH))
	{
		std::string err_msg = "Parsing Amalgam that is more recent than the current version is not supported";
		std::cerr << err_msg << ", version=" << version << std::endl;
		return std::make_pair(err_msg, false);
	}
	else if(AMALGAM_VERSION_MAJOR > major)
	{
		std::string err_msg = "Parsing Amalgam that is older than the current major version is not supported";
		std::cerr << err_msg << ", version=" << version << std::endl;
		return std::make_pair(err_msg, false);
	}

	return std::make_pair("", true);
}
