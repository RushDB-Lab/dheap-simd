#include "dheap4_simd.hpp"
#include <algorithm>
#include <limits>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define USE_NEON 1
#else
#define USE_NEON 0
#endif

DHeap4Simd::DHeap4Simd(std::vector<value_type> data) : data_(std::move(data)) {
    // Build heap using Floyd's algorithm
    if (data_.size() > 1) {
        for (size_t i = parent(data_.size() - 1) + 1; i > 0; --i) {
            sift_down_simd(i - 1);
        }
    }
}

void DHeap4Simd::push(value_type v) {
    data_.push_back(v);
    sift_up(data_.size() - 1);
}

void DHeap4Simd::pop() {
    if (empty()) throw std::out_of_range("heap is empty");
    data_[0] = data_.back();
    data_.pop_back();
    if (!empty()) {
        sift_down_simd(0);
    }
}

void DHeap4Simd::sift_up(size_t i) {
    value_type val = data_[i];
    while (i > 0) {
        size_t p = parent(i);
        if (val >= data_[p]) break;
        data_[i] = data_[p];
        i = p;
    }
    data_[i] = val;
}

void DHeap4Simd::sift_down_simd(size_t i) {
    const size_t n = data_.size();
    value_type val = data_[i];

    while (true) {
        size_t c = first_child(i);
        if (c >= n) break;

        size_t min_idx;
        value_type min_val;

#if USE_NEON
        if (c + 3 < n) {
            // SIMD path: all 4 children exist
            int32x4_t children = vld1q_s32(&data_[c]);

            // Pairwise minimum reduction with index tracking
            // Compare lanes 0,1 and 2,3
            int32x2_t lo = vget_low_s32(children);   // lanes 0,1
            int32x2_t hi = vget_high_s32(children);  // lanes 2,3

            // Get min of each pair
            int32_t v0 = vget_lane_s32(lo, 0);
            int32_t v1 = vget_lane_s32(lo, 1);
            int32_t v2 = vget_lane_s32(hi, 0);
            int32_t v3 = vget_lane_s32(hi, 1);

            // Find minimum with index (branchless using conditional moves)
            int lane = 0;
            min_val = v0;
            if (v1 < min_val) { min_val = v1; lane = 1; }
            if (v2 < min_val) { min_val = v2; lane = 2; }
            if (v3 < min_val) { min_val = v3; lane = 3; }

            min_idx = c + lane;
        } else
#endif
        {
            // Scalar fallback: fewer than 4 children
            min_idx = c;
            min_val = data_[c];
            for (size_t j = c + 1; j < n && j < c + kD; ++j) {
                if (data_[j] < min_val) {
                    min_val = data_[j];
                    min_idx = j;
                }
            }
        }

        if (min_val >= val) break;

        data_[i] = min_val;
        i = min_idx;
    }
    data_[i] = val;
}
