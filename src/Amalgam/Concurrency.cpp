//project headers:
#include "Concurrency.h"
#include "PlatformSpecific.h"

#if defined(MULTITHREAD_SUPPORT)
ThreadPool Concurrency::threadPool;

size_t _max_num_threads = std::thread::hardware_concurrency();
#endif

#if defined(_OPENMP)
//default with half the number of threads on the system (since Amalgam typically is bottlenecked by bus bandwidth)
size_t _max_num_threads = std::thread::hardware_concurrency() / 2;
#endif

#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)

size_t Concurrency::GetMaxNumThreads()
{
	return _max_num_threads;
}

void Concurrency::SetMaxNumThreads(size_t max_num_threads)
{
	if(max_num_threads > 0)
		_max_num_threads = max_num_threads;
	else
	{
#ifdef MULTITHREAD_SUPPORT
		_max_num_threads = std::thread::hardware_concurrency();
#else //_OPENMP
		//half rounded up if an odd number for some reason
		_max_num_threads = (std::thread::hardware_concurrency() + 1) / 2;
#endif
	}

#ifdef _OPENMP
	int num_threads = static_cast<int>(_max_num_threads);
	if(num_threads > 0)
		omp_set_num_threads(num_threads);
#endif

#ifdef MULTITHREAD_SUPPORT
	threadPool.SetMaxNumActiveThreads(_max_num_threads);
#endif

}

#endif
