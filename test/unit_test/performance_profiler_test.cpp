//project headers:
#include "PerformanceProfiler.h"

//system headers:
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static bool CheckSection(const std::string &report, const std::string &heading,
	const std::string &expected)
{
	size_t start = report.find(heading + "\n");
	if(start == std::string::npos)
	{
		std::cerr << "Missing report section: " << heading << std::endl;
		return false;
	}
	start += heading.size() + 1;
	size_t end = report.find("\n\n", start - 1);
	std::string actual = report.substr(start, end + 1 - start);
	if(actual != expected)
	{
		std::cerr << heading << "\nExpected:\n" << expected << "Actual:\n" << actual;
		return false;
	}
	return true;
}

static bool CheckDecreasedMemory(const std::string &report,
	const std::string &total, const std::string &average)
{
	bool successful = CheckSection(report,
		"Operations that decreased the memory usage the most in total exclusive (nodes): ", total);
	successful = CheckSection(report,
		"Operations that decreased the memory usage the most on average exclusive (nodes): ", average)
		&& successful;
	return successful;
}

static std::string CaptureStdout(size_t max_print_count)
{
	std::ostringstream report;
	std::streambuf *original = std::cout.rdbuf(report.rdbuf());
	PerformanceProfiler::PrintProfilingInformation("", max_print_count);
	std::cout.rdbuf(original);
	return report.str();
}

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		std::cerr << "Expected a report file path" << std::endl;
		return 1;
	}

	bool successful = CheckDecreasedMemory(CaptureStdout(0), "", "");

	//Different call counts reverse the total and average rankings.
	for(size_t i = 0; i < 3; i++)
	{
		PerformanceProfiler::StartOperation("frequent_release", 100);
		PerformanceProfiler::EndOperation(60);
	}
	for(size_t i = 0; i < 2; i++)
	{
		PerformanceProfiler::StartOperation("larger_release", 100);
		PerformanceProfiler::EndOperation(55);
	}
	PerformanceProfiler::StartOperation("unchanged", 100);
	PerformanceProfiler::EndOperation(100);
	PerformanceProfiler::StartOperation("allocation", 100);
	PerformanceProfiler::EndOperation(110);

	const std::string total = "frequent_release: -120\nlarger_release: -90\n";
	const std::string average = "larger_release: -45\nfrequent_release: -40\n";

	//The command line passes --p-count directly to this report function.
	successful = CheckDecreasedMemory(CaptureStdout(1),
		"frequent_release: -120\n", "larger_release: -45\n") && successful;
	for(size_t count : { 0, 2, 10 })
		successful = CheckDecreasedMemory(CaptureStdout(count), total, average) && successful;

	//Exceed the stdout default with distinct decreases below the focused pair.
	std::string all_total = total;
	std::string all_average = average;
	std::string capped_total = total;
	std::string capped_average = average;
	for(size_t decrease = 21; decrease > 0; decrease--)
	{
		const std::string operation = "release_" + std::to_string(decrease);
		PerformanceProfiler::StartOperation(operation, 100);
		PerformanceProfiler::EndOperation(100 - decrease);
		const std::string row = operation + ": -" + std::to_string(decrease) + "\n";
		all_total += row;
		all_average += row;
		//The focused pair plus the first 18 additional operations fill 20 rows.
		if(decrease >= 4)
		{
			capped_total += row;
			capped_average += row;
		}
	}
	successful = CheckDecreasedMemory(CaptureStdout(0), capped_total, capped_average) && successful;

	//Omitting the count with --p-file requests the full report.
	PerformanceProfiler::PrintProfilingInformation(argv[1]);
	std::ifstream report_file(argv[1]);
	if(!report_file.is_open())
	{
		std::cerr << "Could not read report file: " << argv[1] << std::endl;
		return 1;
	}
	std::ostringstream report;
	report << report_file.rdbuf();
	report_file.close();
	successful = CheckDecreasedMemory(report.str(), all_total, all_average) && successful;
	std::remove(argv[1]);

	return successful ? 0 : 1;
}
