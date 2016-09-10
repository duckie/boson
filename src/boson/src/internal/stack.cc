#include "internal/stack.h"

namespace boson {
namespace internal {

void deallocate(stack_context& sctx) noexcept {
#if defined(BOSON_USE_VALGRIND)
  VALGRIND_STACK_DEREGISTER(sctx.valgrind_stack_id);
#endif

  void* vp = static_cast<char*>(sctx.sp) - sctx.size;
  // conform to POSIX.4 (POSIX.1b-1993, _POSIX_C_SOURCE=199309L)
  ::munmap(vp, sctx.size);
}
}
}
