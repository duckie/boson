#ifndef BOSON_LOGGER_H_
#define BOSON_LOGGER_H_

#include <ostream>
#include <string>
#include <thread>
#include <mutex>
#include "fmt/format.h"
#include "json_backbone.hpp"

namespace boson {

/**
 * Logger is a thread safe logger
 *
 * Useful to get readable logs through concurrent execution. The logger is
 * only used for debug in the boson framework.
 */
class logger {
  std::ostream& stream_;
  std::mutex lock_;

 public:
  logger(std::ostream& ouput);
  logger(logger const&) = delete;
  logger(logger&&) = default;
  logger& operator=(logger const&) = delete;
  logger& operator=(logger&&) = default;
  ~logger();

  template <class... Args>
  void log(char const* line_format, Args&&... args) {
    std::lock_guard<std::mutex> guard(lock_);
    this->stream_ << fmt::format(line_format, std::forward<Args>(args)...) << std::endl;
  }
};

namespace debug {

logger* logger_instance(std::ostream* new_stream = nullptr);

template <class... Args>
inline void log(char const* fmt, Args&&... args) {
#ifndef NDEBUG
  auto logger_ptr = logger_instance();
  if (logger_ptr) logger_ptr->log(fmt, std::forward<Args>(args)...);
#endif
}

}  // namespace debug

}  // namespace boson

#endif  // BOSON_LOGGER_H_
