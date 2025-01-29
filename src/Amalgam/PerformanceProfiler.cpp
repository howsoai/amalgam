//project headers:
#include "Concurrency.h"
#include "PerformanceProfiler.h"

//system headers:
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>

//if true, then will record profiling data
bool PerformanceProfiler::_profiler_enabled;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
Concurrency::SingleMutex performance_profiler_mutex;
#endif

struct PerformanceCounters
{
	size_t numCalls;
	double totalTimeExclusive;
	int64_t totalMemChangeExclusive;
	double totalTimeInclusive;
	int64_t totalMemChangeInclusive;

#if defined(MULTITHREAD_SUPPORT)
	double elapsedTimeExclusive;
	double elapsedTimeInclusive;
#endif
};

struct StartTimeAndMemUse
{
	double startTimeExclusive;
	int64_t memUseExclusive;
	double startTimeInclusive;
	int64_t memUseInclusive;
};

FastHashMap<std::string, PerformanceCounters> _profiler_counters;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
FastHashMap<std::string, size_t> _lock_contention_counters;
#endif

//contains the type and start time of each instruction
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
	std::vector<std::pair<std::string, StartTimeAndMemUse>> instructionStackTypeAndStartTimeAndMemUse;

FastHashMap<std::string, size_t> _side_effect_total_memory_write_counters;
FastHashMap<std::string, size_t> _side_effect_initial_memory_write_counters;

//gets the current time with nanosecond resolution cast to a double measured in seconds
inline double GetCurTime()
{
	typedef std::chrono::steady_clock clk;
	auto cur_time = std::chrono::duration_cast<std::chrono::nanoseconds>(clk::now().time_since_epoch()).count();
	return cur_time / 1000.0 / 1000.0 / 1000.0;
}

void PerformanceProfiler::StartOperation(const std::string &t, int64_t memory_use)
{
	double cur_time = GetCurTime();
	instructionStackTypeAndStartTimeAndMemUse.push_back(std::make_pair(t,
			StartTimeAndMemUse{ cur_time, memory_use, cur_time, memory_use }));
}

void PerformanceProfiler::EndOperation(int64_t memory_use = 0)
{
	//get and remove data from scope stack
	auto type_and_time_and_mem = instructionStackTypeAndStartTimeAndMemUse.back();
	auto operation_type = type_and_time_and_mem.first;
	auto counters = type_and_time_and_mem.second;
	instructionStackTypeAndStartTimeAndMemUse.pop_back();

	double cur_time = GetCurTime();
	double total_operation_time_exclusive = cur_time - counters.startTimeExclusive;
	int64_t total_operation_memory_exclusive = memory_use - counters.memUseExclusive;

	double total_operation_time_inclusive = cur_time - counters.startTimeInclusive;
	int64_t total_operation_memory_inclusive = memory_use - counters.memUseInclusive;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//accumulate stats
	auto stat = _profiler_counters.find(operation_type);
	if(stat != end(_profiler_counters))
	{
		auto &perf_counter = stat->second;

		perf_counter.numCalls++;

		perf_counter.totalTimeExclusive += total_operation_time_exclusive;
		perf_counter.totalMemChangeExclusive += total_operation_memory_exclusive;

		perf_counter.totalTimeInclusive += total_operation_time_inclusive;
		perf_counter.totalMemChangeInclusive += total_operation_memory_inclusive;

	#if defined(MULTITHREAD_SUPPORT)
		auto num_active_threads = Concurrency::threadPool.GetNumActiveThreads()
			+ Concurrency::urgentThreadPool.GetNumActiveThreads();
		perf_counter.elapsedTimeExclusive += total_operation_time_exclusive / num_active_threads;
		perf_counter.elapsedTimeInclusive += total_operation_time_inclusive / num_active_threads;
	#endif
	}
	else
	{
		PerformanceCounters pc = { 1,
			total_operation_time_exclusive, total_operation_memory_exclusive,
			total_operation_time_inclusive, total_operation_memory_inclusive };
		_profiler_counters[operation_type] = pc;
	}
	
	//for exclusive counters, remove the time on this instruction for any that are currently pending
	// on the stack by adding it to start time
	for(auto &record : instructionStackTypeAndStartTimeAndMemUse)
	{
		record.second.startTimeExclusive += total_operation_time_exclusive;
		record.second.memUseExclusive += total_operation_memory_exclusive;
	}
}

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
void PerformanceProfiler::AccumulateLockContentionCount(std::string t)
{
	Concurrency::SingleLock lock(performance_profiler_mutex);

	//attempt to insert a first count, if not, increment
	auto insertion = _lock_contention_counters.emplace(t, 1);
	if(!insertion.second)
		insertion.first->second++;
}
#endif

void PerformanceProfiler::AccumulateTotalSideEffectMemoryWrites(std::string t)
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//attempt to insert a first count, if not, increment
	auto insertion = _side_effect_total_memory_write_counters.emplace(t, 1);
	if(!insertion.second)
		insertion.first->second++;
}

void PerformanceProfiler::AccumulateInitialSideEffectMemoryWrites(std::string t)
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//attempt to insert a first count, if not, increment
	auto insertion = _side_effect_initial_memory_write_counters.emplace(t, 1);
	if(!insertion.second)
		insertion.first->second++;
}

void PerformanceProfiler::PrintProfilingInformation(std::string outfile_name, size_t max_print_count)
{
	std::ofstream outfile;
	if(!outfile_name.empty())
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
	out_dest << "Operations that took the longest total exclusive time (s): " << std::endl;
	auto longest_total_time_exclusive = PerformanceProfiler::GetNumCallsByTotalTimeExclusive();
	for(size_t i = 0; i < max_print_count && i < longest_total_time_exclusive.size(); i++)
		out_dest << longest_total_time_exclusive[i].first << ": " << longest_total_time_exclusive[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that took the longest total inclusive time (s): " << std::endl;
	auto longest_total_time_inclusive = PerformanceProfiler::GetNumCallsByTotalTimeInclusive();
	for(size_t i = 0; i < max_print_count && i < longest_total_time_inclusive.size(); i++)
		out_dest << longest_total_time_inclusive[i].first << ": " << longest_total_time_inclusive[i].second << std::endl;
	out_dest << std::endl;

#if defined(MULTITHREAD_SUPPORT)
	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that contributed the longest total exclusive elapsed time (accumulated time divided by active thread count) (s): " << std::endl;
	auto longest_total_elapsed_time_exclusive = PerformanceProfiler::GetNumCallsByTotalElapsedTimeExclusive();
	for(size_t i = 0; i < max_print_count && i < longest_total_elapsed_time_exclusive.size(); i++)
		out_dest << longest_total_elapsed_time_exclusive[i].first << ": " << longest_total_elapsed_time_exclusive[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that contributed the longest total inclusive elapsed time (accumulated time divided by active thread count) (s): " << std::endl;
	auto longest_total_elapsed_time_inclusive = PerformanceProfiler::GetNumCallsByTotalElapsedTimeInclusive();
	for(size_t i = 0; i < max_print_count && i < longest_total_elapsed_time_inclusive.size(); i++)
		out_dest << longest_total_elapsed_time_inclusive[i].first << ": " << longest_total_elapsed_time_inclusive[i].second << std::endl;
	out_dest << std::endl;
#endif

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations called the most number of times: " << std::endl;
	auto most_calls = PerformanceProfiler::GetNumCallsByType();
	for(size_t i = 0; i < max_print_count && i < most_calls.size(); i++)
		out_dest << most_calls[i].first << ": " << most_calls[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that took the longest average exclusive time (s): " << std::endl;
	auto longest_ave_time_exclusive = PerformanceProfiler::GetNumCallsByAveTimeExclusive();
	for(size_t i = 0; i < max_print_count && i < longest_ave_time_exclusive.size(); i++)
		out_dest << longest_ave_time_exclusive[i].first << ": " << longest_ave_time_exclusive[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that took the longest average inclusive time (s): " << std::endl;
	auto longest_ave_time_inclusive = PerformanceProfiler::GetNumCallsByAveTimeInclusive();
	for(size_t i = 0; i < max_print_count && i < longest_ave_time_inclusive.size(); i++)
		out_dest << longest_ave_time_inclusive[i].first << ": " << longest_ave_time_inclusive[i].second << std::endl;
	out_dest << std::endl;

#if defined(MULTITHREAD_SUPPORT)
	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that contributed the longest average exclusive elapsed time (average time divided by active thread count) (s): " << std::endl;
	auto longest_ave_elapsed_time_exclusive = PerformanceProfiler::GetNumCallsByAveElapsedTimeExclusive();
	for(size_t i = 0; i < max_print_count && i < longest_ave_elapsed_time_exclusive.size(); i++)
		out_dest << longest_ave_elapsed_time_exclusive[i].first << ": " << longest_ave_elapsed_time_exclusive[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that contributed the longest average inclusive elapsed time (average time divided by active thread count) (s): " << std::endl;
	auto longest_ave_elapsed_time_inclusive = PerformanceProfiler::GetNumCallsByAveElapsedTimeInclusive();
	for(size_t i = 0; i < max_print_count && i < longest_ave_elapsed_time_inclusive.size(); i++)
		out_dest << longest_ave_elapsed_time_inclusive[i].first << ": " << longest_ave_elapsed_time_inclusive[i].second << std::endl;
	out_dest << std::endl;
#endif

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that increased the memory usage the most in total exclusive (nodes): " << std::endl;
	auto most_total_memory_exclusive = PerformanceProfiler::GetNumCallsByTotalMemoryIncreaseExclusive();
	for(size_t i = 0; i < max_print_count && i < most_total_memory_exclusive.size(); i++)
		out_dest << most_total_memory_exclusive[i].first << ": " << most_total_memory_exclusive[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that increased the memory usage the most in total inclusive (nodes): " << std::endl;
	auto most_total_memory_inclusive = PerformanceProfiler::GetNumCallsByTotalMemoryIncreaseInclusive();
	for(size_t i = 0; i < max_print_count && i < most_total_memory_inclusive.size(); i++)
		out_dest << most_total_memory_inclusive[i].first << ": " << most_total_memory_inclusive[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that increased the memory usage the most on average exclusive (nodes): " << std::endl;
	auto most_ave_memory_exclusive = PerformanceProfiler::GetNumCallsByAveMemoryIncreaseExclusive();
	for(size_t i = 0; i < max_print_count && i < most_ave_memory_exclusive.size(); i++)
		out_dest << most_ave_memory_exclusive[i].first << ": " << most_ave_memory_exclusive[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that increased the memory usage the most on average inclusive (nodes): " << std::endl;
	auto most_ave_memory_inclusive = PerformanceProfiler::GetNumCallsByAveMemoryIncreaseInclusive();
	for(size_t i = 0; i < max_print_count && i < most_ave_memory_inclusive.size(); i++)
		out_dest << most_ave_memory_inclusive[i].first << ": " << most_ave_memory_inclusive[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that decreased the memory usage the most in total exclusive (nodes): " << std::endl;
	for(size_t i = 0; i < max_print_count && i < most_total_memory_exclusive.size(); i++)
	{
		//only write out those that had a net decrease
		double mem_delta = most_total_memory_exclusive[most_total_memory_exclusive.size() - 1 - i].second;
		if(mem_delta >= 0)
			break;
		out_dest << most_total_memory_exclusive[i].first << ": " << mem_delta << std::endl;
	}
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Operations that decreased the memory usage the most on average exclusive (nodes): " << std::endl;
	for(size_t i = 0; i < max_print_count && i < most_ave_memory_exclusive.size(); i++)
	{
		//only write out those that had a net decrease
		double mem_delta = most_ave_memory_exclusive[most_total_memory_exclusive.size() - 1 - i].second;
		if(mem_delta >= 0)
			break;
		out_dest << most_total_memory_exclusive[i].first << ": " << mem_delta << std::endl;
	}
	out_dest << std::endl;

#if defined(MULTITHREAD_SUPPORT)
	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Variable assignments that had the most lock contention: " << std::endl;
	auto most_lock_contention = PerformanceProfiler::GetPerformanceCounterResultsSortedByCount(_lock_contention_counters);
	for(size_t i = 0; i < max_print_count && i < most_lock_contention.size(); i++)
		out_dest << most_lock_contention[i].first << ": " << most_lock_contention[i].second << std::endl;
	out_dest << std::endl;
#endif

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Opcodes with the most total memory writes when constructing results: " << std::endl;
	auto most_side_effects = PerformanceProfiler::GetPerformanceCounterResultsSortedByCount(_side_effect_total_memory_write_counters);
	for(size_t i = 0; i < max_print_count && i < most_side_effects.size(); i++)
		out_dest << most_side_effects[i].first << ": " << most_side_effects[i].second << std::endl;
	out_dest << std::endl;

	out_dest << "------------------------------------------------------" << std::endl;
	out_dest << "Opcodes with the most initial memory writes when constructing results: " << std::endl;
	auto most_initial_side_effects = PerformanceProfiler::GetPerformanceCounterResultsSortedByCount(_side_effect_initial_memory_write_counters);
	for(size_t i = 0; i < max_print_count && i < most_initial_side_effects.size(); i++)
		out_dest << most_initial_side_effects[i].first << ": " << most_initial_side_effects[i].second << std::endl;
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
	for(auto &c : _profiler_counters)
		total_call_count += c.second.numCalls;
	return total_call_count;
}

std::pair<int64_t, int64_t> PerformanceProfiler::GetTotalAndPositiveMemoryIncreases()
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	int64_t total_mem_increase = 0;
	int64_t positive_mem_increase = 0;
	for(auto &c : _profiler_counters)
	{
		total_mem_increase += c.second.totalMemChangeExclusive;
		if(c.second.totalMemChangeExclusive > 0)
			positive_mem_increase += c.second.totalMemChangeExclusive;
	}
	return std::make_pair(total_mem_increase, positive_mem_increase);
}

template<typename StatValueType, typename CounterValueType, typename CounterMapType>
inline std::vector<std::pair<std::string, StatValueType>> GetPerformanceStat(CounterMapType &counters,
	std::function<StatValueType(CounterValueType &)> counter_function)
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleLock lock(performance_profiler_mutex);
#endif

	//copy to proper data structure
	std::vector<std::pair<std::string, StatValueType>> results;
	results.reserve(counters.size());

	for(auto &[s, value] : counters)
		results.emplace_back(s, counter_function(value));

	//sort high to low
	std::sort(begin(results), end(results),
		[](std::pair<std::string, StatValueType> a, std::pair<std::string, StatValueType> b) -> bool
		{	return (a.second) > (b.second);	});
	return results;
}

std::vector<std::pair<std::string, size_t>> PerformanceProfiler::GetNumCallsByType()
{
	return GetPerformanceStat<size_t, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return counter_values.numCalls;
		});
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByTotalTimeExclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return counter_values.totalTimeExclusive;
		});
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveTimeExclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return counter_values.totalTimeExclusive / counter_values.numCalls;
		});
}


std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByTotalTimeInclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return counter_values.totalTimeInclusive;
		});
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveTimeInclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return counter_values.totalTimeInclusive / counter_values.numCalls;
		});
}

#if defined(MULTITHREAD_SUPPORT)
std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByTotalElapsedTimeExclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return counter_values.elapsedTimeExclusive;
		});
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveElapsedTimeExclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return counter_values.elapsedTimeExclusive / counter_values.numCalls;
		});
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByTotalElapsedTimeInclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return counter_values.elapsedTimeInclusive;
		});
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveElapsedTimeInclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return counter_values.elapsedTimeInclusive / counter_values.numCalls;
		});
}
#endif

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByTotalMemoryIncreaseExclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return static_cast<double>(counter_values.totalMemChangeExclusive);
		});
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveMemoryIncreaseExclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return static_cast<double>(counter_values.totalMemChangeExclusive) / counter_values.numCalls;
		});
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByTotalMemoryIncreaseInclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return static_cast<double>(counter_values.totalMemChangeExclusive);
		});
}

std::vector<std::pair<std::string, double>> PerformanceProfiler::GetNumCallsByAveMemoryIncreaseInclusive()
{
	return GetPerformanceStat<double, PerformanceCounters>(_profiler_counters,
		[](auto &counter_values) {
			return static_cast<double>(counter_values.totalMemChangeInclusive) / counter_values.numCalls;
		});
}

std::vector<std::pair<std::string, size_t>> PerformanceProfiler::GetPerformanceCounterResultsSortedByCount(
	FastHashMap<std::string, size_t> &counters)
{
	return GetPerformanceStat<size_t, size_t>(counters,
		[](auto &value) {
			return value;
		});
}