# DHeap4-SIMD

A high-performance 4-ary heap (d-heap) implementation with SIMD acceleration for ARM NEON.

## Features

- **4-ary Heap**: Better cache locality compared to binary heaps
- **SIMD Optimized**: Uses ARM NEON intrinsics for parallel child comparison in `sift_down`
- **Scalar Fallback**: Automatically falls back to scalar code on non-NEON platforms
- **STL-like Interface**: Familiar `push`, `pop`, `top`, `empty`, `size` operations
- **Batch Construction**: O(n) heap construction from vector using Floyd's algorithm

## Requirements

- C++17 or later
- CMake 3.16+
- ARM64 processor (Apple Silicon, ARM servers) for SIMD acceleration

## Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Usage

```cpp
#include "dheap4_simd.hpp"

// Create empty heap
DHeap4Simd heap;
heap.push(5);
heap.push(3);
heap.push(7);

// Min element
int min = heap.top();  // 3

heap.pop();  // Remove minimum

// Build from vector (O(n))
std::vector<int32_t> data = {9, 4, 7, 1, 8, 2};
DHeap4Simd heap2(data);
```

## API

| Method | Description |
|--------|-------------|
| `push(v)` | Insert element |
| `pop()` | Remove minimum element |
| `top()` | Access minimum element |
| `empty()` | Check if heap is empty |
| `size()` | Get number of elements |
| `clear()` | Remove all elements |
| `reserve(n)` | Reserve capacity |

## Benchmark

Run the benchmark to compare against `std::priority_queue`:

```bash
./build/bench_dheap4
```

### Sample Results (Apple M1)

| Test | N | DHeap4 (ms) | STL (ms) | Speedup |
|------|---|-------------|----------|---------|
| pop-only | 100,000 | 2.1 | 3.8 | 1.8x |
| pop-only | 1,000,000 | 28.5 | 52.3 | 1.8x |
| mixed | 1,000,000 | 45.2 | 78.6 | 1.7x |

## Tests

```bash
./build/test_dheap4
```

## How It Works

The 4-ary heap stores elements in a flat array where each node has up to 4 children:
- Parent of node `i`: `(i - 1) / 4`
- Children of node `i`: `4*i + 1`, `4*i + 2`, `4*i + 3`, `4*i + 4`

During `sift_down`, when all 4 children exist, ARM NEON loads all 4 values in a single `vld1q_s32` instruction and finds the minimum using SIMD operations, reducing comparison overhead.

## License

MIT
