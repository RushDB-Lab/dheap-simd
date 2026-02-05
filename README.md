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

Optional benchmark parameters:

```bash
./build/bench_dheap4 --warmup 3 --iters 15
./build/bench_dheap4 -w 1 -i 5
./build/bench_dheap4 --help
```

Output columns:
- `DHeap p50` / `STL p50`: median latency (typical performance)
- `DHeap p95` / `STL p95`: tail latency (stability under variance)
- `Spd(p50)` / `Spd(p95)`: speedup based on median and p95

Benchmark fairness notes:
- Push and mixed tests reserve capacity for both heaps.
- Mixed operation sequence is generated once per run with a fixed seed.

## Tests

```bash
./build/test_dheap4
```

Test coverage includes:
- Basic heap operations and sorted output checks
- Empty-heap exception paths (`top()` / `pop()` must throw)
- Boundary values (`INT32_MIN`, `INT32_MAX`, duplicates)
- Random differential tests against `std::priority_queue`
- Long-run random differential test (200k operations)

## How It Works

The 4-ary heap stores elements in a flat array where each node has up to 4 children:
- Parent of node `i`: `(i - 1) / 4`
- Children of node `i`: `4*i + 1`, `4*i + 2`, `4*i + 3`, `4*i + 4`

During `sift_down`, when all 4 children exist, ARM NEON loads all 4 values in a single `vld1q_s32` instruction and finds the minimum using SIMD operations, reducing comparison overhead.

## License

MIT
