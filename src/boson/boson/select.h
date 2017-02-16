#ifndef BOSON_SELECT_H_
#define BOSON_SELECT_H_
#include "syscalls.h"
#include "channel.h"
#include "mutex.h"
#include "exception.h"
#include "syscall_traits.h"
#include "std/experimental/apply.h"

namespace boson {

namespace internal {
namespace select_impl {

template <class Func, class Arguments, class LocalData> class event_storage;
template <class Func, class ... Args, class ... Data> class event_storage<Func, std::tuple<Args...>, std::tuple<Data...>> {
 protected:
  Func func_;
  std::tuple<Args...> args_;
  std::tuple<Data...> data_;

 public:
  using func_type = Func;

  event_storage(Func && cb, Args... args, Data... data) : func_{std::move(cb)}, args_{args...}, data_{data...} {
  }

  event_storage(Func const& cb, Args... args, Data... data) : func_{cb}, args_{args...}, data_{data...} {
  }
};

template <class Func>
struct event_timer_storage
    : public event_storage<Func, std::tuple<>, std::tuple<internal::routine_time_point>> {
  // Reference parent type
  using parent_storage =
      event_storage<Func, std::tuple<>, std::tuple<internal::routine_time_point>>;
  // Inherit ctors
  using parent_storage::parent_storage;
  using return_type = decltype(std::declval<Func>()());

  static return_type execute(event_timer_storage* self, internal::event_type, bool) {
    return self->func_();
  }

  bool subscribe(internal::routine* current) {
    current->add_timer(std::get<0>(this->data_));
    return false;
  }
};

template <class Func, int SyscallId, class... Args>
struct event_syscall_storage : protected event_storage<Func, std::tuple<Args...>, std::tuple<int>> {
  // Reference parent type
  using parent_storage = event_storage<Func, std::tuple<Args...>, std::tuple<int>>;
  // Inherit ctors
  using parent_storage::parent_storage;
  using return_type = decltype(std::declval<Func>()(std::declval<int>()));

  static return_type execute(event_syscall_storage* self, internal::event_type,
                                      bool event_round_cancelled) {
    return event_round_cancelled
               ? self->func_(std::get<0>(self->data_))
               : self->func_(syscall_callable<SyscallId>::apply_call(self->args_));
  }

  bool subscribe(internal::routine* current) {
    std::get<0>(this->data_) = syscall_callable<SyscallId>::apply_call(this->args_);
    if (std::get<0>(this->data_) < 0 && (EAGAIN == errno || EWOULDBLOCK == errno)) {
      add_event<syscall_traits<SyscallId>::is_read>::apply(current, std::get<0>(this->args_));
      return false;
    }
    return true;
  }
};

// Specialization for the connect syscall
template <class Func, class... Args>
struct event_syscall_storage<Func, SYS_connect, Args...> : protected event_storage<Func, std::tuple<Args...>, std::tuple<int>> {
  // Reference parent type
  using parent_storage = event_storage<Func, std::tuple<Args...>, std::tuple<int>>;
  // Inherit ctors
  using parent_storage::parent_storage;
  using return_type = decltype(std::declval<Func>()(std::declval<int>()));

  static return_type execute(event_syscall_storage* self, internal::event_type type,
                             bool event_round_cancelled) {
    int& return_code{std::get<0>(self->data_)};
    if (!event_round_cancelled) {
      return_code = -1;
      if (internal::event_type::io_write == type) {
        socklen_t optlen = sizeof(return_code);
        if (::getsockopt(std::get<0>(self->args_), SOL_SOCKET, SO_ERROR, &return_code, &optlen) < 0)
          throw boson::exception(std::string("boson::select_any(event_connect) getsockopt failed (") + ::strerror(errno) + ")");
        errno = 0;
        if (0 != return_code) {
          errno = return_code;
          return_code = -1;
        }
      }
    }
    return self->func_(return_code);
  }

  bool subscribe(internal::routine* current) {
    std::get<0>(this->data_) = syscall_callable<SYS_connect>::apply_call(this->args_);
    if (std::get<0>(this->data_) < 0 && EINPROGRESS == errno) {
      add_event<syscall_traits<SYS_connect>::is_read>::apply(current, std::get<0>(this->args_));
      return false;
    }
    return true;
  }
};

class event_semaphore_wait_base_storage {
    shared_semaphore& sema_;

 public:
    inline event_semaphore_wait_base_storage(shared_semaphore& sema) : sema_{sema} {
    }

    inline bool subscribe(internal::routine* current) {
      int result = sema_.impl_->counter_.fetch_sub(1, std::memory_order_acquire);
      if (result <= 0) {
        current->add_semaphore_wait(sema_.impl_.get());
        return false;
      }
      return true;
    }
};

template <class Func>
class event_mutex_lock_storage : public event_semaphore_wait_base_storage {
    Func func_;

 public:
    using func_type = Func;
    using return_type = decltype(std::declval<Func>()());

    static return_type execute(event_mutex_lock_storage* self, internal::event_type type,bool) {
        return self->func_();
    }

    event_mutex_lock_storage (mutex& mut, Func&& cb)
        : event_semaphore_wait_base_storage{mut.impl_},
          func_{std::move(cb)} {
    }

    event_mutex_lock_storage (mutex& mut, Func const& cb)
        : event_semaphore_wait_base_storage{mut.impl_},
          func_{cb} {
    }
};

template <class ContentType, std::size_t Size, class Func>
class event_channel_read_storage : public event_semaphore_wait_base_storage {
    channel<ContentType,Size>& channel_;
    ContentType& value_;
    Func func_;

 public:
    using channel_type = channel<ContentType,Size>;
    using func_type = Func;
    using return_type = decltype(std::declval<Func>()(bool{}));

    static return_type execute(event_channel_read_storage* self, internal::event_type type,bool) {
        self->channel_.consume_read(self->value_);
        return self->func_(type == internal::event_type::sema_wait);
    }

    event_channel_read_storage(channel_type& channel, ContentType& value, Func&& cb)
        : event_semaphore_wait_base_storage{channel.channel_->readers_slots_},
          channel_{channel},
          value_{value},
          func_{std::move(cb)} {
    }

    event_channel_read_storage(channel_type& channel, ContentType& value, Func const& cb)
        : event_semaphore_wait_base_storage{channel.channel_->readers_slots_},
          channel_{channel},
          value_{value},
          func_{cb} {
    }
};

template <class ContentType, std::size_t Size, class Func>
class event_channel_write_storage : public event_semaphore_wait_base_storage {
    channel<ContentType,Size>& channel_;
    ContentType value_;
    Func func_;

 public:
    using channel_type = channel<ContentType,Size>;
    using func_type = Func;
    using return_type = decltype(std::declval<Func>()(bool{}));

    static return_type execute(event_channel_write_storage* self, internal::event_type type,bool) {
        self->channel_.consume_write(std::move(self->value_));
        return self->func_(type == internal::event_type::sema_wait);
    }

    event_channel_write_storage(channel_type& channel, ContentType value, Func&& cb)
        : event_semaphore_wait_base_storage{channel.channel_->writer_slots_}, channel_{channel}, value_{value}, func_{std::move(cb)} {
    }

    event_channel_write_storage(channel_type& channel, ContentType value, Func const& cb)
        : event_semaphore_wait_base_storage{channel.channel_->writer_slots_}, channel_{channel}, value_{value}, func_{cb} {
    }
};

template <class Selector, class ReturnType> 
auto make_selector_execute() -> decltype(auto) {
  return [](void* data, internal::event_type type, bool event_round_cancelled) -> ReturnType {
    return Selector::execute(static_cast<Selector*>(data), type, event_round_cancelled);
  };
}

template <class Selector> 
auto make_selector_subscribe() -> decltype(auto) {
  return [](void* data, internal::routine* current) -> bool {
    return static_cast<Selector*>(data)->subscribe(current);
  };
}

}
}

template <class Func>
internal::select_impl::event_timer_storage<Func> event_timer(int timeout_ms, Func&& cb) {
  return {std::forward<Func>(cb),
          std::chrono::time_point_cast<std::chrono::milliseconds>(
              std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(timeout_ms))};
}

template <class Func>
internal::select_impl::event_timer_storage<Func> event_timer(std::chrono::milliseconds timeout, Func&& cb) {
  return {std::forward<Func>(cb), std::chrono::time_point_cast<std::chrono::milliseconds>(
                                      std::chrono::high_resolution_clock::now() + timeout)};
}

template <class Func> 
internal::select_impl::event_syscall_storage<Func, SYS_read, fd_t, void*, size_t>
event_read(fd_t fd, void* buf, size_t count, Func&& cb) {
    return {std::forward<Func>(cb),fd,buf,count,0};
}

template <class Func> 
internal::select_impl::event_syscall_storage<Func, SYS_recvfrom, fd_t, void*, size_t, int, sockaddr *, socklen_t *>
event_recv(fd_t fd, void* buf, size_t count, int flags, Func&& cb) {
    return {std::forward<Func>(cb),fd,buf,count,flags,nullptr,nullptr,0};
}

template <class Func> 
internal::select_impl::event_syscall_storage<Func, SYS_accept, socket_t, sockaddr*, socklen_t*>
event_accept(socket_t socket, sockaddr* address, socklen_t* address_len, Func&& cb) {
    return {std::forward<Func>(cb), socket, address, address_len, 0};
}

template <class Func> 
internal::select_impl::event_syscall_storage<Func, SYS_write, fd_t, void*, size_t>
event_write(fd_t fd, void* buf, size_t count, Func&& cb) {
    return {std::forward<Func>(cb),fd,buf,count,0};
}

template <class Func> 
internal::select_impl::event_syscall_storage<Func, SYS_sendto, fd_t, void*, size_t, int, sockaddr *, socklen_t *>
event_send(fd_t fd, void* buf, size_t count, int flags, Func&& cb) {
    return {std::forward<Func>(cb),fd,buf,count,flags,nullptr,nullptr,0};
}

template <class Func> 
internal::select_impl::event_syscall_storage<Func, SYS_connect, socket_t, const sockaddr*, socklen_t>
event_connect(socket_t sockfd, const sockaddr *addr, socklen_t addrlen, Func&& cb) {
    return {std::forward<Func>(cb),sockfd,addr,addrlen,0};
}

template <class Func>
internal::select_impl::event_mutex_lock_storage<Func> 
event_lock(mutex& mut, Func&& cb) {
  return {mut, std::forward<Func>(cb)};
}

template <class ContentType, std::size_t Size, class Func>
internal::select_impl::event_channel_read_storage<ContentType, Size, Func>
event_read(channel<ContentType,Size>& chan, ContentType& value, Func&& cb) {
    return {chan, value, std::forward<Func>(cb)};
}

template <class ContentType, std::size_t Size, class Func>
internal::select_impl::event_channel_write_storage<ContentType, Size, Func>
event_write(channel<ContentType,Size>& chan, ContentType value, Func&& cb) {
    return {chan, std::move(value), std::forward<Func>(cb)};
}


template <class ... Selectors> 
auto select_any(Selectors&& ... selectors) 
    -> std::common_type_t<typename Selectors::return_type ...>
{
  using return_type = std::common_type_t<typename Selectors::return_type...>;
  static std::array<bool (*)(void*, internal::routine*), sizeof...(Selectors)> subscribers{
      internal::select_impl::make_selector_subscribe<Selectors>()...};
  static std::array<return_type (*)(void*, internal::event_type, bool), sizeof...(Selectors)>
      callers{internal::select_impl::make_selector_execute<Selectors, return_type>()...};
  std::array<void*, sizeof...(Selectors)> selector_ptrs{(&selectors)...};

  internal::thread* this_thread = internal::current_thread();
  internal::routine* current_routine = this_thread->running_routine();
  current_routine->start_event_round();

  bool cancel = false;
  size_t index = 0;
  for (; index < sizeof ... (Selectors); ++index) {
    cancel = (*subscribers[index])(selector_ptrs[index],current_routine);
    if (cancel)
        break;
  }
  if (cancel) {
    current_routine->cancel_event_round();
    //yield();
  }
  else {
    current_routine->commit_event_round();
    index = current_routine->happened_index();
  }
  return (*callers[index])(selector_ptrs[index], current_routine->happened_type(), cancel);
}



};  // namespace boson

#endif  // BOSON_SELECT_H_
