#ifndef BOSON_MEMORY_FLATSET_H_
#define BOSON_MEMORY_FLATSET_H_

#include <algorithm>
#include <vector>

namespace boson {
namespace memory {

/**
 * flat_unordered_set is like boost::flat_unordered_set, but simpler
 *
 * This is a simple implementation to be used for resource
 * management algorithms. It does not need a full implementation
 */
template <class Value>
class flat_unordered_set {
  std::vector<Value> values_;

 public:
  using iterator = typename std::vector<Value>::iterator;
  using const_iterator = typename std::vector<Value>::const_iterator;

  flat_unordered_set() = default;
  flat_unordered_set(std::size_t size, Value default_value = Value{})
      : values_(size, default_value) {
  }

  iterator begin() {
    return std::begin(values_);
  }

  iterator end() {
    return std::end(values_);
  }

  const_iterator begin() const {
    return std::begin(values_);
  }

  const_iterator end() const {
    return std::end(values_);
  }

  std::pair<iterator, bool> insert(Value value) {
    auto position = std::find(std::begin(values_), std::end(values_), value);
    bool inserted = false;
    if (std::end(values_) == position) {
      values_.emplace_back(value);
      inserted = true;
    }

    return {position, inserted};
  }

  std::size_t erase(Value const& value) {
    auto pos = std::find(std::begin(values_),std::end(values_), value);
    if (pos == std::end(values_))
      return 0;
    values_.erase(pos);
    return 1;
  }

  inline std::size_t size() const {
    return values_.size();
  }
};

}  // namespace memory
}  // namespace boson

#endif  // BOSON_MEMORY_FLATSET_H_
