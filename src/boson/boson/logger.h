#ifndef BOSON_LOGGER_H_
#define BOSON_LOGGER_H_

#include <thread>
#include <string>
#include <ostream>
#include "queues/mpmc.h"
#include "fmt/format.h"

namespace boson {

/**
 * Logger is a thread safe logger
 *
 * Useful to get readable logs through concurrent execution. The logger is
 * only used for debug in the boson framework.
 */
class logger {
  std::ostream& stream_;
  queues::unbounded_mpmc<std::string> queue_;
  std::thread writer_;
  
  void log_line(std::string line);

 public:
  logger(std::otstream& ouput);
  logger(logger const&) = delete;
  logger(logger&&) = default;
  logger& operator=(logger const&) = delete;
  logger& operator=(logger&&) = default;
  ~logger();

  template <class ... Args>
  void log(char const * line_format, Args&& ... args) {
    this->log_line(fmt::format(line_format, std::foward<Args>(args)...));
  }
};

}  // namespace boson

#endif  // BOSON_LOGGER_H_
