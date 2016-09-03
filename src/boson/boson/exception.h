#ifndef BOSON_EXCEPTION_H_
#define BOSON_EXCEPTION_H_
#include <exception>
#include <string>

namespace boson {

class exception : public std::exception {
  std::string message_;

 public:
  exception(std::string const& message);
  exception(std::string&& message);
  exception(exception const&) = default;
  exception(exception&&) = default;
  exception& operator=(exception const&) = default;
  exception& operator=(exception&&) = default;

  char const* what() const noexcept override;
};

}  // namespace boson

#endif  // BOSON_EXCEPTION_H_
