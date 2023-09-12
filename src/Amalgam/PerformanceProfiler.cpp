//project headers:
#include "PerformanceProfiler.h"

PerformanceProfiler performance_profiler;

void PerformanceProfiler::StartOperation(const std::string &t, int64_t memory_use)
{
	if(!profilingEnabled)
		return;
	
	instructionStackTypeAndStartTimeAndMemUse.push_back(std::make_pair(t, std::make_pair(GetCurTime(), memory_use)));
}

void PerformanceProfiler::EndOperation(int64_t memory_use = 0)
{
	if(!profilingEnabled)
		return;
	
	//get and remove data from call stack
	auto type_and_time_and_mem = instructionStackTypeAndStartTimeAndMemUse.back();
	auto inst_type = type_and_time_and_mem.first;
	double inst_start_time = type_and_time_and_mem.second.first;
	int64_t inst_start_mem = type_and_time_and_mem.second.second;
	instructionStackTypeAndStartTimeAndMemUse.pop_back();
	
	double total_instruction_time = GetCurTime() - inst_start_time;
	int64_t total_instruction_memory = memory_use - inst_start_mem;
	
	//accumulate stats
	auto stat = numCallsByInstructionType.find(inst_type);
	if(stat != end(numCallsByInstructionType))
	{
		numCallsByInstructionType[inst_type]++;
		timeSpentInInstructionType[inst_type] += total_instruction_time;
		memoryAccumulatedInInstructionType[inst_type] += total_instruction_memory;
	}
	else
	{
		numCallsByInstructionType[inst_type] = 1;
		timeSpentInInstructionType[inst_type] = total_instruction_time;
		memoryAccumulatedInInstructionType[inst_type] = total_instruction_memory;
	}
	
	//remove the time on this instruction for any that are currently pending on the stack by adding it to start time
	for(auto &record : instructionStackTypeAndStartTimeAndMemUse)
	{
		record.second.first += total_instruction_time;
		record.second.second += total_instruction_memory;
	}
}

size_t PerformanceProfiler::GetTotalNumCalls()
{
	size_t total_call_count = 0;
	for(auto &c : numCallsByInstructionType)
		total_call_count += c.second;
	return total_call_count;
}

std::pair<int64_t, int64_t> PerformanceProfiler::GetTotalAndPositiveMemoryIncreases()
{
	int64_t total_mem_increase = 0;
	int64_t positive_mem_increase = 0;
	for(auto &c : memoryAccumulatedInInstructionType)
	{
		total_mem_increase += c.second;
		if(c.second > 0)
			positive_mem_increase += c.second;
	}
	return std::make_pair(total_mem_increase, positive_mem_increase);
}

std::vector<std::pair<std::string, size_t>> PerformanceProfiler::GetNumCallsByType()
{
	//copy to proper data structure
	std::vector<std::pair<std::string, size_t>> results;
	results.reserve(numCallsByInstructionType.size());
	for(auto &[s, value] : numCallsByInstructionType)
		results.push_back(std::make_pair(s, value));
	
	//sort high to low
	std::sort(begin(results), end(results),
			  [](std::pair<std::string, size_t> a, std::pair<std::string, size_t> b) -> bool
			  {	return (a.second) > (b.second);	});
	return results;
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByTotalTime()
{
	//copy to proper data structure
	std::vector<std::pair<std::string, double>> results;
	results.reserve(numCallsByInstructionType.size());
	for(auto &[s, value] : timeSpentInInstructionType)
		results.push_back(std::make_pair(static_cast<std::string>(s), value));
	
	//sort high to low
	std::sort(begin(results), end(results),
			  [](std::pair<std::string, double> a, std::pair<std::string, double> b) -> bool
			  {	return (a.second) > (b.second);	});
	return results;
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveTime()
{
	//copy to proper data structure
	std::vector<std::pair<std::string, double>> results;
	results.reserve(numCallsByInstructionType.size());
	for(auto &[s, value] : timeSpentInInstructionType)
	{
		auto ncbit = numCallsByInstructionType.find(s);
		if(ncbit != end(numCallsByInstructionType))
		{
			size_t num_calls = ncbit->second;
			results.push_back(std::make_pair(static_cast<std::string>(s), value / num_calls));
		}
	}
	
	//sort high to low
	std::sort(begin(results), end(results),
			  [](std::pair<std::string, double> a, std::pair<std::string, double> b) -> bool
			  {	return (a.second) > (b.second);	});
	return results;
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByTotalMemoryIncrease()
{
	//copy to proper data structure
	std::vector<std::pair<std::string, double>> results;
	results.reserve(memoryAccumulatedInInstructionType.size());
	for(auto &[s, value] : memoryAccumulatedInInstructionType)
		results.push_back(std::make_pair(static_cast<std::string>(s), static_cast<double>(value)));

	//sort high to low
	std::sort(begin(results), end(results),
		[](std::pair<std::string, double> a, std::pair<std::string, double> b) -> bool
	{	return (a.second) > (b.second);	});
	return results;
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveMemoryIncrease()
{
	//copy to proper data structure
	std::vector<std::pair<std::string, double>> results;
	results.reserve(memoryAccumulatedInInstructionType.size());
	for(auto &[s, value] : memoryAccumulatedInInstructionType)
	{
		auto ncbit = numCallsByInstructionType.find(s);
		if(ncbit != end(numCallsByInstructionType))
		{
			size_t num_calls = ncbit->second;
			results.push_back(std::make_pair(static_cast<std::string>(s), static_cast<double>(value) / num_calls));
		}
	}

	//sort high to low
	std::sort(begin(results), end(results),
		[](std::pair<std::string, double> a, std::pair<std::string, double> b) -> bool
	{	return (a.second) > (b.second);	});
	return results;
}
