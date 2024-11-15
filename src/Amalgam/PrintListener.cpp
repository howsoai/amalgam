//project headers:
#include "PrintListener.h"

//system headers:
#include <iostream>

PrintListener::PrintListener(const std::string &filename, bool mirror_to_stdio)
{
	if(!filename.empty())
		logFile.open(filename, std::ios::binary);

	mirrorToStdio = mirror_to_stdio;
}

PrintListener::~PrintListener()
{
	if(logFile.is_open())
		logFile.close();
}

void PrintListener::LogPrint(std::string &print_string)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::WriteLock lock(mutex);
#endif

	if(logFile.is_open() && logFile.good())
		logFile << print_string;

	if(mirrorToStdio)
		std::cout << print_string;
}

void PrintListener::FlushLogFile()
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::WriteLock lock(mutex);
#endif

	if(logFile.is_open() && logFile.good())
		logFile.flush();

	if(mirrorToStdio)
		std::cout.flush();
}
