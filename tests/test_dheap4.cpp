#include "dheap4_simd.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

[[noreturn]] void fail_check(const char* expr, const char* file, int line) {
    std::ostringstream oss;
    oss << "CHECK failed: " << expr << " at " << file << ":" << line;
    throw std::runtime_error(oss.str());
}

}  // namespace

#define CHECK(expr) do { if (!(expr)) fail_check(#expr, __FILE__, __LINE__); } while (0)

void check_throws_out_of_range(const std::function<void()>& fn) {
    bool thrown = false;
    try {
        fn();
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    CHECK(thrown);
}

void test_empty_heap_exceptions() {
    std::cout << "Test: empty heap exceptions... ";

    DHeap4Simd heap;
    check_throws_out_of_range([&]() { static_cast<void>(heap.top()); });
    check_throws_out_of_range([&]() { heap.pop(); });

    std::cout << "PASSED\n";
}

void test_basic_operations() {
    std::cout << "Test: basic operations... ";

    DHeap4Simd heap;
    CHECK(heap.empty());
    CHECK(heap.size() == 0);

    heap.push(5);
    CHECK(!heap.empty());
    CHECK(heap.size() == 1);
    CHECK(heap.top() == 5);

    heap.push(3);
    CHECK(heap.top() == 3);

    heap.push(7);
    heap.push(1);
    CHECK(heap.top() == 1);

    heap.pop();
    CHECK(heap.top() == 3);

    heap.pop();
    CHECK(heap.top() == 5);

    heap.pop();
    CHECK(heap.top() == 7);

    heap.pop();
    CHECK(heap.empty());

    std::cout << "PASSED\n";
}

void test_sorted_output() {
    std::cout << "Test: sorted output... ";

    std::vector<int32_t> input = {42, 17, 89, 3, 56, 12, 78, 23, 45, 67, 1, 99};
    DHeap4Simd heap;

    for (int32_t v : input) {
        heap.push(v);
    }

    std::vector<int32_t> output;
    while (!heap.empty()) {
        output.push_back(heap.top());
        heap.pop();
    }

    std::vector<int32_t> expected = input;
    std::sort(expected.begin(), expected.end());

    CHECK(output == expected);
    std::cout << "PASSED\n";
}

void test_random_operations() {
    std::cout << "Test: random operations... ";

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int32_t> dist(-100000, 100000);

    DHeap4Simd heap;
    std::priority_queue<int32_t, std::vector<int32_t>, std::greater<int32_t>> stl_heap;

    for (int i = 0; i < 10000; ++i) {
        int op = static_cast<int>(rng() % 3);

        if (op == 0 || heap.empty()) {
            int32_t val = dist(rng);
            heap.push(val);
            stl_heap.push(val);
        } else if (op == 1) {
            CHECK(heap.top() == stl_heap.top());
        } else {
            CHECK(heap.top() == stl_heap.top());
            heap.pop();
            stl_heap.pop();
        }
    }

    while (!heap.empty()) {
        CHECK(heap.top() == stl_heap.top());
        heap.pop();
        stl_heap.pop();
    }

    std::cout << "PASSED\n";
}

void test_build_from_vector() {
    std::cout << "Test: build from vector... ";

    std::vector<int32_t> input = {9, 4, 7, 1, 8, 2, 6, 3, 5};
    DHeap4Simd heap(input);

    std::vector<int32_t> output;
    while (!heap.empty()) {
        output.push_back(heap.top());
        heap.pop();
    }

    std::vector<int32_t> expected = input;
    std::sort(expected.begin(), expected.end());

    CHECK(output == expected);
    std::cout << "PASSED\n";
}

void test_edge_cases() {
    std::cout << "Test: edge cases... ";

    DHeap4Simd heap1;
    heap1.push(42);
    CHECK(heap1.top() == 42);
    heap1.pop();
    CHECK(heap1.empty());

    DHeap4Simd heap2;
    for (int i = 0; i < 20; ++i) {
        heap2.push(5);
    }
    for (int i = 0; i < 20; ++i) {
        CHECK(heap2.top() == 5);
        heap2.pop();
    }

    DHeap4Simd heap3;
    for (int i = 5; i >= 1; --i) {
        heap3.push(i);
    }
    CHECK(heap3.top() == 1);

    DHeap4Simd heap4;
    for (int i = 4; i >= 1; --i) {
        heap4.push(i);
    }
    CHECK(heap4.top() == 1);

    std::cout << "PASSED\n";
}

void test_boundary_values() {
    std::cout << "Test: boundary values... ";

    const int32_t min_v = std::numeric_limits<int32_t>::min();
    const int32_t max_v = std::numeric_limits<int32_t>::max();
    const int32_t near_min = min_v + 1;
    const int32_t near_max = max_v - 1;

    std::vector<int32_t> input = {
        max_v, min_v, 0, near_min, near_max,
        min_v, max_v, near_min, near_max, 0
    };

    DHeap4Simd heap;
    for (int32_t v : input) {
        heap.push(v);
    }

    std::sort(input.begin(), input.end());
    for (int32_t expected : input) {
        CHECK(!heap.empty());
        CHECK(heap.top() == expected);
        heap.pop();
    }
    CHECK(heap.empty());

    std::cout << "PASSED\n";
}

void test_large_scale() {
    std::cout << "Test: large scale (100k elements)... ";

    std::mt19937 rng(54321);
    std::uniform_int_distribution<int32_t> dist(0, 1000000);

    std::vector<int32_t> data(100000);
    for (auto& v : data) {
        v = dist(rng);
    }

    DHeap4Simd heap(data);

    int32_t prev = std::numeric_limits<int32_t>::min();
    while (!heap.empty()) {
        int32_t curr = heap.top();
        CHECK(curr >= prev);
        prev = curr;
        heap.pop();
    }

    std::cout << "PASSED\n";
}

void test_long_random_differential() {
    std::cout << "Test: long random differential (200k ops)... ";

    std::mt19937 rng(20260206);
    std::uniform_int_distribution<int32_t> value_dist(
        std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
    std::uniform_int_distribution<int> op_dist(0, 99);

    DHeap4Simd heap;
    std::priority_queue<int32_t, std::vector<int32_t>, std::greater<int32_t>> stl_heap;

    constexpr int kOps = 200000;
    for (int i = 0; i < kOps; ++i) {
        const int op = op_dist(rng);

        if (op < 50 || heap.empty()) {  // 50% push (or forced when empty)
            const int32_t v = value_dist(rng);
            heap.push(v);
            stl_heap.push(v);
        } else if (op < 75) {  // 25% top check
            CHECK(!heap.empty());
            CHECK(heap.top() == stl_heap.top());
        } else {  // 25% pop
            CHECK(!heap.empty());
            CHECK(heap.top() == stl_heap.top());
            heap.pop();
            stl_heap.pop();
        }

        CHECK(heap.size() == stl_heap.size());
        if (!heap.empty()) {
            CHECK(heap.top() == stl_heap.top());
        }
    }

    while (!heap.empty()) {
        CHECK(heap.top() == stl_heap.top());
        heap.pop();
        stl_heap.pop();
    }
    CHECK(stl_heap.empty());

    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== DHeap4Simd Unit Tests ===\n\n";

    try {
        test_empty_heap_exceptions();
        test_basic_operations();
        test_sorted_output();
        test_random_operations();
        test_build_from_vector();
        test_edge_cases();
        test_boundary_values();
        test_large_scale();
        test_long_random_differential();
    } catch (const std::exception& e) {
        std::cerr << "\n=== Test failed ===\n" << e.what() << "\n";
        return 1;
    }

    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}
