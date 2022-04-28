
#include <atomic>
#include <thread>

#ifdef _WIN32
#include <intrin.h>
#else
#include <x86intrin.h>
#include <cpuid.h>
#endif

#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <memory>

using namespace std::chrono_literals;

struct alignas(64) CacheLine
{
	std::atomic<uint64_t> v1;
	std::atomic<uint64_t> v2;
};

uint64_t rdtscp()
{	
	unsigned int dummy = 0;
	return __rdtscp(&dummy);
}

double tsc2ns_factor()
{
	uint64_t s, e;
	constexpr double ticks_to_wait = 1e9;

	auto start = std::chrono::high_resolution_clock::now();

	s = rdtscp();
	e = s;

	while (e - s < ticks_to_wait) {
		e = rdtscp();
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

	return ticks_to_wait / nanoseconds.count();
}

double tsc2ns(uint64_t ticks)
{
	return ticks / tsc2ns_factor();
}

void threadA(CacheLine& cache_line)
{
	while (cache_line.v1.load(std::memory_order_acquire) == 0) {}

	cache_line.v2.store(rdtscp());
}

void threadB(CacheLine& cache_line, std::vector<uint64_t>& samples, uint64_t& current_sample)
{
	cache_line.v1.store(rdtscp());

	while (cache_line.v2.load(std::memory_order_acquire) == 0) {}
	samples[current_sample++] = (cache_line.v2.load() - cache_line.v1.load()) / 2;
}

void benchmark()
{
	constexpr uint64_t n_samples = 500;

	std::vector<uint64_t> samples;
	samples.resize(n_samples);

	CacheLine cache_line;
	uint64_t current_sample = 0;

	for (size_t i = 0; i < samples.size(); ++i)
	{
		cache_line.v1.store(0);
		cache_line.v2.store(0);

		auto t1 = std::thread(threadA, std::ref(cache_line));
		std::this_thread::sleep_for(0.01s); // just to be sure that thread A has time to cache cache_line
		auto t2 = std::thread(threadB, std::ref(cache_line), std::ref(samples), std::ref(current_sample));

		t1.join();
		t2.join();
	}

	std::sort(samples.begin(), samples.end());
	std::cout << "Median value is: " << tsc2ns(samples[samples.size() / 2]) << std::endl;
}

int main()
{
	benchmark();
}
