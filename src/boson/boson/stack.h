#ifndef BOSON_STACK_H_
#define BOSON_STACK_H_
#pragma once

extern "C" {
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
}

#include <cmath>
#include <cstddef>
#include <new>

#include <boost/assert.hpp>
#include <boost/config.hpp>

#include <boost/context/detail/config.hpp>
#include <boost/context/stack_context.hpp>
#include <boost/context/stack_traits.hpp>

#define BOSON_USE_VALGRIND
#if defined(BOSON_USE_VALGRIND)
#include <valgrind/valgrind.h>
#endif

namespace boson {
namespace context {

struct stack_context {
  std::size_t size{0};
  void* sp{nullptr};
  unsigned valgrind_stack_id{0};
};

template <std::size_t Size, std::size_t PageSize>
struct basic_stack_traits {
  using stack_size_type = std::integral_constant<std::size_t, Size>;
  using page_size_type = std::integral_constant<std::size_t, PageSize>;
};

template <typename Traits>
class stack_allocator {
 public:
  using traits_type = Traits;

  stack_allocator() = default;

  stack_context allocate() {
#if defined(MAP_ANON)
    void* vp = ::mmap(0, Traits::stack_size_type::value, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
#else
    void* vp = ::mmap(0, Traits::stack_size_type::value, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    if (MAP_FAILED == vp) throw std::bad_alloc();

    //// conforming to POSIX.1-2001
    //#if defined(BOOST_DISABLE_ASSERTS)
    //::mprotect( vp, traits_type::page_size(), PROT_NONE);
    //#else
    // const int result( ::mprotect( vp, traits_type::page_size(), PROT_NONE) );
    // BOOST_ASSERT( 0 == result);
    //#endif

    stack_context sctx;
    sctx.size = Traits::stack_size_type::value;
    sctx.sp = static_cast<char*>(vp) + sctx.size;
#if defined(BOSON_USE_VALGRIND)
    sctx.valgrind_stack_id = VALGRIND_STACK_REGISTER(sctx.sp, vp);
#endif
    return sctx;
  }

  void deallocate(stack_context& sctx) BOOST_NOEXCEPT_OR_NOTHROW {
#if defined(BOSON_USE_VALGRIND)
    VALGRIND_STACK_DEREGISTER(sctx.valgrind_stack_id);
#endif

    void* vp = static_cast<char*>(sctx.sp) - sctx.size;
    // conform to POSIX.4 (POSIX.1b-1993, _POSIX_C_SOURCE=199309L)
    ::munmap(vp, sctx.size);
  }
};
}
}

#endif  // BOSON_STACK_H_
