
#include <atomic>
#include <thread>

#ifdef _WIN32
#include <intrin.h>
#include <windows.h>
#else
#include <x86intrin.h>
#include <cpuid.h>
#endif

#include <iostream>
#include <vector>
#include <algorithm>

uint64_t rdtscp()
{	
	unsigned int dummy;
	return __rdtscp(&dummy);
}

constexpr uint32_t num_treads = 2;
constexpr uint32_t iterations = (1 << 25);

using shared_type = volatile std::atomic<uint32_t>;

void increment_single_thread(shared_type& sum, uint32_t n_iterations)
{
	for (uint32_t i = 0; i < n_iterations; i++) {
		sum += 1;
	}
}

volatile std::atomic<uint32_t> atomic_flag; // should be lock-free
void increment_multithreaded(shared_type& shared_variable, uint32_t iterations, uint32_t thread_id)
{
	for (uint32_t i = 0; i < iterations; i++) {
		while ((atomic_flag.load(std::memory_order_acquire) % num_treads) != thread_id) {}
		shared_variable += 1;
		atomic_flag++;
	}
}

#define UNUSED(x) ((void)(x))
void emulate_atomic_reads_writes(uint32_t iterations)
{
	for (size_t i = 0; i < iterations; ++i) {

		uint32_t loaded = atomic_flag;
		UNUSED(loaded);

		atomic_flag++;
	}
}

uint64_t benchmark()
{
	uint64_t start, end;
	uint64_t elapsed_single, elapsed_multithreaded, elapsed_atomic_reads_writes;

	std::thread threads[num_treads];
	shared_type shared_variable { 0 };

	// multithreaded benchmarks
	start = rdtscp();

	for (int i = 0; i < num_treads; i++)
		threads[i] = std::thread(increment_multithreaded, std::ref(shared_variable), iterations / num_treads, i);

	for (int i = 0; i < num_treads; i++)
		threads[i].join();

	end = rdtscp();
	elapsed_multithreaded = end - start;

	// single threaded increments
	start = rdtscp();
	increment_single_thread(shared_variable, iterations);
	end = rdtscp();
	elapsed_single = end - start;

	// emulating N volatile atomic read/writes
	start = rdtscp();
	emulate_atomic_reads_writes(iterations);
	end = rdtscp();
	elapsed_atomic_reads_writes = end - start;

	return (elapsed_multithreaded - elapsed_single - elapsed_atomic_reads_writes) / iterations;
}

int main()
{
	constexpr uint64_t benchmark_iterations = 10;
	std::vector<uint64_t> results;

	for (uint64_t i = 0; i < benchmark_iterations; ++i) {
		results.push_back(benchmark());
	}

	auto median = results.begin() + results.size() / 2;
	std::nth_element(results.begin(), median, results.end());

	std::cout << "\nThe median is " << results[results.size() / 2] << std::endl;
}
