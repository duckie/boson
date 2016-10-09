#include "boson/logger.h"

namespace boson {

logger::logger(std::ostream& output) : stream_(output) {
}

logger::~logger() {
}

namespace debug {

logger* logger_instance(std::ostream* new_stream) {
  static std::unique_ptr<logger> global_logger_;
  if (new_stream) global_logger_.reset(new logger(*new_stream));
  return global_logger_.get();
}
}

}  // namespace boson
