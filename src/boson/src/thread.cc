#include "thread.h"
#include "routine.h"
#include "engine.h"

namespace boson {
namespace context {

void engine_proxy::set_id() {
  current_thread_id_ = engine_->register_thread_id();
}

}
}  // namespace boson
