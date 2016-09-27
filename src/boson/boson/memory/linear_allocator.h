#ifndef BOSON_MEMORY_LINEAR_ALLOCATOR_H_
#define BOSON_MEMORY_LINEAR_ALLOCATOR_H_
#include <cstdint>
#include <vector>

namespace boson {
namespace memory {

/**
 * Linear allocator allocates a pool og same objects in a continuous fashion
 *
 * Linear allocator uses more space to keep a chaunded index and make allocation
 * and deallocation in O(1)
 *
 * The linear allocator can only allocate one block at a time, so it is most
 * suitable for linked lists
 *
 */
template <class ValueType, std::size_t InitialSize = 0>
class linear_allocator {
  static constexpr std::size_t initial_size = InitialSize;

  std::vector<ValueType> data_;
  std::vector<int64_t> free_cells_;
  int64_t first_free_cell_{-1};

 public:
  using value_type = ValueType;
  // using pointer = ValueType*;
  // using const_pointer = ValueType const*;
  // using reference = ValueType&;
  // using const_reference = ValueType const&;

  linear_allocator(linear_allocator const&) = default;
  linear_allocator(linear_allocator&&) = default;
  linear_allocator& operator=(linear_allocator const&) = default;
  linear_allocator& operator=(linear_allocator&&) = default;

  linear_allocator() : data_(initial_size, ValueType{}) {
    // create the chain of free cells
    if (0 == initial_size) {
      first_free_cell_ = -1;
    } else {
      free_cells_.reserve(initial_size);
      for (std::size_t index = 0; index < initial_size - 1; ++index)
        free_cells_.push_back(index + 1);
      first_free_cell_ = 0;
      free_cells_.push_back(-1);
    }
  }

  // std::vector<ValueType> const& data() const& {
  // return data_;
  //}
  //
  // ValueType& operator[](size_t index) {
  // return data_[index];
  //}
  //
  // ValueType const& operator[](size_t index) const {
  // return data_[index];
  //}
  //
  // ValueType& at(size_t index) {
  // return data_.at(index);
  //}
  //
  // ValueType const& at(size_t index) const {
  // return data_.at(index);
  //}

  /**
   * Allocates gets a cell in constant time if there is some available
   *
   * If no cell is available, the vector must be strecthed thus costing
   * memory movements.
   */
  ValueType* allocate() {
    if (first_free_cell_ == -1) {
      // No free cell available,have to allocate
      data_.emplace_back();
      free_cells_.emplace_back(-1);
      return &data_.back();
    } else {
      std::size_t cell_index = first_free_cell_;
      first_free_cell_ = free_cells_[cell_index];
      free_cells_[cell_index] = -1;
      return &data_[cell_index];
    }
  }

  /**
   * free frees a cell in constant time
   */
  void free(size_t index) {
    free_cells_[index] = first_free_cell_;
    first_free_cell_ = index;
  }
};

}  // namespace memory
}  // namespace boson

#endif  // BOSON_MEMORY_LINEAR_ALLOCATOR_H_
