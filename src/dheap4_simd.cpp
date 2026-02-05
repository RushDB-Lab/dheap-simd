#include "dheap4_simd.hpp"
#include <algorithm>

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
    if (data_.size() == 1) {
        data_.pop_back();
        return;
    }
    data_[0] = data_.back();
    data_.pop_back();
    sift_down_simd(0);
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
        const size_t c = first_child(i);
        if (c >= n) break;

        size_t min_idx;
        value_type min_val;

        if (c + 3 < n) {
#if USE_NEON
            const int32x4_t children = vld1q_s32(&data_[c]);
            const int32_t v0 = vgetq_lane_s32(children, 0);
            const int32_t v1 = vgetq_lane_s32(children, 1);
            const int32_t v2 = vgetq_lane_s32(children, 2);
            const int32_t v3 = vgetq_lane_s32(children, 3);
#else
            const value_type v0 = data_[c];
            const value_type v1 = data_[c + 1];
            const value_type v2 = data_[c + 2];
            const value_type v3 = data_[c + 3];
#endif
            const bool take_1 = (v1 < v0);
            const value_type min01 = take_1 ? v1 : v0;
            const size_t idx01 = c + static_cast<size_t>(take_1 ? 1 : 0);

            const bool take_3 = (v3 < v2);
            const value_type min23 = take_3 ? v3 : v2;
            const size_t idx23 = c + static_cast<size_t>(take_3 ? 3 : 2);

            const bool take_23 = (min23 < min01);
            min_val = take_23 ? min23 : min01;
            min_idx = take_23 ? idx23 : idx01;
        } else {
            min_idx = c;
            min_val = data_[c];
            const size_t end = std::min(c + static_cast<size_t>(kD), n);
            for (size_t j = c + 1; j < end; ++j) {
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
