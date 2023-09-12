#pragma once

//project headers:
#include "EvaluableNode.h"
#include "HashMaps.h"

//system headers:
#include <ctime>
#include <chrono>

//forward declarations:
class PerformanceProfiler;
extern PerformanceProfiler performance_profiler;

class PerformanceProfiler
{
public:
	PerformanceProfiler()
	{	EnableProfiling(false);	}
	
	//begins performance timers for the specified operation type, specified by the string t
	// pushes current instruction on the stack, such that it will be cleared when the
	// corresponding EndOperation is called
	void StartOperation(const std::string &t, int64_t memory_use);
	
	void EndOperation(int64_t memory_use);

	size_t GetTotalNumCalls();

	std::pair<int64_t, int64_t> GetTotalAndPositiveMemoryIncreases();
	
	std::vector<std::pair<std::string, size_t>> GetNumCallsByType();
	
	std::vector<std::pair<std::string, double>> GetNumCallsByTotalTime();
	
	std::vector<std::pair<std::string, double>> GetNumCallsByAveTime();

	std::vector<std::pair<std::string, double>> GetNumCallsByTotalMemoryIncrease();

	std::vector<std::pair<std::string, double>> GetNumCallsByAveMemoryIncrease();
	
	void EnableProfiling(bool enable = true)
	{	profilingEnabled = enable;	}
	
	bool IsProfilingEnabled()
	{	return profilingEnabled;	}

	//gets the current time with nanosecond resolution cast to a double measured in seconds
	static inline double GetCurTime()
	{
		typedef std::chrono::steady_clock clk;
		auto cur_time = std::chrono::duration_cast<std::chrono::nanoseconds>(clk::now().time_since_epoch()).count();
		return cur_time / 1000.0 / 1000.0 / 1000.0;
	}
	
protected:
	
	//if true, then will record profiling data
	bool profilingEnabled;
	
	//keeps track of number of instructions and time spent in them
	FastHashMap<std::string, size_t> numCallsByInstructionType;
	FastHashMap<std::string, double> timeSpentInInstructionType;
	FastHashMap<std::string, int64_t> memoryAccumulatedInInstructionType;
	
	//contains the type and start time of each instruction
	std::vector<std::pair<std::string, std::pair<double, int64_t>>> instructionStackTypeAndStartTimeAndMemUse;
};