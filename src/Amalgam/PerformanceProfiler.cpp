//project headers:
#include "PerformanceProfiler.h"

//if true, then will record profiling data
bool profilingEnabled;

//keeps track of number of instructions and time spent in them
FastHashMap<std::string, size_t> numCallsByInstructionType;
FastHashMap<std::string, double> timeSpentInInstructionType;
FastHashMap<std::string, int64_t> memoryAccumulatedInInstructionType;

//TODO 17597: make thread local
//contains the type and start time of each instruction
std::vector<std::pair<std::string, std::pair<double, int64_t>>> instructionStackTypeAndStartTimeAndMemUse;

//gets the current time with nanosecond resolution cast to a double measured in seconds
inline double GetCurTime()
{
	typedef std::chrono::steady_clock clk;
	auto cur_time = std::chrono::duration_cast<std::chrono::nanoseconds>(clk::now().time_since_epoch()).count();
	return cur_time / 1000.0 / 1000.0 / 1000.0;
}

void PerformanceProfiler::EnableProfiling(bool enable)
{
	profilingEnabled = enable;
}

bool PerformanceProfiler::IsProfilingEnabled()
{
	return profilingEnabled;
}

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

void PerformanceProfiler::PrintProfilingInformation()
{
	size_t max_num_perf_counters_to_display = 20;
	std::cout << "Operations that took the longest total time (s): " << std::endl;
	auto longest_total_time = PerformanceProfiler::GetNumCallsByTotalTime();
	for(size_t i = 0; i < max_num_perf_counters_to_display && i < longest_total_time.size(); i++)
		std::cout << longest_total_time[i].first << ": " << longest_total_time[i].second << std::endl;
	std::cout << std::endl;

	std::cout << "Operations called the most number of times: " << std::endl;
	auto most_calls = PerformanceProfiler::GetNumCallsByType();
	for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_calls.size(); i++)
		std::cout << most_calls[i].first << ": " << most_calls[i].second << std::endl;
	std::cout << std::endl;

	std::cout << "Operations that took the longest average time (s): " << std::endl;
	auto longest_ave_time = PerformanceProfiler::GetNumCallsByAveTime();
	for(size_t i = 0; i < max_num_perf_counters_to_display && i < longest_ave_time.size(); i++)
		std::cout << longest_ave_time[i].first << ": " << longest_ave_time[i].second << std::endl;
	std::cout << std::endl;

	std::cout << "Operations that increased the memory usage the most in total (nodes): " << std::endl;
	auto most_total_memory = PerformanceProfiler::GetNumCallsByTotalMemoryIncrease();
	for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_total_memory.size(); i++)
		std::cout << most_total_memory[i].first << ": " << most_total_memory[i].second << std::endl;
	std::cout << std::endl;

	std::cout << "Operations that increased the memory usage the most on average (nodes): " << std::endl;
	auto most_ave_memory = PerformanceProfiler::GetNumCallsByAveMemoryIncrease();
	for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_ave_memory.size(); i++)
		std::cout << most_ave_memory[i].first << ": " << most_ave_memory[i].second << std::endl;
	std::cout << std::endl;

	std::cout << "Operations that decreased the memory usage the most in total (nodes): " << std::endl;
	for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_total_memory.size(); i++)
	{
		//only write out those that had a net decrease
		double mem_delta = most_total_memory[most_total_memory.size() - 1 - i].second;
		if(mem_delta >= 0)
			break;
		std::cout << most_total_memory[i].first << ": " << mem_delta << std::endl;
	}
	std::cout << std::endl;

	std::cout << "Operations that decreased the memory usage the most on average (nodes): " << std::endl;
	for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_ave_memory.size(); i++)
	{
		//only write out those that had a net decrease
		double mem_delta = most_ave_memory[most_total_memory.size() - 1 - i].second;
		if(mem_delta >= 0)
			break;
		std::cout << most_total_memory[i].first << ": " << mem_delta << std::endl;
	}
	std::cout << std::endl;

	size_t total_call_count = 0;
	for(auto &c : numCallsByInstructionType)
		total_call_count += c.second;

	std::cout << "Total number of operations: " << total_call_count << std::endl;

	auto [total_mem_increase, positive_mem_increase] = PerformanceProfiler::GetTotalAndPositiveMemoryIncreases();
	std::cout << "Net number of nodes allocated: " << total_mem_increase << std::endl;
	std::cout << "Total node increases: " << positive_mem_increase << std::endl;
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
