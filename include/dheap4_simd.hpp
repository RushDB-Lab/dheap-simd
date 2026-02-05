#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>

class DHeap4Simd {
public:
    using value_type = int32_t;

    DHeap4Simd() = default;
    explicit DHeap4Simd(std::vector<value_type> data);

    size_t size() const noexcept { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }

    const value_type& top() const {
        if (empty()) throw std::out_of_range("heap is empty");
        return data_[0];
    }

    void push(value_type v);
    void pop();
    void clear() noexcept { data_.clear(); }
    void reserve(size_t n) { data_.reserve(n); }

private:
    static constexpr int kD = 4;
    std::vector<value_type> data_;

    static size_t parent(size_t i) noexcept { return (i - 1) >> 2; }
    static size_t first_child(size_t i) noexcept { return (i << 2) + 1; }

    void sift_up(size_t i);
    void sift_down_simd(size_t i);
};
