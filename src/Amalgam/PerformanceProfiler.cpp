//project headers:
#include "PerformanceProfiler.h"
#include "Concurrency.h"

//if true, then will record profiling data
bool PerformanceProfiler::_profiler_enabled;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
Concurrency::SingleMutex performance_profiler_mutex;
#endif

//keeps track of number of instructions and time spent in them
FastHashMap<std::string, size_t> _profiler_num_calls_by_instruction_type;
FastHashMap<std::string, double> _profiler_time_spent_in_instruction_type;
FastHashMap<std::string, int64_t> _profiler_memory_accumulated_in_instruction_type;

//contains the type and start time of each instruction
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
	std::vector<std::pair<std::string, std::pair<double, int64_t>>> instructionStackTypeAndStartTimeAndMemUse;

//gets the current time with nanosecond resolution cast to a double measured in seconds
inline double GetCurTime()
{
	typedef std::chrono::steady_clock clk;
	auto cur_time = std::chrono::duration_cast<std::chrono::nanoseconds>(clk::now().time_since_epoch()).count();
	return cur_time / 1000.0 / 1000.0 / 1000.0;
}

void PerformanceProfiler::StartOperation(const std::string &t, int64_t memory_use)
{	
	instructionStackTypeAndStartTimeAndMemUse.push_back(std::make_pair(t, std::make_pair(GetCurTime(), memory_use)));
}

void PerformanceProfiler::EndOperation(int64_t memory_use = 0)
{
	//get and remove data from call stack
	auto type_and_time_and_mem = instructionStackTypeAndStartTimeAndMemUse.back();
	auto inst_type = type_and_time_and_mem.first;
	double inst_start_time = type_and_time_and_mem.second.first;
	int64_t inst_start_mem = type_and_time_and_mem.second.second;
	instructionStackTypeAndStartTimeAndMemUse.pop_back();
	
	double total_instruction_time = GetCurTime() - inst_start_time;
	int64_t total_instruction_memory = memory_use - inst_start_mem;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//accumulate stats
	auto stat = _profiler_num_calls_by_instruction_type.find(inst_type);
	if(stat != end(_profiler_num_calls_by_instruction_type))
	{
		_profiler_num_calls_by_instruction_type[inst_type]++;
		_profiler_time_spent_in_instruction_type[inst_type] += total_instruction_time;
		_profiler_memory_accumulated_in_instruction_type[inst_type] += total_instruction_memory;
	}
	else
	{
		_profiler_num_calls_by_instruction_type[inst_type] = 1;
		_profiler_time_spent_in_instruction_type[inst_type] = total_instruction_time;
		_profiler_memory_accumulated_in_instruction_type[inst_type] = total_instruction_memory;
	}
	
	//remove the time on this instruction for any that are currently pending on the stack by adding it to start time
	for(auto &record : instructionStackTypeAndStartTimeAndMemUse)
	{
		record.second.first += total_instruction_time;
		record.second.second += total_instruction_memory;
	}
}

void PerformanceProfiler::PrintProfilingInformation(std::string outfile_name, size_t max_print_count)
{
	std::ofstream outfile;
	if(outfile_name != "")
		outfile.open(outfile_name);

	std::ostream &out_dest = (outfile.is_open() ? outfile : std::cout);

	if(max_print_count == 0)
	{
		if(outfile.is_open())
			max_print_count = std::numeric_limits<size_t>::max();
		else
			max_print_count = 20;
	}

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that took the longest total time (s): " << std::endl;
	auto longest_total_time = PerformanceProfiler::GetNumCallsByTotalTime();
	for(size_t i = 0; i < max_print_count && i < longest_total_time.size(); i++)
		out_dest << longest_total_time[i].first << ": " << longest_total_time[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations called the most number of times: " << std::endl;
	auto most_calls = PerformanceProfiler::GetNumCallsByType();
	for(size_t i = 0; i < max_print_count && i < most_calls.size(); i++)
		out_dest << most_calls[i].first << ": " << most_calls[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that took the longest average time (s): " << std::endl;
	auto longest_ave_time = PerformanceProfiler::GetNumCallsByAveTime();
	for(size_t i = 0; i < max_print_count && i < longest_ave_time.size(); i++)
		out_dest << longest_ave_time[i].first << ": " << longest_ave_time[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that increased the memory usage the most in total (nodes): " << std::endl;
	auto most_total_memory = PerformanceProfiler::GetNumCallsByTotalMemoryIncrease();
	for(size_t i = 0; i < max_print_count && i < most_total_memory.size(); i++)
		out_dest << most_total_memory[i].first << ": " << most_total_memory[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that increased the memory usage the most on average (nodes): " << std::endl;
	auto most_ave_memory = PerformanceProfiler::GetNumCallsByAveMemoryIncrease();
	for(size_t i = 0; i < max_print_count && i < most_ave_memory.size(); i++)
		out_dest << most_ave_memory[i].first << ": " << most_ave_memory[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that decreased the memory usage the most in total (nodes): " << std::endl;
	for(size_t i = 0; i < max_print_count && i < most_total_memory.size(); i++)
	{
		//only write out those that had a net decrease
		double mem_delta = most_total_memory[most_total_memory.size() - 1 - i].second;
		if(mem_delta >= 0)
			break;
		out_dest << most_total_memory[i].first << ": " << mem_delta << std::endl;
	}
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that decreased the memory usage the most on average (nodes): " << std::endl;
	for(size_t i = 0; i < max_print_count && i < most_ave_memory.size(); i++)
	{
		//only write out those that had a net decrease
		double mem_delta = most_ave_memory[most_total_memory.size() - 1 - i].second;
		if(mem_delta >= 0)
			break;
		out_dest << most_total_memory[i].first << ": " << mem_delta << std::endl;
	}
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	size_t total_call_count = GetTotalNumCalls();
	out_dest << "Total number of operations: " << total_call_count << std::endl;

	auto [total_mem_increase, positive_mem_increase] = PerformanceProfiler::GetTotalAndPositiveMemoryIncreases();
	out_dest << "Net number of nodes allocated: " << total_mem_increase << std::endl;
	out_dest << "Total node increases: " << positive_mem_increase << std::endl;
}

size_t PerformanceProfiler::GetTotalNumCalls()
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	size_t total_call_count = 0;
	for(auto &c : _profiler_num_calls_by_instruction_type)
		total_call_count += c.second;
	return total_call_count;
}

std::pair<int64_t, int64_t> PerformanceProfiler::GetTotalAndPositiveMemoryIncreases()
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	int64_t total_mem_increase = 0;
	int64_t positive_mem_increase = 0;
	for(auto &c : _profiler_memory_accumulated_in_instruction_type)
	{
		total_mem_increase += c.second;
		if(c.second > 0)
			positive_mem_increase += c.second;
	}
	return std::make_pair(total_mem_increase, positive_mem_increase);
}

std::vector<std::pair<std::string, size_t>> PerformanceProfiler::GetNumCallsByType()
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//copy to proper data structure
	std::vector<std::pair<std::string, size_t>> results;
	results.reserve(_profiler_num_calls_by_instruction_type.size());
	for(auto &[s, value] : _profiler_num_calls_by_instruction_type)
		results.push_back(std::make_pair(s, value));
	
	//sort high to low
	std::sort(begin(results), end(results),
			  [](std::pair<std::string, size_t> a, std::pair<std::string, size_t> b) -> bool
			  {	return (a.second) > (b.second);	});
	return results;
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByTotalTime()
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//copy to proper data structure
	std::vector<std::pair<std::string, double>> results;
	results.reserve(_profiler_num_calls_by_instruction_type.size());
	for(auto &[s, value] : _profiler_time_spent_in_instruction_type)
		results.push_back(std::make_pair(static_cast<std::string>(s), value));
	
	//sort high to low
	std::sort(begin(results), end(results),
			  [](std::pair<std::string, double> a, std::pair<std::string, double> b) -> bool
			  {	return (a.second) > (b.second);	});
	return results;
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveTime()
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//copy to proper data structure
	std::vector<std::pair<std::string, double>> results;
	results.reserve(_profiler_num_calls_by_instruction_type.size());
	for(auto &[s, value] : _profiler_time_spent_in_instruction_type)
	{
		auto ncbit = _profiler_num_calls_by_instruction_type.find(s);
		if(ncbit != end(_profiler_num_calls_by_instruction_type))
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
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//copy to proper data structure
	std::vector<std::pair<std::string, double>> results;
	results.reserve(_profiler_memory_accumulated_in_instruction_type.size());
	for(auto &[s, value] : _profiler_memory_accumulated_in_instruction_type)
		results.push_back(std::make_pair(static_cast<std::string>(s), static_cast<double>(value)));

	//sort high to low
	std::sort(begin(results), end(results),
		[](std::pair<std::string, double> a, std::pair<std::string, double> b) -> bool
	{	return (a.second) > (b.second);	});
	return results;
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveMemoryIncrease()
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//copy to proper data structure
	std::vector<std::pair<std::string, double>> results;
	results.reserve(_profiler_memory_accumulated_in_instruction_type.size());
	for(auto &[s, value] : _profiler_memory_accumulated_in_instruction_type)
	{
		auto ncbit = _profiler_num_calls_by_instruction_type.find(s);
		if(ncbit != end(_profiler_num_calls_by_instruction_type))
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
