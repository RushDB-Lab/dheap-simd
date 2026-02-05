#include "dheap4_simd.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <queue>
#include <cassert>

void test_basic_operations() {
    std::cout << "Test: basic operations... ";

    DHeap4Simd heap;
    assert(heap.empty());
    assert(heap.size() == 0);

    heap.push(5);
    assert(!heap.empty());
    assert(heap.size() == 1);
    assert(heap.top() == 5);

    heap.push(3);
    assert(heap.top() == 3);

    heap.push(7);
    heap.push(1);
    assert(heap.top() == 1);

    heap.pop();
    assert(heap.top() == 3);

    heap.pop();
    assert(heap.top() == 5);

    heap.pop();
    assert(heap.top() == 7);

    heap.pop();
    assert(heap.empty());

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

    assert(output == expected);
    std::cout << "PASSED\n";
}

void test_random_operations() {
    std::cout << "Test: random operations... ";

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int32_t> dist(-100000, 100000);

    DHeap4Simd heap;
    std::priority_queue<int32_t, std::vector<int32_t>, std::greater<int32_t>> stl_heap;

    for (int i = 0; i < 10000; ++i) {
        int op = rng() % 3;

        if (op == 0 || heap.empty()) {
            int32_t val = dist(rng);
            heap.push(val);
            stl_heap.push(val);
        } else if (op == 1) {
            assert(heap.top() == stl_heap.top());
        } else {
            assert(heap.top() == stl_heap.top());
            heap.pop();
            stl_heap.pop();
        }
    }

    while (!heap.empty()) {
        assert(heap.top() == stl_heap.top());
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

    assert(output == expected);
    std::cout << "PASSED\n";
}

void test_edge_cases() {
    std::cout << "Test: edge cases... ";

    // Single element
    DHeap4Simd heap1;
    heap1.push(42);
    assert(heap1.top() == 42);
    heap1.pop();
    assert(heap1.empty());

    // Duplicate values
    DHeap4Simd heap2;
    for (int i = 0; i < 20; ++i) {
        heap2.push(5);
    }
    for (int i = 0; i < 20; ++i) {
        assert(heap2.top() == 5);
        heap2.pop();
    }

    // Boundary: exactly 4 children
    DHeap4Simd heap3;
    for (int i = 5; i >= 1; --i) {
        heap3.push(i);
    }
    assert(heap3.top() == 1);

    // Boundary: 3 children (triggers scalar fallback)
    DHeap4Simd heap4;
    for (int i = 4; i >= 1; --i) {
        heap4.push(i);
    }
    assert(heap4.top() == 1);

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
        assert(curr >= prev);
        prev = curr;
        heap.pop();
    }

    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== DHeap4Simd Unit Tests ===\n\n";

    test_basic_operations();
    test_sorted_output();
    test_random_operations();
    test_build_from_vector();
    test_edge_cases();
    test_large_scale();

    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}
