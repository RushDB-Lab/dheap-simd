#include "dheap4_simd.hpp"

#include <algorithm>

#if DHEAP4_SIMD_ENABLED
#include <arm_neon.h>
#define USE_NEON 1
#if defined(__aarch64__)
#define USE_NEON_MINV 1
#else
#define USE_NEON_MINV 0
#endif
#else
#define USE_NEON 0
#define USE_NEON_MINV 0
#endif

#if USE_NEON
namespace {
int32_t neon_min4(const int32x4_t v) noexcept {
#if USE_NEON_MINV
    return vminvq_s32(v);
#else
    int32x2_t min2 = vmin_s32(vget_low_s32(v), vget_high_s32(v));
    min2 = vpmin_s32(min2, min2);
    return vget_lane_s32(min2, 0);
#endif
}

uint32_t neon_equal_mask4(const int32x4_t v, const int32_t target) noexcept {
    const uint32x4_t eq = vceqq_s32(v, vdupq_n_s32(target));
#if defined(__aarch64__)
    static const uint32_t kLaneBitsData[4] = {1u, 2u, 4u, 8u};
    const uint32x4_t lane_bits = vld1q_u32(kLaneBitsData);
    const uint32x4_t bits = vandq_u32(eq, lane_bits);
    return vaddvq_u32(bits);
#else
    const uint32_t m0 = vgetq_lane_u32(eq, 0) >> 31;
    const uint32_t m1 = vgetq_lane_u32(eq, 1) >> 31;
    const uint32_t m2 = vgetq_lane_u32(eq, 2) >> 31;
    const uint32_t m3 = vgetq_lane_u32(eq, 3) >> 31;
    return m0 | (m1 << 1) | (m2 << 2) | (m3 << 3);
#endif
}

size_t neon_first_equal_index4(const int32x4_t v, const int32_t target) noexcept {
    const uint32_t mask = neon_equal_mask4(v, target);
    if (mask == 0u) {
        return 0;
    }
    return static_cast<size_t>(__builtin_ctz(mask));
}
}  // namespace
#endif

DHeap4Simd::DHeap4Simd(std::vector<value_type> data)
    : heap_keys_(std::move(data)) {
    heap_slots_.reserve(heap_keys_.size());
    for (size_t i = 0; i < heap_keys_.size(); ++i) {
        heap_slots_.push_back(acquire_slot());
    }

    if (heap_keys_.size() > 1) {
        for (size_t i = parent(heap_keys_.size() - 1) + 1; i > 0; --i) {
            sift_down(i - 1, true);
        }
    }
}

void DHeap4Simd::clear() noexcept {
    heap_keys_.clear();
    heap_slots_.clear();
    free_slots_.clear();
    next_slot_ = 0;
#if DHEAP_NODE_PAYLOAD_BYTES > 0
    payload_store_.clear();
#endif
}

void DHeap4Simd::reserve(size_t n) {
    heap_keys_.reserve(n);
    heap_slots_.reserve(n);
}

DHeap4Simd::slot_type DHeap4Simd::acquire_slot() {
    if (!free_slots_.empty()) {
        const slot_type slot = free_slots_.back();
        free_slots_.pop_back();
        return slot;
    }

    const slot_type slot = next_slot_++;
#if DHEAP_NODE_PAYLOAD_BYTES > 0
    payload_store_.emplace_back();
#endif
    return slot;
}

void DHeap4Simd::push(value_type v) {
    heap_keys_.push_back(v);
    heap_slots_.push_back(acquire_slot());
    sift_up(heap_keys_.size() - 1);
}

void DHeap4Simd::pop() {
    if (empty()) throw std::out_of_range("heap is empty");

    const slot_type root_slot = heap_slots_[0];
    if (heap_keys_.size() == 1) {
        heap_keys_.pop_back();
        heap_slots_.pop_back();
        release_slot(root_slot);
        return;
    }

    heap_keys_[0] = heap_keys_.back();
    heap_slots_[0] = heap_slots_.back();
    heap_keys_.pop_back();
    heap_slots_.pop_back();
    release_slot(root_slot);
    sift_down(0, false);
}

void DHeap4Simd::sift_up(size_t i) {
    const value_type key = heap_keys_[i];
    const slot_type slot = heap_slots_[i];
    while (i > 0) {
        const size_t p = parent(i);
        if (key >= heap_keys_[p]) break;
        heap_keys_[i] = heap_keys_[p];
        heap_slots_[i] = heap_slots_[p];
        i = p;
    }
    heap_keys_[i] = key;
    heap_slots_[i] = slot;
}

void DHeap4Simd::sift_down(size_t i, bool heapify_phase) {
    const size_t n = heap_keys_.size();
    const value_type key = heap_keys_[i];
    const slot_type slot = heap_slots_[i];
    constexpr size_t kArity = static_cast<size_t>(kD);
    constexpr size_t kBuildMinArity = static_cast<size_t>(DHEAP_SIMD_BUILD_MIN_ARITY);
    constexpr size_t kPopMinSize = static_cast<size_t>(DHEAP_SIMD_POP_MIN_SIZE);

    while (true) {
        const size_t c = first_child(i);
        if (c >= n) break;

        size_t min_idx = c;
        value_type min_val = heap_keys_[c];
        bool used_simd = false;
        bool allow_simd = false;

#if USE_NEON
        if (kSimdPolicy == 1) {
            allow_simd = true;
        } else if (kSimdPolicy == 2) {
            if (heapify_phase) {
                if (kArity >= kBuildMinArity) {
                    const size_t top_level_limit = n / (kArity * kArity);
                    allow_simd = (i <= top_level_limit);
                }
            } else {
                allow_simd = (kD >= 16) && (i == 0) && (n >= kPopMinSize);
            }
        }

        if (allow_simd && kD == 4 && c + 3 < n) {
            const int32x4_t v = vld1q_s32(&heap_keys_[c]);
            const int32_t reduced_min = neon_min4(v);
            min_val = reduced_min;
            min_idx = c + neon_first_equal_index4(v, reduced_min);
            used_simd = true;
        } else if (allow_simd && kD == 8 && c + 7 < n) {
            const int32x4_t v0 = vld1q_s32(&heap_keys_[c]);
            const int32x4_t v1 = vld1q_s32(&heap_keys_[c + 4]);
            const int32_t min0 = neon_min4(v0);
            const int32_t min1 = neon_min4(v1);
            if (min1 < min0) {
                min_val = min1;
                min_idx = c + 4 + neon_first_equal_index4(v1, min1);
            } else {
                min_val = min0;
                min_idx = c + neon_first_equal_index4(v0, min0);
            }
            used_simd = true;
        } else if (allow_simd && kD == 16 && c + 15 < n) {
            const int32x4_t v0 = vld1q_s32(&heap_keys_[c]);
            const int32x4_t v1 = vld1q_s32(&heap_keys_[c + 4]);
            const int32x4_t v2 = vld1q_s32(&heap_keys_[c + 8]);
            const int32x4_t v3 = vld1q_s32(&heap_keys_[c + 12]);
            const int32_t min0 = neon_min4(v0);
            const int32_t min1 = neon_min4(v1);
            const int32_t min2 = neon_min4(v2);
            const int32_t min3 = neon_min4(v3);

            int32x4_t block_mins = vdupq_n_s32(min0);
            block_mins = vsetq_lane_s32(min1, block_mins, 1);
            block_mins = vsetq_lane_s32(min2, block_mins, 2);
            block_mins = vsetq_lane_s32(min3, block_mins, 3);

            const int32_t best_block_min = neon_min4(block_mins);
            const size_t block_idx = neon_first_equal_index4(block_mins, best_block_min);

            min_val = best_block_min;
            if (block_idx == 0) {
                min_idx = c + neon_first_equal_index4(v0, best_block_min);
            } else if (block_idx == 1) {
                min_idx = c + 4 + neon_first_equal_index4(v1, best_block_min);
            } else if (block_idx == 2) {
                min_idx = c + 8 + neon_first_equal_index4(v2, best_block_min);
            } else {
                min_idx = c + 12 + neon_first_equal_index4(v3, best_block_min);
            }
            used_simd = true;
        }
#endif
        if (!used_simd) {
            const size_t end = std::min(c + kArity, n);
            for (size_t j = c + 1; j < end; ++j) {
                if (heap_keys_[j] < min_val) {
                    min_val = heap_keys_[j];
                    min_idx = j;
                }
            }
        }

        if (min_val >= key) break;

        heap_keys_[i] = heap_keys_[min_idx];
        heap_slots_[i] = heap_slots_[min_idx];
        i = min_idx;
    }

    heap_keys_[i] = key;
    heap_slots_[i] = slot;
}
