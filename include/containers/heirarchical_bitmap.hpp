#ifndef HEIRARCHICAL_BITMAP_HPP
#define HEIRARCHICAL_BITMAP_HPP

#include <array>
#include <cstdint>

template <typename T>
class LeafNode {
 private:
  uint64_t mask;  // Mask indicates that a  value exists at that index.
  std::array<T, 64> data;

 public:
  T get(uint32_t idx) {
    idx = (idx << 6);  // Least significant 6 bits.
    idx = (idx >> 6);  // Converting those to a number between 0 and 63.
    return data[idx];
  }
};

// NOTE: currently supporting 32 bit price ONLY
// So 32 - 6 = 26 bits remaining.
// Possible entires = 2 ** 26  = (2 ** 6) * (2 ** 20) = 64 * (2 ** 20)
//
template <typename T>
class HeirarchicalBitmap {
 private:
};
#endif  // !HEIRARCHICAL_BITMAP_HPP
