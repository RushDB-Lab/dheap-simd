#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#ifndef DHEAP4_SIMD_ENABLED
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define DHEAP4_SIMD_ENABLED 1
#else
#define DHEAP4_SIMD_ENABLED 0
#endif
#endif

#ifndef DHEAP_NODE_PAYLOAD_BYTES
#define DHEAP_NODE_PAYLOAD_BYTES 0
#endif

#ifndef DHEAP_ARITY
#define DHEAP_ARITY 4
#endif

#ifndef DHEAP_SIMD_POLICY
#define DHEAP_SIMD_POLICY 2
#endif

#ifndef DHEAP_SIMD_BUILD_MIN_ARITY
#define DHEAP_SIMD_BUILD_MIN_ARITY 8
#endif

#ifndef DHEAP_SIMD_POP_MIN_SIZE
#define DHEAP_SIMD_POP_MIN_SIZE 4194304
#endif

class DHeap4Simd {
public:
    using value_type = int32_t;

    DHeap4Simd() = default;
    explicit DHeap4Simd(std::vector<value_type> data);

    size_t size() const noexcept { return heap_keys_.size(); }
    bool empty() const noexcept { return heap_keys_.empty(); }

    const value_type& top() const {
        if (empty()) throw std::out_of_range("heap is empty");
        return heap_keys_[0];
    }

    void push(value_type v);
    void pop();
    void clear() noexcept;
    void reserve(size_t n);

private:
    static constexpr int kD = DHEAP_ARITY;
    static constexpr int kSimdPolicy = DHEAP_SIMD_POLICY;
    using slot_type = uint32_t;

#if DHEAP_NODE_PAYLOAD_BYTES > 0
    struct Payload {
        std::array<uint8_t, DHEAP_NODE_PAYLOAD_BYTES> bytes{};
    };
#endif

    std::vector<value_type> heap_keys_;
    std::vector<slot_type> heap_slots_;
#if DHEAP_NODE_PAYLOAD_BYTES > 0
    std::vector<Payload> payload_store_;
#endif
    std::vector<slot_type> free_slots_;
    slot_type next_slot_ = 0;

    static_assert(kD >= 2, "DHEAP_ARITY must be >= 2");
    static_assert(kSimdPolicy >= 0 && kSimdPolicy <= 2, "DHEAP_SIMD_POLICY must be 0, 1, or 2");
    static size_t parent(size_t i) noexcept { return (i - 1) / static_cast<size_t>(kD); }
    static size_t first_child(size_t i) noexcept { return i * static_cast<size_t>(kD) + 1; }

    slot_type acquire_slot();
    void release_slot(slot_type slot) noexcept { free_slots_.push_back(slot); }
    void sift_up(size_t i);
    void sift_down(size_t i, bool heapify_phase);
};
