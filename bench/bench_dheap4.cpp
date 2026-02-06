#include "dheap4_simd.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double, std::milli>;
using MinQueue = std::priority_queue<int32_t, std::vector<int32_t>, std::greater<int32_t>>;

struct SummaryStats {
    double median_ms;
    double p95_ms;
};

struct BenchResult {
    SummaryStats dheap;
    SummaryStats stl;
    double speedup_p50;
    double speedup_p95;
};

struct BenchConfig {
    int warmup_iterations;
    int measured_iterations;
};

struct Op {
    bool is_push;
    int32_t value;
};

bool parse_int_arg(const std::string& text, int min_value, int* out) {
    if (text.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return false;
    }
    if (parsed < static_cast<long>(min_value) ||
        parsed > static_cast<long>(std::numeric_limits<int>::max())) {
        return false;
    }

    *out = static_cast<int>(parsed);
    return true;
}

bool parse_sizes_arg(const std::string& text, std::vector<size_t>* out) {
    std::vector<size_t> parsed;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t comma = text.find(',', start);
        const size_t end = (comma == std::string::npos) ? text.size() : comma;
        if (end == start) {
            return false;
        }

        const std::string token = text.substr(start, end - start);
        char* tail = nullptr;
        errno = 0;
        const unsigned long long v = std::strtoull(token.c_str(), &tail, 10);
        if (errno != 0 || tail == token.c_str() || *tail != '\0' || v == 0ULL) {
            return false;
        }
        parsed.push_back(static_cast<size_t>(v));

        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }

    if (parsed.empty()) {
        return false;
    }
    *out = std::move(parsed);
    return true;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name
              << " [--warmup N|-w N] [--iters N|-i N] [--sizes A,B,C|-s A,B,C]\n"
              << "  --warmup N  Warmup iterations (N >= 0, default 2)\n"
              << "  --iters N   Measured iterations (N >= 1, default 9)\n"
              << "  --sizes L   Comma-separated positive sizes (default 10000,100000,1000000)\n";
}

std::vector<int32_t> generate_data(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int32_t> dist(0, 1000000);
    std::vector<int32_t> data(n);
    for (auto& v : data) {
        v = dist(rng);
    }
    return data;
}

std::vector<Op> generate_mixed_ops(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int32_t> dist(0, 1000000);

    std::vector<Op> ops;
    ops.reserve(n * 2);

    size_t heap_size = 0;
    for (size_t i = 0; i < n * 2; ++i) {
        const bool do_push = (rng() % 2 == 0) || (heap_size == 0);
        if (do_push && heap_size < n) {
            ops.push_back({true, dist(rng)});
            ++heap_size;
        } else if (heap_size > 0) {
            ops.push_back({false, 0});
            --heap_size;
        } else {
            ops.push_back({true, dist(rng)});
            ++heap_size;
        }
    }

    return ops;
}

MinQueue make_reserved_min_queue(size_t reserve_n) {
    std::vector<int32_t> storage;
    storage.reserve(reserve_n);
    return MinQueue(std::greater<int32_t>(), std::move(storage));
}

double percentile_sorted(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) {
        return 0.0;
    }
    const double pos = p * static_cast<double>(sorted.size() - 1);
    const size_t lo = static_cast<size_t>(std::floor(pos));
    const size_t hi = static_cast<size_t>(std::ceil(pos));
    if (lo == hi) {
        return sorted[lo];
    }
    const double weight = pos - static_cast<double>(lo);
    return sorted[lo] + (sorted[hi] - sorted[lo]) * weight;
}

SummaryStats summarize_samples(const std::vector<double>& samples) {
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    return {
        percentile_sorted(sorted, 0.50),
        percentile_sorted(sorted, 0.95),
    };
}

BenchResult finalize_result(
    const std::vector<double>& dheap_samples,
    const std::vector<double>& stl_samples) {
    const SummaryStats dheap_stats = summarize_samples(dheap_samples);
    const SummaryStats stl_stats = summarize_samples(stl_samples);
    return {
        dheap_stats,
        stl_stats,
        stl_stats.median_ms / dheap_stats.median_ms,
        stl_stats.p95_ms / dheap_stats.p95_ms,
    };
}

BenchResult bench_push_only(size_t n, const BenchConfig& cfg) {
    const auto data = generate_data(n, 12345);
    std::vector<double> dheap_samples;
    std::vector<double> stl_samples;
    dheap_samples.reserve(static_cast<size_t>(cfg.measured_iterations));
    stl_samples.reserve(static_cast<size_t>(cfg.measured_iterations));

    for (int iter = 0; iter < cfg.warmup_iterations + cfg.measured_iterations; ++iter) {
        {
            DHeap4Simd heap;
            heap.reserve(n);
            const auto start = Clock::now();
            for (int32_t v : data) {
                heap.push(v);
            }
            const auto end = Clock::now();
            if (iter >= cfg.warmup_iterations) {
                dheap_samples.push_back(Duration(end - start).count());
            }
        }

        {
            MinQueue heap = make_reserved_min_queue(n);
            const auto start = Clock::now();
            for (int32_t v : data) {
                heap.push(v);
            }
            const auto end = Clock::now();
            if (iter >= cfg.warmup_iterations) {
                stl_samples.push_back(Duration(end - start).count());
            }
        }
    }

    return finalize_result(dheap_samples, stl_samples);
}

BenchResult bench_pop_only(size_t n, const BenchConfig& cfg) {
    const auto data = generate_data(n, 12345);
    std::vector<double> dheap_samples;
    std::vector<double> stl_samples;
    dheap_samples.reserve(static_cast<size_t>(cfg.measured_iterations));
    stl_samples.reserve(static_cast<size_t>(cfg.measured_iterations));

    for (int iter = 0; iter < cfg.warmup_iterations + cfg.measured_iterations; ++iter) {
        {
            DHeap4Simd heap(data);
            const auto start = Clock::now();
            while (!heap.empty()) {
                heap.pop();
            }
            const auto end = Clock::now();
            if (iter >= cfg.warmup_iterations) {
                dheap_samples.push_back(Duration(end - start).count());
            }
        }

        {
            MinQueue heap(std::greater<int32_t>(), data);
            const auto start = Clock::now();
            while (!heap.empty()) {
                heap.pop();
            }
            const auto end = Clock::now();
            if (iter >= cfg.warmup_iterations) {
                stl_samples.push_back(Duration(end - start).count());
            }
        }
    }

    return finalize_result(dheap_samples, stl_samples);
}

BenchResult bench_mixed(size_t n, const BenchConfig& cfg) {
    const auto ops = generate_mixed_ops(n, 54321);
    std::vector<double> dheap_samples;
    std::vector<double> stl_samples;
    dheap_samples.reserve(static_cast<size_t>(cfg.measured_iterations));
    stl_samples.reserve(static_cast<size_t>(cfg.measured_iterations));

    for (int iter = 0; iter < cfg.warmup_iterations + cfg.measured_iterations; ++iter) {
        {
            DHeap4Simd heap;
            heap.reserve(n);
            const auto start = Clock::now();
            for (const Op& op : ops) {
                if (op.is_push) {
                    heap.push(op.value);
                } else if (!heap.empty()) {
                    heap.pop();
                }
            }
            const auto end = Clock::now();
            if (iter >= cfg.warmup_iterations) {
                dheap_samples.push_back(Duration(end - start).count());
            }
        }

        {
            MinQueue heap = make_reserved_min_queue(n);
            const auto start = Clock::now();
            for (const Op& op : ops) {
                if (op.is_push) {
                    heap.push(op.value);
                } else if (!heap.empty()) {
                    heap.pop();
                }
            }
            const auto end = Clock::now();
            if (iter >= cfg.warmup_iterations) {
                stl_samples.push_back(Duration(end - start).count());
            }
        }
    }

    return finalize_result(dheap_samples, stl_samples);
}

void print_result(const std::string& test_name, size_t n, const BenchResult& result) {
    std::cout << std::left << std::setw(20) << test_name
              << std::right << std::setw(10) << n
              << std::fixed << std::setprecision(3)
              << std::setw(13) << result.dheap.median_ms
              << std::setw(13) << result.dheap.p95_ms
              << std::setw(13) << result.stl.median_ms
              << std::setw(13) << result.stl.p95_ms
              << std::setw(13) << result.speedup_p50 << "x"
              << std::setw(13) << result.speedup_p95 << "x"
              << "\n";
}

int main(int argc, char** argv) {
    std::cout << "=== DHeap4Simd vs std::priority_queue Benchmark ===\n\n";

#if DHEAP4_SIMD_ENABLED
    std::cout << "SIMD: ARM NEON enabled\n\n";
#else
    std::cout << "SIMD: Disabled (scalar fallback)\n\n";
#endif

    std::cout << "SIMD policy: ";
#if DHEAP_SIMD_POLICY == 0
    std::cout << "NEVER";
#elif DHEAP_SIMD_POLICY == 1
    std::cout << "ALWAYS";
#else
    std::cout << "HYBRID";
#endif
    std::cout << "\n";
#if DHEAP_SIMD_POLICY == 2
    std::cout << "HYBRID thresholds: build_min_arity=" << DHEAP_SIMD_BUILD_MIN_ARITY
              << ", pop_min_size=" << DHEAP_SIMD_POP_MIN_SIZE << "\n";
#endif
    std::cout << "Heap arity (d): " << DHEAP_ARITY << "\n";
    std::cout << "Node payload bytes: " << DHEAP_NODE_PAYLOAD_BYTES << "\n\n";

    BenchConfig cfg{2, 9};
    std::vector<size_t> sizes = {10000, 100000, 1000000};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        if (arg == "--warmup" || arg == "-w") {
            if (i + 1 >= argc || !parse_int_arg(argv[i + 1], 0, &cfg.warmup_iterations)) {
                std::cerr << "Invalid value for " << arg << "\n";
                print_usage(argv[0]);
                return 1;
            }
            ++i;
            continue;
        }
        if (arg == "--iters" || arg == "-i") {
            if (i + 1 >= argc || !parse_int_arg(argv[i + 1], 1, &cfg.measured_iterations)) {
                std::cerr << "Invalid value for " << arg << "\n";
                print_usage(argv[0]);
                return 1;
            }
            ++i;
            continue;
        }
        if (arg == "--sizes" || arg == "-s") {
            if (i + 1 >= argc || !parse_sizes_arg(argv[i + 1], &sizes)) {
                std::cerr << "Invalid value for " << arg << "\n";
                print_usage(argv[0]);
                return 1;
            }
            ++i;
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "Warmup iterations: " << cfg.warmup_iterations
              << ", measured iterations: " << cfg.measured_iterations << "\n\n";

    std::cout << std::left << std::setw(20) << "Test"
              << std::right << std::setw(10) << "N"
              << std::setw(13) << "DHeap p50"
              << std::setw(13) << "DHeap p95"
              << std::setw(13) << "STL p50"
              << std::setw(13) << "STL p95"
              << std::setw(13) << "Spd(p50)"
              << std::setw(13) << "Spd(p95)"
              << "\n";
    std::cout << std::string(105, '-') << "\n";

    for (size_t n : sizes) {
        print_result("push-only", n, bench_push_only(n, cfg));
    }

    std::cout << "\n";

    for (size_t n : sizes) {
        print_result("pop-only", n, bench_pop_only(n, cfg));
    }

    std::cout << "\n";

    for (size_t n : sizes) {
        print_result("mixed (50/50)", n, bench_mixed(n, cfg));
    }

    std::cout << "\n=== Benchmark complete ===\n";
    return 0;
}
