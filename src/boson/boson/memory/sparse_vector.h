#ifndef BOSON_MEMORY_SPARSE_VECTOR_H_
#define BOSON_MEMORY_SPARSE_VECTOR_H_
#include <vector>
#include <cstdint>

namespace boson {
namespace memory {

/**
 * Sparse vector maintains a changing array in memory
 *
 * The idea of sparse vector is to keep allocated a given
 * number of elements and to keep indexes always valid even
 * if there is a deletion in the midelle of it.
 *
 * The "free" cells will be stored in a chained way and can be claimed
 * back when a new element needs to be stored.
 *
 * There is no way for the user to know wether an allocated
 * cell is valid or not
 */
template <class ValueType> class sparse_vector {
  std::vector<ValueType> data_;
  std::vector<int64_t> free_cells_;
  int64_t first_free_cell_{-1};

 public:
  sparse_vector() = default;
  sparse_vector(sparse_vector const&) = default;
  sparse_vector(sparse_vector&&) = default;
  sparse_vector& operator=(sparse_vector const&) = default;
  sparse_vector& operator=(sparse_vector&&) = default;

  sparse_vector(size_t initial_size) : data_(initial_size, ValueType{})  {
    // create the chain of free cells
    if (0 == initial_size) {
      first_free_cell_ = -1;
    }
    else {
      free_cells_.reserve(initial_size);
      for (size_t index = 0; index < initial_size - 1; ++index) free_cells_.push_back(index + 1);
      first_free_cell_ = 0;
      free_cells_.push_back(-1);
    }
  }

  std::vector<ValueType> const& data() const & { return data_; }

  ValueType& operator[] (size_t index) {
    return data_[index];
  }

  ValueType const& operator[] (size_t index) const {
    return data_[index];
  }

  ValueType& at(size_t index) {
    return data_.at(index);
  }

  ValueType const& at (size_t index) const {
    return data_.at(index);
  }

  /**
   * Allocates gets a cell in constant time if there is some available
   *
   * If no cell is available, the vector must be strecthed thus costing
   * memory movements.
   */
  size_t allocate() {
    if (first_free_cell_ == -1) {
      // No free cell available,have to allocate
      data_.emplace_back();
      free_cells_.emplace_back(-1);
      return data_.size()-1;
    }
    else {
      size_t cell_index = first_free_cell_;
      first_free_cell_ = free_cells_[cell_index];
      free_cells_[cell_index] = -1;
      return cell_index;
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

#endif  // BOSON_MEMORY_SPARSE_VECTOR_H_
