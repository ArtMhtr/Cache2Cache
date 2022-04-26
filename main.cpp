
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

uint64_t rdtscp()
{	
	unsigned int dummy = 0;
	return __rdtscp(&dummy);
}

double get_tsc_to_ns_factor()
{
	uint64_t t1 = rdtscp();
	std::this_thread::sleep_for(std::chrono::seconds(10));
	uint64_t t2 = rdtscp();

	return ((double)(t2 - t1) / 1e10);
}

uint64_t tsc_to_ns(uint64_t ticks, double factor)
{
	return (uint64_t)(ticks / factor);
}

constexpr uint32_t num_treads = 2;
constexpr uint32_t iterations = (1 << 24);

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

uint64_t benchmark(double factor)
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
	elapsed_multithreaded = tsc_to_ns(end - start, factor);

	// single threaded increments
	start = rdtscp();
	increment_single_thread(shared_variable, iterations);
	end = rdtscp();
	elapsed_single = tsc_to_ns(end - start, factor);

	// emulating N volatile atomic read/writes
	start = rdtscp();
	emulate_atomic_reads_writes(iterations);
	end = rdtscp();
	elapsed_atomic_reads_writes = tsc_to_ns(end - start, factor);

	return (elapsed_multithreaded - elapsed_single - elapsed_atomic_reads_writes) / iterations;
}

int main()
{
	constexpr uint64_t benchmark_iterations = 10;
	double factor = get_tsc_to_ns_factor();

	std::vector<uint64_t> results;
	for (uint64_t i = 0; i < benchmark_iterations; ++i) {
		results.push_back(benchmark(factor));
	}

	std::sort(results.begin(), results.end());

	auto Q1 = results.begin() + results.size() / 4;
	auto Q3 = results.end() - results.size() / 4;

	std::cout << "\nThe Interquartile range is " << *Q3 - *Q1 << std::endl;
	std::cout << "The Average of Interquartile range is " << std::accumulate(Q1, Q3, 0.0) / (Q3 - Q1) << std::endl;
}
