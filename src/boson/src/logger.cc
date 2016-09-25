#include "boson/logger.h"

namespace boson {

void logger::log_line(std::string line) {
  queue_.push(std::move(line));
}

logger::logger(std::ostream& output)
    : stream_(output), queue_(1000), writer_([this]() {
        for (;;) {
          thread_message_t msg;
          queue_.blocking_pop(msg);
          if (msg.is<std::string>())
            this->stream_ << msg.raw<std::string>() << std::endl;
          else
            break;
        }
      }) {
}

logger::~logger() {
  queue_.push(nullptr);
  writer_.join();
}

namespace debug {

logger* logger_instance(std::ostream* new_stream) {
  static std::unique_ptr<logger> global_logger_;
  if (new_stream)
    global_logger_.reset(new logger(*new_stream));
  return global_logger_.get();
}

}

}  // namespace boson
