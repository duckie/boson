#include "boson/logger.h"

namespace boson {

void logger::log_line(std::string line) {
  queue_.push(line);
}

logger::logger(std::ostream& output) 
: stream_(output), queue_(1000), writer_{
{
}
  

}  // namespace boson
