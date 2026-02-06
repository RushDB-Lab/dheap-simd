# DHeap4-SIMD

A high-performance d-ary heap implementation. SIMD exploration is retained as historical research only.

## Features

- **Configurable Arity (`d`)**: Tune fanout for different workloads/cache behavior
- **SIMD Research Archive**: SIMD paths are kept for reference, not as a recommended optimization direction
- **Scalar Fallback**: Automatically falls back to scalar code on non-NEON platforms
- **STL-like Interface**: Familiar `push`, `pop`, `top`, `empty`, `size` operations
- **Batch Construction**: O(n) heap construction from vector using Floyd's algorithm

## Final Conclusion (Read First)

This repository ran extensive SIMD-vs-scalar heap experiments across:
- `d = 4/8/16`
- `ALWAYS/HYBRID/NEVER` SIMD policies
- multiple `N`, warmup/iteration counts, and repeated runs
- payload-size and threshold sweeps

Observed outcome:
- SIMD speedup was not stable across repeated runs.
- Local wins in one case frequently disappeared or reversed in nearby cases.
- Aggregate results were often near parity or negative once variance was included.

Final recommendation for this codebase:
- **Do not continue investing in SIMD heap optimization.**
- Treat SIMD code and scripts as archived experiments, not the main performance path.
- Prefer scalar-focused improvements: arity tuning, memory layout, branch behavior, batching, and parallel decomposition.

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

Quantify SIMD contribution (historical research only):

```bash
python3 scripts/quantify_simd.py --warmup 1 --iters 7
python3 scripts/quantify_simd.py --simd-policy ALWAYS --warmup 1 --iters 7
python3 scripts/quantify_simd.py --arity 8 --sizes 1000000,2000000,4000000,8000000 --warmup 1 --iters 3
python3 scripts/quantify_simd.py --arity 8 --payload-bytes 64 --sizes 1000000,2000000 --warmup 1 --iters 5
```

This script builds two variants:
- `DHEAP_FORCE_SCALAR=OFF` (uses `DHEAP_SIMD_POLICY`, default `HYBRID`)
- `DHEAP_FORCE_SCALAR=ON` (force scalar fallback)

Useful CMake switches for experiments:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDHEAP_FORCE_SCALAR=ON
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDHEAP_SIMD_POLICY=ALWAYS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDHEAP_SIMD_POLICY=HYBRID -DDHEAP_SIMD_BUILD_MIN_ARITY=8 -DDHEAP_SIMD_POP_MIN_SIZE=4194304
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDHEAP_ARITY=8
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDHEAP_NODE_PAYLOAD_BYTES=128
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

The d-ary heap stores elements in a flat array where each node has up to `d` children:
- Parent of node `i`: `(i - 1) / d`
- First child of node `i`: `d*i + 1`

During `sift_down`, SIMD fast paths exist for full child blocks (`d=4/8/16`), and `DHEAP_SIMD_POLICY` controls when they are used:
- `NEVER`: scalar only
- `ALWAYS`: use SIMD whenever a full SIMD block is available
- `HYBRID` (default): only enable SIMD for selected scenarios (e.g. bulk heapify upper levels / very large pop-root cases), scalar elsewhere

## License

MIT
