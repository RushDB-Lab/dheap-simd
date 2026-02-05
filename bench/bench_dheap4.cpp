#include "dheap4_simd.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <queue>
#include <iomanip>
#include <functional>

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double, std::milli>;

struct BenchResult {
    double dheap_ms;
    double stl_ms;
    double speedup;
};

// Generate random data with fixed seed for reproducibility
std::vector<int32_t> generate_data(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int32_t> dist(0, 1000000);
    std::vector<int32_t> data(n);
    for (auto& v : data) {
        v = dist(rng);
    }
    return data;
}

BenchResult bench_push_only(size_t n, int iterations) {
    auto data = generate_data(n, 12345);

    double dheap_total = 0;
    double stl_total = 0;

    for (int iter = 0; iter < iterations; ++iter) {
        // DHeap4Simd
        {
            DHeap4Simd heap;
            heap.reserve(n);
            auto start = Clock::now();
            for (int32_t v : data) {
                heap.push(v);
            }
            auto end = Clock::now();
            dheap_total += Duration(end - start).count();
        }

        // STL priority_queue
        {
            std::priority_queue<int32_t, std::vector<int32_t>, std::greater<int32_t>> heap;
            auto start = Clock::now();
            for (int32_t v : data) {
                heap.push(v);
            }
            auto end = Clock::now();
            stl_total += Duration(end - start).count();
        }
    }

    double dheap_avg = dheap_total / iterations;
    double stl_avg = stl_total / iterations;

    return {dheap_avg, stl_avg, stl_avg / dheap_avg};
}

BenchResult bench_pop_only(size_t n, int iterations) {
    auto data = generate_data(n, 12345);

    double dheap_total = 0;
    double stl_total = 0;

    for (int iter = 0; iter < iterations; ++iter) {
        // DHeap4Simd
        {
            DHeap4Simd heap(data);
            auto start = Clock::now();
            while (!heap.empty()) {
                heap.pop();
            }
            auto end = Clock::now();
            dheap_total += Duration(end - start).count();
        }

        // STL priority_queue
        {
            std::priority_queue<int32_t, std::vector<int32_t>, std::greater<int32_t>> heap(
                std::greater<int32_t>(), data);
            auto start = Clock::now();
            while (!heap.empty()) {
                heap.pop();
            }
            auto end = Clock::now();
            stl_total += Duration(end - start).count();
        }
    }

    double dheap_avg = dheap_total / iterations;
    double stl_avg = stl_total / iterations;

    return {dheap_avg, stl_avg, stl_avg / dheap_avg};
}

BenchResult bench_mixed(size_t n, int iterations) {
    std::mt19937 rng(54321);
    std::uniform_int_distribution<int32_t> dist(0, 1000000);

    double dheap_total = 0;
    double stl_total = 0;

    for (int iter = 0; iter < iterations; ++iter) {
        // Generate operation sequence: 50% push, 50% pop
        std::vector<std::pair<bool, int32_t>> ops;
        ops.reserve(n * 2);

        // First fill with n pushes
        for (size_t i = 0; i < n; ++i) {
            ops.push_back({true, dist(rng)});
        }
        // Then n pops
        for (size_t i = 0; i < n; ++i) {
            ops.push_back({false, 0});
        }
        // Shuffle to mix operations (but ensure heap never goes negative)
        // Use a valid mixed sequence instead
        ops.clear();
        size_t heap_size = 0;
        for (size_t i = 0; i < n * 2; ++i) {
            bool do_push = (rng() % 2 == 0) || (heap_size == 0);
            if (do_push && heap_size < n) {
                ops.push_back({true, dist(rng)});
                heap_size++;
            } else if (heap_size > 0) {
                ops.push_back({false, 0});
                heap_size--;
            } else {
                ops.push_back({true, dist(rng)});
                heap_size++;
            }
        }

        // DHeap4Simd
        {
            DHeap4Simd heap;
            heap.reserve(n);
            auto start = Clock::now();
            for (const auto& op : ops) {
                if (op.first) {
                    heap.push(op.second);
                } else if (!heap.empty()) {
                    heap.pop();
                }
            }
            auto end = Clock::now();
            dheap_total += Duration(end - start).count();
        }

        // STL priority_queue
        {
            std::priority_queue<int32_t, std::vector<int32_t>, std::greater<int32_t>> heap;
            auto start = Clock::now();
            for (const auto& op : ops) {
                if (op.first) {
                    heap.push(op.second);
                } else if (!heap.empty()) {
                    heap.pop();
                }
            }
            auto end = Clock::now();
            stl_total += Duration(end - start).count();
        }
    }

    double dheap_avg = dheap_total / iterations;
    double stl_avg = stl_total / iterations;

    return {dheap_avg, stl_avg, stl_avg / dheap_avg};
}

void print_result(const std::string& test_name, size_t n, const BenchResult& result) {
    std::cout << std::left << std::setw(20) << test_name
              << std::right << std::setw(10) << n
              << std::fixed << std::setprecision(3)
              << std::setw(12) << result.dheap_ms
              << std::setw(12) << result.stl_ms
              << std::setw(10) << result.speedup << "x"
              << "\n";
}

int main() {
    std::cout << "=== DHeap4Simd vs std::priority_queue Benchmark ===\n\n";

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    std::cout << "SIMD: ARM NEON enabled\n\n";
#else
    std::cout << "SIMD: Disabled (scalar fallback)\n\n";
#endif

    std::cout << std::left << std::setw(20) << "Test"
              << std::right << std::setw(10) << "N"
              << std::setw(12) << "DHeap(ms)"
              << std::setw(12) << "STL(ms)"
              << std::setw(11) << "Speedup"
              << "\n";
    std::cout << std::string(65, '-') << "\n";

    std::vector<size_t> sizes = {10000, 100000, 1000000};
    int iterations = 5;

    for (size_t n : sizes) {
        auto result = bench_push_only(n, iterations);
        print_result("push-only", n, result);
    }

    std::cout << "\n";

    for (size_t n : sizes) {
        auto result = bench_pop_only(n, iterations);
        print_result("pop-only", n, result);
    }

    std::cout << "\n";

    for (size_t n : sizes) {
        auto result = bench_mixed(n, iterations);
        print_result("mixed (50/50)", n, result);
    }

    std::cout << "\n=== Benchmark complete ===\n";
    return 0;
}
