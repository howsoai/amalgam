//
// Unit test driver for Amalgam interpreter
//

//3rd party:
#include "subprocess_h/subprocess.h"

//system headers:
#include <cassert>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#ifdef WIN32
int32_t NEWLINE_SIZE = 2;
#else
int32_t NEWLINE_SIZE = 1;
#endif

//simple Process class that should only be used for single-threaded handling of a process
class Process
{
public:

	Process(std::string path)
		: path(path), process(nullptr), pStdin(nullptr), pStdout(nullptr), pStderr(nullptr)
	{
		// Create process:
		const char* command_line[] = { path.c_str(), "--repl", NULL };
		struct subprocess_s subprocess;
		int result = subprocess_create(command_line, subprocess_option_enable_async, &subprocess);
		if(0 != result)
		{
			throw std::runtime_error("Could not start process: " + std::to_string(result));
		}
		else
		{
			process = &subprocess;
			pStdin = subprocess_stdin(process);
			pStdout = subprocess_stdout(process);
			pStderr = subprocess_stderr(process);
		}
	}

	~Process()
	{
		if(isAlive())
		{
			int result = subprocess_terminate(process);
			if(0 != result) {
				std::cerr << "Unable to terminate process" << std::endl;
			}
		}
	}

	bool isAlive()
	{
		if(process != nullptr)
		{
			int result = subprocess_alive(process);
			if(0 != result)
			{
				return true;
			}
		}

		return false;
	}

	std::string getPath()
	{
		return path;
	}

	void send(const std::string& in)
	{
		size_t n = in.size();
		
		//write data size
		if(1 != fwrite(&n, sizeof n, 1, pStdin))
		{
			std::cerr << "Failed: send data size" << std::endl;
		}
		
		//write data
		if(n != fwrite(in.data(), 1, n, pStdin))
		{
			std::cerr << "Failed: send data" << std::endl;
		}
		
		//flush
		if(0 != fflush(pStdin))
		{
			std::cerr << "Failed: flush stdin" << std::endl;
		}
	}

	std::string receive()
	{
		//read data size
		size_t size = 0;
		if(1 != fread(&size, sizeof size, 1, pStdout))
		{
			std::cerr << "Failed: receive data size" << std::endl;
		}

		//read data
		std::string output;
		char* data(new char[size + 1]);
		if(size != fread(data, sizeof data[0], size, pStdout))
		{
			std::cerr << "Failed: receive data" << std::endl;
		}
		data[size] = '\0';
		output = data;
		delete[] data;

		return output;
	}

	std::string communicate(const std::string& in)
	{
		send(in);
		return receive();
	}

private:

	std::string path;
	subprocess_s *process;
	FILE *pStdin;
	FILE *pStdout;
	FILE *pStderr;
};

int main(int argc, char* argv[])
{
	Process amlg_proc((argc > 1) ? argv[1] : "amalgam-mt");
	std::cout << "Running Amalgam process: " << amlg_proc.getPath() << std::endl;

	std::string test = R"(
		(print 5)
	)";
	std::string expected = "5";
	auto actual = amlg_proc.communicate(test);
	//actual.erase(actual.size() - NEWLINE_SIZE);
	assert(actual == expected);

	//let Amalgam exit:
	amlg_proc.send(R"((system "exit"))");

	return 0;
}