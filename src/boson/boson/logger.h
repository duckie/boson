#ifndef BOSON_LOGGER_H_
#define BOSON_LOGGER_H_

#include <ostream>
#include <string>
#include <thread>
#include "fmt/format.h"
#include "json_backbone.hpp"
#include "queues/mpmc.h"

namespace boson {

/**
 * Logger is a thread safe logger
 *
 * Useful to get readable logs through concurrent execution. The logger is
 * only used for debug in the boson framework.
 */
class logger {
  using thread_message_t = json_backbone::variant<std::nullptr_t, std::string>;

  std::ostream& stream_;
  queues::unbounded_mpmc<thread_message_t> queue_;
  std::thread writer_;

  void log_line(std::string line);

 public:
  logger(std::ostream& ouput);
  logger(logger const&) = delete;
  logger(logger&&) = default;
  logger& operator=(logger const&) = delete;
  logger& operator=(logger&&) = default;
  ~logger();

  template <class... Args>
  void log(char const* line_format, Args&&... args) {
    this->log_line(fmt::format(line_format, std::forward<Args>(args)...));
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
