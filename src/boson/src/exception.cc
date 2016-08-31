#include "exception.h"

namespace boson {
exception::exception(std::string const& message) : message_(message) {
}
exception::exception(std::string&& message) : message_(std::move(message)) {
}
char const * exception::what() const noexcept {
  return message_.c_str();
}
}  // namespace boson
