#pragma once

//project headers:
#include "EvaluableNode.h"
#include "HashMaps.h"

//system headers:
#include <ctime>
#include <chrono>

namespace PerformanceProfiler
{
	void EnableProfiling(bool enable = true);

	bool IsProfilingEnabled();
	
	//begins performance timers for the specified operation type, specified by the string t
	// pushes current instruction on the stack, such that it will be cleared when the
	// corresponding EndOperation is called
	void StartOperation(const std::string &t, int64_t memory_use);
	
	void EndOperation(int64_t memory_use);

	void PrintProfilingInformation();

	std::pair<int64_t, int64_t> GetTotalAndPositiveMemoryIncreases();
	
	std::vector<std::pair<std::string, size_t>> GetNumCallsByType();
	
	std::vector<std::pair<std::string, double>> GetNumCallsByTotalTime();
	
	std::vector<std::pair<std::string, double>> GetNumCallsByAveTime();

	std::vector<std::pair<std::string, double>> GetNumCallsByTotalMemoryIncrease();

	std::vector<std::pair<std::string, double>> GetNumCallsByAveMemoryIncrease();
};
