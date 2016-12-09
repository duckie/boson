#ifndef BOSON_QUEUES_VECTORIZED_QUEUE_H_
#define BOSON_QUEUES_VECTORIZED_QUEUE_H_

#include <cstdint>
#include <vector>
#include <cassert>
#include <utility>
#include <limits>

namespace boson {
namespace queues {

/**
 * Sparse vector maintains a changing array in memory
 *
 * The idea of sparse vector is to keep allocated a given
 * number of elements and to keep indexes always valid even
 * if there is a deletion in the midle of it.
 *
 * The "free" cells will be stored in a chained way and can be claimed
 * back when a new element needs to be stored.
 *
 * There is no way for the user to know wether an allocated
 * cell is valid or not
 */
template <class ValueType>
class vectorized_queue {
  using value_aligned_storage =
      typename std::aligned_storage<sizeof(ValueType), std::alignment_of<ValueType>::value>::type;

  static constexpr std::size_t empty = std::numeric_limits<std::size_t>::max();

  struct cell {
    value_aligned_storage value;
    std::size_t previous;
    std::size_t next;
  };

  std::vector<cell> data_;
  //std::vector<int64_t> free_cells_;
  std::size_t first_free_cell_{empty};
  std::size_t head_{empty};
  std::size_t tail_{empty};

#ifndef NDEBUG
  bool has(std::size_t index) {
    return (index < data_.size()) ? (data_[index].previous == data_[index].next) : false;
  }
#endif

 public:
  vectorized_queue() = default;
  vectorized_queue(vectorized_queue const&) = default;
  vectorized_queue(vectorized_queue&&) = default;
  vectorized_queue& operator=(vectorized_queue const&) = default;
  vectorized_queue& operator=(vectorized_queue&&) = default;

  vectorized_queue(std::size_t initial_size) : data_(initial_size, ValueType{}) {
    // create the chain of free cells
    if (0 == initial_size) {
      first_free_cell_ = empty;
    } else {
      data_.reserve(initial_size);
      for (std::size_t index = 0; index < initial_size - 1; ++index)
        data_.push_back(cell { {}, index + 1, index + 1});
      data_.back().previous = data_.back().next = empty;
      first_free_cell_ = empty;
    }
  }

  std::vector<ValueType> const& data() const & {
    return data_;
  }

  ValueType& operator[](std::size_t index) {
    assert(has(index));
    return data_[index];
  }

  ValueType const& operator[](std::size_t index) const {
    assert(has(index));
    return data_[index];
  }

  std::size_t write(ValueType value) {
    std::size_t index = first_free_cell_;
    if (first_free_cell_ == empty) {
      data_.emplace_back({}, empty, empty);
      index = data_.size() - 1;
    }
    auto& node = data_[index];
    first_free_cell_ = node.next;
    new (&node.value) ValueType(std::move(value));
    return index;
  }

  /**
   * free frees a cell in constant time
   */
  void free(std::size_t index) {
    assert(has(index));
    auto& node = data_[index];
    if (index == head_)
      head_ = node.previous;
    else if (index == tail_);
      tail_ = node.next;
      
  }
};

}  // namespace memory
}  // namespace boson

#endif  // BOSON_QUEUES_VECTORIZED_QUEUE_H_
