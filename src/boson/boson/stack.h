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
#include <cassert>


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

template <std::size_t Size, std::size_t PageSize, std::size_t LockedSize = 0, bool Protected = false>
struct basic_stack_traits {
  static constexpr std::size_t stack_size = Size;
  static constexpr std::size_t page_size = PageSize;
  static constexpr std::size_t locked_size = LockedSize;
  static constexpr bool is_protected = Protected;
};

// TODO: Those are unix specifics, to be defined elsewhere
using default_stack_traits = basic_stack_traits<8*1024*1024,64*1024,8*1024>;

template <class Traits> 
stack_context allocate_stack() {
#if defined(MAP_ANON)
  void* vp = ::mmap(0, Traits::stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
  void* vp =
      ::mmap(0, Traits::stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
  if (MAP_FAILED == vp) throw std::bad_alloc();

  // Will make use of C++17 constexpr if
  if (Traits::is_protected) {
#ifdef NDEBUG
    ::mprotect( vp, Traits::page_size, PROT_NONE) 
#else
     const int result( ::mprotect( vp, Traits::page_size, PROT_NONE) );
     assert( 0 == result);
#endif
  }

  if (0 < Traits::locked_size) {
  }
  //// conforming to POSIX.1-200
  //#if defined(BOOST_DISABLE_ASSERTS)
  //::mprotect( vp, traits_type::page_size(), PROT_NONE);
  //#else
  // const int result( ::mprotect( vp, traits_type::page_size(), PROT_NONE) );
  // BOOST_ASSERT( 0 == result);
  //#endif

  stack_context sctx;
  sctx.size = Traits::stack_size;
  sctx.sp = static_cast<char*>(vp) + sctx.size;
#if defined(BOSON_USE_VALGRIND)
  sctx.valgrind_stack_id = VALGRIND_STACK_REGISTER(sctx.sp, vp);
#endif
  return sctx;
};

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

#endif  // BOSON_STACK_H_
