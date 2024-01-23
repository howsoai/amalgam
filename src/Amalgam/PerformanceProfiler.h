#pragma once

//project headers:
#include "EvaluableNode.h"
#include "HashMaps.h"

//system headers:
#include <ctime>
#include <chrono>

namespace PerformanceProfiler
{
	extern bool _profiler_enabled;

	inline void SetProfilingState(bool enabled)
	{
		_profiler_enabled = enabled;
	}

	inline bool IsProfilingEnabled()
	{
		return _profiler_enabled;
	}
	
	//begins performance timers for the specified operation type, specified by the string t
	// pushes current instruction on the stack, such that it will be cleared when the
	// corresponding EndOperation is called
	void StartOperation(const std::string &t, int64_t memory_use);
	
	void EndOperation(int64_t memory_use);

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	//accumulates lock contention for string t
	void AccumulateLockContentionCount(std::string t);
#endif

	//prints profiling information
	//if outfile_name is empty string, will print to stdout
	//if max_print_count is 0, will print a default of 20 for stdout, will print all for a file
	void PrintProfilingInformation(std::string outfile_name = "", size_t max_print_count = 0);

	size_t GetTotalNumCalls();

	std::pair<int64_t, int64_t> GetTotalAndPositiveMemoryIncreases();
	
	std::vector<std::pair<std::string, size_t>> GetNumCallsByType();

	std::vector<std::pair<std::string, double>> GetNumCallsByTotalTimeExclusive();
	std::vector<std::pair<std::string, double>> GetNumCallsByAveTimeExclusive();

	std::vector<std::pair<std::string, double>> GetNumCallsByTotalTimeInclusive();
	std::vector<std::pair<std::string, double>> GetNumCallsByAveTimeInclusive();

	std::vector<std::pair<std::string, double>> GetNumCallsByTotalMemoryIncreaseExclusive();
	std::vector<std::pair<std::string, double>> GetNumCallsByAveMemoryIncreaseExclusive();

	std::vector<std::pair<std::string, double>> GetNumCallsByTotalMemoryIncreaseInclusive();
	std::vector<std::pair<std::string, double>> GetNumCallsByAveMemoryIncreaseInclusive();

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	std::vector<std::pair<std::string, size_t>> GetVariableAssignmentsByLockContentionCount();
#endif
};
