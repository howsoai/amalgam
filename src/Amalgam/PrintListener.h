#pragma once

//project headers:
#include "Concurrency.h"

//system headers:
#include <fstream>

class PrintListener
{
public:
	//stores all prints to file
	PrintListener(const std::string &filename = std::string(), bool mirror_to_stdio = false);

	~PrintListener();

	void LogPrint(std::string &print_string);

	void FlushLogFile();

protected:
	std::ofstream logFile;
	bool mirrorToStdio = false;

#ifdef MULTITHREAD_SUPPORT
	//mutex for writing to make sure all streams are written in the same order
	Concurrency::ReadWriteMutex mutex;
#endif
};
